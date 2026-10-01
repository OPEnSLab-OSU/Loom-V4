'use strict';

// Imported records stay in this page. No requests, accounts, or live database are used.
const element = id => document.getElementById(id);
let records = [];

function packet(receipt) {
    try {
        const value = typeof receipt.payload === 'string' ? JSON.parse(receipt.payload) : receipt.payload;
        return value && typeof value === 'object' && !Array.isArray(value) ? value : {};
    } catch {
        return {};
    }
}

function reading(data, key) {
    const modules = Array.isArray(data.contents) ? data.contents : [];
    for (const module of modules) {
        if (module && module.data && Object.hasOwn(module.data, key)) {
            return module.data[key];
        }
    }
    return null;
}

function deviceName(receipt) {
    return receipt.device || receipt.topic.split('/').at(-1);
}

function batteryVolts(data) {
    const volts = data.battery_voltage ?? reading(data, 'Vbat') ?? reading(data, 'BatteryVoltage');
    if (typeof volts === 'number' && Number.isFinite(volts) && volts > 0) return volts;
    const millivolts = reading(data, 'Vbat_MV');
    return typeof millivolts === 'number' && Number.isFinite(millivolts) && millivolts > 0
        ? millivolts / 1000 : '—';
}

function cell(text) {
    const node = document.createElement('td');
    node.textContent = text === null || text === undefined || text === '' ? '—' : text;
    return node;
}

function svgNode(tag, attributes, text) {
    const node = document.createElementNS('http://www.w3.org/2000/svg', tag);
    for (const [key, value] of Object.entries(attributes)) node.setAttribute(key, value);
    if (text !== undefined) node.textContent = text;
    return node;
}

function load(data) {
    if (!Array.isArray(data) || data.length > 10000) {
        throw Error('Import a receipt array containing at most 10,000 rows.');
    }
    for (const row of data) {
        if (!row || typeof row !== 'object' || typeof row.topic !== 'string') {
            throw Error('Each receipt needs a topic.');
        }
    }
    records = data;
    const picker = element('device');
    picker.replaceChildren();
    for (const name of [...new Set(data.map(deviceName))].sort()) {
        const option = document.createElement('option');
        option.value = name;
        option.textContent = name;
        picker.append(option);
    }
    render();
}

function selected() {
    return records.filter(row => deviceName(row) === element('device').value);
}

function render() {
    const rows = selected();
    element('receipts').replaceChildren();
    for (const row of rows.slice(-200).reverse()) {
        const tr = document.createElement('tr');
        tr.append(cell(`${deviceName(row)} / ${row.packet_type || packet(row).type || 'unknown'}`),
                  cell(row.measured_utc), cell(row.received_utc), cell(row.inserted_utc));
        const result = cell(row.error || row.status);
        result.className = 'status ' + (['inserted', 'duplicate'].includes(row.status) ? 'ok' : 'bad');
        tr.append(result);
        element('receipts').append(tr);
    }
    const latest = rows.at(-1) || {};
    const data = packet(latest);
    // Metadata and battery can arrive separately from ordinary measurements.
    const located = rows.slice().reverse().filter(row => row.status !== 'rejected')
        .map(packet).find(p => p.location) || {};
    const location = located.location || {};
    const fields = {
        'Device': element('device').value || 'None',
        'Project': latest.project || '—',
        'Database': latest.database_name || '—',
        'Client ID': latest.client_id || '—',
        'Source IP': latest.source_ip || '—',
        'Battery (V)': batteryVolts(data),
        'Location method': location.LocationMethod || '—',
        'Latitude': location.Latitude ?? '—',
        'Longitude': location.Longitude ?? '—',
        'Location UTC': location.time_utc || '—',
        'Visible arrivals': rows.length,
    };
    element('details').replaceChildren();
    for (const [key, value] of Object.entries(fields)) {
        const dt = document.createElement('dt'), dd = document.createElement('dd');
        dt.textContent = key;
        dd.textContent = value;
        element('details').append(dt, dd);
    }
    drawRose(rows);
}

function windBins(rows) {
    const bins = Array.from({length: 16}, () => ({count: 0, pm: 0, pmCount: 0, speed: 0}));
    const seen = new Set();
    let calm = 0, missing = 0;
    for (const row of rows) {
        const data = packet(row);
        if (data.type !== 'data' || row.status === 'rejected' || row.status === 'duplicate') continue;
        // Multiple broker arrivals for the same bytes represent one physical observation.
        const identity = row.topic + ':' + (row.digest || JSON.stringify(data));
        if (seen.has(identity)) continue;
        seen.add(identity);
        const direction = reading(data, 'WindDirection_deg');
        const speed = reading(data, 'WindSpeed_mps');
        const pm = reading(data, 'PM2.5') ?? reading(data, 'PM2p5') ?? reading(data, 'PM2_5');
        if (typeof speed !== 'number' || !Number.isFinite(speed) || speed < 0) {
            missing++;
            continue;
        }
        if (speed < 0.2) {
            calm++;
            continue;
        }
        if (typeof direction !== 'number' || !Number.isFinite(direction) || direction < 0 || direction > 360) {
            missing++;
            continue;
        }
        const bin = bins[Math.floor(((direction + 11.25) % 360) / 22.5)];
        bin.count++;
        bin.speed += speed;
        if (typeof pm === 'number' && Number.isFinite(pm) && pm >= 0) {
            bin.pm += pm;
            bin.pmCount++;
        }
    }
    return {bins, calm, missing};
}

function drawRose(rows) {
    const {bins, calm, missing} = windBins(rows);
    const canvas = element('rose');
    canvas.replaceChildren();
    const cx = 210, cy = 190;
    const maximum = Math.max(1, ...bins.map(bin => bin.count));
    for (const radius of [40, 80, 120, 150]) {
        canvas.append(svgNode('circle', {cx, cy, r: radius, fill: 'none', stroke: '#dbe5e0'}));
    }
    bins.forEach((bin, index) => {
        const angle = index * Math.PI / 8 - Math.PI / 2;
        const radius = 20 + 130 * bin.count / maximum;
        const first = angle - .14, last = angle + .14;
        const pm = bin.pmCount ? bin.pm / bin.pmCount : null;
        const path = `M ${cx} ${cy} L ${cx + radius * Math.cos(first)} ${cy + radius * Math.sin(first)}` +
                     ` A ${radius} ${radius} 0 0 1 ${cx + radius * Math.cos(last)} ${cy + radius * Math.sin(last)} Z`;
        const color = pm === null ? '#acb9b5' : pm < 12 ? '#548e85' : pm <= 35 ? '#b99842' : '#bc654e';
        const petal = svgNode('path', {d: path, fill: color, opacity: bin.count ? '.9' : '0'});
        const speed = bin.count ? (bin.speed / bin.count).toFixed(1) : '—';
        petal.append(svgNode('title', {}, `${index * 22.5}°: ${bin.count} observations; mean speed ${speed} m/s;` +
                            ` mean PM2.5 ${pm === null ? '—' : pm.toFixed(1)}`));
        canvas.append(petal);
    });
    for (const [label, x, y] of [['N', cx, 20], ['E', 385, cy + 5], ['S', cx, 375], ['W', 35, cy + 5]]) {
        canvas.append(svgNode('text', {x, y, 'text-anchor': 'middle', fill: '#42655e'}, label));
    }
    const count = bins.reduce((total, bin) => total + bin.count, 0);
    element('windSummary').textContent = `${count} wind observations · ${calm} calm · ${missing} missing wind readings.` +
                                         ' Hover over a petal for speed and PM2.5.';
}

function demo() {
    const sample = [];
    for (let index = 0; index < 48; index++) {
        const name = index % 3 ? 'Wisp1' : 'Wisp2';
        const hour = String(index % 24).padStart(2, '0');
        const measured = `2026-09-30T${hour}:00:00Z`;
        const data = {
            type: index % 10 ? 'data' : 'heartbeat', id: {name: 'Wisp', instance: name === 'Wisp1' ? 1 : 2},
            timestamp: {time_utc: measured}, battery_voltage: 3.94,
            location: {LocationMethod: 'GPS', Latitude: 44.56, Longitude: -123.27, time_utc: measured},
            contents: index % 10 ? [{module: 'Weather', data: {
                WindSpeed_mps: index % 9 * .7, WindDirection_deg: index * 37 % 360, 'PM2.5': index % 7 * 8,
            }}] : [],
        };
        sample.push({id: index + 1, topic: `RemoteTest/Sensors/${name}`, device: name, project: 'RemoteTest',
            database_name: 'Sensors', client_id: `mock-feather-${name}`, source_ip: '192.0.2.10',
            packet_type: data.type, measured_utc: measured, received_utc: `2026-09-30T${hour}:05:00Z`,
            inserted_utc: index % 11 ? `2026-09-30T${hour}:06:00Z` : null,
            status: index % 11 ? 'inserted' : 'insertion_failed',
            error: index % 11 ? '' : 'Simulated insertion failure', payload: JSON.stringify(data)});
    }
    load(sample);
    element('notice').textContent = 'Showing illustrative sample data.';
}

element('device').addEventListener('change', render);
element('demo').addEventListener('click', demo);
element('upload').addEventListener('change', async event => {
    try {
        const file = event.target.files[0];
        if (!file) return;
        if (file.size > 20 * 1024 * 1024) throw Error('Receipt file exceeds 20 MB.');
        load(JSON.parse(await file.text()));
        element('notice').textContent = 'Imported locally; no data sent.';
    } catch (error) {
        element('notice').textContent = error.message;
    }
});
element('download').addEventListener('click', () => {
    const link = document.createElement('a');
    const url = URL.createObjectURL(new Blob([JSON.stringify(selected(), null, 2)], {type: 'application/json'}));
    link.href = url;
    link.download = 'loom-visible-receipts.json';
    link.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
});
demo();
