export function installDeviceTools({ api, el, toast, getState, save, onJob, reviewCode, showCodeLocation }) {
  const $ = id => document.getElementById(id);
  let info, job, build, polling = false, wasRunning = false, mode='build',refreshing=false;
  let projectChoices=[],selectedProject=null,preferredProject=null;
  let diagnosticsMatch=false,diagnosticProjectId=null,lastDiagnosticKey='';
  let jobMatchesSelected=false,lastBinaryKey='';
  const stored = key => { try { return localStorage.getItem(key) || ''; } catch { return ''; } };
  const remember = (key,value) => { try { localStorage.setItem(key,value); } catch {} };
  function render() {
    const running = job?.status === 'running', current = getState();
    $('device-review-title').textContent=mode==='build'?'Project to build':'Project to flash';
    $('device-review-description').textContent=selectedProject?.hasCode?selectedProject.files.length+' '+(selectedProject.files.length===1?'file':'files')+' · '+(selectedProject.files.find(file=>file.name.endsWith('.ino'))?.name||''):'Assemble a project to make its code available here.';
    $('device-review-code').disabled=!selectedProject?.hasCode||(running&&selectedProject.id!==current.projects?.activeId);
    $('device-project').disabled=running||refreshing||!projectChoices.some(project=>project.hasCode);
    $('device-build').textContent=refreshing?'Checking tools…':'Build';
    $('device-build').disabled = !info?.available || !selectedProject?.hasCode || !$('device-board').value || running || refreshing || current.job?.status === 'running';
    $('device-flash').disabled = $('device-build').disabled || !build || !$('device-port').value;
    $('device-open-binaries').disabled=!build||running||refreshing;
    $('device-go-flash').hidden=!build;$('device-go-flash').disabled=running||refreshing||current.job?.status==='running';
    $('device-go-build').hidden=!!build||!selectedProject?.hasCode;$('device-go-build').disabled=running||refreshing;
    renderBuildFiles(running);
    $('device-flash-board').textContent='Board: '+($('device-board').selectedOptions[0]?.textContent||'Loomified Feather M0');
    $('device-cancel').hidden = !running;
    $('get-arduino-cli').hidden = !!info?.available;
    for (const id of ['device-refresh','device-search','device-update-indexes','device-save-settings','get-arduino-cli']) $(id).disabled = running;
    $('device-update-indexes').disabled ||= !info?.available;
    $('device-search').disabled ||= !info?.available;
    $('device-board').disabled = true; $('device-port').disabled = running;
    document.querySelectorAll('#device-search-results button').forEach(b=>b.disabled=running);
    $('device-build-hint').textContent = !selectedProject?.hasCode ? 'No assembled code in the open project tabs.' :mode==='build'?
      build?'The binaries folder is ready.':'Build uses the saved code and the current Loom checkout.':
      build?'Choose a connected COM port to upload this project’s successful build.':'Build this project first, then return here to flash it.';
    const firmwareJob=job&&['build','flash'].includes(job.kind),otherProject=job?.projectId&&job.projectId!==selectedProject?.id;
    const relevant=job&&(!firmwareJob||job.kind===mode&&(!otherProject||running)),historical=relevant&&firmwareJob&&!jobMatchesSelected&&!running;
    $('device-build-hint').hidden=mode==='build'&&!!build&&job?.kind==='build'&&job.status==='succeeded'&&job.buildKey===build.key;
    $('device-job-context').textContent=relevant&&otherProject?'Last '+job.kind+': '+(current.projects?.items?.find(project=>project.id===job.projectId)?.name||'another project'):'';
    $('device-job-status').hidden=!relevant||refreshing;
    $('device-job-status').textContent = historical?'Output from an earlier '+job.kind+'.':job?.kind==='build'&&job.status==='failed'&&job.diagnostics?.length?'Build failed. Review the compiler messages below.':job?.message || '';
    document.querySelector('.device-log-details').hidden=!relevant||refreshing;
    document.querySelector('.device-log-details>summary').textContent=historical?'Earlier '+job.kind+' output':'Full tool output';
    $('device-log').textContent = relevant?job.log||'Waiting for tool output…':'';
    $('device-job-status').classList.toggle('warning',!historical&&job?.status==='failed');
    $('device-missing').replaceChildren(...(relevant?job?.missingHeaders||[]:[]).map(header=>el('button',{class:'quiet compact',onclick:()=>{
      $('device-search-kind').value='lib'; $('device-package-query').value='provides:'+header;document.querySelector('.device-setup').open=true;$('device-package-query').focus();search();
    }},'Find library for '+header)));
    renderDiagnostics(current);
    onJob(job);
  }
  const bytes=value=>value<1000?value+' B':new Intl.NumberFormat(undefined,{maximumFractionDigits:1}).format(value/(value<1000000?1000:1000000))+(value<1000000?' KB':' MB');
  function renderBuildFiles(running) {
    $('device-build-result').hidden=!build||running;
    if(!build)return;
    const key=JSON.stringify([build.key,build.builtAt,build.files]);if(key===lastBinaryKey)return;lastBinaryKey=key;
    $('device-built-time').textContent=build.builtAt?'Built '+new Date(build.builtAt).toLocaleString(undefined,{month:'short',day:'numeric',hour:'numeric',minute:'2-digit'}):'Saved build';
    const files=build.files||[];
    $('device-binary-summary').textContent=files.length+' build '+(files.length===1?'file':'files')+' · '+bytes(build.totalBytes||0);
    $('device-binary-list').replaceChildren(...files.map(file=>el('li',{},el('code',{},file.name),el('span',{},bytes(file.bytes)))));
  }
  function renderDiagnostics(current) {
    const projectId=selectedProject?.id,items=mode==='build'&&job?.kind==='build'&&job.projectId===projectId?job.diagnostics||[]:[];
    const canJump=diagnosticsMatch&&diagnosticProjectId===projectId&&job?.projectId===projectId;
    const key=JSON.stringify([job?.id,items,job?.diagnosticsTruncated,canJump,projectId,current.projects?.activeId,job?.status]);
    if(key===lastDiagnosticKey)return;lastDiagnosticKey=key;
    $('device-diagnostics').hidden=!items.length;if(!items.length)return;
    const errors=items.filter(item=>item.severity==='error').length,warnings=items.filter(item=>item.severity==='warning').length;
    $('device-diagnostics-title').textContent=[errors?errors+' '+(errors===1?'error':'errors'):'',warnings?warnings+' '+(warnings===1?'warning':'warnings'):''].filter(Boolean).join(' · ')||'Compiler notes';
    const otherProject=job.projectId!==current.projects?.activeId;
    const name=current.projects?.items?.find(project=>project.id===job.projectId)?.name||'another project';
    $('device-diagnostics-context').textContent=!canJump?'Messages from an earlier version of the code. Review the full output for its original file locations.':otherProject?'Select a location to open it in '+name+'.':'Select a sketch location to review the reported line.';
    const list=$('device-diagnostics-list');list.replaceChildren();
    const buildId=job.id,notes=el('details',{},el('summary',{},'Compiler notes ('+items.filter(item=>item.severity==='note').length+')'));
    items.forEach((item,index)=>{
      const card=el('article',{class:'device-diagnostic diagnostic-'+item.severity},
        el('strong',{},item.severity==='note'?'Note':item.severity==='warning'?'Warning':'Error'),el('p',{},item.message),
        el('small',{class:'diagnostic-location'},(item.applicationFile||item.file.split('/').pop())+':'+item.line+(item.column?':'+item.column:'')));
      if(item.file.includes('/'))card.append(el('details',{class:'diagnostic-path'},el('summary',{},'Full file path'),el('code',{},item.file)));
      if(item.applicationFile&&canJump)card.append(el('button',{class:'quiet compact',disabled:job.status==='running'?'':null,onclick:async event=>{
        const button=event.currentTarget;button.disabled=true;
        try {
          const route='/api/device/diagnostic?'+new URLSearchParams({jobId:buildId,index,projectId});
          await api(route);
          $('device-dialog').close();
          if(await reviewCode(projectId,true)===false)return;
          const location=await api(route);
          if(getState().projects?.activeId!==location.projectId)return toast('The active project changed. Reopen its build messages.');
          showCodeLocation(location,()=>open('build',projectId));
        }catch(error){toast(error.message);}
        finally{if(button.isConnected)button.disabled=false;}
      }},'View '+item.applicationFile+':'+item.line));
      (item.severity==='note'?notes:list).append(card);
    });
    if(items.some(item=>item.severity==='note'))list.append(notes);
    if(job.diagnosticsTruncated)list.append(el('p',{class:'small'},'Showing the first 100 compiler messages. See Full tool output for the rest.'));
  }
  async function refresh(force=false) {
    if(refreshing)return;refreshing=true;
    const refreshMode=mode;
    render();
    $('device-availability').textContent=mode==='build'?'Checking Arduino CLI and the Feather M0 board package…':'Checking Arduino CLI and connected COM ports…';
    $('device-refresh').disabled=true;
    try {
      const port=$('device-port').value||stored('studio-port');
      info=await api('/api/device/status?mode='+refreshMode+(force?'&refresh=true':''));if(refreshMode!==mode)return;job=info.job;
      const loom=info.boards.find(b=>b.fqbn===info.defaultBoard&&/loomified feather m0/i.test(b.name))||info.boards.find(b=>/loomified feather m0/i.test(b.name));
      $('device-availability').textContent=info.available?'Arduino CLI ready · '+(loom?'Loomified Feather M0 installed':'Loomified Feather M0 needs installation')+(mode==='flash'?' · '+info.ports.length+' connected '+(info.ports.length===1?'port':'ports'):''):'Arduino CLI was not found. Get it below or provide the installed executable path.';
      $('device-board').replaceChildren(el('option',{value:loom?.fqbn||''},loom?.name||'Loomified Feather M0 needs installation'));
      $('device-board').value=loom?.fqbn||'';
      $('device-port').replaceChildren(el('option',{value:''},info.ports.length?'Choose a COM port…':'No connected COM ports'),...info.ports.map(p=>el('option',{value:p.address},p.address+(p.boards.length?' · '+p.boards.join(', '):''))));
      $('device-port').value=info.ports.some(p=>p.address===port)?port:'';
      $('arduino-cli-path').value=info.config.cliPath||info.executable||'';
      $('arduino-config-path').value=info.config.configFile||'';
      $('arduino-board-urls').value=info.config.additionalUrls||'';
      $('device-installed').textContent=info.cores.length?'Installed board packages: '+info.cores.map(c=>c.id+(c.version?' '+c.version:'')).join(' · '):'No board packages reported. Find one in Setup below.';
      $('device-warnings').replaceChildren(...info.warnings.map(w=>el('p',{class:'small muted'},w)));
      const choices=await api('/api/device/projects');if(refreshMode!==mode)return;
      projectChoices=choices.projects;const available=projectChoices.filter(project=>project.hasCode);
      const choice=preferredProject||$('device-project').value||choices.activeId;
      $('device-project').replaceChildren(...(available.length?available.map(project=>el('option',{value:project.id},project.name)):[el('option',{value:''},'No assembled project tabs')]));
      $('device-project').value=available.some(project=>project.id===choice)?choice:available[0]?.id||'';
      selectedProject=available.find(project=>project.id===$('device-project').value)||null;build=null;preferredProject=null;
      await pollJob();
    } catch(error) { $('device-availability').textContent=error.message; }
    finally { refreshing=false;render();if(refreshMode!==mode&&$('device-dialog').open)refresh(); }
  }
  async function pollJob() {
    if(polling)return; polling=true;
    const projectId=$('device-project').value,board=$('device-board').value;
    try {if(!projectId){build=null;return;}const next=await api('/api/device/job?'+new URLSearchParams({board,projectId}));if(projectId!==$('device-project').value||board!==$('device-board').value)return;job=next.job;jobMatchesSelected=next.jobMatchesSelected;build=next.build;selectedProject=next.selectedProject;diagnosticsMatch=next.diagnosticsMatch;diagnosticProjectId=next.diagnosticProjectId;render();}
    catch(error){$('device-job-status').textContent=error.message;}
    finally{polling=false;}
  }
  async function action(route,body={}) {
    try {
      await save();
      job=await api('/api/device/'+route,body); wasRunning=job?.status==='running'; render();
    } catch(error){toast(error.message);}
  }
  async function search() {
    $('device-search').disabled=true;
    $('device-search-results').textContent='Searching Arduino’s package catalog…';
    const kind=$('device-search-kind').value;
    try {
      const result=await api('/api/device/search?'+new URLSearchParams({kind,q:$('device-package-query').value.trim()}));
      $('device-search-results').replaceChildren(...result.choices.map(c=>el('div',{class:'device-package'},
        el('div',{},el('strong',{},c.name),el('small',{},c.id+(c.version?' · '+c.version:'')+(c.description?' · '+c.description:''))),
        el('button',{class:'quiet compact',onclick:()=>action('install',{kind,id:c.id})},'Install'))));
      if(!result.choices.length)$('device-search-results').textContent='No matching package. Try a name or refresh the catalogs. Third-party boards may need their Boards Manager URL below.';
    } catch(error){$('device-search-results').textContent=error.message;}
    finally{render();}
  }
  function open(nextMode,projectId){preferredProject=projectId||(nextMode==='flash'?$('device-project').value||getState().projects.activeId:getState().projects.activeId);mode=nextMode;jobMatchesSelected=false;const dialog=$('device-dialog');dialog.dataset.mode=mode;
    const known=projectChoices.find(project=>project.id===preferredProject&&project.hasCode);
    if(known&&known.id!==selectedProject?.id){selectedProject=known;$('device-project').value=known.id;build=null;}
    $('device-dialog-title').textContent=mode==='flash'?'3 · Flash':'2 · Build';
    $('device-dialog-intro').textContent=mode==='flash'?'Upload a project’s successful build to a connected Loomified Feather M0.':'Compile saved code for the Loomified Feather M0. No board connection needed.';
    $('device-refresh').textContent=mode==='flash'?'Refresh ports':'Refresh tools';
    render();
    if(!dialog.open)dialog.showModal();refresh();
  }
  $('build-menu').onclick=()=>open('build');$('flash-menu').onclick=()=>open('flash');
  $('device-go-flash').onclick=()=>open('flash',selectedProject?.id);$('device-go-build').onclick=()=>open('build',selectedProject?.id);
  $('device-review-code').onclick=()=>{$('device-dialog').close();reviewCode(selectedProject?.id);};
  $('device-project').onchange=()=>{selectedProject=projectChoices.find(project=>project.id===$('device-project').value)||null;build=null;jobMatchesSelected=false;render();pollJob();};
  $('device-refresh').onclick=()=>refresh(true);
  $('device-board').onchange=()=>{remember('studio-board',$('device-board').value);build=null;render();pollJob();};
  $('device-port').onchange=()=>{remember('studio-port',$('device-port').value);render();};
  const selectedRequest=()=>({board:$('device-board').value,projectId:selectedProject?.id,filesFingerprint:selectedProject?.filesFingerprint});
  $('device-build').onclick=()=>action('build',selectedRequest());
  $('device-flash').onclick=()=>action('flash',{...selectedRequest(),port:$('device-port').value,buildKey:build?.key});
  $('device-open-binaries').onclick=async()=>{try{await api('/api/device/open-binaries',{...selectedRequest(),buildKey:build?.key});toast('Opened binaries folder.');}catch(error){toast(error.message);}};
  $('device-cancel').onclick=()=>action('cancel');
  $('get-arduino-cli').onclick=()=>action('get-cli');
  $('device-update-indexes').onclick=()=>action('update-indexes');
  $('device-search').onclick=search;
  $('device-package-query').onkeydown=e=>{if(e.key==='Enter'){e.preventDefault();search();}};
  $('device-save-settings').onclick=async()=>{
    try {await api('/api/device/configure',{cliPath:$('arduino-cli-path').value.trim(),configFile:$('arduino-config-path').value.trim(),additionalUrls:$('arduino-board-urls').value.trim()});await refresh();toast('Arduino tool settings saved.');}catch(error){toast(error.message);}
  };
  setInterval(async()=>{
    if(!$('device-dialog').open)return;
    await pollJob();
    if(wasRunning&&job?.status!=='running'){wasRunning=false;if(['install','get Arduino CLI','update catalogs'].includes(job?.kind))await refresh();}
    if(job?.status==='running')wasRunning=true;
  },1500);
}
