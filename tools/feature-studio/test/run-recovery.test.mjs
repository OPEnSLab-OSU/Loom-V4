import {test} from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,mkdir,writeFile,rm} from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import {fileURLToPath} from 'node:url';
import {randomUUID} from 'node:crypto';
import {startStudio} from '../server.mjs';

async function studio(t) {
  const folder=await mkdtemp(path.join(os.tmpdir(),'studio-activity-')),root=path.join(folder,'repo'),workspace=path.join(folder,'local');
  await mkdir(path.join(root,'src'),{recursive:true});
  await writeFile(path.join(root,'src','Future.h'),'class Loom_Future { public: Loom_Future(Manager &man); };');
  const options={root,workspace,port:0,codexPath:fileURLToPath(new URL('fixtures/fake-codex.mjs',import.meta.url))};
  let app=await startStudio(options);
  const stop=async()=>{await app.runner.stop();await new Promise(resolve=>{app.server.close(resolve);app.server.closeAllConnections();});};
  t.after(async()=>{await stop();await rm(folder,{recursive:true,force:true});});
  const get=async route=>(await fetch(app.url+route)).json();
  const post=async(route,body={},status=200)=>{
    const response=await fetch(app.url+route,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
    const result=await response.json();assert.equal(response.status,status,JSON.stringify(result));return result;
  };
  const wait=async status=>{
    for(let i=0;i<150;i++) {
      const state=await get('/api/state');if(state.job?.status===status)return state;
      if(state.job&&state.job.status!=='running')assert.fail('Unexpected '+state.job.status+': '+state.job.message);
      await new Promise(resolve=>setTimeout(resolve,30));
    }
    assert.fail('Fixture did not reach '+status);
  };
  const initial=await get('/api/state');
  await post('/api/design',{projectId:initial.projects.activeId,design:{...initial.design,name:'Station',blocks:[{id:'sensor',catalogId:'class:Loom_Future',label:'Sensor',count:1,settings:{},notes:'',position:{x:100,y:100}}]}});
  return {get,post,wait,workspace,restart:async()=>{await stop();app=await startStudio(options);},settle:()=>app.runner.stop()};
}

test('public activity survives project switches and restarts; reopening never starts another run',async t=>{
  const s=await studio(t);let state=await s.get('/api/state');const project=state.projects.activeId;
  await s.post('/api/assemble',{model:'test-activity',effort:'medium',sandbox:'read-only'},202);
  state=await s.wait('succeeded');await s.settle();
  const completed=state.job;
  assert.equal(completed.commandCount,3);assert.equal(completed.events.find(e=>e.kind==='tool').count,3);
  assert.ok(completed.events.some(e=>e.message==='Checking the selected example and the sensor setup.'));
  assert.doesNotMatch(JSON.stringify(completed),/INTERNAL_REASONING|RAW_COMMAND_OUTPUT/);
  await s.post('/api/design/new');assert.equal((await s.get('/api/state')).job,null);
  await s.post('/api/projects/open',{id:project});state=await s.get('/api/state');
  assert.equal(state.job.id,completed.id);assert.equal(state.job.status,'succeeded');
  await s.restart();state=await s.get('/api/state');
  assert.equal(state.job.id,completed.id);assert.equal(state.job.effort,'medium');assert.equal(state.job.model,'test-activity');
  assert.equal(state.assembly.summary,'Fixture assembly');assert.ok(state.job.finishedAt);
  await s.post('/api/cancel');assert.equal((await s.get('/api/state')).job.status,'succeeded','finished runs cannot be cancelled retroactively');
  await s.post('/api/assemble',{model:'test-stale',sandbox:'read-only'},202);state=await s.wait('failed');const failedId=state.job.id;
  assert.match(state.job.error,/different source/);await s.restart();
  state=await s.get('/api/state');assert.equal(state.job.id,failedId);assert.equal(state.job.status,'failed');assert.ok(state.assembly);
  await s.post('/api/assemble',{model:'test-wait',sandbox:'read-only'},202);
  await s.post('/api/cancel');state=await s.wait('cancelled');const cancelledId=state.job.id;await s.restart();
  state=await s.get('/api/state');assert.equal(state.job.id,cancelledId);assert.equal(state.job.status,'cancelled');assert.ok(state.assembly);
});

test('a title-only edit during assembly preserves the result; changed feature requirements reject it',async t=>{
  const s=await studio(t);let state=await s.get('/api/state');
  await s.post('/api/assemble',{model:'test-delay',sandbox:'read-only'},202);
  await s.post('/api/design',{projectId:state.projects.activeId,design:{...state.design,name:'Renamed while running'}});
  state=await s.wait('succeeded');await s.settle();
  assert.equal(state.design.name,'Renamed while running');assert.equal(state.assembly.designRevision,state.designRevision);
  assert.equal(state.job.acceptedDesignRevision,state.designRevision);
  await s.post('/api/design',{projectId:state.projects.activeId,design:{...state.design,name:'Renamed after completion'}});
  state=await s.get('/api/state');assert.equal(state.job.acceptedDesignRevision,state.designRevision,'a harmless later rename keeps Activity associated with the current result');
  const previous=structuredClone(state.assembly);
  await s.post('/api/assemble',{model:'test-delay',sandbox:'read-only'},202);
  await s.post('/api/design',{projectId:state.projects.activeId,design:{...state.design,description:'Sample once every ten minutes'}});
  state=await s.wait('failed');assert.match(state.job.error,/different design/);
  assert.equal(state.assembly,null);assert.deepEqual(state.lastBuild.assembly,previous);
});

test('server restart restores an unfinished run as interrupted and never resumes its CLI process',async t=>{
  const s=await studio(t),state=await s.get('/api/state');
  const saved={id:randomUUID(),projectId:state.projects.activeId,status:'running',model:'test-wait',effort:'high',sandbox:'read-only',message:'Reviewing source.',startedAt:'2026-11-04T20:00:00Z',lastActivityAt:'2026-11-04T20:00:12Z',phase:'working',events:[]};
  await mkdir(path.join(s.workspace,'runs'),{recursive:true});
  await writeFile(path.join(s.workspace,'runs','project-'+saved.projectId+'.json'),JSON.stringify(saved));
  await s.restart();const restored=await s.get('/api/state');
  assert.equal(restored.job.id,saved.id);assert.equal(restored.job.status,'interrupted');assert.equal(restored.job.finishedAt,saved.lastActivityAt);
  assert.equal(restored.assembly,null);assert.equal(restored.job.sessionId,undefined);
  await s.post('/api/design/new');assert.equal((await s.get('/api/state')).job,null,'an interrupted run does not block editing or start another run');
});
