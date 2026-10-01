'use strict';
const assert = require('node:assert/strict');
const trace = require('../tools/trace/trace_converter.js');
const header = { kind: 'session', v: 1, clock: 'active_us', heap_hooks: true };
const row = (kind, ts, values = {}) => ({ kind, ts, addr: '0x0', old: '0x0',
    size: 0, line: 0, gap: 10000, name: '', file: '', ...values });
const source = (rows, session = header) => [session, ...rows].map(JSON.stringify).join('\n') + '\n';
let tests = 0;
function test(name, run) { run(); ++tests; console.log('PASS ' + name); }

test('Nested calls carry qualified names, source, and labelled object context', () => {
    const result = trace.convert(source([
        row('T', 10, { addr: '0x2000', name: 'MQTT publisher' }),
        row('B', 20, { name: 'void loop()', file: 'test.ino', line: 12 }),
        row('B', 30, { name: 'bool Loom_MongoDB::publish()', addr: '0x2000' }),
        row('A', 40, { addr: '0x4000', size: 512, line: 0x1001 }),
        row('E', 50), row('E', 60)
    ]));
    assert.deepEqual(result.report.allocations[0].stack, ['loop', 'Loom_MongoDB::publish']);
    assert.equal(result.report.allocations[0].owner, 'MQTT publisher');
    assert.equal(result.report.allocations[0].callerPc, '0x1001');
    assert.equal(result.report.calls[1].liveBytesAtEntry, 0);
    assert.equal(result.report.calls[1].liveBytesAtExit, 512);
    assert.equal(result.report.calls[0].file, 'test.ino');
    assert.equal(result.report.calls[0].line, 12);
    assert.equal(result.perfetto.traceEvents.find(e => e.args.Function === 'Loom_MongoDB::publish').dur, 20);
});
test('Snapshots distinguish allocation and free at the same timestamp by event index', () => {
    const report = trace.convert(source([row('A', 10, { addr: '0x1', size: 7 }),
        row('F', 10, { addr: '0x1' })])).report;
    assert.equal(trace.snapshot(report, 0).liveBytes, 7);
    assert.equal(trace.snapshot(report, 1).liveBytes, 0);
    assert.equal(report.allocations[0].endReason, 'Freed');
});
test('Reallocation moving a block replaces the old lifetime', () => {
    const report = trace.convert(source([row('A', 1, { addr: '0x1', size: 8 }),
        row('R', 2, { addr: '0x2', old: '0x1', size: 16 })])).report;
    assert.equal(trace.snapshot(report, 1).liveBytes, 16);
    assert.equal(report.allocations[0].endReason, 'Reallocated');
    assert.equal(report.allocations[1].address, '0x2');
});
test('In-place realloc has one live block with the new requested size', () => {
    const report = trace.convert(source([row('A', 1, { addr: '0x1', size: 8 }),
        row('R', 2, { addr: '0x1', old: '0x1', size: 16 })])).report;
    assert.equal(trace.snapshot(report, 1).live.length, 1);
    assert.equal(trace.snapshot(report, 1).liveBytes, 16);
});
test('Failed realloc keeps its previous allocation live', () => {
    const report = trace.convert(source([row('A', 1, { addr: '0x1', size: 8 }),
        row('N', 2, { old: '0x1', size: 1024 })])).report;
    assert.equal(trace.snapshot(report, 1).liveBytes, 8);
    assert.equal(report.allocationFailures, 1);
});
test('Zero-sized realloc marks uncertainty rather than claiming a free', () => {
    const report = trace.convert(source([row('A', 1, { addr: '0x1', size: 8 }),
        row('Z', 2, { old: '0x1' })])).report;
    assert.equal(trace.snapshot(report, 1).incomplete, true);
    assert.match(report.allocations[0].endReason, /unknown/);
});
test('Free of a pre-existing block does not create negative captured bytes', () => {
    const report = trace.convert(source([row('F', 1, { addr: '0x1' })])).report;
    assert.equal(trace.snapshot(report, 0).liveBytes, 0);
    assert.equal(report.allocations.length, 0);
});
test('Lost events invalidate identity and call boundaries explicitly', () => {
    const report = trace.convert(source([row('B', 1, { name: 'void loop()' }),
        row('A', 2, { addr: '0x1', size: 8 }), { kind: 'lost', ts: 3, count: 4 },
        row('A', 4, { addr: '0x2', size: 9 }), row('E', 5)])).report;
    assert.match(report.calls[0].status, /unknown/);
    assert.match(report.allocations[0].endReason, /unknown/);
    assert.equal(trace.snapshot(report, 3).liveBytes, 9);
    assert.equal(trace.snapshot(report, 3).incomplete, true);
});
test('Missing final return is incomplete, not a successful function return', () => {
    const result = trace.convert(source([row('B', 1, { name: 'void loop()' }), row('I', 9)]));
    assert.match(result.report.calls[0].status, /before return/);
    assert.match(result.perfetto.traceEvents.find(e => e.ph === 'X').name, /incomplete/);
});
test('Incomplete final JSON is recoverable, malformed interior JSON is rejected', () => {
    const result = trace.convert(source([row('I', 1)]) + '{"kind":"A"');
    assert.ok(result.report.warnings.some(x => /Unfinished/.test(x)));
    assert.throws(() => trace.convert(source([]) + 'broken\n' + JSON.stringify(row('I', 1))), /Invalid JSON/);
});
test('Concatenated boots, backward timestamps and invalid sizes are rejected', () => {
    assert.throws(() => trace.convert(source([header])), /Unknown/);
    assert.throws(() => trace.convert(source([row('I', 3), row('I', 2)])), /backwards/);
    assert.throws(() => trace.convert(source([row('A', 1, { size: -1 })])), /size/);
    assert.throws(() => trace.convert(source([row('A', 1, { addr: 'javascript:bad' })])), /address/);
});
test('Allocator checkpoint separates real totals from captured requested bytes', () => {
    const report = trace.convert(source([row('A', 1, { addr: '0x1', size: 100 }),
        row('C', 2, { size: 5000, line: 400, addr: '0x3', old: '0x100', gap: 12000 })])).report;
    const snapshot = trace.snapshot(report, 1);
    assert.equal(snapshot.liveBytes, 100);
    assert.equal(snapshot.checkpoint.usedBytes, 5000);
    assert.equal(snapshot.checkpoint.freeChunks, 3);
    assert.equal(snapshot.checkpoint.topFreeBytes, 256);
    assert.deepEqual(snapshot.checkpoint.liveAllocationIds, [report.allocations[0].id]);
});
test('Object labels do not count as extra heap and address reuse loses old labels', () => {
    const report = trace.convert(source([row('A', 1, { addr: '0x1', size: 100 }),
        row('T', 2, { addr: '0x1', size: 30, name: 'Packet buffer' }),
        row('F', 3, { addr: '0x1' }), row('A', 4, { addr: '0x1', size: 50 })])).report;
    assert.equal(report.allocations[0].label, 'Packet buffer');
    assert.equal(report.allocations[1].label, 'Unlabelled heap block');
    assert.equal(trace.snapshot(report, 1).liveBytes, 100);
});
test('Recorder overhead gets its own track and high timestamps retain precision', () => {
    const first = 4294967296 * 1000;
    const result = trace.convert(source([row('I', first), row('O', first + 10, { size: 25 })]));
    assert.equal(result.report.overheadUs, 25);
    const event = result.perfetto.traceEvents.find(e => e.ph === 'X');
    assert.equal(event.tid, 4); assert.equal(event.ts, 10); assert.equal(event.dur, 25);
});
test('Call-only mode explicitly states missing individual allocation capture', () => {
    const result = trace.convert(source([row('C', 1, { size: 3000 })], { ...header, heap_hooks: false }));
    assert.ok(result.report.warnings.some(x => /not linked/.test(x)));
    assert.ok(!result.perfetto.traceEvents.some(e => e.name === 'Captured heap allocations (requested bytes)'));
});
test('Object inventory includes baseline mux children without inventing heap bytes', () => {
    const report = trace.convert(source([
        row('U', 1, { addr: '0x10', name: 'SHT31_2', old: '0x20', line: 3, gap: 0x44, file: 'ready' }),
        row('B', 2, { name: 'void loop()' }),
        row('B', 3, { name: 'void Loom_SHT31::measure()', addr: '0x10', size: 7000, old: '0x100' }),
        row('A', 4, { addr: '0x30', size: 16 }),
        row('E', 5, { size: 7024, old: '0xf0' }), row('E', 6), row('D', 7, { addr: '0x10' })
    ])).report;
    const snapshot = trace.snapshot(report, 3);
    assert.equal(snapshot.activeObjects.length, 1);
    assert.equal(snapshot.activeObjects[0].port, 2);
    assert.equal(snapshot.activeObjects[0].i2cAddress, 0x44);
    assert.equal(snapshot.activeObjects[0].allocationId, null);
    assert.equal(snapshot.liveBytes, 16);
    assert.deepEqual(snapshot.callStack.map(call => call.name), ['loop', 'Loom_SHT31::measure']);
    assert.match(report.allocations[0].owner, /SHT31_2.*mux port 2/);
    assert.equal(report.calls[1].heapChange, 24);
    assert.deepEqual(report.calls[1].stillLiveAtExitIds, [1]);
    assert.equal(trace.snapshot(report, 6).activeObjects.length, 0);
});
test('Object states, deletion and reuse are ordered at exact event boundaries', () => {
    const report = trace.convert(source([
        row('A', 1, { addr: '0x10', size: 128 }),
        row('U', 2, { addr: '0x10', name: 'SEN66_1', file: 'unavailable', gap: 0x6b, line: 2 }),
        row('U', 3, { addr: '0x10', name: 'SEN66_1', file: 'ready', gap: 0x6b, line: 2 }),
        row('D', 4, { addr: '0x10' }), row('F', 4, { addr: '0x10' }),
        row('U', 5, { addr: '0x10', name: 'Replacement sensor', file: 'ready', gap: -1 })
    ])).report;
    assert.equal(trace.snapshot(report, 1).activeObjects[0].state, 'unavailable');
    assert.equal(trace.snapshot(report, 2).activeObjects[0].state, 'ready');
    assert.equal(trace.snapshot(report, 2).activeObjects[0].allocationId, 1);
    assert.equal(trace.snapshot(report, 3).activeObjects.length, 0);
    assert.equal(trace.snapshot(report, 3).liveBytes, 128); // Retirement precedes allocator free.
    assert.equal(trace.snapshot(report, 4).liveBytes, 0);
    assert.equal(trace.snapshot(report, 5).activeObjects[0].moduleName, 'Replacement sensor');
});
test('Loss invalidates object states and re-observation starts a new known inventory', () => {
    const report = trace.convert(source([
        row('U', 1, { addr: '0x10', name: 'Sensor', file: 'ready', gap: -1 }),
        row('lost', 2, { count: 3 }),
        row('U', 3, { addr: '0x10', name: 'Sensor', file: 'unavailable', gap: -1 })
    ])).report;
    assert.equal(trace.snapshot(report, 1).activeObjects.length, 0);
    assert.equal(trace.snapshot(report, 2).activeObjects[0].state, 'unavailable');
    assert.equal(trace.snapshot(report, 2).incomplete, true);
});
test('Baseline object deletion clears names before the allocator reuses its address', () => {
    const report = trace.convert(source([
        row('U', 1, { addr: '0x10', name: 'Old baseline sensor', file: 'ready', gap: -1 }),
        row('F', 2, { addr: '0x10' }), row('A', 3, { addr: '0x10', size: 64 })
    ])).report;
    assert.equal(trace.snapshot(report, 1).activeObjects.length, 0);
    assert.equal(report.allocations[0].label, 'Unlabelled heap block');
});
test('A later label does not change the historical label at allocation time', () => {
    const report = trace.convert(source([
        row('A', 1, { addr: '0x10', size: 64 }), row('T', 2, { addr: '0x10', name: 'Known packet' })
    ])).report;
    assert.equal(trace.snapshot(report, 0).live[0].label, 'Unlabelled heap block');
    assert.equal(trace.snapshot(report, 1).live[0].label, 'Known packet');
});
test('Free heap and stack gap persist on call entry/return and unknown exits stay unknown', () => {
    const result = trace.convert(source([
        row('B', 1, { name: 'void measure()', size: 400, old: '0x100', gap: 12000 }),
        row('E', 2, { size: 450, old: '0xc0', gap: 11900 }),
        row('B', 3, { name: 'void unfinished()', size: 450, old: '0xc0', gap: 11800 })
    ]));
    const call = result.report.calls[0];
    assert.equal(call.heapFreeAtEntry, 256); assert.equal(call.heapFreeAtExit, 192);
    assert.equal(call.gapAtEntry, 12000); assert.equal(call.gapAtExit, 11900);
    assert.equal(result.report.checkpoints[1].freeBytes, 192);
    assert.equal(result.report.calls[1].heapFreeAtExit, null);
    assert.equal(result.report.calls[1].gapAtExit, null);
    const slice = result.perfetto.traceEvents.find(event => event.ph === 'X' && event.name === 'measure');
    assert.equal(slice.args['Free heap bytes on exit'], 192);
});
test('Production native recorder Chrome JSON and NDJSON reconstruct the same histories', () => {
    const fs = require('node:fs'), path = require('node:path');
    const read = name => fs.readFileSync(path.join(__dirname, 'fixtures', name), 'utf8');
    const chrome = trace.convert(read('native_recorder.perfetto.json')).report;
    const raw = trace.convert(read('native_recorder.ndjson')).report;
    for (const field of ['calls', 'allocations', 'objects', 'checkpoints']) assert.deepEqual(chrome[field], raw[field], field);
    assert.equal(chrome.calls.length, 1); assert.equal(chrome.allocations.length, 2);
});
test('Gas calls and returns distinguish ports while preserving source signatures', () => {
    const result = trace.convert(source([
        row('U', 1, { addr: '0x100', name: 'SO2 / DFMultiGasSensor_0', line: 1, gap: 0x74, file: 'ready', size: 128 }),
        row('U', 2, { addr: '0x200', name: 'CO / DFMultiGasSensor_1', line: 2, gap: 0x74, file: 'ready', size: 128 }),
        row('B', 3, { addr: '0x100', name: 'void Loom_DFMultiGasSensor::measure()', file: 'gas.cpp', line: 42 }),
        row('E', 4), row('B', 5, { addr: '0x200', name: 'void Loom_DFMultiGasSensor::measure()' }), row('E', 6)
    ]));
    assert.match(result.report.calls[0].displayName, /SO2.*mux port 0/);
    assert.match(result.report.calls[1].displayName, /CO.*mux port 1/);
    assert.match(result.report.history[3].eventName, /Return:.*SO2.*mux port 0/);
    const slice = result.perfetto.traceEvents.find(event => event.ph === 'X' && event.args['Call ID'] === 1);
    assert.match(slice.name, /SO2.*mux port 0/); assert.equal(slice.args['Source line'], 42);
});
test('Diagnostic values preserve signed status and UTC beyond 2038 in both formats', () => {
    const rows = [row('V', 1, { name: 'RTC UTC', file: 'UTC epoch seconds', size: 2200000000, gap: 0 }),
        row('V', 2, { name: 'RTC elapsed', file: 'seconds', size: 4294967295, gap: -1 })];
    const report = trace.convert(source(rows)).report;
    assert.deepEqual(report.history.map(event => event.value), [2200000000, -1]);
    assert.equal(report.history[0].category, 'Sleep & RTC');
    const chrome = JSON.stringify({ traceEvents: rows.map((row, i) => ({ ph: 'I', name: row.name, ts: row.ts,
        args: { 'Record kind': 'V', Value: i === 0 ? 2200000000 : -1, Unit: row.file } })) });
    assert.deepEqual(trace.convert(chrome).report.history.map(event => event.value), [2200000000, -1]);
    assert.throws(() => trace.convert(source([row('V', 1, { size: 0, gap: 2147483647 })])), /diagnostic value/);
});
test('Event labels resolve frees before deleting the ledger and keep past labels stable', () => {
    const report = trace.convert(source([row('A', 1, { addr: '0x100', size: 32 }),
        row('T', 2, { addr: '0x100', name: 'Packet buffer' }), row('F', 3, { addr: '0x100' })])).report;
    assert.match(report.history[0].eventName, /Allocate: 32 B.*0x100/);
    assert.match(report.history[2].eventName, /Free: 32 B.*Packet buffer/);
    assert.equal(trace.snapshot(report, 0).live[0].label, 'Unlabelled heap block');
});
test('Available RAM counters include reusable heap and only a positive measured gap', () => {
    const result = trace.convert(source([row('C', 1, { line: 128, gap: -10 }), row('B', 2, { old: '0x80', gap: 1000 }), row('E', 3, { old: '0x90', gap: 900 })]));
    assert.deepEqual(result.perfetto.traceEvents.filter(event => event.name === 'Available RAM estimate (no future stack reserve)').map(event => event.args.Bytes), [128, 1128, 1044]);
});
console.log(`All ${tests} trace converter tests passed.`);
