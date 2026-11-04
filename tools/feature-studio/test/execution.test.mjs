import {checkout} from './checkout.mjs';
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { discover } from '../adapters/loom.mjs';
import { reconstructExample } from '../examples.mjs';
import { validateAssembly } from '../core.mjs';
import { workflowPresentation, graphBounds, graphRoutes, supportingFeatures, executionOutline, visibleBlockSpot, NODE_W, NODE_H } from '../public/graph-layout.js';
import { routineConnections } from '../public/execution-wiring.js';

const root = checkout;
const catalogPromise = discover(root);
async function example(file) {
  const catalog = await catalogPromise;
  return reconstructExample(await readFile(path.join(root, file), 'utf8'), file, catalog);
}
test('execution outline keeps callback registration and caller membership separate from routine steps',async()=>{
  const {assembly}=await example('examples/Hypnos/Hypnos_SD_Sleep/Hypnos_SD_Sleep.ino');
  const before=JSON.stringify(assembly),callback=assembly.routines.find(r=>r.kind==='callback'),step=callback.nodeIds[0];
  const sources={nodes:{[step]:{owners:[{object:'hypnos',className:'Loom_Hypnos'}],reference:{scope:'application',file:'Sleep.ino',line:80,endLine:80,match:'mapped'}}}};
  const outline=executionOutline(assembly,sources),group=outline.find(r=>r.id===callback.id);
  assert.deepEqual(outline.slice(0,2).map(r=>r.kind),['setup','loop']);
  assert.equal(group.bindings[0].kind,'registration');assert.equal(group.bindings[0].callerRoutine,'setup');
  assert.ok(!group.steps.some(s=>s.id===group.bindings[0].nodeId),'registering a callback is not a step inside that callback');
  assert.equal(group.steps[0].owners[0].className,'Loom_Hypnos');assert.equal(group.steps[0].reference.line,80);
  assert.equal(JSON.stringify(assembly),before,'navigation never changes code or graph membership');
});

test('every checkout example has an outline containing exactly its routine members in source order',async()=>{
  const catalog=await catalogPromise;
  for(const file of catalog.examplePaths) {
    const {assembly}=await example(file),outline=executionOutline(assembly);
    assert.equal(outline.length,assembly.routines.length,file);
    for(const routine of assembly.routines) {
      const group=outline.find(r=>r.id===routine.id);
      assert.deepEqual(group.steps.map(step=>step.id),routine.nodeIds,file+' · '+routine.name);
      assert.deepEqual(group.steps.map(step=>step.number),routine.nodeIds.map((_,index)=>index+1));
      assert.ok(group.steps.every(step=>!step.owners.length&&step.reference===null),'unloaded references are not invented');
    }
    const members=new Set(assembly.routines.flatMap(r=>r.nodeIds));
    assert.deepEqual(new Set(outline.flatMap(r=>r.steps.map(step=>step.id))),members,file);
  }
});

test('revealing a step finds space clear of both the settings panel and the ingredient dock',()=>{
  const viewport={x:0,y:0,w:1000,h:600},settings={x:630,y:0,w:370,h:600},dock={x:15,y:490,w:480,h:90};
  const size={w:200,h:140},preferred={x:750,y:400};
  const spot=visibleBlockSpot(viewport,[settings,dock],size,preferred);
  assert.ok(spot);assert.ok(spot.x+spot.w<settings.x);assert.ok(spot.y+spot.h<dock.y||spot.x>dock.x+dock.w);
});
test('sleep example separates setup, loop and registered callback with exact source membership', async () => {
  const r = await example('examples/Hypnos/Hypnos_SD_Sleep/Hypnos_SD_Sleep.ino'), g = r.assembly;
  const setup = g.routines.find(r => r.kind === 'setup'), loop = g.routines.find(r => r.kind === 'loop'), callback = g.routines.find(r => r.kind === 'callback');
  assert.equal(callback.name, 'isrTrigger');
  const node = id => g.nodes.find(n => n.id === id);
  assert.ok(setup.nodeIds.some(id => /manager.initialize\(\)/.test(node(id).reason)));
  const binding = callback.bindings[0];
  assert.equal(binding.kind, 'registration');
  assert.ok(setup.nodeIds.includes(binding.nodeId));
  assert.match(node(binding.nodeId).reason, /hypnos.registerInterrupt\(isrTrigger\)/);
  assert.ok(callback.nodeIds.some(id => /hypnos.wakeup\(\)/.test(node(id).reason)));
  const schedule = loop.nodeIds.map(node).find(n => n.reason.includes('setInterruptDuration'));
  assert.match(schedule.reason, /TimeSpan\(0, 0, 0, 10\)\)/);
  assert.ok(!g.edges.some(e => e.from === schedule.id && e.kind === 'cycle'), 'configuring a wake time does not itself restart loop');
  assert.ok(g.edges.some(e => e.kind === 'cycle' && e.to === schedule.id), 'the repeat route returns to the beginning of loop, including its configuration step');
  validateAssembly(g, r.design, await catalogPromise);
});
test('data callbacks with custom bodies and repeated calls remain distinct', async () => {
  const r = await example('examples/Internet/Logging/ThingSpeak/ThingSpeak.ino'), g = r.assembly;
  const callbacks = g.routines.filter(r => r.kind === 'callback');
  assert.deepEqual(callbacks.map(r => r.name), ['exampleNoParam', 'exampleParam']);
  const registrations = callbacks.flatMap(r => r.bindings.map(b => b.nodeId));
  assert.equal(new Set(registrations).size, 2, 'separate addFunction calls survive');
  for (const cb of callbacks) assert.ok(cb.nodeIds.some(id => g.nodes.find(n => n.id === id).label.startsWith('Custom code')));
  const native = await example('examples/Hardware/TippingBucket/TippingBucket.ino');
  assert.ok(native.assembly.routines.some(r => r.name === 'tipTrigger' && r.bindings.some(b => b.label.includes('attachInterrupt'))));
});
test('routine lanes preserve routes, collapse locally, and include empty sections in canvas bounds', async () => {
  const { assembly: g } = await example('examples/Hypnos/Hypnos_SD_Sleep/Hypnos_SD_Sleep.ino');
  const before = JSON.stringify(g), p = workflowPresentation(g);
  const operations=new Set(g.routines.flatMap(r=>r.nodeIds));
  assert.deepEqual(p.graph.nodes,g.nodes.filter(n=>operations.has(n.id)));
  assert.ok(!p.sections.some(s=>s.id==='components'),'feature cards are not repeated below the routines');
  assert.equal(p.sections[0].title, 'setup()'); assert.equal(p.sections[1].title, 'loop()');
  for (const r of g.routines) for (const id of r.nodeIds) {
    const section = p.sections.find(s => s.id === r.id), pos = p.positions[id];
    assert.ok(pos.y > section.y && pos.y + 118 < section.y + section.h);
  }
  const callback = g.routines.find(r => r.kind === 'callback');
  const closed = workflowPresentation(g, false, new Set([callback.id]));
  assert.ok(callback.nodeIds.every(id => !closed.positions[id]));
  assert.ok(closed.sections.find(s => s.id === callback.id).collapsed);
  assert.equal(JSON.stringify(g), before);
  const bounds = graphBounds(p.positions, graphRoutes(p.graph, p.positions), p.sections);
  assert.ok(bounds.y < p.sections[0].y);
});
test('routine metadata rejects dangling references and duplicate membership, while accepting older assemblies', async () => {
  const r = await example('examples/Hypnos/Hypnos_SD_Sleep/Hypnos_SD_Sleep.ino'), catalog = await catalogPromise;
  const duplicate = structuredClone(r.assembly); duplicate.routines[0].nodeIds.push(duplicate.routines[1].nodeIds[0]);
  assert.throws(() => validateAssembly(duplicate, r.design, catalog), /routine step/);
  const dangling = structuredClone(r.assembly); dangling.routines[0].bindings.push({nodeId:'missing', kind:'registration', label:'bad'});
  assert.throws(() => validateAssembly(dangling, r.design, catalog), /routine binding/);
  const legacy = structuredClone(r.assembly); delete legacy.routines;
  validateAssembly(legacy, r.design, catalog);
});

test('setup and callbacks are visibly wired by default, including legacy routine-only graphs', async () => {
  const { assembly:g } = await example('examples/Hypnos/Hypnos_SD_Sleep/Hypnos_SD_Sleep.ino');
  const setup = g.routines.find(r=>r.kind==='setup'), callback=g.routines.find(r=>r.kind==='callback');
  const p = workflowPresentation(g);
  for(let i=1;i<setup.nodeIds.length;i++) assert.ok(p.graph.edges.some(e=>e.kind==='flow'&&e.from===setup.nodeIds[i-1]&&e.to===setup.nodeIds[i]));
  assert.ok(p.graph.edges.some(e=>e.kind==='callback'&&e.from===callback.bindings[0].nodeId&&e.to===callback.nodeIds[0]));
  const legacy=structuredClone(g);legacy.edges=legacy.edges.filter(e=>!['flow','callback'].includes(e.kind));
  const original=JSON.stringify(legacy),wired=workflowPresentation(legacy);
  assert.ok(wired.graph.edges.some(e=>e.kind==='flow'&&e.derived));
  assert.ok(wired.graph.edges.some(e=>e.kind==='callback'&&e.navigationNodeId));
  assert.equal(JSON.stringify(legacy),original);
  assert.equal(routineConnections(g).length,g.edges.length,'explicit wires are not duplicated');
});

test('nested argument reads precede the consuming call and have a result wire', async () => {
  const {assembly:g}=await example('examples/Hypnos/Hypnos_SD_Metadata/Hypnos_SD_Metadata.ino');
  const setup=g.routines.find(r=>r.kind==='setup');
  const load=g.nodes.find(n=>n.reason.includes('mqtt.loadConfigFromJSON('));
  const read=g.nodes.find(n=>n.reason.startsWith('Source call in setup: hypnos.readFile("mqtt_creds.json")'));
  assert.ok(setup.nodeIds.includes(read.id));
  assert.ok(setup.nodeIds.indexOf(read.id)<setup.nodeIds.indexOf(load.id));
  assert.ok(g.edges.some(e=>e.from===read.id&&e.to===load.id&&e.label==='Argument result'));
  const presentation=workflowPresentation(g), routes=graphRoutes(presentation.graph,presentation.positions);
  const control=routes.find(r=>r.edge.kind==='flow'&&r.edge.from===read.id&&r.edge.to===load.id);
  const data=routes.find(r=>r.edge.kind==='data'&&r.edge.from===read.id&&r.edge.to===load.id);
  assert.ok(data?.edge.carriesOrder,'the single result wire also represents the step transition');
  assert.equal(control,undefined,'duplicate parallel wires are combined in the overview');
  assert.ok(g.edges.some(e=>e.kind==='flow'&&e.from===read.id&&e.to===load.id),'the underlying graph retains explicit order');
});

test('agent-authored conditional flow is preserved instead of inventing a linear route through both branches', () => {
  const nodes=['a','b','c','d'].map(id=>({id}));
  const edges=[['a','b','if ready'],['a','c','otherwise'],['b','d','complete'],['c','d','complete']].map(([from,to,label])=>({from,to,label,kind:'flow'}));
  const wired=routineConnections({nodes,edges,routines:[{kind:'setup',nodeIds:['a','b','c','d'],bindings:[]}]});
  assert.deepEqual(wired,edges);
});

test('routine branches share a column and rejoin in a continuous horizontal flow without folding', () => {
  const nodes=['a','b','c','d','e','f','g','h'].map(id=>({id,label:id,reason:'Operation',category:'system'}));
  const edges=[['a','b'],['a','c'],['b','d'],['c','d'],['d','e'],['e','f'],['f','g'],['g','h'],['h','a']].map(([from,to])=>({from,to,kind:'flow',label:'Next'}));
  const graph={nodes,edges,routines:[{id:'loop',name:'loop',kind:'loop',nodeIds:nodes.map(n=>n.id),bindings:[],reason:'Branch test'}]};
  const p=workflowPresentation(graph), pos=p.positions;
  assert.equal(pos.b.x,pos.c.x);assert.notEqual(pos.b.y,pos.c.y);assert.ok(pos.d.x>pos.b.x);
  assert.ok(pos.f.x>pos.e.x&&pos.h.x>pos.g.x,'long sequences keep advancing horizontally');
  assert.equal(pos.f.y,pos.a.y,'only branches need additional rows');
  for(let i=0;i<nodes.length;i++)for(let j=i+1;j<nodes.length;j++) {
    const a=pos[nodes[i].id],b=pos[nodes[j].id];
    assert.ok(Math.abs(a.x-b.x)>=NODE_W||Math.abs(a.y-b.y)>=NODE_H);
  }
  for(const r of graphRoutes(p.graph,pos))assert.ok(!/NaN|undefined/.test(r.d));
  assert.deepEqual(p.graph.edges.map(e=>[e.from,e.to]),edges.map(e=>[e.from,e.to]));
});

test('the feature overview includes added components without copying operation steps or editing the request', () => {
  const design={blocks:[{id:'sensor'}]},original=JSON.stringify(design);
  const nodes=['sensor','manager','hypnos','read','log'].map(id=>({id,added:id!=='sensor',reason:'Needed for requested logging',requestIds:['sensor']}));
  const assembly={nodes,routines:[{nodeIds:['read','log']}]};
  assert.deepEqual(supportingFeatures(design,assembly).map(n=>n.id),['manager','hypnos']);
  assert.equal(JSON.stringify(design),original);
});

test('component input links stay in details while the workflow shows operation payloads', () => {
  const graph={nodes:['component','read','log'].map(id=>({id,label:id,reason:'Verified source',category:'system'})),
    edges:[{from:'component',to:'read',kind:'data',label:'Sensor values'},{from:'read',to:'log',kind:'data',label:'Packet'}],
    routines:[{id:'loop',name:'loop',kind:'loop',nodeIds:['read','log'],bindings:[],reason:'Repeats'}]};
  const p=workflowPresentation(graph);
  assert.deepEqual(p.graph.nodes.map(n=>n.id),['read','log']);
  assert.ok(!p.graph.edges.some(e=>e.from==='component'));
  assert.ok(p.graph.edges.some(e=>e.from==='read'&&e.to==='log'&&e.carriesOrder));
  assert.equal(workflowPresentation(graph,true).graph.edges.length,2,'all component links remain available');
});
