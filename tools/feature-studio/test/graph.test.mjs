import {checkout} from './checkout.mjs';
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { layoutGraph, workflowPresentation, routeEdge, graphRoutes, graphBounds, canvasFrame, NODE_W, NODE_H } from '../public/graph-layout.js';
import { extractHeader, discover } from '../adapters/loom.mjs';
import { reconstructExample } from '../examples.mjs';

test('dependency hierarchy and data flow advance through layers; timing cycles do not distort the ranks', () => {
  const g = {
    nodes: ['manager', 'sensor', 'measure', 'packet', 'store'].map(id => ({ id })),
    edges: [
      { from: 'manager', to: 'sensor', kind: 'dependency' },
      { from: 'sensor', to: 'measure', kind: 'data' },
      { from: 'measure', to: 'packet', kind: 'data' },
      { from: 'packet', to: 'store', kind: 'data' },
      { from: 'store', to: 'measure', kind: 'cycle' }
    ]
  };
  const p = layoutGraph(g);
  for (const e of g.edges.filter(e => e.kind !== 'cycle')) assert.ok(p[e.to].x > p[e.from].x);
  assert.deepEqual(layoutGraph(g), p);
});
test('mutually dependent components stay in a finite tier with distinct positions', () => {
  const g = { nodes: ['a', 'b', 'out'].map(id => ({ id })), edges: [
    { from: 'a', to: 'b', kind: 'dependency' }, { from: 'b', to: 'a', kind: 'dependency' },
    { from: 'b', to: 'out', kind: 'data' }
  ] };
  const p = layoutGraph(g);
  assert.equal(p.a.x, p.b.x);
  assert.ok(Math.abs(p.a.y - p.b.y) >= NODE_H + 15);
  assert.ok(p.out.x > p.b.x);
});
test('return arrows terminate at the incoming left port, with an outside loop', () => {
  const p = { source: { x: 523, y: 80 }, middle: { x: 279, y: 80 }, target: { x: 35, y: 80 } };
  const r = routeEdge({ from: 'source', to: 'target', kind: 'cycle' }, p);
  assert.deepEqual(r.start, { x: 523 + NODE_W, y: 80 + NODE_H / 2 });
  assert.deepEqual(r.end, { x: 35, y: 80 + NODE_H / 2 });
  assert.ok(r.points.some(v => v.y > 80 + NODE_H + 15));
  assert.ok(r.d.endsWith('L35,139'));
});
test('terminal cadence sits after the workflow and its outside rail and label are inside the graph bounds', () => {
  const g = { nodes: ['manager', 'measure', 'packet', 'send', 'wait'].map(id => ({ id })), edges: [
    { from: 'manager', to: 'measure', kind: 'dependency' },
    { from: 'measure', to: 'packet', kind: 'data' },
    { from: 'packet', to: 'send', kind: 'data' },
    { from: 'manager', to: 'wait', kind: 'dependency' },
    { from: 'wait', to: 'measure', kind: 'cycle', label: 'loop cadence; review conditions' }
  ] };
  const positions = layoutGraph(g), routes = graphRoutes(g, positions), bounds = graphBounds(positions, routes);
  assert.ok(positions.wait.x > positions.send.x);
  for (const route of routes) {
    for (const p of route.points || [route.start, route.end]) {
      assert.ok(p.x >= bounds.x && p.x <= bounds.x + bounds.w);
      assert.ok(p.y >= bounds.y && p.y <= bounds.y + bounds.h);
    }
    if (route.label) {
      assert.ok(route.label.y + route.label.h <= bounds.y + bounds.h);
      assert.ok(route.label.x + route.label.w <= bounds.x + bounds.w);
    }
  }
});
test('panning slack survives zoom and includes negative return rails on every side', () => {
  const bounds = { x: -64, y: -30, w: 1800, h: 900 };
  for (const zoom of [.2, 1, 3]) {
    const frame = canvasFrame(bounds, zoom, 1048, 547);
    const left = frame.x + bounds.x * zoom, top = frame.y + bounds.y * zoom;
    const right = frame.width - (left + bounds.w * zoom), bottom = frame.height - (top + bounds.h * zoom);
    assert.ok(left >= 180 && right >= 180);
    assert.ok(top >= 547 && bottom >= 547, 'a full viewport of slack clears even an expanded dock');
  }
});
test('a long forward wire detours around an intervening card', () => {
  const p = { source: { x: 35, y: 80 }, obstacle: { x: 279, y: 80 }, target: { x: 523, y: 80 } };
  const r = routeEdge({ from: 'source', to: 'target', kind: 'data' }, p);
  assert.equal(r.direct, false);
  assert.ok(r.labelY < 80 || r.labelY > 80 + NODE_H);
  assert.deepEqual(r.end, { x: 523, y: 139 });
});
test('connection tags preserve full names and occupy separate readable positions', () => {
  const g = {
    nodes: ['a', 'b', 'c'].map(id => ({ id })),
    edges: [
      { from: 'a', to: 'b', kind: 'dependency', label: 'A complete connection name that needs more than one line' },
      { from: 'a', to: 'c', kind: 'data', label: 'The next complete connection name' }
    ]
  };
  const routes = graphRoutes(g, layoutGraph(g));
  for (const r of routes) {
    assert.equal(r.label.lines.join(' '), r.edge.label);
    assert.ok(r.label.w > 0 && r.label.h >= 20);
    assert.ok(!r.label.lines.some(line => line.includes('…')));
  }
  assert.notDeepEqual(routes[0].label, routes[1].label);
});

test('long step labels fit between horizontal cards; crowded labels never fall back behind cards or headings', () => {
  const nodes=Array.from({length:9},(_,i)=>({id:'n'+i,label:'Step '+i,reason:'Actual operation',category:'system'}));
  const edges=nodes.slice(1).map((n,i)=>({from:nodes[i].id,to:n.id,kind:'flow',label:'After rails, RTC and SD initialization returned'}));
  const graph={nodes,edges,routines:[{id:'setup',name:'setup',kind:'setup',nodeIds:nodes.map(n=>n.id),bindings:[],reason:'Initialization'}]};
  const p=workflowPresentation(graph),routes=graphRoutes(p.graph,p.positions,p.sections);
  for(const r of routes) {
    const a=p.positions[r.edge.from],b=p.positions[r.edge.to];
    assert.equal(a.y,b.y,'sequential work stays horizontal');
    assert.ok(r.label.x>a.x+NODE_W+14&&r.label.x+r.label.w<b.x-14,'label has a dedicated gap');
  }
  const crowded={nodes:nodes.slice(0,2),edges:Array.from({length:35},(_,i)=>({from:'n0',to:'n1',kind:'data',label:'A distinct crowded connection condition '+i}))};
  const pos={n0:{x:35,y:150},n1:{x:240,y:150}},sections=[{x:20,y:70,w:450,h:64,collapsed:true}];
  const all=graphRoutes(crowded,pos,sections),overlap=(a,b)=>a.x<b.x+b.w&&a.x+a.w>b.x&&a.y<b.y+b.h&&a.y+a.h>b.y;
  const obstacles=[...Object.values(pos).map(p=>({...p,w:NODE_W,h:NODE_H})),...sections];
  for(let i=0;i<all.length;i++) {
    assert.ok(!obstacles.some(b=>overlap(all[i].label,b)),'no node or header covers label '+i);
    assert.ok(!all.slice(0,i).some(r=>overlap(all[i].label,r.label)),'labels do not cover one another');
  }
  assert.ok(all.some(r=>r.leader),'displaced labels stay attached to their connections');
});
test('example routes do not jump across functions or different Manager instances', () => {
  const blocks = [
    ...extractHeader('class Manager { public: Manager(const char *name, int id); };', 'Loom_Manager.h'),
    ...extractHeader('class Loom_Test { public: Loom_Test(Manager &man); };', 'Internet/Logging/Test.h')
  ];
  const catalog = { adapter: { id: 'loom', target: 'Feather M0' }, fingerprint: 'test', blocks };
  const source = 'Manager first("a",1);\nManager second("b",2);\nLoom_Test reporter(first);\nvoid setup(){ first.package(); }\nvoid helper(){ first.measure(); }\nvoid loop(){ second.package(); reporter.publish(); }\n';
  const r = reconstructExample(source, 'examples/Scopes/Scopes.ino', catalog);
  assert.equal(r.assembly.edges.filter(e => e.kind === 'data').length, 0);
  assert.ok(r.assembly.nodes.some(n => n.label === 'Package measurements · setup'));
  const correct = reconstructExample(source.replace('second.package(); reporter.publish();', 'first.measure(); first.package(); reporter.publish();'), 'examples/Scopes/Scopes.ino', catalog);
  assert.ok(correct.assembly.edges.some(e => e.label === 'packet route · loop'));
});
test('every repository example lays out and routes without invented graph references', async () => {
  const root = checkout, catalog = await discover(root);
  for (const file of catalog.examplePaths) {
    const g = reconstructExample(await readFile(path.join(root, file), 'utf8'), file, catalog).assembly;
    const p = layoutGraph(g);
    for (const [i, e] of g.edges.entries()) {
      const route = routeEdge(e, p, i);
      assert.ok(route && !route.d.includes('NaN'), file);
      if (['flow','callback'].includes(e.kind)) assert.ok(route.end.x >= p[e.to].x && route.end.x <= p[e.to].x + 184, file);
      else assert.equal(route.end.x, p[e.to].x, file);
      if (e.kind !== 'cycle') assert.ok(p[e.to].x >= p[e.from].x, file);
    }
  }
  const file = 'examples/Internet/Logging/ThingSpeak/ThingSpeak.ino';
  const g = reconstructExample(await readFile(path.join(root, file), 'utf8'), file, catalog).assembly;
  const custom = g.nodes.find(n => n.id === 'example-custom');
  const callbackOwner = g.nodes.find(n => n.label === 'ThingSpeak · thingspeak');
  assert.ok(g.edges.some(e => e.from === custom.id && e.to === callbackOwner.id && e.label === 'callback behavior reference'));
});
