// Read Loom's serial preamble and two-level CSV headings without losing quoted fields.
export function parseSensorCsv(text) {
    const records = [];
    let cells = [], cell = '', quoted = false, rawStart = 0;
    text = text.replace(/^\uFEFF/, '');
    function endRecord(end, terminated) {
        cells.push(cell);
        records.push({ cells, raw: text.slice(rawStart, end), terminated });
        cells = []; cell = ''; rawStart = end;
    }
    for (let i = 0; i < text.length; i++) {
        const ch = text[i];
        if (ch === '"') {
            if (quoted && text[i + 1] === '"') { cell += '"'; i++; }
            else quoted = !quoted;
        } else if (!quoted && ch === ',') { cells.push(cell); cell = ''; }
        else if (!quoted && (ch === '\r' || ch === '\n')) {
            endRecord(i, true);
            if (ch === '\r' && text[i + 1] === '\n') i++;
            rawStart = i + 1;
        } else cell += ch;
    }
    if (quoted) throw new Error('CSV ends inside a quoted field; copy a complete file.');
    if (cell || cells.length) endRecord(text.length, false);
    const headerIndex = records.findIndex(row => row.cells.includes('time_utc'));
    if (headerIndex < 1) throw new Error('Expected a Loom CSV with sensor headings and time_utc.');
    const fields = records[headerIndex].cells;
    const groups = records[headerIndex - 1].cells;
    const data = records.slice(headerIndex + 1).filter(row => row.cells.some(value => value !== ''));
    const warnings = [];
    const checksumIndex = fields.indexOf('checksum');
    let group = '';
    const columns = fields.map((field, index) => {
        group = groups[index] || group;
        return { index, field, label: group && field ? group + ' · ' + field : field };
    }).filter(column => column.field || data.some(row => row.cells[column.index]));
    const utcIndex = fields.indexOf('time_utc');
    const rows = data.map((record, index) => {
        if (record.cells.length !== fields.length) warnings.push('Sample ' + (index + 1) + ' has an unexpected number of columns.');
        if (!record.terminated) warnings.push('Final sample has no record terminator; it may be a partial write.');
        const value = record.cells[utcIndex] || '';
        const iso = /^\d{4}-\d\d-\d\d[ T]\d\d:\d\d:\d\d(?:\.\d+)?Z?$/.test(value) ? value.replace(' ', 'T').replace(/Z?$/, 'Z') : '';
        const parsed = iso ? Date.parse(iso) : NaN;
        const utcMs = Number.isFinite(parsed) ? parsed : null;
        if (utcMs === null) warnings.push('Sample ' + (index + 1) + ' has an invalid UTC timestamp.');
        let checksum = null;
        if (checksumIndex >= 0) {
            // Firmware checksum covers raw UTF-8 bytes through the final comma, excluding CR/LF.
            const comma = record.raw.lastIndexOf(',');
            const sum = new TextEncoder().encode(record.raw.slice(0, comma + 1)).reduce((total, byte) => (total + byte) & 65535, 0);
            const digits = record.cells[checksumIndex] || '';
            checksum = checksumIndex === fields.length - 1 && /^\d{1,5}$/.test(digits) && Number(digits) === sum && record.terminated;
            if (!checksum) warnings.push('Sample ' + (index + 1) + ' failed its row checksum.');
        }
        return { cells: record.cells, utcMs, checksum };
    });
    const intervals = rows.slice(1).map((row, i) => row.utcMs !== null && rows[i].utcMs !== null ? (row.utcMs - rows[i].utcMs) / 1000 : null);
    if (intervals.some(value => value !== null && value <= 0)) warnings.push('Sample UTC timestamps repeat or move backwards.');
    return { columns, rows, intervals, warnings, hasChecksums: checksumIndex >= 0 };
}

export function mountSensorCsv(uPlot, selectView) {
    const $ = id => document.getElementById(id);
    let plot = null, loaded = null, request = 0, page = 0;
    const pageSize = 100;
    function render() {
        const { columns, rows } = loaded;
        const column = columns.find(item => item.index === Number($('csvField').value));
        plot?.destroy(); plot = null;
        const points = rows.map((row, i) => ({ row, i })).filter(point => point.row.utcMs !== null);
        if (column && points.length && points.every((point, i) => i === 0 || point.row.utcMs > points[i - 1].row.utcMs)) {
            const start = points[0].row.utcMs;
            plot = new uPlot({ width: Math.max(280, $('csvGraph').clientWidth), height: 230,
                scales: { x: { time: false } },
                axes: [{ label: 'Minutes since first sample' }, {}],
                series: [{ label: 'Recorded sample UTC', value: (u, value) => value === null ? '—' :
                    new Date(start + value * 60000).toISOString().slice(0, 19).replace('T', ' ') + ' UTC' },
                    { label: column.label, stroke: '#176c58', width: 2, points: { show: true, size: 8 } }],
                cursor: { drag: { x: true, y: false } }
            }, [points.map(point => (point.row.utcMs - start) / 60000), points.map(({ row }) => {
                const value = row.cells[column.index];
                return value?.trim() && Number.isFinite(Number(value)) ? Number(value) : null;
            })], $('csvGraph'));
        }
        const head = document.createElement('tr');
        for (const item of columns) { const th = document.createElement('th'); th.textContent = item.label; head.append(th); }
        $('csvHead').replaceChildren(head);
        const fragment = document.createDocumentFragment();
        for (const row of rows.slice(page * pageSize, (page + 1) * pageSize)) {
            const tr = document.createElement('tr');
            for (const item of columns) { const td = document.createElement('td'); td.textContent = row.cells[item.index] ?? ''; tr.append(td); }
            fragment.append(tr);
        }
        $('csvRows').replaceChildren(fragment);
        $('csvPage').textContent = rows.length ? 'Samples ' + (page * pageSize + 1) + '–' + Math.min(rows.length, (page + 1) * pageSize) + ' of ' + rows.length : 'No samples';
        $('csvPrevious').disabled = page === 0;
        $('csvNext').disabled = (page + 1) * pageSize >= rows.length;
    }
    $('csvFile').onchange = async event => {
        const file = event.target.files[0]; event.target.value = '';
        if (!file) return;
        const id = ++request;
        try {
            if (file.size > 10 * 1024 * 1024) throw new Error('Sensor CSV inspection accepts files up to 10 MB.');
            const data = parseSensorCsv(await file.text());
            if (id !== request) return;
            loaded = data; page = 0;
            $('csvLoaded').classList.remove('hidden');
            $('csvStatus').textContent = file.name + ' · ' + data.rows.length + ' samples';
            const validIntervals = data.intervals.filter(value => value !== null);
            const timeRange = data.rows.length ? (data.rows[0].cells[data.columns.find(c => c.field === 'time_utc').index] + ' → ' + data.rows.at(-1).cells[data.columns.find(c => c.field === 'time_utc').index] + ' UTC') : '';
            const intervalRange = validIntervals.reduce((range, value) => [Math.min(range[0], value), Math.max(range[1], value)], [Infinity, -Infinity]);
            $('csvRange').textContent = timeRange + (validIntervals.length ? ' · Recorded intervals: ' + intervalRange[0] + '–' + intervalRange[1] + ' seconds' : '');
            $('csvIntegrity').textContent = data.hasChecksums ? 'Row checksums: ' + data.rows.filter(row => row.checksum).length + ' verified of ' + data.rows.length : 'No row checksums were recorded in this CSV. Its serial-number preamble is not a checksum.';
            $('csvWarnings').textContent = data.warnings.join('\n');
            const numeric = data.columns.filter(column => !['checksum', 'instance', 'Number'].includes(column.field) && data.rows.some(row => row.cells[column.index]?.trim() && Number.isFinite(Number(row.cells[column.index]))));
            $('csvField').replaceChildren(...numeric.map(column => { const option = document.createElement('option'); option.value = column.index; option.textContent = column.label; return option; }));
            selectView('samples'); render();
        } catch (error) {
            if (id === request) { $('csvStatus').textContent = 'Could not read CSV: ' + error.message; $('csvLoaded').classList.add('hidden'); loaded = null; plot?.destroy(); plot = null; selectView('samples'); }
        }
    };
    $('csvField').onchange = render;
    $('csvPrevious').onclick = () => { if (loaded && page) { page--; render(); } };
    $('csvNext').onclick = () => { if (loaded && (page + 1) * pageSize < loaded.rows.length) { page++; render(); } };
    new ResizeObserver(() => {
        if (plot && $('csvGraph').clientWidth > 0) plot.setSize({ width: Math.max(280, $('csvGraph').clientWidth), height: 230 });
    }).observe($('csvGraph'));
}
