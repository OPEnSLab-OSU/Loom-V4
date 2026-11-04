import {test} from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import os from 'node:os';
import {mkdtemp,mkdir,writeFile,readFile,rm} from 'node:fs/promises';
import {startStudio} from '../server.mjs';

test('explicit code edits persist without rewriting graphs, reject stale saves, and are the exact files compiled',async t=>{
  const temp=await mkdtemp(path.join(os.tmpdir(),'studio-code-edit-')),root=path.join(temp,'repo'),workspace=path.join(temp,'local');
  await mkdir(path.join(root,'src'),{recursive:true});await mkdir(path.join(root,'examples','Probe'),{recursive:true});
  await writeFile(path.join(root,'src','Manager.h'),'class Manager{public:Manager(const char*,int);};');
  await writeFile(path.join(root,'examples','Probe','Probe.ino'),'Manager manager("Test",1);\nvoid setup(){manager.initialize();}\nvoid loop(){manager.measure();}\n');
  const calls=[],board='test:samd:feather';let compiled;
  const app=await startStudio({root,workspace,port:0,arduinoExecute:async(args)=>{
    calls.push(args);
    if(args[0]==='board')return JSON.stringify({boards:[{name:'Loomified Feather M0',fqbn:board}]});
    if(args[0]==='core')return JSON.stringify({platforms:[]});
    if(args[0]==='compile'){compiled=await readFile(path.join(args.at(-1),'FeatureApplication.ino'),'utf8');await writeFile(path.join(args[args.indexOf('--output-dir')+1],'fixture.bin'),'fake binary');return '';}
    throw new Error('Unexpected device command');
  }});
  t.after(async()=>{await new Promise(resolve=>{app.server.close(resolve);app.server.closeAllConnections();});await rm(temp,{recursive:true,force:true});});
  const get=async(route)=>{const response=await fetch(app.url+route);assert(response.ok);return response.json();};
  const post=async(route,body={},status=200)=>{const response=await fetch(app.url+route,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)}),result=await response.json();assert.equal(response.status,status,JSON.stringify(result));return result;};
  await post('/api/example',{path:'examples/Probe/Probe.ino'});const before=await get('/api/state'),id=before.projects.activeId,file=before.assembly.files[0],edited=file.content+'\n// User customization\n';
  await post('/api/code',{projectId:'wrong',file:file.name,previousContent:file.content,content:edited},400);
  await post('/api/code',{projectId:id,file:file.name,previousContent:file.content,content:edited});
  let state=await get('/api/state');assert.equal(state.assemblyOrigin,'custom');assert.equal(state.assembly.files[0].content,edited);
  assert.deepEqual(state.assembly.nodes,before.assembly.nodes);assert.deepEqual(state.assembly.edges,before.assembly.edges);assert.deepEqual(state.design,before.design);assert.equal(calls.length,0,'saving never compiles or runs Codex');
  await post('/api/code',{projectId:id,file:file.name,previousContent:file.content,content:'stale'},400);
  await post('/api/code',{projectId:id,file:'../escape.ino',previousContent:edited,content:'bad'},400);
  await post('/api/design/new');await post('/api/projects/open',{id});state=await get('/api/state');assert.equal(state.assemblyOrigin,'custom');assert.equal(state.assembly.files[0].content,edited);
  const cli=path.join(temp,'arduino-cli.exe');await writeFile(cli,'not executed');await post('/api/device/configure',{cliPath:cli});
  await get('/api/device/status?mode=build');await get('/api/device/status?mode=build');assert.equal(calls.filter(args=>args[0]==='core').length,1,'offline tool checks are cached');
  await post('/api/device/build',{projectId:id,board},202);
  for(let i=0;i<100;i++){const job=await get('/api/device/job?'+new URLSearchParams({projectId:id,board}));if(job.job.status!=='running'){assert.equal(job.job.status,'succeeded');break;}await new Promise(resolve=>setTimeout(resolve,5));}
  assert.equal(compiled,edited);assert.equal(calls.filter(args=>args[0]==='compile').length,1);assert.equal(calls.filter(args=>args[0]==='upload').length,0);
  // Editing code belonging to a previous assembly must not claim new feature demands are fulfilled.
  state=await get('/api/state');await post('/api/design',{projectId:id,design:{...state.design,description:'new requirement'}});
  await post('/api/code',{projectId:id,file:file.name,previousContent:edited,content:edited+'// another edit\n'});
  state=await get('/api/state');assert.equal(state.assembly,null);assert.equal(state.lastBuild.assemblyOrigin,'custom');assert(state.lastBuild.assembly.files[0].content.endsWith('// another edit\n'));
});
