const labels={running:'Running',succeeded:'Completed',failed:'Failed',cancelled:'Cancelled',interrupted:'Interrupted'};
export function elapsedText(milliseconds) {
  const seconds=Math.max(0,Math.floor(Number.isFinite(milliseconds)?milliseconds/1000:0));
  if(seconds<60)return seconds+'s';
  if(seconds<3600)return Math.floor(seconds/60)+'m '+seconds%60+'s';
  return Math.floor(seconds/3600)+'h '+Math.floor(seconds%3600/60)+'m';
}
export function runActivity(job,state,now=Date.now()) {
  if(!job)return null;
  const start=Date.parse(job.startedAt),finish=Date.parse(job.finishedAt),running=job.status==='running';
  const elapsed=elapsedText((running?now:Number.isFinite(finish)?finish:start)-start);
  const phase=job.phase==='complete'?3:job.phase==='validating'?2:job.phase==='working'||job.sessionId?1:0;
  const names=['Connect to Codex','Review and assemble','Check returned files'];
  const steps=names.map((name,index)=>({name,status:job.status==='succeeded'||index<phase?'done':index===phase?running?'active':'stopped':'waiting'}));
  const revision=job.acceptedDesignRevision||job.designRevision;
  const context=!job.sourceFingerprint||!revision?'Revision details were not recorded for this run.':job.sourceFingerprint!==state?.sourceFingerprint?'This run used an earlier Loom checkout.':
    revision&&revision!==state?.designRevision?'This run used an earlier project revision.':'Activity for the current project revision.';
  return {title:labels[job.status]||'Saved run',elapsed,running,steps,context,timeLabel:job.status==='interrupted'?'Recorded duration':'Elapsed',
    lastUpdate:elapsedText(now-Date.parse(job.lastActivityAt||job.events?.at(-1)?.at||job.startedAt))+' ago',
    permissions:job.sandbox==='workspace-write'?'Project writes':job.sandbox==='read-only'?'Source review':'Access not recorded'};
}
export function runReport(job,state,now=Date.now()) {
  if(!job)return 'No assembly run recorded.';
  const activity=runActivity(job,state,now);
  return ['Feature Studio assembly run',
    'Run: '+job.id,'Status: '+activity.title,'Model: '+job.model,'Reasoning: '+(job.effort||'Not recorded'),
    'Access: '+activity.permissions,activity.timeLabel+': '+activity.elapsed,
    'Started: '+job.startedAt,...(job.finishedAt?[(job.status==='interrupted'?'Last recorded activity: ':'Finished: ')+job.finishedAt]:[]),activity.context,
    'Result: '+job.message,...(job.error&&job.error!==job.message?['Error: '+job.error]:[]),
    '', 'Recent activity:',...(job.events||[]).map(event=>event.at+' · '+event.message+((event.count||1)>1?' ('+event.count+' updates)':''))
  ].join('\n');
}

export function installRunActivity({el,toast,getState,reviewCode,reviewRequests,reviewAndSend,cancelRun}) {
  const $=id=>document.getElementById(id);
  let renderedKey=null;
  const text=(id,value)=>{if($(id).textContent!==value)$(id).textContent=value;};
  const date=value=>Number.isFinite(Date.parse(value))?new Date(value).toLocaleString():'Not recorded';
  function refresh() {
    const state=getState(),job=state?.job,activity=runActivity(job,state);
    text('run-elapsed',activity?activity.title+' · '+activity.elapsed:'');
    if(!$('run-activity-dialog').open)return;
    if(!job){$('run-activity-dialog').close();return;}
    text('activity-status',activity.title);$('activity-status').dataset.status=job.status;
    text('activity-clock',activity.timeLabel+': '+activity.elapsed);
    text('activity-last-update',activity.running?'Last update '+activity.lastUpdate:'Started '+date(job.startedAt));
    text('activity-model',job.model+' · '+(job.effort||'default')+' reasoning · '+activity.permissions);
    text('activity-context',activity.context);
    text('activity-message',job.message);
    $('activity-message').classList.toggle('warning',['failed','interrupted'].includes(job.status));
    text('activity-error',job.error&&job.error!==job.message?job.error:'');
    text('activity-save-warning',job.historyWarning||'');
    const shown=state.assembly||state.lastBuild?.assembly;
    $('activity-review-code').disabled=!shown?.files.length;
    $('activity-review-code').textContent=state.assembly?'Review code':'Review previous code';
    $('activity-cancel').hidden=!activity.running;$('activity-send-again').hidden=activity.running;
    $('activity-send-again').disabled=state.deviceJob?.status==='running';
    text('activity-command-count',job.commandCount?job.commandCount+' repository '+(job.commandCount===1?'command':'commands'):'');
    const key=JSON.stringify([job.id,job.status,job.phase,job.events]);if(key===renderedKey)return;renderedKey=key;
    $('activity-steps').replaceChildren(...activity.steps.map((step,index)=>el('li',{'data-status':step.status},
      el('span',{class:'activity-step-number','aria-hidden':'true'},step.status==='done'?'✓':String(index+1)),el('span',{},step.name),
      el('small',{class:'sr-only'},step.status))));
    const list=$('activity-events'),scroll=list.scrollTop;
    list.replaceChildren(...[...(job.events||[])].reverse().map(event=>el('li',{'data-kind':event.kind||'status'},
      el('div',{},el('time',{datetime:event.lastAt||event.at},date(event.lastAt||event.at)),
        event.count>1?el('small',{},event.count+' updates'):null),el('p',{},event.message))));
    if(!job.events?.length)list.append(el('li',{},el('p',{},'Waiting for the first update from Codex…')));
    list.scrollTop=scroll;
  }
  $('run-details').onclick=()=>{renderedKey=null;$('run-activity-dialog').showModal();refresh();};
  $('activity-cancel').onclick=cancelRun;
  $('activity-review-code').onclick=()=>{$('run-activity-dialog').close();reviewCode();};
  $('activity-review-requests').onclick=()=>{$('run-activity-dialog').close();reviewRequests();};
  $('activity-send-again').onclick=()=>{const job=getState().job;$('run-activity-dialog').close();reviewAndSend(job);};
  $('activity-copy').onclick=async()=>{try{await navigator.clipboard.writeText(runReport(getState().job,getState()));toast('Copied assembly run details.');}catch{toast('Could not copy. Select the activity text to copy it manually.');}};
  setInterval(()=>{if(getState()?.job?.status==='running'||$('run-activity-dialog').open)refresh();},1000);
  refresh();return refresh;
}
