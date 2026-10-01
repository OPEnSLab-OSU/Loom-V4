const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
(async () => {
    const { dateLabel, traceDisplayClock, clockTicks, zoneOffset } = await import(pathToFileURL(path.join(__dirname, '../tools/trace-studio/display-clock.js')).href);
    const { perfettoWallTrace, wallSelectionRange } = await import(pathToFileURL(path.join(__dirname, '../tools/trace-studio/perfetto-wall.js')).href);
    const converter = require('../tools/trace/trace_converter.js');
    const { reconstructWallClock } = await import(pathToFileURL(path.join(__dirname, '../tools/trace-studio/wall-clock.js')).href);
    const october = Date.UTC(2026, 9, 1, 9, 17, 26) * 1000;
    assert.match(dateLabel(october, 'America/Los_Angeles'), /02:17:26.*PDT/);
    assert.match(dateLabel(Date.UTC(2026, 11, 1, 9, 17, 26) * 1000, 'America/Los_Angeles'), /01:17:26.*PST/);
    assert.equal(dateLabel(october), '2026-10-01 09:17:26.000 UTC');
    assert.equal(zoneOffset(october, 'America/Los_Angeles'), 'UTC-07:00');
    assert.throws(() => dateLabel(october, 'invalid-zone'));
    const traceFile = process.argv[2];
    if (!traceFile) throw new Error('Pass the overnight NDJSON to verify native Perfetto conversion.');
    const converted = converter.convert(fs.readFileSync(traceFile, 'utf8')), report = converted.report, wall = reconstructWallClock(report);
    assert.ok(wall && wall.sleepCount === 18);
    const utc = traceDisplayClock(report, wall, 'utc', 'America/Los_Angeles');
    const local = traceDisplayClock(report, wall, 'local', 'America/Los_Angeles');
    assert.equal(utc.at(2600), local.at(2600)); // Display timezone cannot alter event coordinates.
    assert.equal(traceDisplayClock(report, null, 'local', 'UTC').mode, 'active');
    assert.ok(clockTicks(0, .001).length > 1);
    const row = report.history[2600], call = report.calls.find(c => c.startIndex <= row.index && row.index <= c.endIndex);
    const range = wallSelectionRange(report, wall, row, call);
    assert.equal(range.startUs, wall.rows[call.startIndex].utcUs - wall.startUtcUs);
    const buffer = perfettoWallTrace(converted, wall, 'America/Los_Angeles');
    // Independent wire decoder checks packet structure, REALTIME anchor and nested
    // call balance. This catches unit/64-bit precision errors, not just byte output.
    function fields(bytes) {
        let p = 0; const result = [];
        const read = () => { let n = 0n, shift = 0n, byte; do { byte = bytes[p++]; n |= BigInt(byte & 127) << shift; shift += 7n; } while (byte & 128); return n; };
        while (p < bytes.length) { const tag = Number(read()), wire = tag & 7; assert.ok(wire === 0 || wire === 2);
            const value = wire === 0 ? read() : (() => { const size = Number(read()), data = bytes.slice(p, p + size); p += size; return data; })();
            result.push({ field: tag >> 3, value }); }
        assert.equal(p, bytes.length); return result;
    }
    const value = (list, field) => list.find(f => f.field === field)?.value;
    const packets = fields(buffer).map(f => { assert.equal(f.field, 1); return fields(f.value); });
    const snapshot = fields(value(packets[0], 6));
    const clocks = snapshot.filter(f => f.field === 1).map(f => fields(f.value));
    assert.equal(value(clocks.find(c => value(c, 1) === 1n), 2), BigInt(Math.round(wall.startUtcUs)) * 1000n);
    assert.equal(value(clocks.find(c => value(c, 1) === 6n), 2), 0n);
    const counterTracks = new Set(), sliceNames = new Map(), trackNames = new Map();
    for (const packet of packets) {
        const data = value(packet, 60); if (!data) continue;
        const descriptor = fields(data), id = value(descriptor, 1);
        trackNames.set(id, Buffer.from(value(descriptor, 2)).toString());
        if (value(descriptor, 8)) counterTracks.add(id);
    }
    const depths = new Map(); let starts = 0, ends = 0, counters = 0, lastTs = -1n;
    for (const packet of packets) {
        const eventData = value(packet, 11); if (!eventData) continue;
        const ts = value(packet, 8); assert.ok(ts >= lastTs && ts >= 0n); lastTs = ts;
        const event = fields(eventData), type = value(event, 9), id = value(event, 11);
        if (type === 1n) { starts++; depths.set(id, (depths.get(id) || 0) + 1); const stack = sliceNames.get(id) || []; stack.push(ts); sliceNames.set(id, stack); }
        if (type === 2n) { ends++; assert.ok(depths.get(id) > 0); depths.set(id, depths.get(id) - 1);
            const began = sliceNames.get(id).pop();
            if (trackNames.get(id) === 'Nested function calls') assert.ok(ts - began < 120n * 1000000000n, 'An awake call must not span hours of standby');
        }
        if (type === 4n) { counters++; assert.ok(counterTracks.has(id), 'Every counter must have a counter descriptor before its first value'); }
    }
    assert.equal(starts, ends); assert.ok(counters > 100); assert.ok([...depths.values()].every(n => n === 0));
    assert.ok(lastTs >= BigInt(Math.round(wall.endUtcUs - wall.startUtcUs)) * 1000n);
    assert.ok(converted.perfetto.traceEvents.filter(e => e.ph === 'C').every(e => !('Loom event index' in e.args)));
    console.log('PASS UTC/local DST display, unchanged coordinates, wall selections, native Perfetto clocks/counters/slice balance (' + buffer.length + ' bytes)');
})().catch(error => { console.error(error); process.exitCode = 1; });
