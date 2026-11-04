import {test} from 'node:test';
import assert from 'node:assert/strict';
import {filterProjectItems} from '../public/graph-layout.js';
const items=[
  {id:'init',label:'Initialize components',context:'setup() · step 3',description:'Initialize sensors.',searchText:'manager Manager initialize'},
  {id:'manager',label:'Manager',context:'Requested feature',description:'Manager object'},
  {id:'save',label:'Save packet to SD',context:'loop() · step 4',description:'Store the current reading.',searchText:'hypnos Loom_Hypnos hypnos.logToSD()'},
  {id:'sleep',label:'Sleep',context:'loop() · step 5',description:'Sleep until the RTC alarm.'}
].map(Object.freeze);
test('project search matches code and classes and ranks exact feature names first',()=>{
  assert.deepEqual(filterProjectItems(items,'MANAGER').map(item=>item.id),['manager','init']);
  assert.deepEqual(filterProjectItems(items,'Loom_Hypnos').map(item=>item.id),['save']);
  assert.deepEqual(filterProjectItems(items,'hypnos.logToSD()').map(item=>item.id),['save']);
});
test('project search combines terms across routine and behavior without changing the index',()=>{
  const before=JSON.stringify(items);
  assert.deepEqual(filterProjectItems(items,'loop SD').map(item=>item.id),['save']);
  assert.deepEqual(filterProjectItems(items,'setup SD'),[]);
  assert.deepEqual(filterProjectItems(items,'['),[]);
  assert.deepEqual(filterProjectItems(items,'  '),items);
  assert.equal(JSON.stringify(items),before);
});
