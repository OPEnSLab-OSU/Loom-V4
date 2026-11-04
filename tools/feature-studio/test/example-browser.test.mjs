import {checkout} from './checkout.mjs';
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,mkdir,writeFile,readFile,rm} from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {extractHeader,discover} from '../adapters/loom.mjs';
import {reconstructExample} from '../examples.mjs';
import {indexRecipe,recipeCapabilities} from '../recipe-index.mjs';
import {exampleOverview} from '../example-overview.mjs';
import {filterExamples} from '../public/example-browser.js';
import {startStudio} from '../server.mjs';

const header='class Manager {public:Manager(const char *name,int id);};\nclass Loom_WIFI {public:Loom_WIFI(Manager &man,const char *ssid);};\nclass Loom_Archive {public:Loom_Archive(Manager &man);};';
const source='Manager manager("Device",1);\nLoom_WIFI wifi(manager,"PRIVATE_PREVIEW_LITERAL");\nLoom_Archive archive(manager);\nvoid isrTrigger(){manager.measure();}\nvoid setup(){attachInterrupt(1,isrTrigger,RISING);}\nvoid loop(){if(ready()) archive.logToSD();}\n// wifi.publish();\n';
function overview(code=source,file='examples/Unfamiliar/Logger.ino') {
  const blocks=extractHeader(header,'Example.h'),recipe=indexRecipe(code,file,blocks),catalog={adapter:{id:'loom',target:'Feather M0'},fingerprint:'fixture',blocks,recipes:[recipe]};
  return exampleOverview({path:file,indexed:reconstructExample(code,file,catalog),companions:['examples/Unfamiliar/settings.h']},catalog);
}
test('example preview exposes source structure and callback registration without copying constructor values',()=>{
  const result=overview();
  assert.ok(result.methods.includes('logToSD'));assert.ok(!result.methods.includes('publish'));
  assert.ok(result.capabilities.includes('Local logging'));assert.ok(result.capabilities.includes('Wi-Fi'));
  const callback=result.routines.find(r=>r.name==='isrTrigger');assert.equal(callback.kind,'callback');assert.equal(callback.bindings[0].kind,'registration');
  assert.deepEqual(result.routines.map(r=>r.kind),['setup','loop','callback'],'callback definitions above setup are not presented as the startup phase');
  assert.equal(result.controlFlow,1);assert.ok(result.helperFunctions.includes('isrTrigger'));
  assert.ok(result.files.some(file=>file.endsWith('settings.h')));
  assert.ok(!JSON.stringify(result).includes('PRIVATE_PREVIEW_LITERAL'));
  assert.deepEqual(recipeCapabilities({path:'examples/SD_Sleep/WiFi.ino',classNames:['Manager'],methods:[]}),[],'a filename alone does not claim a capability');
});
test('example search combines terms across classes, API calls, behavior and callback structure',()=>{
  const logger=overview(),other={...overview('Manager manager("x",1);\nvoid setup(){}\nvoid loop(){manager.measure();}','examples/Basic/Probe.ino'),title:'Probe'};
  const examples=[other,logger],before=JSON.stringify(examples);
  assert.deepEqual(filterExamples(examples,'WiFi SD'),[logger]);
  assert.deepEqual(filterExamples(examples,'logToSD()'),[logger]);
  assert.deepEqual(filterExamples(examples,'isrTrigger() callback'),[logger]);
  assert.deepEqual(filterExamples(examples,'Loom_Archive'),[logger]);
  assert.deepEqual(filterExamples(examples,'unmatched ['),[]);
  assert.deepEqual(filterExamples(examples,'PROBE'),[other]);
  assert.equal(JSON.stringify(examples),before);
});
test('previews do not open or mutate projects, reject unknown files and follow changed example source',async t=>{
  const temp=await mkdtemp(path.join(os.tmpdir(),'studio-example-preview-')),root=path.join(temp,'repo');
  await mkdir(path.join(root,'src'),{recursive:true});await mkdir(path.join(root,'examples/Unfamiliar'),{recursive:true});
  await writeFile(path.join(root,'src/Example.h'),header);
  const file='examples/Unfamiliar/Logger.ino';await writeFile(path.join(root,file),source);
  const app=await startStudio({root,workspace:path.join(temp,'local'),port:0});
  t.after(async()=>{await new Promise(resolve=>{app.server.close(resolve);app.server.closeAllConnections();});await rm(temp,{recursive:true,force:true});});
  const get=async(route,status=200)=>{const response=await fetch(app.url+route),body=await response.json();assert.equal(response.status,status,JSON.stringify(body));return body;};
  const before=await get('/api/state'),preview=await get('/api/example-preview?'+new URLSearchParams({path:file}));
  assert.ok(preview.example.methods.includes('logToSD'));assert.equal(preview.fingerprint,before.sourceFingerprint);
  const after=await get('/api/state');assert.deepEqual(after.projects,before.projects);assert.deepEqual(after.design,before.design);assert.equal(after.assembly,null);
  await get('/api/example-preview?path=../outside.ino',400);
  await writeFile(path.join(root,file),source.replace('archive.logToSD()','wifi.publish()'));
  const refresh=await fetch(app.url+'/api/refresh',{method:'POST',headers:{'Content-Type':'application/json'},body:'{}'});assert.equal(refresh.status,200);
  const updated=await get('/api/example-preview?'+new URLSearchParams({path:file}));
  assert.notEqual(updated.fingerprint,preview.fingerprint);assert.ok(updated.example.methods.includes('publish'));assert.ok(!updated.example.methods.includes('logToSD'));
  assert.ok(!updated.example.capabilities.includes('Local logging'));
  const listed=(await get('/api/examples')).examples.find(example=>example.path===file);
  assert.deepEqual(listed,updated.example,'browse cards and previews use the same current index');
});
test('every current checkout example produces a browse overview from its reconstructed source',async()=>{
  const root=checkout,catalog=await discover(root);
  for(const file of catalog.examplePaths) {
    const indexed=reconstructExample(await readFile(path.join(root,file),'utf8'),file,catalog);
    const result=exampleOverview({path:file,indexed,companions:[]},catalog);
    assert.equal(result.path,file);assert.equal(result.routines.length,indexed.assembly.routines.length);
    assert.ok(result.classes.every(item=>catalog.blocks.some(block=>block.className===item.className)));
    assert.ok(result.files.includes(file));assert.equal(result.lines,indexed.summary.lines);
  }
});
