import {test} from 'node:test';
import assert from 'node:assert/strict';
import {assemblySchema,completionInstructions,revisionOf,validateAssembly} from '../core.mjs';
import {compatibilityForNode,compatibilityConcerns} from '../public/compatibility.js';

function fixture() {
  const design={schemaVersion:1,name:'Review',adapter:'loom',target:'Feather M0',description:'Record light',blocks:[{id:'sensor',catalogId:'custom',label:'Sensor',count:1,settings:{},notes:''}]};
  const catalog={fingerprint:'source'};
  const assembly={schemaVersion:1,designRevision:revisionOf(design),sourceFingerprint:'source',summary:'Feasible subset',validation:'Source review only',
    nodes:[{id:'requested',label:'Sensor',category:'sensors',added:false,reason:'Requested',requestIds:['sensor']},{id:'manager',label:'Manager',category:'system',added:true,reason:'Required',requestIds:['sensor']}],
    edges:[],files:[{name:'FeatureApplication.ino',content:'void setup() {}\nvoid loop() {}'}],decisions:[],warnings:[],featureAssessments:[
      {requestId:'sensor',status:'partial',summary:'The selected sensor cannot measure this quantity.',nodeIds:['requested'],issues:[{requirement:'Measure that quantity',reason:'The selected object exposes a different measurement.',evidence:'Checked the current driver implementation.'}],suggestion:'Select a sensor that measures the requested quantity.'},
      {requestId:'$brief',status:'needs-info',summary:'Storage hardware needs confirmation.',nodeIds:[],issues:[{requirement:'Record light',reason:'No storage hardware specified.',evidence:'The brief and selected blocks do not identify storage.'}],suggestion:'Specify the storage hardware.'}
    ]};
  return {design,catalog,assembly};
}
test('feasibility reviews remain linked through the assembly contract; legacy recipes still load',()=>{
  const {design,catalog,assembly}=fixture();
  assert.equal(validateAssembly(assembly,design,catalog),assembly);
  assert.ok(assemblySchema.required.includes('featureAssessments'));
  delete assembly.featureAssessments;
  assert.equal(validateAssembly(assembly,design,catalog),assembly);
});
test('reviews reject invented targets, missing explanations, omissions and duplicate requests',()=>{
  const {design,catalog,assembly}=fixture();
  for(const mutate of [a=>a.featureAssessments[0].nodeIds.push('absent'),a=>a.featureAssessments[0].requestId='unknown',
    a=>a.featureAssessments[0].issues[0].evidence='',a=>a.featureAssessments[0].issues=[],a=>a.featureAssessments.pop(),
    a=>a.featureAssessments.push(a.featureAssessments[0]),a=>a.featureAssessments[0].status='probably']) {
    const copy=structuredClone(assembly);mutate(copy);assert.throws(()=>validateAssembly(copy,design,catalog),/assessment/i);
  }
});
test('a conflict colors its request and explicitly affected behavior, never shared supporting nodes',()=>{
  const {assembly}=fixture();
  assert.equal(compatibilityForNode(assembly,'sensor').status,'partial');
  assert.equal(compatibilityForNode(assembly,'requested').status,'partial');
  assert.equal(compatibilityForNode(assembly,'manager'),null);
  assert.equal(compatibilityForNode(null,'sensor'),null);
  assert.equal(compatibilityConcerns(assembly).length,2);
  assembly.featureAssessments.push({requestId:'other',nodeIds:['requested'],status:'unsupported'});
  assert.equal(compatibilityForNode(assembly,'requested').status,'unsupported');
});
test('cold-run instructions distinguish Arduino extensions, unknowns and verified conflicts',()=>{
  const text=completionInstructions.join('\n');
  assert.match(text,/custom Arduino code and compatible external libraries are allowed/);
  assert.match(text,/needs-info, not proven incompatibility/);
  assert.match(text,/non-executable graph nodes outside routines/);
});
