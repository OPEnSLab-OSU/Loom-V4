// A bounded, paged view of the entire event ledger. All labels are plain text.
export function createEventTimeline({ report, select, selectCall, getClock }) {
    const $ = id => document.getElementById(id);
    const ns = 'http://www.w3.org/2000/svg', pageSize = 100;
    const time = us => (us / 1000).toLocaleString(undefined, { maximumFractionDigits: 3 }) + ' ms';
    let matches = report.history, page = 0, selectedIndex = -1, zoom = null, suppressClick = false;
    const stamp = row => getClock().format(getClock().at(row.index));
    const categories = [...new Set(report.history.map(row => row.category))];
    $('eventCategory').replaceChildren(new Option('All event types', ''), ...categories.map(name => new Option(name, name)));
    function tooltip(row) {
        return row.eventName + '\nEvent ' + (row.index + 1) + ' · ' + stamp(row) + '\nAwake: ' + time(row.timeUs) +
            (row.context ? '\nDuring: ' + row.context : '') + (row.address && row.address !== '0x0' ? '\nAddress: ' + row.address : '') +
            (row.incomplete ? '\nCapture coverage has gaps; see Capture quality' : '');
    }
    function choose(row) {
        selectCall(row.callId ? report.calls.find(call => call.id === row.callId) : null);
        select(row.index);
    }
    function svgElement(name, attributes = {}, text = '') {
        const element = document.createElementNS(ns, name);
        for (const [key, value] of Object.entries(attributes)) element.setAttribute(key, String(value));
        if (text) element.textContent = text;
        return element;
    }
    function interact(element, description, index, click) {
        element.setAttribute('tabindex', '0'); element.setAttribute('role', 'button');
        element.setAttribute('aria-label', description); element.dataset.eventIndex = index;
        element.append(svgElement('title', {}, description));
        const show = () => { $('timelineHover').textContent = description; };
        element.onpointerenter = show; element.onfocus = show; element.onclick = () => { if (!suppressClick) click(); };
        element.onkeydown = event => { if (event.key === 'Enter' || event.key === ' ') { event.preventDefault(); click(); } };
    }
    function render() {
        const clock = getClock(), pageRows = matches.slice(page * pageSize, (page + 1) * pageSize);
        const fullStart = pageRows.length ? clock.at(pageRows[0].index) : 0;
        const fullEnd = pageRows.length ? Math.max(fullStart + 1, clock.at(pageRows.at(-1).index)) : 1;
        const start = zoom?.start ?? fullStart, end = zoom?.end ?? fullEnd;
        const rows = pageRows.filter(row => clock.at(row.index) >= start && clock.at(row.index) <= end);
        $('timelineRange').textContent = clock.label + ' · ' + clock.format(start) + ' → ' + clock.format(end) + (zoom ? ' · zoomed' : '');
        $('timelineReset').disabled = !zoom;
        $('eventsPrevious').disabled = page === 0; $('eventsNext').disabled = (page + 1) * pageSize >= matches.length;
        $('eventPage').textContent = rows.length ? 'Matches ' + (page * pageSize + 1) + '–' + (page * pageSize + rows.length) + ' of ' + matches.length +
            ' · ' + rows.length + ' visible in this time range' : 'No events in this range';
        $('eventRows').replaceChildren(...rows.map(row => {
            const button = document.createElement('button'); button.className = 'eventRow';
            button.dataset.eventIndex = row.index; button.title = tooltip(row);
            const labelTime = document.createElement('small'); labelTime.textContent = '#' + (row.index + 1) + ' · ' + stamp(row) + ' · ' + row.category;
            const label = document.createElement('span'); label.textContent = row.eventName;
            button.append(labelTime, label); button.onclick = () => choose(row); return button;
        }));
        const svg = svgElement('svg', { role: 'group', 'aria-label': 'Scrollable function and event timeline' });
        if (!pageRows.length) { $('eventTimeline').replaceChildren(svg); return; }
        const width = Math.max(1000, $('timelineScroll').clientWidth) * Number($('timelineZoom').value);
        const left = 170, right = width - 40, x = t => left + (Math.max(start, Math.min(end, t)) - start) / (end - start) * (right - left);
        svg.setAttribute('width', width); let y = 40;
        for (let tick = 0; tick <= 6; tick++) {
            const tx = left + tick / 6 * (right - left);
            svg.append(svgElement('text', { x: tx, y: 20, 'text-anchor': tick === 6 ? 'end' : 'start', class: 'timeTick' }, clock.tick(start + (end - start) * tick / 6)));
        }
        const calls = report.calls.filter(call => clock.at(call.startIndex) <= end && clock.at(call.endIndex) >= start &&
            call.startIndex <= pageRows.at(-1).index && call.endIndex >= pageRows[0].index);
        const depths = [...new Set(calls.map(call => call.depth))].sort((a, b) => a - b);
        for (const depth of depths) {
            svg.append(svgElement('text', { x: 12, y: y + 20, class: 'laneLabel' }, depth === 0 ? 'Functions' : 'Nested level ' + depth));
            for (const call of calls.filter(call => call.depth === depth)) {
                const callStart = clock.at(call.startIndex), callEnd = clock.at(call.endIndex);
                const spans = []; let cursor = callStart;
                if (clock.wall) {
                    for (let i = call.startIndex + 1; i <= call.endIndex; i++) {
                        // Coordinate discontinuities represent standby, not CPU work.
                        const gapStart = clock.at(i - 1) + report.history[i].timeUs - report.history[i - 1].timeUs;
                        if (clock.at(i) - gapStart > 1000) { spans.push([cursor, gapStart]); cursor = clock.at(i); }
                    }
                }
                spans.push([cursor, callEnd]);
                for (const [spanStart, spanEnd] of spans) {
                if (spanEnd < start || spanStart > end) continue;
                const gx = x(spanStart), w = Math.max(3, x(spanEnd) - gx);
                const group = svgElement('g', { class: 'callSlice' });
                group.append(svgElement('rect', { x: gx, y, width: w, height: 30, rx: 4 }));
                // Clip each text label to its own slice; zoom creates more room.
                const clipId = 'call-clip-' + call.id + '-' + spanStart;
                const clip = svgElement('clipPath', { id: clipId }); clip.append(svgElement('rect', { x: gx + 5, y, width: Math.max(0, w - 10), height: 30 }));
                svg.append(clip);
                group.append(svgElement('text', { x: gx + 7, y: y + 20, 'clip-path': 'url(#' + clipId + ')' }, call.displayName || call.name));
                interact(group, (call.displayName || call.name) + '\n' + call.signature + '\n' + call.file + ':' + call.line +
                    '\nEntry: ' + clock.format(clock.at(call.startIndex)) + '\nAwake duration: ' + time(call.endUs - call.startUs) +
                    (clock.wall ? '\nWall elapsed: ' + time(clock.at(call.endIndex) - clock.at(call.startIndex)) : '') + '\n' + call.status,
                    call.startIndex, () => { selectCall(call); select(call.startIndex); });
                svg.append(group);
                }
            }
            y += 38;
        }
        const colors = { Calls: '#527dbe', Heap: '#b47816', Objects: '#176c58', 'Sleep & RTC': '#8762a1', Recorder: '#60717a' };
        for (const category of categories) {
            const items = rows.filter(row => row.category === category); if (!items.length) continue;
            const ends = [];
            svg.append(svgElement('text', { x: 12, y: y + 20, class: 'laneLabel' }, category));
            for (const row of items) {
                const gx = x(clock.at(row.index)), labelWidth = Math.min(460, row.eventName.length * 7 + 16);
                let lane = ends.findIndex(endX => endX + 12 < gx);
                if (lane < 0) lane = ends.length;
                ends[lane] = gx + labelWidth;
                const gy = y + lane * 28, group = svgElement('g', { class: 'eventMarker' });
                group.append(svgElement('circle', { cx: gx, cy: gy + 15, r: 5, fill: colors[category] || '#527dbe' }));
                const label = row.eventName.length > 62 ? row.eventName.slice(0, 59) + '…' : row.eventName;
                group.append(svgElement('text', { x: gx + 10, y: gy + 19 }, label));
                // Give even a very short event a generous pointer target.
                group.append(svgElement('rect', { x: gx - 7, y: gy + 2, width: labelWidth + 12, height: 26, fill: 'transparent' }));
                interact(group, tooltip(row), row.index, () => choose(row)); svg.append(group);
            }
            y += Math.max(1, ends.length) * 28 + 12;
        }
        // The right margin includes the final event label rather than cutting it off.
        svg.setAttribute('width', width + 480); svg.setAttribute('height', y + 20);
        $('eventTimeline').replaceChildren(svg); highlight(selectedIndex);
        const band = svgElement('rect', { class: 'timelineZoomBand', y: 30, height: y - 10, visibility: 'hidden', 'pointer-events': 'none' });
        svg.append(band);
        let drag = null;
        const localX = event => Math.max(left, Math.min(right, event.clientX - svg.getBoundingClientRect().left));
        const fromX = value => start + (value - left) / (right - left) * (end - start);
        svg.onpointerdown = event => {
            if (event.button !== 0 || localX(event) <= left) return;
            suppressClick = false; drag = { start: localX(event), current: localX(event) };
        };
        svg.onpointermove = event => {
            if (!drag) return; drag.current = localX(event);
            if (Math.abs(drag.current - drag.start) >= 6) svg.setPointerCapture(event.pointerId);
            band.setAttribute('x', Math.min(drag.start, drag.current)); band.setAttribute('width', Math.abs(drag.current - drag.start));
            band.setAttribute('visibility', 'visible');
        };
        svg.onpointerup = event => {
            if (!drag) return;
            const stop = localX(event), moved = Math.abs(stop - drag.start);
            if (moved >= 6) {
                suppressClick = true;
                zoom = { start: fromX(Math.min(stop, drag.start)), end: fromX(Math.max(stop, drag.start)) };
                drag = null; $('timelineScroll').scrollLeft = 0; render();
            } else { drag = null; band.setAttribute('visibility', 'hidden'); }
        };
        svg.onpointercancel = () => { drag = null; band.setAttribute('visibility', 'hidden'); };
        svg.ondblclick = event => { event.preventDefault(); zoom = null; suppressClick = false; render(); };
    }
    function highlight(index) {
        selectedIndex = index;
        for (const item of document.querySelectorAll('[data-event-index]')) item.classList.toggle('selectedEvent', Number(item.dataset.eventIndex) === index);
        const row = report.history[index]; if (row) $('timelineSelection').textContent = tooltip(row);
    }
    function filter() {
        const query = $('eventSearch').value.toLowerCase(), category = $('eventCategory').value;
        matches = report.history.filter(row => (!category || row.category === category) &&
            (row.eventName + ' ' + row.context + ' ' + row.address).toLowerCase().includes(query));
        page = 0; zoom = null; render();
    }
    $('eventSearch').oninput = filter; $('eventCategory').onchange = filter;
    $('eventsPrevious').onclick = () => { if (page) { page--; zoom = null; render(); } };
    $('eventsNext').onclick = () => { if ((page + 1) * pageSize < matches.length) { page++; zoom = null; render(); } };
    $('timelineZoom').oninput = () => { $('timelineZoomLabel').textContent = $('timelineZoom').value + '×'; render(); };
    function reveal() {
        $('eventSearch').value = ''; $('eventCategory').value = ''; matches = report.history;
        page = Math.floor(Math.max(0, selectedIndex) / pageSize); zoom = null; render();
        const marker = $('eventTimeline').querySelector('.eventMarker.selectedEvent') ||
            $('eventTimeline').querySelector('.callSlice.selectedEvent');
        // Move the visible SVG viewport, not the collapsed text list underneath it.
        marker?.scrollIntoView({ block: 'center', inline: 'center' });
        $('timelineHover').textContent = tooltip(report.history[selectedIndex]);
    }
    $('timelineFollow').onclick = reveal;
    $('timelineReset').onclick = () => { zoom = null; suppressClick = false; render(); };
    $('timelineZoomSelected').onclick = () => {
        if (selectedIndex < 0) return;
        $('eventSearch').value = ''; $('eventCategory').value = ''; matches = report.history;
        page = Math.floor(selectedIndex / pageSize);
        const clock = getClock(), center = clock.at(selectedIndex), first = clock.at(page * pageSize), last = clock.at(Math.min(matches.length - 1, (page + 1) * pageSize - 1));
        const radius = Math.max(100, (last - first) / 20);
        zoom = { start: Math.max(first, center - radius), end: Math.max(center + 1, Math.min(last, center + radius)) };
        suppressClick = false; render(); $('timelineScroll').scrollLeft = 0;
    };
    $('eventSearch').value = ''; $('eventCategory').value = ''; render();
    return { highlight, reveal, refresh: () => { zoom = null; render(); } };
}
