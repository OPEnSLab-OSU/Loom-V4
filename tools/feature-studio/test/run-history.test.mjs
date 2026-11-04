import {test} from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,rm,writeFile,readFile} from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import {randomUUID} from 'node:crypto';
import {runHistory} from '../run-history.mjs';
const job=(projectId,status='succeeded')=>({id:randomUUID(),projectId,status,model:'test-model',effort:'high',sandbox:'read-only',message:'Example received.',startedAt:'2026-11-04T20:00:00Z',lastActivityAt:'2026-11-04T20:00:04Z',finishedAt:status==='running'?null:'2026-11-04T20:00:05Z',events:[{at:'2026-11-04T20:00:04Z',message:'Checking files.',kind:'status'}]});

test('run history preserves separate projects and an old completion cannot replace the latest run',async t=>{
  const folder=await mkdtemp(path.join(os.tmpdir(),'studio-runs-'));t.after(()=>rm(folder,{recursive:true,force:true}));
  const history=runHistory(folder),first=job(randomUUID()),second=job(randomUUID(),'failed');
  await history.begin(first);await history.begin(second);
  const newer=job(first.projectId,'cancelled');
  await history.begin(newer);await history.save({...first,message:'Late old completion'});
  const restored=runHistory(folder);
  assert.deepEqual(await restored.load(first.projectId),newer);
  assert.deepEqual(await restored.load(second.projectId),second);
  assert.equal((await restored.load(second.projectId)).effort,'high');
});

test('unfinished run recovers as interrupted using only recorded elapsed time, without claiming success',async t=>{
  const folder=await mkdtemp(path.join(os.tmpdir(),'studio-interrupted-'));t.after(()=>rm(folder,{recursive:true,force:true}));
  const history=runHistory(folder),running=job(randomUUID(),'running');
  await history.begin(running);
  const restored=await runHistory(folder).load(running.projectId);
  assert.equal(restored.status,'interrupted');assert.equal(restored.finishedAt,running.lastActivityAt);
  assert.match(restored.message,/Review the project/);assert.ok(restored.recoveredAt);
  assert.equal(JSON.parse(await readFile(path.join(folder,'runs','project-'+running.projectId+'.json'),'utf8')).status,'interrupted');
});

test('missing or malformed run metadata never blocks a project; filenames require project UUIDs',async t=>{
  const folder=await mkdtemp(path.join(os.tmpdir(),'studio-bad-run-'));t.after(()=>rm(folder,{recursive:true,force:true}));
  const history=runHistory(folder),saved=job(randomUUID());await history.begin(saved);
  assert.equal(await history.load('../outside'),null);assert.equal(await history.load(randomUUID()),null);
  await writeFile(path.join(folder,'runs','project-'+saved.projectId+'.json'),'{unfinished');
  assert.equal(await history.load(saved.projectId),null);
  await writeFile(path.join(folder,'runs','project-'+saved.projectId+'.json'),JSON.stringify({...saved,projectId:randomUUID()}));
  assert.equal(await history.load(saved.projectId),null);
});
