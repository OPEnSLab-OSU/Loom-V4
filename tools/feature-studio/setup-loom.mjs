import path from 'node:path';
import {fileURLToPath,pathToFileURL} from 'node:url';
import {mkdir,writeFile,rename} from 'node:fs/promises';
import {ArduinoTools} from './arduino.mjs';
import {downloadArduinoCLI} from './cli-download.mjs';
import {installedLooms,arduinoDataFolders} from './find-loom.mjs';

const directory=path.dirname(fileURLToPath(import.meta.url));
export const boardIndexURLs=[
  'https://adafruit.github.io/arduino-board-index/package_adafruit_index.json',
  'https://raw.githubusercontent.com/OPEnSLab-OSU/Loom-V4/main/auxilary/package_loom4_index.json'
];
export const setupCores=['arduino:samd','adafruit:samd','loom4:samd'];
const atomicJSON=async(file,value)=>{await writeFile(file+'.tmp',JSON.stringify(value,null,2)+'\n');await rename(file+'.tmp',file);};
export async function setupLoom({workspace=path.join(directory,'.local'),tools,log=console.log,download=downloadArduinoCLI,findInstalled=installedLooms}={}) {
  await mkdir(workspace,{recursive:true});
  tools ||= new ArduinoTools({root:directory,workspace,atomicJSON});await tools.init();
  const cancel=()=>tools.cancel();process.once('SIGINT',cancel);
  try {
    tools.start('setup',async(jobLog,signal)=>{
      const output=text=>{jobLog(text);log(text);};
      if(!tools.cli){tools.config.cliPath=await download({directory:path.join(workspace,'toolchain'),signal,log:output});await tools.findCLI();}
      if(!tools.cli)throw new Error('Arduino CLI was not found after setup.');
      tools.config.additionalUrls=[...new Set([...(tools.config.additionalUrls||'').split(',').filter(Boolean),...boardIndexURLs])].join(',');
      await atomicJSON(path.join(workspace,'arduino-settings.json'),tools.config);
      output('Refreshing Arduino, Adafruit and Loom package indexes…\n');await tools.execute(['core','update-index'],output,300000);signal.throwIfAborted();
      const result=await tools.json(['core','list']),installed=new Set((result.platforms||result||[]).filter(item=>item.installed_version||item.installed||item.version).map(item=>item.id));
      for(const id of setupCores){signal.throwIfAborted();if(installed.has(id)){output(id+' is already installed; keeping its version.\n');continue;}output('Installing '+id+'…\n');await tools.execute(['core','install',id],output,900000);}
      signal.throwIfAborted();await tools.changedToolchain();
      const boards=await tools.json(['board','listall','feather']);
      if(!boards.boards?.some(board=>board.fqbn?.startsWith('loom4:samd:')&&/feather.*m0/i.test(board.name)))throw new Error('The Loomified Feather M0 board was not reported after installation. Review the setup output.');
    });
    await tools.work;if(tools.job.status!=='succeeded')throw new Error(tools.job.message);
    let data;
    try{data=JSON.parse(await tools.execute(['config','dump','--format','json'])).directories?.data;}catch{}
    const candidates=await findInstalled([...new Set([...arduinoDataFolders(),...(data?[data]:[])])]);
    if(!candidates.length)throw new Error('Board packages installed, but no bundled Loom library was found. Check the package release or choose a Loom folder with --loom-root.');
    const root=candidates[0].root;await atomicJSON(path.join(workspace,'studio-config.json'),{loomRoot:root});
    log('Loom is ready: '+root+'\nNo application was compiled or flashed.\n');return root;
  }finally{process.removeListener('SIGINT',cancel);}
}
if(process.argv[1]&&import.meta.url===pathToFileURL(path.resolve(process.argv[1])).href){
  const args=process.argv.slice(2),index=args.indexOf('--workspace'),workspace=index<0?undefined:path.resolve(args[index+1]);
  console.log('Setup: Arduino CLI if missing, then Arduino SAMD → Adafruit SAMD → Loom SAMD. Existing board versions are kept.');
  if(!args.includes('--yes')){console.log('Run setup.cmd --yes or sh setup.sh --yes to install these packages.');}
  else setupLoom({workspace,log:text=>process.stdout.write(text)}).catch(error=>{console.error('Setup stopped: '+error.message);process.exitCode=1;});
}
