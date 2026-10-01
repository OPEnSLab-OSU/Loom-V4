const assert = require('node:assert/strict');
const { pathToFileURL } = require('node:url');
const path = require('node:path');
class Element {
    constructor(tag = 'div') { this.tag = tag; this.children = []; this.attrs = {}; this.dataset = {}; this.value = ''; this.clientWidth = 1000; this.classList = { toggle() {} }; }
    setAttribute(k, v) { this.attrs[k] = v; }
    append(...items) { this.children.push(...items); }
    replaceChildren(...items) { this.children = items; }
    getBoundingClientRect() { return { left: 0 }; }
    setPointerCapture() { this.captured = true; }
}
const ids = new Map();
global.document = { getElementById(id) { if (!ids.has(id)) ids.set(id, new Element()); return ids.get(id); },
    createElement: tag => new Element(tag), createElementNS: (_, tag) => new Element(tag), querySelectorAll: () => [] };
global.Option = function(text, value) { this.text = text; this.value = value; };
(async () => {
    const { createEventTimeline } = await import(pathToFileURL(path.join(__dirname, '../tools/trace-studio/event-timeline.js')).href);
    const history = Array.from({ length: 200 }, (_, index) => ({ index, timeUs: index * 1000, eventName: 'Checkpoint ' + index, category: 'Heap' }));
    document.getElementById('timelineZoom').value = 1;
    let selected = -1;
    const clock = { wall: false, at: index => history[index].timeUs, format: us => us + ' us', tick: us => us + ' us', label: 'Execution' };
    createEventTimeline({ report: { history, calls: [] }, select: index => selected = index, selectCall() {}, getClock: () => clock });
    const svg = () => document.getElementById('eventTimeline').children[0];
    const e = x => ({ button: 0, pointerId: 1, clientX: x });
    const first = svg();
    first.onpointerdown(e(250)); first.onpointerup(e(251));
    assert.ok(!first.captured, 'A simple click must retain its original event target');
    first.children.find(child => child.attrs.class === 'eventMarker').onclick();
    assert.equal(selected, 0);
    first.onpointerdown(e(250)); first.onpointermove(e(500)); first.onpointerup(e(500));
    assert.ok(first.captured);
    assert.match(document.getElementById('timelineRange').textContent, /zoomed/);
    const visible = document.getElementById('eventRows').children;
    assert.ok(visible.length > 0 && visible.length < 100);
    const bounds = visible.map(row => row.dataset.eventIndex);
    assert.ok(bounds[0] > 0 && bounds.at(-1) < 99);
    document.getElementById('timelineReset').onclick();
    assert.equal(document.getElementById('eventRows').children.length, 100);
    document.getElementById('eventsNext').onclick();
    assert.equal(document.getElementById('eventRows').children[0].dataset.eventIndex, 100);
    assert.doesNotMatch(document.getElementById('timelineRange').textContent, /zoomed/);
    console.log('PASS event timeline: click target preserved, drag narrows actual event range, reset and next page restore bounds');
})().catch(error => { console.error(error); process.exitCode = 1; });
