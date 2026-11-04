import {test} from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,mkdir,readFile,writeFile,rm} from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import {startStudio} from '../server.mjs';
import {revisionOf} from '../core.mjs';

test('compiler links resolve another open project and helper file without navigation, then reject changed or closed project code',async t=>{
  const temp=await mkdtemp(path.join(os.tmpdir(),'studio-diagnostic-project-')),root=path.join(temp,'repo'),workspace=path.join(temp,'local');
  await mkdir(path.join(root,'src'),{recursive:true});await mkdir(path.join(root,'examples','Probe'),{recursive:true});
  await writeFile(path.join(root,'src','Manager.h'),'class Manager {public:Manager(const char*,int);};');
  await writeFile(path.join(root,'examples','Probe','Probe.ino'),'#include "Helper.h"\nManager manager("Device",1);\nvoid setup(){manager.initialize();}\nvoid loop(){\n manager.measure();\n}\n');
  const helper='#pragma once\nint readValue();\n';await writeFile(path.join(root,'examples','Probe','Helper.h'),helper);
  const calls=[],board='test:samd:feather';
  const app=await startStudio({root,workspace,port:0,arduinoExecute:async(args,log=()=>{})=>{
    calls.push(args);
    if(args[0]==='board'&&args[1]==='listall')return JSON.stringify({boards:[{name:'Loomified Feather M0',fqbn:board}]});
    if(args[0]==='core')return JSON.stringify({platforms:[]});
    if(args[0]==='compile'){
      log(path.join(args.at(-1),'Helper.h')+':2:4: error: fixture helper failure\n'+path.join(args.at(-1),'FeatureApplication.ino')+':5:3: warning: fixture sketch warning\n'+path.join(root,'src','Manager.h')+':1:1: note: library context\n');
      throw new Error('Simulated compiler failure.');
    }
    throw new Error('Unexpected command '+args[0]);
  }});
  t.after(async()=>{await new Promise(resolve=>{app.server.close(resolve);app.server.closeAllConnections();});await rm(temp,{recursive:true,force:true});});
  const get=async(route,status=200)=>{const response=await fetch(app.url+route),body=await response.json();assert.equal(response.status,status,JSON.stringify(body));return body;};
  const post=async(route,body={},status=200)=>{const response=await fetch(app.url+route,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)}),result=await response.json();assert.equal(response.status,status,JSON.stringify(result));return result;};
  const cli=path.join(temp,'arduino-cli.exe');await writeFile(cli,'not executed');await post('/api/device/configure',{cliPath:cli});
  await post('/api/example',{path:'examples/Probe/Probe.ino'});const a=(await get('/api/state')).projects.activeId;
  await post('/api/design/new');let state=await get('/api/state');const b=state.projects.activeId;
  const selected=(await get('/api/device/projects')).projects.find(project=>project.id===a);
  await post('/api/device/build',{projectId:a,board,filesFingerprint:selected.filesFingerprint},202);
  let job;for(let i=0;i<100;i++){job=await get('/api/device/job?'+new URLSearchParams({projectId:a,board}));if(job.job.status!=='running')break;await new Promise(resolve=>setTimeout(resolve,5));}
  assert.equal(job.job.status,'failed');assert.equal(job.diagnosticsMatch,true);assert.equal(job.diagnosticProjectId,a);assert.equal(job.projectId,b);
  const url=index=>'/api/device/diagnostic?'+new URLSearchParams({jobId:job.job.id,index,projectId:a});
  assert.deepEqual(await get(url(0)),{file:'Helper.h',line:2,endLine:2,projectId:a});
  assert.equal((await get('/api/state')).projects.activeId,b,'diagnostic lookup is read only');
  assert.deepEqual(await get(url(1)),{file:'FeatureApplication.ino',line:5,endLine:5,projectId:a});
  await get(url(2),400);await get(url(0).replace(job.job.id,'obsolete-build'),400);
  await post('/api/projects/open',{id:a});state=await get('/api/state');assert.equal(state.assembly.files.find(file=>file.name==='Helper.h').content,helper);assert.equal((await get(url(0))).line,2);
  await post('/api/design',{projectId:a,design:{...state.design,description:'An edited feature draft with the same assembled code'}});
  assert.equal((await get(url(0))).line,2,'editing requirements does not change saved code locations');
  state=await get('/api/state');const changed=structuredClone(state.lastBuild.assembly);changed.designRevision=revisionOf(state.design);changed.files.find(file=>file.name==='Helper.h').content+='// code changed\n';
  await writeFile(path.join(workspace,'assembly.json'),JSON.stringify(changed));await get('/api/state');await get(url(0),400);
  assert.equal((await get('/api/device/job?'+new URLSearchParams({projectId:a,board}))).diagnosticsMatch,false);
  await post('/api/projects/open',{id:b});await get(url(0),400);await post('/api/projects/close',{id:a});await get(url(0),400);
  assert.equal(calls.filter(args=>args[0]==='compile').length,1);assert.ok(!calls.some(args=>args[0]==='upload'),'reviewing messages never builds or uploads');
});
