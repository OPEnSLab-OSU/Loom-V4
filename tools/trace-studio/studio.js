import uPlot from 'uplot';
import 'uplot/dist/uPlot.min.css';
import './studio.css';
import LoomTrace from '../trace/trace_converter.js';
import { fictionalCycle } from './demo.js';
import { perfettoUrl, sendTrace, scrollTrace } from './perfetto.js';
import { createEventTimeline } from './event-timeline.js';
import { selectionRange } from './selection.js';
import { mountSensorCsv } from './sensor-csv.js';
import { reconstructWallClock } from './wall-clock.js';
import { traceDisplayClock, clockTicks, zoneOffset } from './display-clock.js';
import { perfettoWallTrace, wallSelectionRange } from './perfetto-wall.js';

const $ = id => document.getElementById(id);
const ms = us => (us / 1000).toLocaleString(undefined, { maximumFractionDigits: 3 }) + ' ms';
const bytes = n => n.toLocaleString() + ' B';
const signed = n => (n >= 0 ? '+' : '') + bytes(n);
let converted = null, selectedCall = null, sourceBlob = null, sourceName = '', worker = null;
let loadId = 0, plot = null, plotRows = [], snapshotCache = null;
let eventTimeline = null, wallClock = null;
let perfettoFrameLoadId = null, perfettoRequest = 0;
let perfettoDocumentKey = null;
let localZone = Intl.DateTimeFormat().resolvedOptions().timeZone, wallBlob = null;
$('localTimezone').value = localZone;
$('timezoneInfo').textContent = 'Local display uses ' + localZone + '; recorded UTC stays unchanged.';
const displayClock = () => traceDisplayClock(converted.report, wallClock, $('memoryClock').value, localZone);
function perfettoClockInfo() {
    $('perfettoClock').options[1].disabled = !wallClock;
    $('exportWall').disabled = !wallClock;
    $('perfettoClockInfo').textContent = $('perfettoClock').value === 'wall' && wallClock ?
        'Estimated wall time includes standby. If tracks are folded, click Perfetto’s Expand all groups (↕) button. In its search box, type > and choose “Set timestamp and duration format”, then “Realtime (UTC)” or “Custom Timezone”. For ' + localZone + ', choose ' + zoneOffset(wallClock.startUtcUs, localZone) + ' for this recording’s start (Perfetto uses a fixed offset). Calls are split around standby; details retain whole-call durations. Counters are last observed values; RAM was not sampled during standby.' :
        'Awake execution excludes standby. In Perfetto, use the native timestamp format “Timecode” or “Seconds” for this clock. ' + (wallClock ? 'Choose Wall time to enable UTC / time-zone display with a REALTIME clock anchor.' : 'This file has no trustworthy Loom RTC anchors; wall time is unavailable.');
}

function buttons() {
    for (const id of ['perfettoInline', 'perfettoTab']) $(id).disabled = !sourceBlob;
    $('export').disabled = !converted;
    $('report').disabled = !converted;
}
function view(timeline) {
    $('perfettoView').classList.toggle('hidden', timeline !== true);
    $('inspectView').classList.toggle('hidden', timeline !== false);
    $('csvView').classList.toggle('hidden', timeline !== 'samples');
    $('inspectTab').classList.toggle('selected', timeline === false);
    $('timelineTab').classList.toggle('selected', timeline === true);
    $('csvTab').classList.toggle('selected', timeline === 'samples');
}
function download(value, suffix) {
    const url = URL.createObjectURL(new Blob([JSON.stringify(value)], { type: 'application/json' }));
    const link = document.createElement('a'); link.href = url;
    link.download = sourceName.replace(/\.(?:perfetto\.)?(ndjson|jsonl|json)$/i, '') + suffix;
    link.click(); setTimeout(() => URL.revokeObjectURL(url), 1000);
}
function prepare(name) {
    worker?.terminate(); worker = null; ++loadId;
    plot?.destroy(); plot = null; plotRows = [];
    converted = null; selectedCall = null; sourceBlob = null; sourceName = name; snapshotCache = null; eventTimeline = null; wallClock = null; wallBlob = null;
    $('perfettoClock').value = 'active'; perfettoClockInfo();
    $('loaded').classList.add('hidden'); $('welcome').classList.remove('hidden');
    $('entry').disabled = true; $('exit').disabled = true;
    $('handoff').textContent = ''; $('blockDetails').open = false; $('objectDetails').open = false;
    $('timelineHover').textContent = 'Hover or focus a function or event to read its full label.';
    for (const id of ['callSearch', 'blockSearch', 'objectSearch']) $(id).value = '';
    $('blockInfo').textContent = 'Select an allocation.'; $('objectInfo').textContent = 'Select an object.';
    for (const id of ['graph', 'calls', 'eventTimeline', 'eventRows', 'blocks', 'objects', 'activeStack']) $(id).replaceChildren();
    buttons(); view(false);
    return loadId;
}
function analyze(content, name, fictional = false, rawBlob = null, preparedId = null) {
    const id = preparedId ?? prepare(name);
    $('status').textContent = 'Reading ' + name + ' and reconstructing lifetimes…';
    worker = new Worker('./trace-worker.js');
    worker.onmessage = event => {
        if (event.data.id !== loadId) return;
        worker.terminate(); worker = null;
        if (event.data.error) {
            if (rawBlob && /"traceEvents"\s*:/.test(content.slice(0, 1024)) && /general Chrome trace/.test(event.data.error)) {
                sourceBlob = rawBlob; buttons();
                $('status').textContent = name + ' · General Chrome trace. Open in Perfetto; Loom heap/object records are unavailable.';
            } else $('status').textContent = 'Could not read this trace: ' + event.data.error;
            return;
        }
        converted = event.data.result;
        // Use the enriched export for both MCU JSON and NDJSON, so port labels and
        // normalized timestamps match the inspector and its Perfetto selection.
        sourceBlob = new Blob([JSON.stringify(converted.perfetto)], { type: 'application/json' });
        const report = converted.report;
        wallClock = reconstructWallClock(report);
        $('memoryClock').options[0].disabled = !wallClock;
        $('memoryClock').options[1].disabled = !wallClock;
        $('memoryClock').value = wallClock ? 'utc' : 'active';
        perfettoClockInfo();
        $('allocationCapture').textContent = report.session.heap_hooks ?
            'Individual allocation capture was enabled. Only allocations observed after recording began have lifetimes; earlier object allocation times remain unknown.' :
            'Individual allocation capture was OFF for this recording. The object list identifies observed containers; it does not prove when they were allocated. Heap totals are measured, but block addresses, malloc/free events and allocation lifetimes were not recorded. Use the heap build mode for the next recording; this file cannot recover those events.';
        $('status').textContent = (fictional ? 'FICTIONAL EXAMPLE · ' : '') + name + ' · ' +
            report.calls.length + ' calls · ' + report.allocations.length + ' captured allocations · ' +
            (report.session.heap_hooks ? 'Heap capture enabled' : 'Call-only capture');
        $('loaded').classList.remove('hidden'); $('welcome').classList.add('hidden'); buttons();
        $('position').max = Math.max(0, report.history.length - 1); $('position').value = 0;
        $('overhead').textContent = (report.overheadUs / 1e6).toLocaleString(undefined, { maximumFractionDigits: 3 }) + ' s';
        $('peak').textContent = report.session.heap_hooks ? 'Peak captured: ' + bytes(report.peakTrackedBytes) : 'Individual allocations were not captured';
        $('warnings').replaceChildren(...report.warnings.map(message => {
            const item = document.createElement('li'); item.textContent = message; return item;
        }));
        const first = document.createElement('option'); first.value = ''; first.textContent = 'Choose a checkpoint';
        $('checkpoint').replaceChildren(first, ...report.checkpoints.filter(row => !row.name.startsWith('Call entry:') && row.name !== 'Function returned').map(row => {
            const option = document.createElement('option'); option.value = row.index;
            option.textContent = row.name + ' · ' + ms(row.timeUs); return option;
        }));
        renderCalls(); drawGraph();
        eventTimeline = createEventTimeline({ report, getClock: displayClock, select: setPosition, selectCall: call => {
            selectedCall = call || null; renderCalls();
        } });
        if (fictional) {
            const point = report.checkpoints.find(row => row.name === 'During MQTT upload (fictional)');
            $('position').value = point.index;
            selectedCall = report.calls.find(call => call.name === 'MQTTComponent::publish');
            $('entry').disabled = false; $('exit').disabled = false; renderCalls();
        }
        showPosition();
    };
    worker.onerror = () => {
        if (id !== loadId) return;
        worker?.terminate(); worker = null;
        $('status').textContent = 'The trace worker could not start. Reload this localhost page and try again.';
    };
    worker.postMessage({ id, content });
}
async function readFile(file) {
    if (!file) return;
    const nativeBinary = /\.(pftrace|perfetto-trace)$/i.test(file.name);
    if (nativeBinary || file.size > 50 * 1024 * 1024) {
        const id = prepare(file.name);
        const prefix = nativeBinary ? '' : await file.slice(0, 1024).text();
        if (id !== loadId) return;
        if (!nativeBinary && !/"traceEvents"\s*:/.test(prefix)) {
            $('status').textContent = 'Detailed inspection accepts up to 50 MB. Use the Node converter for larger NDJSON recordings.'; return;
        }
        sourceBlob = file; buttons();
        $('status').textContent = file.name + ' · Ready for Perfetto. Detailed Loom inspection is unavailable for this file type or size.';
        return;
    }
    const id = prepare(file.name);
    $('status').textContent = 'Reading ' + file.name + '…';
    const content = await file.text();
    if (id !== loadId) return;
    // NDJSON must be converted before Perfetto; the MCU Chrome file is already ready.
    analyze(content, file.name, false, /"traceEvents"\s*:/.test(content.slice(0, 1024)) ? file : null, id);
}
function setPosition(index) {
    if (!converted) return;
    $('position').value = Math.max(0, Math.min(converted.report.history.length - 1, index)); showPosition();
}
function renderCalls() {
    if (!converted) return;
    const query = $('callSearch').value.toLowerCase();
    const matched = converted.report.calls.filter(call => (call.name + ' ' + call.signature + ' ' + call.objectLabel).toLowerCase().includes(query));
    $('calls').replaceChildren(...matched.slice(0, 1500).map(call => {
        const button = document.createElement('button');
        button.textContent = '· '.repeat(Math.min(call.depth, 8)) + (call.displayName || call.name);
        const details = document.createElement('small');
        details.textContent = call.objectLabel + ' · ' + ms(call.startUs) + ' · ' +
            ms(call.endUs - call.startUs) + (call.status === 'returned' ? '' : ' · incomplete');
        button.append(details); button.classList.toggle('selected', call.id === selectedCall?.id);
        button.title = call.signature + '\n' + call.file + ':' + call.line;
        button.onclick = () => { selectedCall = call; $('entry').disabled = false; $('exit').disabled = false;
            renderCalls(); setPosition(call.startIndex); };
        return button;
    }));
    $('callLimit').textContent = matched.length > 1500 ? 'Showing first 1,500 matches. Search to narrow the list; the report contains all calls.' : '';
    $('entry').disabled = !selectedCall; $('exit').disabled = !selectedCall;
    $('exit').textContent = selectedCall && selectedCall.status !== 'returned' ? 'Last observed boundary' : 'Call return';
}
function tableRow(values, click) {
    const row = document.createElement('tr');
    for (const value of values) { const cell = document.createElement('td'); cell.textContent = value; row.append(cell); }
    row.tabIndex = 0; row.onclick = click; row.onkeydown = event => { if (event.key === 'Enter') click(); };
    return row;
}
function showPosition() {
    if (!converted) return;
    const report = converted.report, index = Number($('position').value);
    const snapshot = snapshotCache?.index === index ? snapshotCache.value : LoomTrace.snapshot(report, index);
    snapshotCache = { index, value: snapshot };
    $('positionLabel').textContent = 'Event ' + (index + 1) + ' of ' + report.history.length + ' · ' + ms(snapshot.timeUs) + ' active time';
    if (wallClock && displayClock().wall) $('positionLabel').textContent += ' · ≈ ' + displayClock().format(displayClock().at(index));
    $('eventNumber').max = report.history.length; $('eventNumber').value = index + 1;
    $('selectedContext').textContent = report.history[index]?.context ? 'During: ' + report.history[index].context : 'Outside an instrumented call';
    $('snapshotHeading').textContent = 'Memory at event ' + (index + 1);
    $('heapbytes').textContent = snapshot.checkpoint ? bytes(snapshot.checkpoint.usedBytes) : 'No measurement';
    $('heapwhen').textContent = snapshot.checkpoint ? 'Measured at event ' + (snapshot.checkpoint.index + 1) + ': ' + snapshot.checkpoint.name +
        (displayClock().wall ? ' · ≈ ' + displayClock().format(displayClock().at(snapshot.checkpoint.index)) : ' · ' + ms(snapshot.checkpoint.timeUs) + ' awake') : 'Measured at checkpoints and call boundaries';
    const memory = snapshot.checkpoint;
    $('freebytes').textContent = memory ? bytes(memory.freeBytes) : 'No measurement';
    $('availablebytes').textContent = memory ? bytes(memory.freeBytes + Math.max(0, memory.gapBytes)) : 'No measurement';
    $('gapwhen').textContent = memory ? 'Free heap + ' + bytes(memory.gapBytes) + ' stack-to-heap gap; no future stack reserve' : 'Measured at checkpoints and call boundaries';
    $('livebytes').textContent = report.session.heap_hooks ? bytes(snapshot.liveBytes) : 'Not captured';
    $('livecount').textContent = report.session.heap_hooks ? snapshot.live.length + ' blocks with observed allocation times' : 'Allocation events were disabled; object observations are separate';
    $('objectcount').textContent = snapshot.activeObjects.length;
    $('quality').textContent = snapshot.incomplete ? 'Incomplete history after missing or ambiguous events. Objects and blocks shown here belong to the currently observed segment.' :
        'Showing the recorded event boundary. Baseline object identities may be known; baseline allocation times and internal buffers are not fully captured.';
    $('activeStack').replaceChildren(...snapshot.callStack.map(call => {
        const item = document.createElement('li'); item.textContent = call.displayName || call.name;
        item.title = call.signature + '\n' + call.file + ':' + call.line;
        const label = document.createElement('small'); label.textContent = call.file + ':' + call.line;
        item.append(label); return item;
    }));
    $('noStack').textContent = snapshot.callStack.length ? '' : 'No instrumented function is active at this recorded boundary.';
    $('eventDescription').textContent = report.history[index]?.eventName || kindName(report.history[index]?.kind);
    eventTimeline?.highlight(index);
    const call = selectedCall;
    $('callSummary').textContent = call ? 'Heap on entry: ' + bytes(call.heapUsedAtEntry) + ' · recorded return: ' +
        (call.heapUsedAtExit === null ? 'unknown' : bytes(call.heapUsedAtExit)) + ' · change: ' +
        (call.heapChange === null ? 'unknown' : signed(call.heapChange)) : 'Select a call to compare memory.';
    $('callFreeSummary').textContent = call ? 'Free heap on entry: ' + bytes(call.heapFreeAtEntry) + ' · recorded return: ' +
        (call.heapFreeAtExit === null ? 'unknown' : bytes(call.heapFreeAtExit)) + ' · change: ' +
        (call.heapFreeAtExit === null ? 'unknown' : signed(call.heapFreeAtExit - call.heapFreeAtEntry)) +
        '\nStack-to-heap gap on entry: ' + bytes(call.gapAtEntry) + ' · recorded return: ' +
        (call.gapAtExit === null ? 'unknown' : bytes(call.gapAtExit)) : '';
    $('callDetail').textContent = call ? call.signature + '\n' + call.objectLabel + ' · ' + call.file + ':' + call.line +
        '\n' + (call.status === 'returned' ? 'Recorded return: ' : 'Last observed boundary: ') + ms(call.endUs) + ' (' + call.status + ') · ' + call.stillLiveAtExitIds.length + ' allocations created during this call remained live at its last boundary.' : 'Choose a call for its entry/return memory comparison.';
    $('selection').textContent = 'Live captured allocations at ' + ms(snapshot.timeUs);
    const blockQuery = $('blockSearch').value.toLowerCase();
    const blocks = snapshot.live.filter(block => (block.label + ' ' + block.address + ' ' + block.owner).toLowerCase().includes(blockQuery)).sort((a, b) => b.size - a.size);
    $('blocks').replaceChildren(...blocks.slice(0, 250).map(block => tableRow([
        block.label + '\n' + block.address, bytes(block.size), (block.stack.at(-1) || 'Outside traced calls') + '\n' + block.owner
    ], () => {
        $('blockInfo').textContent = 'Allocation #' + block.id + '\nLabel at this event: ' + block.label +
            '\nAddress: ' + block.address + '\nRequested size: ' + bytes(block.size) + '\nCreated at: ' + ms(block.startUs) +
            '\nRecorded end: ' + (block.endUs === null ? 'Still live at final event; not proof of a leak' : ms(block.endUs) + ' · ' + block.endReason) +
            '\nObject context: ' + block.owner + ' (' + block.ownerAddress + ')\nCaller instruction address: ' + block.callerPc +
            '\n\nCreation call stack:\n' + stackText(block.stackFrames, block.stack) +
            '\n\nStack at recorded release/end:\n' + stackText(block.endStack, []);
        $('blockDetails').open = true;
    })));
    $('emptyBlocks').textContent = !report.session.heap_hooks ? 'No allocation events were recorded: heap hooks were OFF. Object observations and allocator totals still work. Enable LOOM_TRACE and LOOM_TRACE_HEAP, install the Loom Trace Heap companion, then re-record to capture new allocations and frees.' : !blocks.length ?
        'No matching captured blocks are live here. Earlier storage can still contribute to heap totals.' :
        blocks.length > 250 ? 'Showing the largest 250 matches; the report contains all allocations.' : 'Select a block for both call stacks and its lifetime.';
    const objectQuery = $('objectSearch').value.toLowerCase();
    const objects = snapshot.activeObjects.filter(object => (object.name + ' ' + object.address + ' ' + object.ownerAddress).toLowerCase().includes(objectQuery));
    $('objects').replaceChildren(...objects.slice(0, 250).map(object => tableRow([
        object.name + '\n' + object.address, object.state,
        object.allocationId ? 'Captured allocation #' + object.allocationId : 'Observed object; allocation time unknown',
        (object.ownerAddress === '0x0' ? 'No owner supplied' : 'Mux ' + object.ownerAddress) + '\n' +
            (object.containerSize ? bytes(object.containerSize) + ' container' : 'Container size unknown')
    ], () => {
        $('objectInfo').textContent = object.name + '\nAddress: ' + object.address + '\nState at this event: ' + object.state +
            '\nOwner: ' + object.ownerAddress + '\nContainer size: ' + (object.containerSize ? bytes(object.containerSize) : 'Unknown') +
            '\nObservation time: ' + ms(object.startUs) + ' (not necessarily creation time)' +
            '\nAllocation coverage: ' + object.allocationCoverage + '\nLast state boundary: ' + (object.endUs === null ? 'No later retirement/update captured' : ms(object.endUs) + ' · ' + object.endReason);
        $('objectDetails').open = true;
    })));
    $('emptyObjects').textContent = objects.length ? 'Object/container sizes do not add to the captured allocation total.' : 'No matching object observations are known here. This is not a claim that the heap is empty.';
    $('previous').disabled = index === 0; $('next').disabled = index >= report.history.length - 1;
}
function kindName(kind) {
    return ({ B: 'Function entered', E: 'Function returned', A: 'Allocation created', F: 'Allocation freed',
        R: 'Allocation resized', N: 'Allocation failed', U: 'Object observed', D: 'Object retired',
        C: 'Memory checkpoint', O: 'SD trace saved', lost: 'Events lost' })[kind] || '';
}
function stackText(frames, fallback) {
    return frames?.length ? frames.map(frame => frame.signature + ' · ' + frame.objectLabel + '\n  ' + frame.file + ':' + frame.line).join('\n  → ') :
        fallback.length ? fallback.join('\n  → ') : 'No instrumented frames captured';
}
function drawGraph() {
    plot?.destroy(); const report = converted.report;
    const clock = displayClock(), utc = clock.wall;
    $('memoryClockInfo').textContent = utc ?
        clock.format(wallClock.startUtcUs) + ' → ' + clock.format(wallClock.endUtcUs) + '. Estimated from captured RTC seconds and measured wake/restoration timing. Sleep gaps are blank: no memory was measured in standby. Fine timing remains available in the awake-execution view.' :
        'Awake execution time only: standby is excluded. ' + (wallClock ? 'Choose UTC wall time to include the sleep gaps.' : 'This recording has no complete, trustworthy RTC/wake anchors for a wall-clock reconstruction.');
    const unique = new Map();
    for (const row of report.history) unique.set(row.timeUs, row);
    plotRows = [...unique.values()];
    let measurement = 0, currentHeap = null, currentFree = null, currentAvailable = null;
    const free = [], available = [];
    const heaps = plotRows.map(row => {
        while (measurement < report.checkpoints.length && report.checkpoints[measurement].index <= row.index) {
            const checkpoint = report.checkpoints[measurement++];
            currentHeap = checkpoint.usedBytes; currentFree = checkpoint.freeBytes;
            currentAvailable = currentFree + Math.max(0, checkpoint.gapBytes);
        }
        free.push(currentFree); available.push(currentAvailable);
        return currentHeap;
    });
    const points = [];
    for (let i = 0; i < plotRows.length; i++) {
        const row = plotRows[i];
        const x = utc ? wallClock.rows[row.index].utcUs / 1e6 : row.timeUs / 1e6;
        if (utc && i && wallClock.rows[row.index].segment !== wallClock.rows[plotRows[i - 1].index].segment) {
            const previous = points.at(-1).x;
            if (x - previous > .002) {
                points.push({ x: previous + .000001, row: null, values: [null, null, null, null] });
                points.push({ x: x - .000001, row: null, values: [null, null, null, null] });
            }
        }
        points.push({ x, row, values: [report.session.heap_hooks ? row.bytes : null, heaps[i], free[i], available[i]] });
    }
    plotRows = points.map(point => point.row);
    const opts = {
        width: Math.max(280, $('graph').clientWidth), height: 230, scales: { x: { time: false } },
        axes: [{ label: clock.label,
            splits: (u, axis, min, max) => clockTicks(min, max),
            values: (u, values) => values.map(value => clock.tick(value * 1e6)) },
            { values: (u, values) => values.map(n => n.toLocaleString() + ' B'), size: 74 }],
        series: [{ label: clock.label, value: (u, v) => v === null ? '—' : clock.format(v * 1e6) },
            { label: 'Live captured bytes', stroke: '#176c58', width: 2, paths: uPlot.paths.stepped({ align: 1 }),
                value: (u, v) => v === null ? (report.session.heap_hooks ? '—' : 'Not captured') : bytes(v) },
            { label: 'Heap in use (last measurement)', stroke: '#527dbe', width: 2, points: { show: !!utc, size: 9 }, paths: uPlot.paths.stepped({ align: 1 }),
                value: (u, v) => v === null ? '—' : bytes(v) },
            { label: 'Free heap (last measurement)', stroke: '#b47816', width: 2, points: { show: !!utc, size: 9 }, paths: uPlot.paths.stepped({ align: 1 }),
                value: (u, v) => v === null ? '—' : bytes(v) },
            { label: 'Available RAM estimate (no stack reserve)', stroke: '#8c6baa', width: 1, points: { show: !!utc, size: 9 }, dash: [5, 4], paths: uPlot.paths.stepped({ align: 1 }),
                value: (u, v) => v === null ? '—' : bytes(v) }],
        cursor: { drag: { x: true, y: false } }
    };
    plot = new uPlot(opts, [points.map(point => point.x), ...[0, 1, 2, 3].map(series => points.map(point => point.values[series]))], $('graph'));
    let pointerStart = null;
    plot.over.addEventListener('pointerdown', event => { pointerStart = { x: event.clientX, y: event.clientY }; });
    plot.over.addEventListener('pointerup', event => {
        if (pointerStart && Math.hypot(event.clientX - pointerStart.x, event.clientY - pointerStart.y) < 4 && plot.cursor.idx !== null) {
            const row = plotRows[plot.cursor.idx];
            if (row) setPosition(row.index);
        }
        pointerStart = null;
    });
}
new ResizeObserver(() => {
    if (plot && $('graph').clientWidth > 0) plot.setSize({ width: Math.max(280, $('graph').clientWidth), height: 230 });
}).observe($('graph'));
new ResizeObserver(entries => {
    document.documentElement.style.setProperty('--event-navigator-height', (entries[0].target.getBoundingClientRect().height + 16) + 'px');
}).observe(document.querySelector('.eventNavigator'));
async function openPerfetto(external = false, selection = false) {
    if (!sourceBlob) return;
    const wall = $('perfettoClock').value === 'wall' && wallClock;
    if (wall && !wallBlob) wallBlob = new Blob([perfettoWallTrace(converted, wallClock, localZone)], { type: 'application/octet-stream' });
    const blob = wall ? wallBlob : sourceBlob, name = sourceName + (wall ? ' · estimated wall time' : ' · awake execution'), id = loadId;
    const frameKey = id + ':' + (wall ? 'wall:' + localZone : 'active');
    const row = converted?.report.history[Number($('position').value)];
    const range = selection ? (wall ? wallSelectionRange(converted.report, wallClock, row, selectedCall) : selectionRange(row, selectedCall)) : null;
    const request = ++perfettoRequest;
    const isCurrent = () => id === loadId && request === perfettoRequest;
    let target;
    if (external) {
        target = window.open('https://ui.perfetto.dev/');
        if (!target) { $('handoff').textContent = 'Your browser blocked the new tab. Use View in Perfetto or download the JSON.'; return; }
    } else {
        view(true);
        if (perfettoFrameLoadId === frameKey && !selection) return; // Keep the existing zoom/SQL/selection.
        if (perfettoDocumentKey !== frameKey) {
            // Importing two clocks concurrently into the same Perfetto document can
            // let the older load win. Give each clock/file a fresh document.
            perfettoDocumentKey = frameKey;
            await new Promise((resolve, reject) => {
                const previous = $('perfettoFrame'), frame = previous.cloneNode(false);
                frame.removeAttribute('src'); previous.replaceWith(frame); perfettoFrameLoadId = null;
                const timeout = setTimeout(() => { frame.removeEventListener('load', ready); reject(new Error('Perfetto did not load. Check your connection.')); }, 30000);
                function ready() { clearTimeout(timeout); resolve(); }
                frame.addEventListener('load', ready, { once: true });
                frame.src = perfettoUrl; // Fresh frame; preserve Perfetto's supported URL.
            });
            if (!isCurrent()) return;
        }
        target = $('perfettoFrame').contentWindow;
    }
    $('handoff').textContent = 'Opening ' + name + ' in Perfetto…';
    try {
        const sent = !external && perfettoFrameLoadId === frameKey && range ?
            await scrollTrace(target, range, isCurrent) : await sendTrace(target, blob, name, range, isCurrent);
        if (sent && isCurrent()) {
            if (!external) perfettoFrameLoadId = frameKey;
            $('handoff').textContent = range ? 'Selected time range sent to Perfetto.' : 'Trace sent to Perfetto for local processing.';
        }
    } catch (error) { if (isCurrent()) $('handoff').textContent = error.message; }
}
$('perfettoFrame').onload = () => { perfettoFrameLoadId = null; };
$('file').onchange = event => {
    const file = event.target.files[0];
    event.target.value = ''; // A freshly copied recording can have the same SD filename.
    readFile(file).catch(error => { $('status').textContent = error.message; });
};
$('drop').ondragover = event => { event.preventDefault(); $('drop').classList.add('dragging'); };
$('drop').ondragleave = () => $('drop').classList.remove('dragging');
$('drop').ondrop = event => { event.preventDefault(); $('drop').classList.remove('dragging'); readFile(event.dataTransfer.files[0]).catch(error => { $('status').textContent = error.message; }); };
$('demo').onclick = () => analyze(fictionalCycle(), 'fictional-wisp-cycle.ndjson', true);
$('position').oninput = showPosition;
$('checkpoint').onchange = event => { if (event.target.value !== '') setPosition(Number(event.target.value)); };
$('previous').onclick = () => setPosition(Number($('position').value) - 1);
$('next').onclick = () => setPosition(Number($('position').value) + 1);
$('showSelectedEvent').onclick = () => eventTimeline?.reveal();
$('goEvent').onclick = () => { if ($('eventNumber').reportValidity()) { setPosition(Number($('eventNumber').value) - 1); $('eventJump').open = false; } };
$('eventNumber').onkeydown = event => { if (event.key === 'Enter') $('goEvent').click(); };
$('callSearch').oninput = renderCalls;
$('blockSearch').oninput = showPosition; $('objectSearch').oninput = showPosition;
$('entry').onclick = () => setPosition(selectedCall.startIndex);
$('exit').onclick = () => setPosition(selectedCall.endIndex);
$('resetZoom').onclick = () => { if (plot && converted) plot.setScale('x', { min: plot.data[0][0], max: Math.max(plot.data[0][0] + .001, plot.data[0].at(-1)) }); };
$('memoryClock').onchange = () => { if (converted) { drawGraph(); eventTimeline?.refresh(); showPosition(); } };
$('applyTimezone').onclick = () => {
    try { new Intl.DateTimeFormat('en', { timeZone: $('localTimezone').value }).format(); }
    catch { $('timezoneInfo').textContent = 'Enter a valid IANA zone, such as America/Los_Angeles.'; return; }
    localZone = $('localTimezone').value; wallBlob = null;
    $('timezoneInfo').textContent = 'Local display uses ' + localZone + '; recorded UTC stays unchanged.';
    if (converted) { drawGraph(); eventTimeline?.refresh(); showPosition(); }
    sensorCsv.refresh(); perfettoClockInfo();
    if (!$('perfettoView').classList.contains('hidden') && $('perfettoClock').value === 'wall' && sourceBlob) openPerfetto();
};
$('perfettoClock').onchange = () => { perfettoClockInfo(); if (sourceBlob) openPerfetto(); };
$('exportWall').onclick = () => {
    if (!wallClock) return;
    const url = URL.createObjectURL(new Blob([perfettoWallTrace(converted, wallClock, localZone)], { type: 'application/octet-stream' }));
    const link = document.createElement('a'); link.href = url;
    link.download = sourceName.replace(/\.(?:perfetto\.)?(ndjson|jsonl|json)$/i, '') + '.wall.pftrace';
    link.click(); setTimeout(() => URL.revokeObjectURL(url), 1000);
};
$('zoomEvent').onclick = () => {
    if (!plot || !converted) return;
    const index = Number($('position').value), utc = displayClock().wall;
    const center = utc ? wallClock.rows[index].utcUs / 1e6 : converted.report.history[index].timeUs / 1e6;
    const radius = utc ? 30 : 1;
    plot.setScale('x', { min: Math.max(plot.data[0][0], center - radius), max: Math.min(plot.data[0].at(-1), center + radius) });
    $('memorySection').scrollIntoView({ block: 'start' });
};
$('export').onclick = () => download(converted.perfetto, '.perfetto.json');
$('report').onclick = () => download(converted.report, '.memory.json');
$('perfettoInline').onclick = () => openPerfetto(); $('perfettoTab').onclick = () => openPerfetto(true);
$('perfettoSelection').onclick = () => openPerfetto(false, true);
$('inspectTab').onclick = () => view(false);
$('csvTab').onclick = () => view('samples');
const sensorCsv = mountSensorCsv(uPlot, view, () => localZone);
$('timelineTab').onclick = () => { if (sourceBlob) openPerfetto(); else { view(true); $('handoff').textContent = 'Load a recording first, or try the fictional Wisp cycle.'; } };
