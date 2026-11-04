const plain=value=>value!==null&&typeof value==='object'&&!Array.isArray(value);
function same(a,b) {
  if(a===b)return true;
  if(Array.isArray(a)&&Array.isArray(b))return a.length===b.length&&a.every((value,index)=>same(value,b[index]));
  if(plain(a)&&plain(b)){const keys=Object.keys(a);return keys.length===Object.keys(b).length&&keys.every(key=>Object.hasOwn(b,key)&&same(a[key],b[key]));}
  return false;
}
function settingsOf(block) {
  return {...block.settings,constructorIndex:block.settings?.constructorIndex??0,parameters:block.settings?.parameters||{}};
}
function fields(before,after,path=[],depth=0) {
  if(same(before,after))return [];
  if(depth<8&&(plain(before)||before===undefined)&&(plain(after)||after===undefined)) {
    return [...new Set([...Object.keys(before||{}),...Object.keys(after||{})])].sort().flatMap(key=>fields(before?.[key],after?.[key],[...path,key],depth+1));
  }
  return [{path,before,after}];
}
export function designChanges(current,previous,{sourceChanged=false}={}) {
  const result={hasBaseline:!!previous,sourceChanged,added:[],removed:[],edited:[],general:[],total:sourceChanged?1:0};
  if(!previous)return result;
  const old=new Map(previous.blocks.map(block=>[block.id,block])),now=new Map(current.blocks.map(block=>[block.id,block]));
  result.added=current.blocks.filter(block=>!old.has(block.id));
  result.removed=previous.blocks.filter(block=>!now.has(block.id));
  for(const block of current.blocks) {
    const prior=old.get(block.id);if(!prior)continue;
    const changes=['label','catalogId','count','notes'].filter(key=>!same(prior[key],block[key])).map(key=>({path:[key],before:prior[key],after:block[key]}));
    changes.push(...fields(settingsOf(prior),settingsOf(block),['settings']));
    if(changes.length)result.edited.push({id:block.id,label:block.label,changes});
  }
  for(const key of ['description','target','adapter'])if(!same(previous[key],current[key]))result.general.push({path:[key],before:previous[key],after:current[key]});
  if(!result.added.length&&!result.removed.length&&!same(previous.blocks.map(b=>b.id),current.blocks.map(b=>b.id)))result.general.push({path:['order'],before:previous.blocks.map(b=>b.label),after:current.blocks.map(b=>b.label)});
  result.total+=result.added.length+result.removed.length+result.edited.length+result.general.length;
  return result;
}
