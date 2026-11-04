import path from 'node:path';
import {spawn} from 'node:child_process';
import {mkdir,writeFile,stat,chmod} from 'node:fs/promises';
import {createHash} from 'node:crypto';

export function cliArchiveSuffix(platform,arch) {
  const system={win32:'Windows',darwin:'macOS',linux:'Linux'}[platform],cpu={x64:'64bit',arm64:'ARM64',arm:'ARMv7'}[arch];
  if(!system||!cpu||platform==='win32'&&arch!=='x64')throw new Error('Install Arduino CLI from https://arduino.cc/en/software for this operating system.');
  return system+'_'+cpu+(platform==='win32'?'.zip':'.tar.gz');
}
export async function downloadArduinoCLI({directory,platform=process.platform,arch=process.arch,fetcher=fetch,signal,log=()=>{},extract}={}) {
  const suffix=cliArchiveSuffix(platform,arch);log('Getting the official Arduino CLI release for '+platform+'/'+arch+'…\n');
  const response=await fetcher('https://api.github.com/repos/arduino/arduino-cli/releases/latest',{headers:{'User-Agent':'Loom-Feature-Studio'},signal});
  if(!response.ok)throw new Error('Could not look up the official Arduino CLI release.');
  const release=await response.json(),asset=release.assets?.find(item=>/^arduino-cli_[\d.]+_[A-Za-z0-9_]+\.(zip|tar\.gz)$/.test(item.name)&&item.name.endsWith('_'+suffix)),checksums=release.assets?.find(item=>/checksums\.txt$/.test(item.name));
  if(!asset||!checksums)throw new Error('The official release does not contain the expected archive and checksums.');
  const get=async url=>{if(!url.startsWith('https://github.com/arduino/arduino-cli/releases/download/'))throw new Error('Unexpected release URL.');const result=await fetcher(url,{signal});if(!result.ok)throw new Error('Arduino CLI download failed.');return result;};
  const archive=Buffer.from(await (await get(asset.browser_download_url)).arrayBuffer());
  if(archive.length>150000000)throw new Error('Arduino CLI archive is unexpectedly large.');
  const checksumText=await (await get(checksums.browser_download_url)).text();
  const expected=checksumText.split(/\r?\n/).find(line=>line.trim().split(/\s+/).at(-1)?.replace(/^\*/,'')===asset.name)?.trim().split(/\s+/)[0];
  if(!expected||createHash('sha256').update(archive).digest('hex')!==expected.toLowerCase())throw new Error('Arduino CLI checksum verification failed.');
  signal?.throwIfAborted();await mkdir(directory,{recursive:true});
  const archivePath=path.join(directory,asset.name);await writeFile(archivePath,archive);
  if(extract)await extract(archivePath,directory);
  else await new Promise((resolve,reject)=>{
    const child=spawn(platform==='win32'?'tar.exe':'tar',['-xf',archivePath,'-C',directory],{windowsHide:true,shell:false,signal,stdio:'ignore'});
    child.once('error',reject);child.once('close',code=>code===0?resolve():reject(new Error('Could not extract Arduino CLI.')));
  });
  const executable=path.join(directory,platform==='win32'?'arduino-cli.exe':'arduino-cli');
  if(!(await stat(executable)).isFile())throw new Error('Arduino CLI executable was not found after extraction.');
  if(platform!=='win32')await chmod(executable,0o755);
  log('Arduino CLI checksum verified.\n');return executable;
}
