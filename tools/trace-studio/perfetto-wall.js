import { dateLabel } from './display-clock.js';

// Minimal writer for the stable Perfetto TrackEvent/ClockSnapshot protobuf API.
// Field numbers: github.com/google/perfetto/protos/perfetto/trace/{trace_packet,
// clock_snapshot,track_event/{track_event,track_descriptor,debug_annotation}}.proto
// Keep wall elapsed time separate from the unchanged awake-time Chrome JSON.
const utf8 = new TextEncoder();
function varint(value) {
    let n = BigInt.asUintN(64, BigInt(value)); const bytes = [];
    do { const byte = Number(n & 127n); n >>= 7n; bytes.push(byte | (n ? 128 : 0)); } while (n);
    return bytes;
}
function join(parts) {
    const result = new Uint8Array(parts.reduce((sum, part) => sum + part.length, 0));
    let offset = 0; for (const part of parts) { result.set(part, offset); offset += part.length; } return result;
}
const v = (field, value) => Uint8Array.from([...varint(field * 8), ...varint(value)]);
const b = (field, bytes) => join([Uint8Array.from([...varint(field * 8 + 2), ...varint(bytes.length)]), bytes]);
const s = (field, text) => b(field, utf8.encode(String(text)));
const ns = us => BigInt(Math.round(us)) * 1000n;
const annotation = (name, value) => b(4, join([s(10, name), s(6, typeof value === 'object' ? JSON.stringify(value) : String(value))]));

export function wallSelectionRange(report, wall, row, call) {
    if (!row) return null;
    const inCall = call && row.index >= call.startIndex && row.index <= call.endIndex;
    const at = index => wall.rows[Math.min(index, wall.rows.length - 1)].utcUs - wall.startUtcUs;
    return inCall ? { startUs: at(call.startIndex), endUs: at(call.endIndex) } : { startUs: at(row.index), endUs: at(row.index) };
}

export function perfettoWallTrace(converted, wall, zone = 'UTC') {
    if (!wall) throw new Error('No trustworthy RTC anchors for wall time. Use awake execution.');
    const chunks = [], descriptors = new Map(); let nextUuid = 10;
    const packet = fields => chunks.push(b(1, join([v(10, 1), ...fields])));
    const snapshotClock = (id, timestamp) => b(1, join([v(1, id), v(2, timestamp)]));
    packet([v(13, 1), b(6, join([snapshotClock(6, 0n), snapshotClock(1, ns(wall.startUtcUs)), v(2, 6)]))]);
    packet([b(60, join([v(1, 1), s(2, 'Calls and memory · estimated wall time'), v(11, 3)]))]);
    function track(key, name, counter = false, sizeUnit = false) {
        if (!descriptors.has(key)) {
            const id = nextUuid++; descriptors.set(key, id);
            const rank = name === 'Nested function calls' ? 0 : counter ? 10 : key === 'sleep' ? 30 : /overhead/i.test(name) ? 40 : /Sleep and RTC/.test(name) ? 25 : 20;
            packet([b(60, join([v(1, id), v(5, 1), v(12, rank), s(2, name), ...(counter ? [b(8, v(3, sizeUnit ? 3 : 2))] : [])]))]);
        }
        return descriptors.get(key);
    }
    const names = new Map(converted.perfetto.traceEvents.filter(e => e.ph === 'M' && e.name === 'thread_name').map(e => [e.tid, e.args.name]));
    const clockRows = wall.rows, history = converted.report.history;
    const at = (index, activeUs) => {
        const i = Math.max(0, Math.min(clockRows.length - 1, index));
        return activeUs + clockRows[i].utcUs - history[i].timeUs - wall.startUtcUs;
    };
    const events = [];
    for (const e of converted.perfetto.traceEvents) {
        if (e.ph === 'M') continue;
        const startIndex = e.args['Call entry event index'] ?? e.loomEventIndex ?? e.args['Loom event index'];
        if (!Number.isInteger(startIndex)) throw new Error('Reconvert this trace to attach event boundaries.');
        const start = at(startIndex, e.ts);
        const args = { ...e.args, 'Awake execution timestamp (us)': e.ts,
            'Estimated UTC': dateLabel(wall.startUtcUs + start), 'Estimated local time': dateLabel(wall.startUtcUs + start, zone),
            'Wall-clock accuracy': 'Estimated from RTC-second anchors; standby is included' };
        if (e.ph === 'C') {
            for (const [key, value] of Object.entries(e.args)) {
                if (key === 'Loom event index' || !Number.isSafeInteger(value)) continue;
                events.push({ time: start, index: startIndex, type: 4, track: track('counter:' + e.name + ':' + key, e.name + ' · ' + key + ' (last observed)', true, /bytes/i.test(key)), value });
            }
        } else {
            const uuid = track('slice:' + e.tid, names.get(e.tid) || 'Events');
            const type = e.ph === 'X' || e.ph === 'B' ? 1 : e.ph === 'E' ? 2 : 3;
            events.push({ time: start, index: startIndex, type, track: uuid, name: e.name, args });
            if (e.ph === 'X') {
                const endIndex = e.args['Call exit event index'] ?? startIndex;
                const end = at(endIndex, e.ts + e.dur);
                args['Awake duration (us)'] = e.dur; args['Wall elapsed duration (us)'] = end - start;
                events.push({ time: end, index: endIndex, type: 2, track: uuid });
            }
        }
    }
    const gaps = [];
    for (let i = 1; i < clockRows.length; i++) {
        if (clockRows[i].segment === clockRows[i - 1].segment) continue;
        const end = clockRows[i].utcUs - wall.startUtcUs;
        const start = history[i].timeUs + clockRows[i - 1].utcUs - history[i - 1].timeUs - wall.startUtcUs;
        if (end > start) gaps.push({ start, end, index: i });
    }
    events.sort((a, c) => a.time - c.time || a.index - c.index);
    // A function can remain on the stack during standby, but its CPU execution
    // must not look like a two-hour active bar. Close/reopen all nested slices
    // at each gap, preserving their hierarchy and original duration annotations.
    const visible = [], stacks = new Map(); let gapIndex = 0;
    const splitGap = gap => {
        for (const [id, stack] of stacks) {
            for (let i = stack.length - 1; i >= 0; i--) visible.push({ time: gap.start, type: 2, track: id });
            for (const begin of stack) visible.push({ ...begin, time: gap.end, args: { ...begin.args, 'Resumed after standby': true } });
        }
    };
    for (const e of events) {
        while (gapIndex < gaps.length && gaps[gapIndex].start <= e.time) splitGap(gaps[gapIndex++]);
        const stack = stacks.get(e.track) || []; stacks.set(e.track, stack);
        if (e.type === 1) stack.push(e);
        if (e.type === 2) stack.pop();
        visible.push(e);
    }
    while (gapIndex < gaps.length) splitGap(gaps[gapIndex++]);
    const sleepTrack = track('sleep', 'Standby gaps · no RAM measurements');
    for (const gap of gaps) visible.push({ time: gap.start, type: 1, track: sleepTrack, name: 'Estimated standby · memory not sampled' }, { time: gap.end, type: 2, track: sleepTrack });
    visible.sort((a, c) => a.time - c.time); // Stable: gap ends precede resumed child/parent events.
    for (const e of visible) {
        const fields = [v(9, e.type), v(11, e.track)];
        if (e.name) fields.push(s(23, e.name), s(22, 'Loom'));
        if (e.type === 4) fields.push(v(30, e.value));
        if (e.args) for (const [key, value] of Object.entries(e.args)) fields.push(annotation(key, value));
        packet([v(8, ns(e.time)), v(58, 6), b(11, join(fields))]);
    }
    return join(chunks);
}
