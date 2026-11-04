import path from 'node:path';
import {mkdir,readFile,writeFile,rename,unlink} from 'node:fs/promises';
import {randomUUID} from 'node:crypto';
const uuid=value=>typeof value==='string'&&/^[a-f0-9]{8}(?:-[a-f0-9]{4}){3}-[a-f0-9]{12}$/.test(value);
const statuses=new Set(['running','succeeded','failed','cancelled','interrupted']);

export function runHistory(workspace) {
  const directory=path.join(workspace,'runs'),latest=new Map();let saving=Promise.resolve();
  async function atomic(file,data) {
    const temporary=file+'.'+randomUUID()+'.tmp';
    try{await writeFile(temporary,JSON.stringify(data,null,2));await rename(temporary,file);}
    catch(error){await unlink(temporary).catch(()=>{});throw error;}
  }
  const file=projectId=>path.join(directory,'project-'+projectId+'.json');
  const save=job=>{
    if(!uuid(job.projectId)||!uuid(job.id))return Promise.resolve();
    const snapshot=structuredClone(job);
    saving=saving.catch(()=>{}).then(async()=>{
      if(latest.get(job.projectId)!==job.id)return;
      await mkdir(path.join(directory,job.id),{recursive:true});
      await atomic(path.join(directory,job.id,'status.json'),snapshot);
      await atomic(file(job.projectId),snapshot);
    });
    return saving;
  };
  return {
    begin(job){if(uuid(job.projectId))latest.set(job.projectId,job.id);return save(job);},save,
    flush:()=>saving.catch(()=>{}),
    async load(projectId) {
      if(!uuid(projectId))return null;
      let job;try{job=JSON.parse(await readFile(file(projectId),'utf8'));}catch{return null;}
      if(!uuid(job?.id)||job.projectId!==projectId||!statuses.has(job.status)||typeof job.model!=='string'||typeof job.message!=='string'||!Number.isFinite(Date.parse(job.startedAt)))return null;
      job.events=(Array.isArray(job.events)?job.events:[]).filter(event=>typeof event?.message==='string'&&Number.isFinite(Date.parse(event.at))).slice(-50);
      if(!Number.isFinite(Date.parse(job.lastActivityAt)))job.lastActivityAt=job.events.at(-1)?.at||job.startedAt;
      latest.set(projectId,job.id);
      if(job.status==='running') {
        job.status='interrupted';job.message='Studio restarted before completion was recorded. Review the project before sending a new run.';
        job.finishedAt=job.lastActivityAt||job.events.at(-1)?.at||job.startedAt;
        job.recoveredAt=new Date().toISOString();
        await save(job).catch(()=>{});
      }
      return job;
    }
  };
}
