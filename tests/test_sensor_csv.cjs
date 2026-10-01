const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
(async () => {
    const source = fs.readFileSync(path.join(__dirname, '../tools/trace-studio/sensor-csv.js'), 'utf8');
    const { parseSensorCsv } = await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
    const preamble = 'device-serial\r\n\r\nID,timestamp,Gas,,\r\nname,time_utc,CO,Temp(C),\r\n';
    const data = parseSensorCsv(preamble + 'Wisp,2026-10-01 09:17:26,2,28,\r\nWisp,2026-10-01 09:20:26,3,29,\r\n');
    assert.deepEqual(data.intervals, [180]);
    assert.equal(data.rows[0].utcMs, Date.UTC(2026, 9, 1, 9, 17, 26));
    assert.equal(data.columns[3].label, 'Gas · Temp(C)');
    assert.equal(data.columns.length, 4); // Loom's legacy trailing separator is not a phantom field.
    assert.equal(data.hasChecksums, false);
    assert.deepEqual(data.warnings, []);
    const header = 'serial\r\n\r\nID,timestamp,Gas,\r\nname,time_utc,CO,checksum\r\n';
    const prefix = '"Wisp, café\nquote""",2026-10-01 09:17:26,2,';
    const sum = Buffer.from(prefix).reduce((total, byte) => (total + byte) & 65535, 0);
    const good = parseSensorCsv(header + prefix + sum + '\r\n');
    assert.equal(good.rows[0].checksum, true);
    assert.equal(good.rows[0].cells[0], 'Wisp, café\nquote"');
    assert.equal(parseSensorCsv(header + prefix + (sum + 1) + '\r\n').rows[0].checksum, false);
    assert.equal(parseSensorCsv(header + prefix + sum).rows[0].checksum, false);
    assert.ok(parseSensorCsv(preamble + 'Wisp,bad-date,2,28,\r\n').warnings.some(value => /invalid UTC/.test(value)));
    assert.ok(parseSensorCsv(preamble + 'Wisp,2026-10-01 09:17:26,2,28,').warnings.some(value => /partial write/.test(value)));
    assert.throws(() => parseSensorCsv(preamble + '"incomplete'), /quoted field/);
    assert.throws(() => parseSensorCsv('a,b\n1,2'), /Expected a Loom CSV/);
    if (process.argv[2]) {
        const actual = parseSensorCsv(fs.readFileSync(process.argv[2], 'utf8'));
        console.log(JSON.stringify({ samples: actual.rows.length, fields: actual.columns.length,
            checksums: actual.hasChecksums, warnings: actual.warnings, intervals: actual.intervals }, null, 2));
        assert.ok(actual.rows.length > 0);
    }
    console.log('PASS sensor CSV: two-level headings, UTC intervals, quoted UTF-8/newlines, checksums and partial records');
})().catch(error => { console.error(error); process.exitCode = 1; });
