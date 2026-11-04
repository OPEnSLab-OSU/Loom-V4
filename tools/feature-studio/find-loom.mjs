import path from 'node:path';
import os from 'node:os';
import {fileURLToPath} from 'node:url';
import {readFile,readdir,realpath,stat} from 'node:fs/promises';

const appDirectory=path.dirname(fileURLToPath(import.meta.url));
export async function isLoomRoot(directory) {
  try {
    const properties=await readFile(path.join(directory,'library.properties'),'utf8');
    return /^name\s*=\s*Loom\s*$/im.test(properties)&&(await stat(path.join(directory,'src'))).isDirectory()&&(await stat(path.join(directory,'examples'))).isDirectory();
  } catch {return false;}
}
async function folders(directory) {
  try {return (await readdir(directory,{withFileTypes:true})).filter(item=>item.isDirectory()).map(item=>path.join(directory,item.name));}
  catch(error){if(['ENOENT','ENOTDIR','EACCES','EPERM'].includes(error.code))return [];throw error;}
}
export function arduinoDataFolders({env=process.env,home=os.homedir(),platform=process.platform}={}) {
  return [...new Set([
    env.ARDUINO_DIRECTORIES_DATA,
    platform==='win32'?path.join(env.LOCALAPPDATA||path.join(home,'AppData','Local'),'Arduino15'):null,
    platform==='darwin'?path.join(home,'Library','Arduino15'):null,
    path.join(home,'.arduino15'),
    // Windows Store installation of Arduino IDE.
    platform==='win32'?path.join(env.USERPROFILE||home,'Documents','ArduinoData'):null
  ].filter(Boolean).map(item=>path.resolve(item)))];
}
export async function installedLooms(dataFolders=arduinoDataFolders()) {
  const found=[];
  for(const data of dataFolders)for(const vendor of await folders(path.join(data,'packages')))
    for(const architecture of await folders(path.join(vendor,'hardware')))
      for(const release of await folders(architecture)) {
        const candidate=path.join(release,'libraries','Loom');
        if(await isLoomRoot(candidate))found.push({root:await realpath(candidate),version:path.basename(release)});
      }
  found.sort((a,b)=>b.version.localeCompare(a.version,undefined,{numeric:true})||a.root.localeCompare(b.root));
  return [...new Map(found.map(item=>[item.root,item])).values()];
}
export async function resolveLoom({explicit,workspace=path.join(appDirectory,'.local'),env=process.env,dataFolders,sourceTree=path.resolve(appDirectory,'../..')}={}) {
  const requested=explicit||env.LOOM_SOURCE_ROOT;
  if(requested) {
    if(!await isLoomRoot(requested))throw new Error('Loom was not found at '+requested+'. Choose the Loom library folder containing library.properties, src and examples.');
    return realpath(requested);
  }
  let saved;
  try {saved=JSON.parse(await readFile(path.join(workspace,'studio-config.json'),'utf8')).loomRoot;}catch(error){if(error.code!=='ENOENT'&&!(error instanceof SyntaxError))throw error;}
  if(saved&&await isLoomRoot(saved))return realpath(saved);
  if(await isLoomRoot(sourceTree))return realpath(sourceTree);
  const choices=await installedLooms(dataFolders||arduinoDataFolders({env}));
  if(choices.length)return choices[0].root;
  throw new Error('No installed Loom library found in the Arduino packages. Install the Loom board package, or run start.cmd --loom-root "path/to/Loom" (Mac/Linux: sh start.sh --loom-root "path/to/Loom").');
}
