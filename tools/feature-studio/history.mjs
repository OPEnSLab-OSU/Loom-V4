import {readFile,mkdir,unlink} from 'node:fs/promises';
import path from 'node:path';
import {randomUUID} from 'node:crypto';

function description(before,after) {
  if(before.name!==after.name)return 'Renamed project';
  if(before.description!==after.description)return 'Edited behavior brief';
  const added=after.blocks.filter(b=>!before.blocks.some(p=>p.id===b.id));
  if(added.length)return 'Added '+(added.length===1?added[0].label:added.length+' blocks');
  const removed=before.blocks.filter(b=>!after.blocks.some(p=>p.id===b.id));
  if(removed.length)return removed.length===before.blocks.length?'Cleared feature board':'Removed '+removed.map(b=>b.label).join(', ');
  const changed=after.blocks.find(b=>JSON.stringify(b)!==JSON.stringify(before.blocks.find(p=>p.id===b.id)));
  if(changed){const old=before.blocks.find(b=>b.id===changed.id);if(JSON.stringify({...old,position:undefined})===JSON.stringify({...changed,position:undefined}))return 'Moved blocks';return 'Updated '+changed.label;}
  return 'Updated design';
}
export async function designHistory(workspace,atomicJSON) {
  const folder=path.join(workspace,'project-history');await mkdir(folder,{recursive:true});
  const cache=new Map();
  const file=id=>{if(!/^[a-f0-9-]{36}$/.test(id))throw new Error('Invalid project.');return path.join(folder,id+'.json');};
  async function load(id,design) {
    if(cache.has(id))return cache.get(id);
    let history;try{history=JSON.parse(await readFile(file(id),'utf8'));}catch(e){if(e.code!=='ENOENT')throw e;}
    if(!history){history={cursor:0,entries:[{id:randomUUID(),at:new Date().toISOString(),label:'Project opened',design:structuredClone(design)}]};await atomicJSON(file(id),history);}
    cache.set(id,history);return history;
  }
  const view=history=>({canUndo:history.cursor>0,canRedo:history.cursor<history.entries.length-1,cursor:history.cursor,
    entries:history.entries.map(({design,...entry},i)=>({...entry,current:i===history.cursor,ahead:i>history.cursor}))});
  return {
    async view(id,design){return view(await load(id,design));},
    async record(id,before,after){
      const history=await load(id,before);
      if(JSON.stringify(before)===JSON.stringify(after))return view(history);
      history.entries=history.entries.slice(0,history.cursor+1);
      history.entries.push({id:randomUUID(),at:new Date().toISOString(),label:description(before,after).slice(0,120),design:structuredClone(after)});
      if(history.entries.length>200)history.entries.shift();history.cursor=history.entries.length-1;
      await atomicJSON(file(id),history);return view(history);
    },
    async target(id,design,action,entryId){
      const history=await load(id,design);
      const cursor=action==='undo'?history.cursor-1:action==='redo'?history.cursor+1:history.entries.findIndex(e=>e.id===entryId);
      if(cursor<0||cursor>=history.entries.length)throw new Error('No '+action+' version is available.');
      return {design:structuredClone(history.entries[cursor].design),commit:async()=>{history.cursor=cursor;await atomicJSON(file(id),history);return view(history);}};
    },
    async delete(id){cache.delete(id);await unlink(file(id)).catch(e=>{if(e.code!=='ENOENT')throw e;});}
  };
}
