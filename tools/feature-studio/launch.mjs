import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {spawn} from 'node:child_process';
import {mkdir,writeFile} from 'node:fs/promises';
import {createInterface} from 'node:readline/promises';
import {resolveLoom} from './find-loom.mjs';
import {startStudio} from './server.mjs';
import {setupLoom} from './setup-loom.mjs';

const directory=path.dirname(fileURLToPath(import.meta.url)),args=process.argv.slice(2);
const option=name=>{const index=args.indexOf(name);return index<0?undefined:args[index+1];};
function openBrowser(url) {
  const command=process.platform==='win32'?'rundll32.exe':process.platform==='darwin'?'open':'xdg-open';
  const child=spawn(command,process.platform==='win32'?['url.dll,FileProtocolHandler',url]:[url],{shell:false,windowsHide:true,stdio:'ignore'});
  child.on('error',()=>console.log('Open '+url+' in your browser.'));child.unref();
}
try {
  const workspace=path.resolve(option('--workspace')||path.join(directory,'.local'));
  const port=Number(option('--port')||process.env.PORT||4173);
  let existing;
  try {existing=await fetch('http://127.0.0.1:'+port+'/api/studio',{signal:AbortSignal.timeout(1500)}).then(res=>res.json());}catch{}
  if(existing?.app==='loom-feature-studio'&&path.resolve(existing.workspace)===workspace&&args.includes('--setup'))throw new Error('Close the running Studio server before installing board packages.');
  let root;
  if(args.includes('--setup'))root=await setupLoom({workspace,log:text=>process.stdout.write(text)});
  try {root=await resolveLoom({explicit:option('--loom-root'),workspace});}
  catch(error) {
    if(!process.stdin.isTTY)throw new Error(error.message+' To install dependencies, run start.cmd --setup (Mac/Linux: sh start.sh --setup).');
    console.log(error.message);
    const terminal=createInterface({input:process.stdin,output:process.stdout});
    try {
      console.log('Install Arduino CLI if needed, then Arduino SAMD → Adafruit SAMD → Loom SAMD. Existing versions will be kept.');
      const choice=(await terminal.question('[I] Install and open Studio · [F] Choose an existing Loom folder · [Q] Quit: ')).trim().toLowerCase();
      if(choice==='i'||choice==='install')root=await setupLoom({workspace,log:text=>process.stdout.write(text)});
      else if(choice==='f'){const answer=(await terminal.question('Loom library folder: ')).trim().replace(/^"|"$/g,'');root=await resolveLoom({explicit:answer,workspace});}
      else throw new Error('Setup cancelled. No packages were installed.');
    }finally{terminal.close();}
  }
  if(existing?.app==='loom-feature-studio'&&path.resolve(existing.workspace)===workspace) {
    if(path.resolve(existing.root)!==root)throw new Error('Studio is already using a different Loom folder. Close its server before changing --loom-root.');
    const url='http://127.0.0.1:'+port;
    console.log('Feature Studio is already running: '+url);if(!args.includes('--no-open'))openBrowser(url);
  } else {
    const studio=await startStudio({root,workspace,port,codexPath:option('--codex'),warmTools:true});
    await mkdir(workspace,{recursive:true});
    await writeFile(path.join(workspace,'studio-config.json'),JSON.stringify({loomRoot:root},null,2)+'\n');
    console.log('Feature Studio: '+studio.url+'\nLoom: '+root+'\nProjects: '+workspace+'\nKeep this window open. Press Ctrl+C to stop.');
    if(!args.includes('--no-open'))openBrowser(studio.url);
    for(const signal of ['SIGINT','SIGTERM'])process.on(signal,()=>studio.server.close(()=>process.exit(0)));
  }
}catch(error){console.error('Feature Studio could not start: '+error.message);process.exitCode=1;}
