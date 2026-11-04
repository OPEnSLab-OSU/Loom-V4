import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import vm from 'node:vm';
const script=await readFile(new URL('../public/theme.js',import.meta.url),'utf8');
function browser(saved,{blocked=false}={}) {
  const events={},clicks={},attributes={},data={},storage=new Map(saved?[['feature-studio-theme',saved]]:[]);
  const button={dataset:data,setAttribute:(key,value)=>attributes[key]=value,addEventListener:(name,handler)=>clicks[name]=handler};
  const root={dataset:{}};
  const context={document:{documentElement:root,getElementById:()=>button,querySelector:()=>({setAttribute(){}}),addEventListener:(name,handler)=>events[name]=handler},
    window:{addEventListener:(name,handler)=>events[name]=handler},localStorage:{getItem:key=>{if(blocked)throw Error('Blocked');return storage.get(key);},setItem:(key,value)=>{if(blocked)throw Error('Blocked');storage.set(key,value);}}};
  vm.runInNewContext(script,context);events.DOMContentLoaded();
  return {root,events,clicks,attributes,data,storage};
}
test('theme is applied before app initialization, remembers a deliberate light choice and syncs tabs',()=>{
  const first=browser();assert.equal(first.root.dataset.theme,'dark');assert.equal(first.attributes['aria-pressed'],'true');
  first.clicks.click();assert.equal(first.root.dataset.theme,'light');assert.equal(first.storage.get('feature-studio-theme'),'light');
  assert.equal(browser('light').root.dataset.theme,'light');
  first.events.storage({key:'feature-studio-theme',newValue:'dark'});assert.equal(first.root.dataset.theme,'dark');
  first.events.storage({key:'feature-studio-theme',newValue:'invalid'});assert.equal(first.root.dataset.theme,'dark');
});
test('disabled browser storage cannot prevent the app from loading or switching themes',()=>{
  const page=browser(null,{blocked:true});assert.equal(page.root.dataset.theme,'dark');
  page.clicks.click();assert.equal(page.root.dataset.theme,'light');
});
