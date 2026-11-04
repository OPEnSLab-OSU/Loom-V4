export function installHistory({api,el,getState,isDirty,isBusy,save,changeProject,toast}) {
  const $=id=>document.getElementById(id);
  function render() {
    const history=getState().history,busy=isBusy();
    $('undo-design').disabled=busy||!(history?.canUndo||isDirty());
    $('redo-design').disabled=busy||!history?.canRedo||isDirty();
    $('design-history').disabled=busy;
    if(!$('history-dialog').open||!history)return;
    $('history-project-name').textContent=getState().design.name;
    $('history-list').replaceChildren(...history.entries.map((entry,i)=>el('button',{
      class:'history-entry'+(entry.current?' current':'')+(entry.ahead?' ahead':''),disabled:entry.current?'':null,
      onclick:async()=>{if(await changeProject('/api/history/restore',{id:entry.id}))$('history-dialog').close();}
    },el('span',{class:'history-dot'},entry.current?'●':'○'),el('span',{},el('strong',{},entry.label),el('small',{},new Date(entry.at).toLocaleString())),
      el('span',{class:'node-badge'},entry.current?'CURRENT':entry.ahead?'REDO':String(i+1)))).reverse());
  }
  const move=async action=>{await changeProject('/api/history/'+action);render();};
  $('undo-design').onclick=()=>move('undo');$('redo-design').onclick=()=>move('redo');
  $('design-history').onclick=async()=>{
    try{await save();getState().history=await api('/api/history');$('history-dialog').showModal();render();}catch(error){toast(error.message);}
  };
  document.addEventListener('keydown',event=>{
    if(event.defaultPrevented||!(event.ctrlKey||event.metaKey)||event.altKey||isBusy())return;
    const focused=document.activeElement;
    if(focused?.matches('input,textarea,select,[contenteditable="true"]')||document.querySelector('dialog[open]:not(#history-dialog)'))return;
    const key=event.key.toLowerCase(),action=key==='z'&&!event.shiftKey?'undo':key==='y'||key==='z'&&event.shiftKey?'redo':null;
    if(!action)return;event.preventDefault();
    if(action==='undo'&&(getState().history?.canUndo||isDirty())||action==='redo'&&getState().history?.canRedo&&!isDirty())move(action);
  });
  render();return render;
}
