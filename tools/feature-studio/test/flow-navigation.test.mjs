import {checkout} from './checkout.mjs';
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {stepConnections} from '../public/flow-navigation.js';
import {routineConnections} from '../public/execution-wiring.js';
import {discover} from '../adapters/loom.mjs';
import {reconstructExample} from '../examples.mjs';
const nodes=ids=>ids.map(id=>({id,label:id,reason:'Source operation',category:'system'}));

test('step navigation preserves both conditional branches and their distinct source references',()=>{
  const graph={nodes:nodes(['read','check','save','wait']),edges:[
    {from:'read',to:'check',kind:'flow',label:'Read result'},
    {from:'check',to:'save',kind:'flow',label:'value > threshold'},
    {from:'check',to:'wait',kind:'flow',label:'Otherwise'},
    {from:'save',to:'wait',kind:'flow',label:'After saving'},
    {from:'wait',to:'read',kind:'cycle',label:'Next loop'}],routines:[{id:'loop',name:'loop',kind:'loop',nodeIds:['read','check','save','wait'],bindings:[]}]};
  const original=JSON.stringify(graph),sources={edges:graph.edges.map((_,index)=>({scope:'application',file:'FeatureApplication.ino',line:20+index,match:'mapped'}))};
  const group=stepConnections(graph,'check',sources).find(group=>group.kind==='flow');
  assert.deepEqual(group.rows.map(row=>[row.direction,row.otherId]),[['incoming','read'],['outgoing','save'],['outgoing','wait']]);
  assert.deepEqual(group.rows.slice(1).map(row=>row.links[0].label),['value > threshold','Otherwise']);
  assert.deepEqual(group.rows.slice(1).map(row=>row.links[0].reference.line),[21,22]);
  assert.equal(group.rows[1].routine.step,3);assert.equal(JSON.stringify(graph),original);
  assert.ok(!stepConnections(graph,'save').flatMap(group=>group.rows).some(row=>row.otherId==='read'),'no shortcut or alternate branch is invented');
});

test('a shared step/data route appears once but keeps both explanations and code anchors',()=>{
  const graph={nodes:nodes(['read','packet']),edges:[{from:'read',to:'packet',kind:'data',label:'Measurements'},
    {from:'read',to:'packet',kind:'flow',label:'Then package'}],routines:[]};
  const groups=stepConnections(graph,'packet',{edges:[{file:'App.ino',line:10,match:'candidate'},{file:'App.ino',line:12,match:'mapped'}]});
  assert.equal(groups.length,1);assert.equal(groups[0].rows.length,1);
  const links=groups[0].rows[0].links;
  assert.deepEqual(links.map(link=>link.kind),['flow','data']);assert.deepEqual(links.map(link=>link.source.edgeIndex),[1,0]);
  assert.equal(links[1].reference.match,'candidate');assert.equal(links[1].reference.line,10);
  graph.edges.push({from:'read',to:'packet',kind:'flow',label:'Alternative condition'});
  const separate=stepConnections(graph,'packet');
  assert.equal(separate.find(group=>group.kind==='flow').rows.length,2,'different conditions are never merged');
  assert.equal(separate.find(group=>group.kind==='data').rows.length,1);
});

test('callback registration, repeat and support remain separate from direct step transitions',()=>{
  const graph={nodes:nodes(['register','wake','hypnos']),edges:[{from:'hypnos',to:'register',kind:'dependency',label:'Owned by Hypnos'},
    {from:'wake',to:'wake',kind:'cycle',label:'While awake'}],routines:[{id:'setup',name:'setup',kind:'setup',nodeIds:['register'],bindings:[]},
    {id:'wake',name:'isrTrigger',kind:'callback',nodeIds:['wake'],bindings:[{nodeId:'register',kind:'registration',label:'registerInterrupt()'}]}]};
  const source={scope:'application',file:'App.ino',line:14,match:'mapped'};
  const registration=stepConnections(graph,'register',{nodes:{register:{reference:source}}});
  assert.deepEqual(registration.map(group=>group.kind),['callback','dependency']);
  assert.equal(registration[0].rows[0].links[0].source.nodeId,'register');assert.equal(registration[0].rows[0].links[0].reference,source);
  assert.equal(stepConnections(graph,'wake').find(group=>group.kind==='cycle').rows[0].direction,'self');
  assert.deepEqual(stepConnections(graph,'missing'),[]);
});

test('all indexed example connections remain navigable with their exact original edge indexes',async()=>{
  const root=checkout,catalog=await discover(root);
  for(const file of catalog.examplePaths) {
    const {assembly}=reconstructExample(await readFile(path.join(root,file),'utf8'),file,catalog),before=JSON.stringify(assembly);
    for(const node of assembly.nodes) {
      const actual=stepConnections(assembly,node.id).flatMap(group=>group.rows.flatMap(row=>row.links)).filter(link=>link.source.edgeIndex!==undefined).map(link=>link.source.edgeIndex).sort((a,b)=>a-b);
      const expected=assembly.edges.flatMap((edge,index)=>edge.from===node.id||edge.to===node.id?[index]:[]);
      assert.deepEqual(actual,expected,file+' · '+node.id);
      for(const row of stepConnections(assembly,node.id).flatMap(group=>group.rows))assert.ok(assembly.nodes.some(node=>node.id===row.otherId));
    }
    assert.equal(JSON.stringify(assembly),before,file+' navigation is read-only');
    assert.equal(routineConnections(assembly).length,assembly.edges.length,file+' no new execution paths');
  }
});
