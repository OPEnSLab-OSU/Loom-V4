import {test} from 'node:test';
import assert from 'node:assert/strict';
import {designChanges} from '../public/design-changes.js';
const block=(id,extra={})=>({id,catalogId:'class:Loom_Probe',label:id,count:1,notes:'',settings:{},position:{x:40,y:70},...extra});
const previous={name:'Sensor logger',adapter:'loom',target:'Feather M0',description:'Read every minute',blocks:[block('sensor',{settings:{parameters:{channel:'A0'}}}),block('power'),block('old-feature')]};
test('request comparison distinguishes addition, removal, edited settings and behavior without mutating the baseline',()=>{
  const before=structuredClone(previous),current=structuredClone(previous);
  current.name='Renamed logger';current.description='Read every five minutes';current.blocks=current.blocks.filter(b=>b.id!=='old-feature');current.blocks.push(block('storage'));
  current.blocks[0].count=3;current.blocks[0].notes='Three physical sensors';current.blocks[0].settings.parameters.channel='A1';current.blocks[1].position={x:900,y:500};
  const delta=designChanges(current,previous);
  assert.equal(delta.total,4);assert.deepEqual(delta.added.map(b=>b.id),['storage']);assert.deepEqual(delta.removed.map(b=>b.id),['old-feature']);
  assert.deepEqual(delta.edited.map(b=>b.id),['sensor']);assert.deepEqual(delta.general.map(c=>c.path),[['description']]);
  const channel=delta.edited[0].changes.find(c=>c.path.join('.')==='settings.parameters.channel');
  assert.equal(channel.before,'A0');assert.equal(channel.after,'A1');assert.deepEqual(previous,before);
});
test('moving blocks, renaming a project, object key order and empty constructor defaults create no request differences',()=>{
  const current=structuredClone(previous);current.name='New title';current.blocks[0].position={x:999,y:999};
  current.blocks[0].settings={parameters:{channel:'A0'},constructorIndex:0};current.blocks[1].settings={parameters:{},constructorIndex:0};
  assert.equal(designChanges(current,previous).total,0);
  const old={...previous,blocks:[block('sensor',{settings:{parameters:{enabled:true,channel:'A0'}}})]};
  const now={...old,blocks:[block('sensor',{settings:{parameters:{channel:'A0',enabled:true}}})]};
  assert.equal(designChanges(now,old).total,0);
});
test('array values, removed parameters, target and explicit feature order changes retain their meaning',()=>{
  const old={...previous,blocks:[block('a',{settings:{pins:[1,2],parameters:{gain:'8'}}}),block('b')]};
  const now=structuredClone(old);now.target='Another board';now.blocks.reverse();now.blocks[1].settings.pins=[2,1];delete now.blocks[1].settings.parameters.gain;
  const delta=designChanges(now,old);assert.equal(delta.edited.length,1);
  assert.ok(delta.general.some(c=>c.path[0]==='target'));assert.ok(delta.general.some(c=>c.path[0]==='order'));
  assert.ok(delta.edited[0].changes.some(c=>c.path.join('.')==='settings.parameters.gain'&&c.before==='8'&&c.after===undefined));
  assert.ok(delta.edited[0].changes.some(c=>c.path.join('.')==='settings.pins'&&c.after[0]===2));
});
test('source-only changes and missing snapshots are reported without inventing feature changes',()=>{
  const source=designChanges(previous,previous,{sourceChanged:true});assert.equal(source.total,1);assert.equal(source.sourceChanged,true);assert.equal(source.edited.length,0);
  const unknown=designChanges(previous,null);assert.equal(unknown.hasBaseline,false);assert.equal(unknown.total,0);assert.deepEqual(unknown.added,[]);
});
