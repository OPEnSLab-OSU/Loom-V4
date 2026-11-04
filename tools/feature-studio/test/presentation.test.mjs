import {checkout} from './checkout.mjs';
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { discover } from '../adapters/loom.mjs';
import { reconstructExample } from '../examples.mjs';
import { workflowPresentation, graphRoutes, friendlyNode, featureBoardPresentation, NODE_H } from '../public/graph-layout.js';
import { dropSamples } from '../public/game-audio.js';

test('feature export preserves requested and supplied feature cards without substituting workflow wiring',()=>{
  const nodes=[{id:'sensor',label:'Sensor',count:3,reason:'Read three sets'},{id:'hypnos',label:'Hypnos',added:true,reason:'Required for SD'}];
  const positions={sensor:{x:0,y:0},hypnos:{x:400,y:150}},before=JSON.stringify({nodes,positions});
  const presentation=featureBoardPresentation(nodes,positions);
  assert.deepEqual(presentation.graph.nodes,nodes);assert.deepEqual(presentation.graph.edges,[]);assert.deepEqual(presentation.sections,[]);
  assert.deepEqual(presentation.positions,positions);assert.equal(presentation.graph.nodes[0].count,3);
  assert.equal(JSON.stringify({nodes,positions}),before);
  assert.ok(Number.isFinite(featureBoardPresentation(nodes,{}).positions.hypnos.x));
});

test('workflow shows operations without repeating feature cards or changing the complete graph', async () => {
  const root = checkout, catalog = await discover(root);
  const file = 'examples/Internet/Communication/Max/Max_AP_MPU/Max_AP_MPU.ino';
  const graph = reconstructExample(await readFile(path.join(root, file), 'utf8'), file, catalog).assembly;
  const original = JSON.stringify(graph), simple = workflowPresentation(graph);
  const operations=new Set(graph.routines.flatMap(r=>r.nodeIds));
  assert.deepEqual(simple.graph.nodes,graph.nodes.filter(n=>operations.has(n.id)));
  for (const original of graph.edges.filter(e=>e.kind!=='dependency'&&(e.kind!=='data'||operations.has(e.from)&&operations.has(e.to)))) {
    assert.ok(simple.graph.edges.some(e=>e.from===original.from&&e.to===original.to&&(e.kind===original.kind||original.kind==='flow'&&e.carriesOrder)), 'every visible relationship survives, including merged step/data routes');
  }
  assert.ok(simple.graph.edges.length < graph.edges.filter(e => e.kind !== 'dependency').length);
  for (const edge of simple.graph.edges) {
    const source = graph.edges[edge.sourceIndex];
    assert.equal(edge.from, source.from); assert.equal(edge.to, source.to); assert.equal(edge.kind, source.kind);
    assert.equal(edge.detailLabel, source.label);
  }
  const read = graph.nodes.find(n => n.label === 'Read sensors');
  assert.equal(friendlyNode(read).category, 'sensors');
  const components = graph.nodes.filter(n => simple.roles[n.id] === 'components');
  assert.equal(components.length, 4);
  const wifi = components.find(n => n.label === 'Wi-Fi · wifi');
  assert.equal(friendlyNode(wifi).label, 'Wi-Fi');
  assert.match(friendlyNode(wifi).reason, /Instance “wifi”/);
  assert.match(friendlyNode({ ...wifi, label: 'Wi-Fi · backup' }).reason, /Instance “backup”/);
  const workflow = graph.nodes.filter(n => simple.roles[n.id] === 'workflow');
  const cycle = graphRoutes(simple.graph, simple.positions).find(r => r.edge.kind === 'cycle');
  assert.ok(cycle.points.some(p => p.y > Math.max(...workflow.map(n => simple.positions[n.id].y + NODE_H))));
  assert.ok(components.every(n=>!simple.positions[n.id]&&!simple.graph.nodes.includes(n)));
  assert.ok(!simple.sections.some(s=>s.id==='components'));
  assert.equal(workflowPresentation(graph, true).graph, graph);
  assert.equal(JSON.stringify(graph), original);
});

test('water drops are audible, bounded, finite and have distinct add/preview sounds', () => {
  for (const kind of ['click', 'add', 'remove', 'success', 'preview']) {
    const samples = dropSamples(kind, 22050);
    let peak = 0, energy = 0;
    for (const sample of samples) { assert.ok(Number.isFinite(sample)); peak = Math.max(peak, Math.abs(sample)); energy += sample * sample; }
    assert.ok(peak > (kind === 'remove' ? .035 : .15) && peak < .9, kind);
    assert.ok(Math.sqrt(energy / samples.length) > (kind === 'remove' ? .005 : .02), kind);
    assert.ok(samples.length >= 22050 * .2);
  }
  assert.notEqual(dropSamples('add').length, dropSamples('preview').length);
});

test('softened sounds start gradually and finish without a sharp cutoff',()=>{
  for(const kind of ['click','remove','add','preview','source']) {
    const samples=dropSamples(kind,44100);
    assert.equal(Math.abs(samples[0]),0);assert.equal(Math.abs(samples.at(-1)),0);
    assert.ok(Math.max(...samples.slice(0,88).map(Math.abs))<.025,kind+' has no sudden initial transient');
    let slope=0;for(let i=1;i<samples.length;i++)slope=Math.max(slope,Math.abs(samples[i]-samples[i-1]));
    assert.ok(slope<.04,kind+' has limited high-frequency transients');
  }
});
