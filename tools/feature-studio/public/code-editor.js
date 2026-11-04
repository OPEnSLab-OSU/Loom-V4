export function installCodeEditor({api,toast,getContext,onSaved}) {
  const $=id=>document.getElementById(id),drafts=new Map();let current=null,saving=false;
  const key=context=>context.projectId+'|'+context.file?.name;
  function render() {
    const context=getContext(),id=key(context),draft=drafts.get(id);
    const busy=saving||context.busy;
    $('edit-code').disabled=!context.file||busy;$('edit-code').hidden=!!draft;
    $('save-code-edit').hidden=$('cancel-code-edit').hidden=!draft;
    $('save-code-edit').disabled=$('cancel-code-edit').disabled=busy;
    $('code-edit-help').hidden=$('code-editor').hidden=!draft;$('code').hidden=!!draft;
    if(draft&&current!==id)$('code-editor').value=draft.content;
    current=id;
    $('code-panel').classList.toggle('editing-code',!!draft);
    for(const id of ['find-code','copy-code','open-code','download-file'])$(id).disabled=!!draft||!context.file;
    $('save-code-edit').textContent=saving?'Saving…':'Save code';
  }
  $('edit-code').onclick=()=>{const context=getContext();if(!context.file)return;drafts.set(key(context),{...context.file,content:context.file.content,previousContent:context.file.content});current=null;render();$('code-editor').focus();};
  $('code-editor').addEventListener('input',()=>{const draft=drafts.get(current);if(draft)draft.content=$('code-editor').value;});
  $('cancel-code-edit').onclick=()=>{drafts.delete(current);render();};
  const save=async()=>{
    const context=getContext(),id=key(context),draft=drafts.get(id);if(!draft||saving)return;
    saving=true;render();
    try{await api('/api/code',{projectId:context.projectId,file:draft.name,content:draft.content,previousContent:draft.previousContent});drafts.delete(id);await onSaved();toast('Code saved. Build uses these edits; the graph describes the original assembly.');}
    catch(error){toast(error.message);}
    finally{saving=false;render();}
  };
  $('save-code-edit').onclick=save;
  $('code-editor').addEventListener('keydown',event=>{
    if((event.ctrlKey||event.metaKey)&&event.key.toLowerCase()==='s'){event.preventDefault();save();}
    if(event.key==='Tab'){event.preventDefault();const area=$('code-editor');area.setRangeText('  ',area.selectionStart,area.selectionEnd,'end');const draft=drafts.get(current);if(draft)draft.content=area.value;}
  });
  window.addEventListener('beforeunload',event=>{if([...drafts.values()].some(draft=>draft.content!==draft.previousContent)){event.preventDefault();event.returnValue='';}});
  return {render,hasDraft:()=>drafts.has(key(getContext()))};
}
