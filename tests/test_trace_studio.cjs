// Navigation decisions use recorded event indices, including simultaneous events and incomplete calls.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
(async () => {
    const source = fs.readFileSync(path.join(__dirname, '../tools/trace-studio/selection.js'), 'utf8');
    const { selectionRange } = await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
    const call = { startIndex: 3, endIndex: 7, startUs: 1000, endUs: 2000 };
    for (const index of [3, 4, 7]) {
        assert.deepEqual(selectionRange({ index, timeUs: 1000 }, call), { startUs: 1000, endUs: 2000 });
    }
    // The same timestamp does not make an unrelated event part of the comparison call.
    assert.deepEqual(selectionRange({ index: 2, timeUs: 1000 }, call), { startUs: 1000, endUs: 1000 });
    assert.deepEqual(selectionRange({ index: 8, timeUs: 2500 }, call), { startUs: 2500, endUs: 2500 });
    assert.deepEqual(selectionRange({ index: 12, timeUs: 4000 }, null), { startUs: 4000, endUs: 4000 });
    assert.equal(selectionRange(null, call), null);
    assert.deepEqual(selectionRange({ index: 5, timeUs: 1500 }, { ...call, status: 'Trace ended before return was captured' }), { startUs: 1000, endUs: 2000 });
    console.log('PASS selected Perfetto ranges: call entry/return, exact event ordering, unrelated events, incomplete calls');
})().catch(error => { console.error(error); process.exitCode = 1; });
