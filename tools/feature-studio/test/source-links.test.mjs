import {test} from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,mkdir,writeFile,readFile,rm} from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import {discover} from '../adapters/loom.mjs';
import {revisionOf,validateAssembly,completionInstructions,assemblySchema,makePrompt} from '../core.mjs';
import {sourceLinks,flowSources} from '../source-links.mjs';
import {sourceLocation} from '../source-navigation.mjs';
import {startStudio} from '../server.mjs';
import {cliPrompt} from '../codex.mjs';

async function fixture(t) {
  const temp=await mkdtemp(path.join(os.tmpdir(),'studio-source-links-')),root=path.join(temp,'repo'),workspace=path.join(temp,'local');
  t.after(()=>rm(temp,{recursive:true,force:true}));await mkdir(path.join(root,'src'),{recursive:true});await mkdir(workspace);
  await writeFile(path.join(root,'src','Archive.h'),'class Manager {public:Manager(const char *name,int id);};\nclass SDManager {public:bool append();};\nclass Loom_Archive {public:Loom_Archive(Manager &man);bool save();private:SDManager*writer;};');
  await writeFile(path.join(root,'src','Archive.cpp'),'#include "Archive.h"\n\nbool Loom_Archive::save() {\n  return writer->append();\n}\n\nbool SDManager::append() {\n  return true;\n}\n');
  const catalog=await discover(root),design={schemaVersion:1,adapter:'loom',target:'Feather M0',name:'Source links',description:'Store values',blocks:[{id:'wanted',catalogId:'class:Loom_Archive',label:'Archive',count:1,notes:'',settings:{}}]};
  const content='#include <Archive.h>\nManager manager("Device",1);\nLoom_Archive archive(manager);\nvoid setup() {}\nvoid loop() {\n  if (!archive.save()) {\n    Serial.println("failed");\n  }\n}\n';
  const assembly={schemaVersion:1,designRevision:revisionOf(design),sourceFingerprint:catalog.fingerprint,summary:'Save records',nodes:[
    {id:'save',label:'Save packet',category:'storage',added:false,reason:'archive.save() delegates the write to owned storage.',requestIds:['wanted']},
    {id:'error',label:'Report storage failure',category:'outputs',added:true,reason:'Serial.println("failed") when storage returns false.',requestIds:['wanted']}
  ],edges:[{from:'save',to:'error',kind:'flow',label:'save returned false'}],routines:[{id:'loop',name:'loop',kind:'loop',nodeIds:['save','error'],bindings:[],reason:'Save then report failure'}],files:[{name:'FeatureApplication.ino',content}],decisions:[],warnings:[],validation:'Fixture only'};
  const output=path.join(workspace,'outputs',revisionOf(design),'FeatureApplication');await mkdir(output,{recursive:true});await writeFile(path.join(output,'FeatureApplication.ino'),content);
  return {root,workspace,catalog,design,assembly,assemblyOrigin:'codex'};
}

test('flow source annotations expose actual object classes and linked edge files and lines',async t=>{
  const ctx=await fixture(t);
  ctx.assembly.sourceLinks=[{targetKind:'edge',targetId:'0',scope:'application',file:'FeatureApplication.ino',anchor:'if (!archive.save()) {',explanation:'Storage failure branch.'}];
  const result=await flowSources(ctx);
  assert.ok(result.nodes.save.owners.some(o=>o.object==='archive'&&o.className==='Loom_Archive'&&o.method==='save'&&o.line===3));
  assert.equal(result.edges[0].file,'FeatureApplication.ino');assert.equal(result.edges[0].line,6);assert.equal(result.edges[0].match,'mapped');
  assert.ok(result.nodes.save.applicationReferences.some(r=>r.file==='FeatureApplication.ino'&&r.line===6));
  assert.ok(result.edges[0].applicationReferences.some(r=>r.line===6&&r.match==='mapped'));
  const example=await sourceLinks({...ctx,assemblyOrigin:'example',origin:{path:'examples/Test/Test.ino'}},{edgeIndex:0});
  assert.ok(example.links.some(r=>r.file==='examples/Test/Test.ino'&&r.applicationFile==='FeatureApplication.ino'&&r.line===6),'example links retain an in-app sketch destination');
});

test('reverse navigation retains multiple explicit sketch anchors for one step',async t=>{
  const ctx=await fixture(t);
  ctx.assembly.sourceLinks=['if (!archive.save()) {','Serial.println("failed");'].map(anchor=>({targetKind:'node',targetId:'save',scope:'application',file:'FeatureApplication.ino',anchor,explanation:'Save and report its result.'}));
  const result=await flowSources(ctx);
  assert.deepEqual(result.nodes.save.applicationReferences.map(r=>[r.line,r.match]),[[6,'mapped'],[7,'mapped']]);
});

test('routine links identify a unique function definition without claiming that it is invoked',async t=>{
  const ctx=await fixture(t),result=await sourceLinks(ctx,{routineId:'loop'});
  const definition=result.links.find(r=>r.scope==='application');
  assert.equal(definition.match,'matched');assert.equal(definition.line,5);
  assert.equal(definition.symbol,'loop');
  assert.ok(definition.excerpt.startsWith('void loop()'));assert.match(definition.explanation,/Function definition/);
  assert.match(definition.explanation,/call sites separately/);
});

test('routine definitions behind alternative compile-time paths remain suggested matches',async t=>{
  const ctx=await fixture(t);
  ctx.assembly.files[0].content+='\n#if ANOTHER_MODE\nvoid loop() { archive.save(); }\n#endif\n';
  const result=await sourceLinks(ctx,{routineId:'loop'}),definitions=result.links.filter(r=>r.scope==='application');
  assert.equal(definitions.length,2);assert.ok(definitions.every(r=>r.match==='candidate'));
  assert.ok(definitions.every(r=>r.explanation.includes('compile-time conditions')));
});

test('existing generated explanations resolve real sketch calls and delegated implementation bodies',async t=>{
  const ctx=await fixture(t),result=await sourceLinks(ctx,{nodeId:'save'});
  assert.ok(result.links.some(r=>r.scope==='application'&&r.line===6&&r.excerpt.includes('archive.save()')));
  const impl=result.links.find(r=>r.scope==='repository'&&r.excerpt.includes('Loom_Archive::save'));
  assert.equal(impl.line,3);
  assert.ok(result.links.some(r=>r.scope==='repository'&&r.line===7&&r.explanation.includes('Called by Loom_Archive::save')),'owned helper implementation is linked');
  const location=await sourceLocation(ctx,{nodeId:'save'});assert.equal(location.line,6);assert.equal(location.exact,true);
  const underIndex=result.links.findIndex(r=>r===impl),under=await sourceLocation(ctx,{nodeId:'save',linkIndex:underIndex});
  assert.equal(under.line,3);assert.equal(under.path,path.join(ctx.root,'src','Archive.cpp'));
  const edge=await sourceLinks(ctx,{edgeIndex:0});assert.ok(edge.links.some(r=>r.excerpt.includes('if (!archive.save())')));
});

test('explicit anchors map custom conditions exactly and reject stale, ambiguous, comment-only or escaping references',async t=>{
  const ctx=await fixture(t);
  ctx.assembly.sourceLinks=[{targetKind:'edge',targetId:'0',scope:'application',file:'FeatureApplication.ino',anchor:'if (!archive.save()) {',explanation:'Failure branch is taken when the store call returns false.'}];
  validateAssembly(ctx.assembly,ctx.design,ctx.catalog);
  const exact=await sourceLinks(ctx,{edgeIndex:0});assert.equal(exact.links[0].match,'mapped');assert.equal(exact.links[0].line,6);
  assert.match(exact.links[0].explanation,/Failure branch/);
  ctx.assembly.sourceLinks[0].anchor='code from an old branch';
  const stale=await sourceLinks(ctx,{edgeIndex:0});assert.ok(stale.issues.some(s=>s.includes('unique code region')));assert.ok(!stale.links.some(r=>r.match==='mapped'));
  ctx.assembly.sourceLinks[0].anchor='}';assert.ok((await sourceLinks(ctx,{edgeIndex:0})).issues.length);
  ctx.assembly.files[0].content+='// comment-only-anchor\n';ctx.assembly.sourceLinks[0].anchor='// comment-only-anchor';
  assert.ok((await sourceLinks(ctx,{edgeIndex:0})).issues.some(s=>s.includes('no executable code')));
  ctx.assembly.sourceLinks[0].scope='repository';ctx.assembly.sourceLinks[0].file='src/../../outside.cpp';assert.throws(()=>validateAssembly(ctx.assembly,ctx.design,ctx.catalog),/source link/);
  assert.ok((await sourceLinks(ctx,{edgeIndex:0})).issues.length);
  ctx.assembly.sourceLinks[0].file='src/Archive.cpp';ctx.assembly.sourceLinks[0].targetId='999';assert.throws(()=>validateAssembly(ctx.assembly,ctx.design,ctx.catalog),/source link/);
  await assert.rejects(sourceLinks(ctx,{nodeId:'not-in-graph'}),/Unknown graph/);
});

test('unmatched logic is labeled context and cold-run prompts require explicit application and implementation evidence',async t=>{
  const ctx=await fixture(t);ctx.assembly.nodes[0].label='Something unusual';ctx.assembly.nodes[0].reason='This behavior is not identifiable from source symbols.';
  const result=await sourceLinks(ctx,{nodeId:'save'});assert.equal(result.links[0].match,'context');
  assert.equal((await sourceLocation(ctx,{nodeId:'save'})).exact,false);
  assert.ok(assemblySchema.required.includes('sourceLinks'));
  for(const prompt of [makePrompt(ctx.design,ctx.catalog,ctx.workspace),cliPrompt({},[],'read-only')]) {
    assert.match(prompt,/sourceLinks/);assert.match(prompt,/exact unique excerpt/);assert.match(prompt,/delegated or implicit/);
  }
  assert.ok(completionInstructions.some(s=>s.includes('zero-based edge index')));
});

test('short macros and printed words do not masquerade as source links for a constant feature',async t=>{
  const ctx=await fixture(t);
  ctx.assembly.files[0].content+='constexpr uint32_t SAMPLE_INTERVAL_MS = 300000;\n';
  ctx.assembly.nodes.push({id:'interval',label:'Sampling interval',reason:'Implements a five-minute cadence.',category:'power',added:true,requestIds:['wanted']});
  const result=await sourceLinks(ctx,{nodeId:'interval'});
  assert.ok(result.links.some(r=>r.excerpt.includes('SAMPLE_INTERVAL_MS')));
  assert.ok(!result.links.some(r=>r.excerpt.includes('Serial.println')));
});

test('HTTP source details and chosen implementation links share the same verified locations',async t=>{
  const ctx=await fixture(t),opened=[];
  await writeFile(path.join(ctx.workspace,'design.json'),JSON.stringify(ctx.design));await writeFile(path.join(ctx.workspace,'assembly.json'),JSON.stringify(ctx.assembly));
  const app=await startStudio({...ctx,port:0,openEditor:async location=>opened.push(location)});
  t.after(()=>new Promise(resolve=>{app.server.close(resolve);app.server.closeAllConnections();}));
  const r=await fetch(app.url+'/api/source-links?nodeId=save'),details=await r.json();assert.equal(r.status,200);
  const linkIndex=details.links.findIndex(r=>r.scope==='repository'&&r.line===7);assert.ok(linkIndex>=0);
  const openedResponse=await fetch(app.url+'/api/open-source',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({nodeId:'save',linkIndex})});
  assert.equal(openedResponse.status,200);assert.equal(opened[0].line,7);
  assert.equal((await fetch(app.url+'/api/source-links?edgeIndex=-1')).status,400);
});
