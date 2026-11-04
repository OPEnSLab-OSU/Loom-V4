import {checkout} from './checkout.mjs';
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,mkdir,writeFile,readFile,rm} from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import {fileURLToPath} from 'node:url';
import {discover} from '../adapters/loom.mjs';
import {prepareCheckout} from '../prepared-source.mjs';
import {validateAssembly} from '../core.mjs';
import {startStudio} from '../server.mjs';

async function fixture(t) {
  const temp=await mkdtemp(path.join(os.tmpdir(),'studio-example-files-')),root=path.join(temp,'repo'),workspace=path.join(temp,'local'),folder=path.join(root,'examples','Probe');
  await mkdir(path.join(root,'src'),{recursive:true});await mkdir(folder,{recursive:true});
  await writeFile(path.join(root,'src','Manager.h'),'class Manager {public:Manager(const char *name,int id);void measure();};');
  const sketch='#include "ProbeHelper.h"\nManager manager("Device",1);\nvoid setup(){}\nvoid loop(){manager.measure();}\n';
  await writeFile(path.join(folder,'Probe.ino'),sketch);
  const helper='#pragma once\nint probeValue();\n',implementation='#include "ProbeHelper.h"\nint probeValue(){return 42;}\n';
  await writeFile(path.join(folder,'ProbeHelper.h'),helper);await writeFile(path.join(folder,'ProbeHelper.cpp'),implementation);
  await writeFile(path.join(folder,'utility.c'),'int utility(void){return 1;}\n');
  await writeFile(path.join(folder,'config.json'),'{"interval":300}');await writeFile(path.join(folder,'README.md'),'Wiring notes.');
  await writeFile(path.join(folder,'settings.h'),'#define VALUE 3\n');
  await writeFile(path.join(folder,'Connection.h'),'const char* password = "DO_NOT_BUNDLE_THIS_VALUE";\n');
  await writeFile(path.join(folder,'arduino_secrets.h'),'DO_NOT_READ_OR_EXPORT');
  t.after(()=>rm(temp,{recursive:true,force:true}));return {root,workspace,folder,sketch,helper,implementation};
}

test('prepared examples bundle unchanged adjacent helper code and identify separate configuration files',async t=>{
  const f=await fixture(t),catalog=await discover(f.root),prepared=await prepareCheckout(f.root,catalog,f.workspace),recipe=prepared.recipes[0];
  assert.deepEqual(recipe.indexed.assembly.files.map(file=>file.name),['FeatureApplication.ino','ProbeHelper.cpp','ProbeHelper.h','utility.c']);
  assert.equal(recipe.indexed.assembly.files[0].content,f.sketch);
  assert.equal(recipe.indexed.assembly.files.find(file=>file.name==='ProbeHelper.h').content,f.helper);
  assert.equal(recipe.indexed.assembly.files.find(file=>file.name==='ProbeHelper.cpp').content,f.implementation);
  assert.equal(recipe.projectFiles.find(file=>file.name==='config.json').included,false);
  assert.equal(recipe.projectFiles.find(file=>file.name==='settings.h').included,false);
  assert.equal(recipe.projectFiles.find(file=>file.name==='Connection.h').included,false);
  assert.ok(!JSON.stringify(recipe).includes('DO_NOT_BUNDLE_THIS_VALUE'));
  assert.ok(!JSON.stringify(prepared).includes('DO_NOT_READ_OR_EXPORT'));
  validateAssembly(recipe.indexed.assembly,recipe.indexed.design,catalog);
  const warm=await prepareCheckout(f.root,catalog,f.workspace);assert.equal(warm.cacheHit,true);assert.deepEqual(warm.recipes[0].projectFiles,recipe.projectFiles);
});

test('helper and runtime configuration changes invalidate source, while documentation and excluded credentials do not',async t=>{
  const f=await fixture(t),original=await discover(f.root);
  await writeFile(path.join(f.folder,'README.md'),'Changed wiring notes.');await writeFile(path.join(f.folder,'arduino_secrets.h'),'CHANGED_PRIVATE_VALUE');
  const docs=await discover(f.root);assert.equal(docs.fingerprint,original.fingerprint);assert.notEqual(docs.supportFingerprint,original.supportFingerprint);
  await writeFile(path.join(f.folder,'ProbeHelper.cpp'),f.implementation.replace('42','43'));const helper=await discover(f.root);assert.notEqual(helper.fingerprint,docs.fingerprint);
  await writeFile(path.join(f.folder,'config.json'),'{"interval":600}');const config=await discover(f.root);assert.notEqual(config.fingerprint,helper.fingerprint);
});

test('opening and exporting an example preserves helper files; refreshing notices their later changes',async t=>{
  const f=await fixture(t),app=await startStudio({root:f.root,workspace:f.workspace,port:0});
  t.after(()=>new Promise(resolve=>{app.server.close(resolve);app.server.closeAllConnections();}));
  const get=async route=>(await fetch(app.url+route)).json();
  const post=async(route,body={})=>{const response=await fetch(app.url+route,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});const result=await response.json();assert.equal(response.status,200,JSON.stringify(result));return result;};
  const file='examples/Probe/Probe.ino',preview=await get('/api/example-preview?'+new URLSearchParams({path:file}));
  assert.equal(preview.example.projectFiles.filter(file=>file.included).length,4);
  await post('/api/example',{path:file});let state=await get('/api/state');
  assert.equal(state.assembly.files.length,4);assert.equal((await get('/api/project-export')).assembly.files.length,4);
  assert.equal(await (await fetch(app.url+'/api/download?file=ProbeHelper.cpp')).text(),f.implementation);
  await writeFile(path.join(f.folder,'EXTRA.md'),'New reference notes.');await post('/api/refresh');
  assert.ok((await get('/api/examples')).examples[0].projectFiles.some(file=>file.name==='EXTRA.md'));
  await writeFile(path.join(f.folder,'ProbeHelper.h'),f.helper+'// helper changed\n');await post('/api/refresh');state=await get('/api/state');
  assert.equal(state.assembly,null);assert.equal(state.lastBuild.assembly.files.length,4,'previous code remains available');
  await post('/api/example',{path:file});state=await get('/api/state');assert.match(state.assembly.files.find(file=>file.name==='ProbeHelper.h').content,/helper changed/);
});

test('every prepared checkout example remains valid with exactly the original main sketch and bundled helpers',async t=>{
  const root=checkout,workspace=await mkdtemp(path.join(os.tmpdir(),'studio-prepared-all-'));t.after(()=>rm(workspace,{recursive:true,force:true}));
  const catalog=await discover(root),prepared=await prepareCheckout(root,catalog,workspace);
  assert.equal(prepared.recipes.length,catalog.examplePaths.length);
  for(const recipe of prepared.recipes) {
    validateAssembly(recipe.indexed.assembly,recipe.indexed.design,catalog);
    assert.equal(recipe.indexed.assembly.files[0].content,await readFile(path.join(root,recipe.path),'utf8'),recipe.path);
    for(const item of recipe.projectFiles.filter(item=>item.included&&item.kind==='code'))assert.equal(recipe.indexed.assembly.files.find(file=>file.name===item.name).content,await readFile(path.join(root,item.source),'utf8'),item.source);
  }
});
