import {checkout} from './checkout.mjs';
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { mkdtemp, mkdir, writeFile, readFile, rm, readdir } from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import { createHash } from 'node:crypto';
import { fileURLToPath } from 'node:url';
import { startStudio } from '../server.mjs';
import { discover, extractHeader } from '../adapters/loom.mjs';
import { indexRecipe, relevantRecipes } from '../recipe-index.mjs';
import { reconstructExample, summarizeExample } from '../examples.mjs';
import { workflowPresentation, graphRoutes, graphBounds } from '../public/graph-layout.js';

test('a fresh laptop starts blank; sparse behavior requests include current recipes; new/restore preserve the design and result', async t => {
  const temp = await mkdtemp(path.join(os.tmpdir(), 'feature-new-design-')), root = path.join(temp, 'repo'), workspace = path.join(temp, 'local');
  await mkdir(path.join(root, 'src'), { recursive: true });
  await mkdir(path.join(root, 'examples', 'Probe'), { recursive: true });
  await mkdir(path.join(root, 'examples', 'Archive'), { recursive: true });
  await writeFile(path.join(root, 'src', 'Probe.h'), 'class Manager { public: Manager(const char *name, int id); };\nclass Loom_Probe { public: Loom_Probe(Manager &man); };\nclass Loom_Archive { public: Loom_Archive(Manager &man); };');
  await writeFile(path.join(root, 'examples', 'Probe', 'Probe.ino'), 'Manager manager("a", 1);\nLoom_Probe probe(manager);\nvoid setup(){manager.initialize();}\nvoid loop(){manager.measure();manager.package();}\n');
  await writeFile(path.join(root, 'examples', 'Archive', 'Log.ino'), 'Manager manager("a", 1);\nLoom_Archive archive(manager);\nvoid setup(){manager.initialize();}\nvoid loop(){manager.measure();manager.package();archive.logToSD();manager.pause(300000);}\n');
  let app = await startStudio({ root, workspace, port: 0 });
  const close = () => new Promise(resolve => { app.server.close(resolve); app.server.closeAllConnections(); });
  t.after(async () => { await close(); await rm(temp, { recursive: true, force: true }); });
  const get = async route => (await fetch(app.url + route)).json();
  const post = async (route, body = {}) => {
    const r = await fetch(app.url + route, { method: 'POST', headers: {'Content-Type':'application/json'}, body:JSON.stringify(body) });
    const result = await r.json(); assert.equal(r.status, 200, JSON.stringify(result)); return result;
  };
  const initial = await get('/api/state');
  assert.deepEqual(initial.design.blocks, []); assert.equal(initial.design.description, '');
  const design = { ...initial.design, name:'Just a sensor', description:'Log each reading to SD every 5 minutes.', blocks:[{id:'probe',catalogId:'class:Loom_Probe',label:'Probe',count:1,settings:{},notes:''}] };
  await post('/api/design', design);
  const request = JSON.parse(await readFile(path.join(workspace, 'request.json'), 'utf8'));
  assert.equal(request.catalog.blocks.length, 1, 'supporting requirements are not demanded from the user');
  assert.ok(request.catalog.relevantRecipes.some(r => r.path === 'examples/Probe/Probe.ino'));
  assert.ok(request.catalog.relevantRecipes.some(r => r.path === 'examples/Archive/Log.ino'));
  assert.equal(request.design.description, design.description);
  assert.ok(request.sourceRoot); assert.ok(request.sourceFingerprint);
  const prompt = await readFile(path.join(workspace, 'codex-prompt.txt'), 'utf8');
  assert.match(prompt, /assume no prior chat context/); assert.match(prompt, /catalog.relevantRecipes/);
  await post('/api/design/new');
  let state = await get('/api/state');
  assert.equal(state.design.blocks.length, 0); assert.equal(state.design.description, '');
  assert.equal(state.assembly, null); assert.equal(state.assemblyError, ''); assert.equal(state.previousDesignAvailable, true);
  await close(); app = await startStudio({ root, workspace, port: 0 });
  await post('/api/design/restore'); state = await get('/api/state');
  assert.deepEqual(state.design, design, 'restore survives an app restart');
  await post('/api/example', {path:'examples/Archive/Log.ino'});
  const recipe = await get('/api/state');
  await close();
  const legacy = structuredClone(recipe.assembly); legacy.edges = legacy.edges.filter(e=>!['flow','callback'].includes(e.kind));
  await writeFile(path.join(workspace,'assembly.json'),JSON.stringify(legacy));
  await writeFile(path.join(workspace,'origin.json'),JSON.stringify({kind:'example',path:'examples/Archive/Log.ino',signature:createHash('sha256').update(JSON.stringify(legacy)).digest('hex')}));
  app=await startStudio({root,workspace,port:0});
  const upgraded=await get('/api/state');
  assert.deepEqual(upgraded.design,recipe.design,'old recipe wiring upgrades without changing requested features');
  assert.ok(upgraded.assembly.edges.some(e=>e.kind==='flow'));
  assert.equal(upgraded.assemblyOrigin,'example');
  await post('/api/design/new'); await post('/api/design/restore');
  state = await get('/api/state');
  assert.deepEqual(state.design, recipe.design); assert.deepEqual(state.assembly, recipe.assembly); assert.equal(state.assemblyOrigin, 'example');
  assert.ok((await readdir(path.join(workspace, 'design-history'))).length >= 4);
});

test('recipe matching uses active code and demand text, not commented alternatives, diagnostic strings or hardcoded paths', () => {
  const blocks = [
    {id:'class:Loom_Probe',className:'Loom_Probe',title:'Probe'},
    {id:'class:Loom_WIFI',className:'Loom_WIFI',title:'Wi-Fi'}
  ];
  const source = '#include <Loom_WIFI.h>\n// Loom_WIFI wifi(manager);\nLoom_Probe probe(manager);\nvoid loop(){Serial.println("Loom_WIFI wifi(manager); probe.logToSD();");probe.measure();}\n';
  const recipe = indexRecipe(source, 'examples/Invented/Renamed.ino', blocks);
  assert.deepEqual(recipe.classNames, ['Loom_Probe']); assert.ok(!recipe.methods.includes('logToSD'));
  const catalog = {blocks, recipes:[recipe, {path:'examples/Whatever/Writes.ino',title:'Writes',classNames:[],methods:['logToSD','setSampleInterval']}]};
  const matches = relevantRecipes({description:'Save to SD every 5 minutes',blocks:[{catalogId:'class:Loom_Probe',label:'Probe',notes:'',settings:{}}]}, catalog);
  assert.ok(matches.some(m => m.path.endsWith('Writes.ino') && m.reasons.includes('Local logging')));
  assert.ok(matches.some(m => m.path.endsWith('Renamed.ino') && m.reasons.includes('Uses Loom_Probe')));
  const complex = Array.from({length:12}, (_,i) => ({path:'examples/Lab/Variant'+i+'.ino',classNames:['Loom_Probe',...Array.from({length:8},(_,j)=>'Extra'+j)],methods:['logToSD','setSampleInterval'],lines:900}));
  const balanced = relevantRecipes({description:'Log to SD every 5 minutes',blocks:[{catalogId:'class:Loom_Probe',label:'Probe',notes:'',settings:{}}]}, {...catalog,recipes:[...complex,...catalog.recipes]},3);
  assert.ok(balanced.some(r => r.path.endsWith('Renamed.ino')));
  assert.ok(balanced.some(r => r.path.endsWith('Writes.ino')), 'basic logging reference survives many lab variants');
  const sleeps=relevantRecipes({description:'Sleep between samples',blocks:[]},{blocks:[],recipes:[
    {path:'examples/UnusedCallback.ino',classNames:[],methods:['wakeup'],lines:10},
    {path:'examples/RealSleep.ino',classNames:[],methods:['registerInterrupt','sleep'],lines:50}
  ]},1);
  assert.equal(sleeps[0].path,'examples/RealSleep.ino','an unused wake helper is not a complete sleeping recipe');
});

test('recipe graphs ignore code printed in strings while retaining source arguments and links', () => {
  const blocks = extractHeader('class Manager { public: Manager(const char *name,int id); };\nclass Loom_Probe { public: Loom_Probe(Manager &man); };', 'Probe.h');
  const c = {adapter:{id:'loom',target:'Feather M0'},fingerprint:'test',blocks};
  const source = 'Manager manager("Device",1);\nLoom_Probe probe(manager);\nvoid setup(){Serial.println("void bogus(){probe.publish();} Loom_Probe fake(manager);");}\nvoid loop(){probe.measure(); manager.pause(5000);}\n';
  const r = reconstructExample(source, 'examples/Print/Print.ino', c);
  assert.equal(r.design.blocks.filter(b => b.catalogId === 'class:Loom_Probe').length, 1);
  assert.ok(!r.assembly.nodes.some(n => n.label.includes('Publish')));
  assert.ok(!summarizeExample(source, 'examples/Print/Print.ino', blocks).helperFunctions.includes('bogus'));
  assert.equal(r.design.blocks.find(b => b.catalogId === 'class:Manager').settings.parameters.name, '"Device"');
});

test('all checkout recipes retain complete source, valid hierarchy bounds and actual operation line links', async () => {
  const root = checkout, catalog = await discover(root);
  for (const file of catalog.examplePaths) {
    const source = await readFile(path.join(root,file),'utf8'), r = reconstructExample(source,file,catalog);
    assert.equal(r.assembly.files[0].content,source,file);
    const p = workflowPresentation(r.assembly), routes = graphRoutes(p.graph,p.positions), bounds = graphBounds(p.positions,routes,p.sections);
    assert.ok([bounds.x,bounds.y,bounds.w,bounds.h].every(Number.isFinite),file);
    for (const n of r.assembly.nodes.filter(n => n.reason.startsWith('Source call in '))) {
      const line = r.locations.nodes[n.id].line;
      const owner = n.reason.match(/: (\w+)\./)[1];
      assert.ok(source.split('\n')[line-1].includes(owner),file+' '+n.label+' line '+line);
    }
  }
});
