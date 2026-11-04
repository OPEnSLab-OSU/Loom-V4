import {test} from 'node:test';
import assert from 'node:assert/strict';
import {elapsedText,runActivity,runReport} from '../public/run-activity.js';
import {cliPrompt} from '../codex.mjs';
const now=Date.parse('2026-11-04T20:01:12Z');
const job={id:'example-run',status:'running',model:'test-model',effort:'high',sandbox:'read-only',phase:'working',designRevision:'d1',sourceFingerprint:'s1',startedAt:'2026-11-04T20:00:00Z',lastActivityAt:'2026-11-04T20:01:05Z',message:'Reading the example.',events:[{at:'2026-11-04T20:01:05Z',message:'Reading the example.',count:3}]};
const state={designRevision:'d1',sourceFingerprint:'s1'};
test('assembly activity reports real phases and elapsed time without a guessed completion percentage',()=>{
  const a=runActivity(job,state,now);
  assert.equal(a.elapsed,'1m 12s');assert.equal(a.lastUpdate,'7s ago');
  assert.deepEqual(a.steps.map(s=>s.status),['done','active','waiting']);
  assert.deepEqual(runActivity({...job,phase:'validating'},state,now).steps.map(s=>s.status),['done','done','active']);
  assert.deepEqual(runActivity({...job,status:'succeeded',finishedAt:'2026-11-04T20:00:45Z'},state,now).steps.map(s=>s.status),['done','done','done']);
  assert.equal(runActivity({...job,status:'interrupted',finishedAt:job.lastActivityAt},state,now).timeLabel,'Recorded duration');
  assert.equal(runActivity({...job,status:'cancelled'},state,now).elapsed,'0s');
  assert.equal(elapsedText(-2000),'0s');assert.equal(elapsedText(NaN),'0s');assert.equal(elapsedText(3_661_000),'1h 1m');
});
test('run context distinguishes renamed accepted results, changed requests and a changed checkout',()=>{
  assert.match(runActivity(job,{...state,designRevision:'d2'},now).context,/earlier project/);
  assert.match(runActivity({...job,acceptedDesignRevision:'d2'},{...state,designRevision:'d2'},now).context,/current project/);
  assert.match(runActivity(job,{...state,sourceFingerprint:'s2'},now).context,/earlier Loom checkout/);
  assert.match(runActivity({...job,sourceFingerprint:null},state,now).context,/not recorded/);
});
test('copyable run report includes public activity but omits request and hidden session data',()=>{
  const report=runReport({...job,sessionId:'hidden-session',request:{secret:'private-code'}},state,now);
  assert.match(report,/Model: test-model/);assert.match(report,/3 updates/);assert.match(report,/Source review/);
  assert.doesNotMatch(report,/hidden-session|private-code/);
  const interrupted=runReport({...job,status:'interrupted',finishedAt:job.lastActivityAt},state,now);
  assert.match(interrupted,/Last recorded activity:/);assert.doesNotMatch(interrupted,/Finished:/);
});
test('both assembly permission modes keep firmware building, installation and upload as explicit user actions',()=>{
  for(const sandbox of ['read-only','workspace-write'])assert.match(cliPrompt({designRevision:'test'},[],sandbox),/Do not compile firmware, upload to a device, install packages/);
});
