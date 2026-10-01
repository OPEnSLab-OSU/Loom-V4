// Interpreted JS check for local mock behavior. No browser connection or firmware compiler.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';

class Element {
    constructor(tag) { this.tag = tag; this.children = []; this.value = ''; this.textContent = ''; }
    replaceChildren() { this.children = []; this.value = ''; }
    append(...children) {
        this.children.push(...children);
        if (this.tag === 'select' && !this.value && children.length) this.value = children[0].value;
    }
    setAttribute() {}
    addEventListener() {}
}
const ids = ['device', 'receipts', 'details', 'rose', 'windSummary', 'notice', 'demo', 'upload', 'download'];
const elements = Object.fromEntries(ids.map(id => [id, new Element(id === 'device' ? 'select' : id)]));
const context = vm.createContext({document: {
    getElementById: id => elements[id],
    createElement: tag => new Element(tag),
    createElementNS: (_, tag) => new Element(tag),
}});
vm.runInContext(fs.readFileSync(new URL('./viewer.js', import.meta.url), 'utf8'), context);
assert.equal(elements.device.value, 'Wisp1');
assert.equal(elements.receipts.children.length, 32);
assert.match(elements.windSummary.textContent, /^29 wind observations/);
elements.device.value = 'Wisp2';
vm.runInContext('render()', context);
assert.equal(elements.receipts.children.length, 16);
assert.match(elements.windSummary.textContent, /^9 wind observations · 5 calm/);
vm.runInContext(`
const sample = {topic:'Sensors/Wisp1',digest:'abc',status:'received',payload:JSON.stringify({
 type:'data',contents:[{data:{WindSpeed_mps:1,WindDirection_deg:360,'PM2.5':20}}]})};
const duplicate = {...sample,status:'duplicate'};
const heartbeat = {...sample,digest:'hb',payload:JSON.stringify({type:'heartbeat',contents:[]})};
const result = windBins([sample,sample,duplicate,heartbeat]);
if (result.bins[0].count !== 1 || result.missing !== 0) throw Error('Retries/heartbeat inflated observations');
if (reading({contents:{invalid:true}},'WindSpeed_mps') !== null) throw Error('Invalid contents not tolerated');
if (batteryVolts({contents:[{data:{Vbat_MV:3940}}]}) !== 3.94) throw Error('Battery units not converted');
if (batteryVolts({contents:[]}) !== '—') throw Error('Missing battery displayed as zero');
let rejected = false;
try { load([{topic:42}]); } catch { rejected = true; }
if (!rejected) throw Error('Malformed import accepted');
load([]);
`, context);
assert.match(elements.windSummary.textContent, /^0 wind observations/);
console.log('PASS local viewer: demo, device filter, duplicate exclusion, heartbeat exclusion, malformed/empty imports.');
