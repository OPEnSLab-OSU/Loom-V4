export function installCodexAccount({api,toast,onAvailable}) {
  const $=id=>document.getElementById(id);let timer;
  function render(state) {
    $('codex-account-status').textContent=state.message;
    $('codex-sign-in').hidden=state.signedIn||!state.available||state.status==='signing-in';
    $('codex-cancel-sign-in').hidden=state.status!=='signing-in';
    $('codex-install-help').hidden=state.available;
    onAvailable(state.available);
    clearTimeout(timer);
    if(state.status==='signing-in'&&$('codex-account-dialog').open)timer=setTimeout(check,2000);
  }
  async function check(){try{render(await api('/api/codex/auth'));}catch(error){toast(error.message);}}
  const open=()=>{$('codex-account-status').textContent='Checking Codex sign-in…';$('codex-account-dialog').showModal();check();};
  $('codex-account').onclick=open;$('run-sign-in').onclick=open;
  $('codex-check-sign-in').onclick=check;
  $('codex-sign-in').onclick=async()=>{try{render(await api('/api/codex/login',{}));}catch(error){toast(error.message);}};
  $('codex-cancel-sign-in').onclick=async()=>{try{render(await api('/api/codex/login/cancel',{}));}catch(error){toast(error.message);}};
  $('codex-account-dialog').addEventListener('close',()=>clearTimeout(timer));
}
