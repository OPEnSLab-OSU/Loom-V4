import {test} from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import os from 'node:os';
import {mkdtemp,writeFile,readFile,rm} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {setupLoom,setupCores,boardIndexURLs} from '../setup-loom.mjs';
import {downloadArduinoCLI,cliArchiveSuffix} from '../cli-download.mjs';

test('first-run setup installs Arduino then Adafruit then Loom, finds the bundled library, and never compiles/uploads',async t=>{
  const workspace=await mkdtemp(path.join(os.tmpdir(),'studio-bootstrap-'));t.after(()=>rm(workspace,{recursive:true,force:true}));
  const calls=[],root=path.join(workspace,'installed-Loom');let downloads=0;
  const tools={cli:null,config:{additionalUrls:'https://example.test/extra.json'},async init(){},async findCLI(){this.cli=this.config.cliPath;},
    async execute(args){calls.push(args);if(args[0]==='config')return JSON.stringify({directories:{data:'/custom-arduino-data'}});return '';},
    async json(args){calls.push(args);if(args[0]==='core')return {platforms:[]};return {boards:[{name:'Loomified Adafruit Feather M0',fqbn:'loom4:samd:adafruit_feather_m0'}]};},
    async changedToolchain(){},cancel(){this.job.status='cancelled';},
    start(kind,operation){this.job={status:'running'};this.work=operation(()=>{},new AbortController().signal).then(()=>{this.job.status='succeeded';}).catch(error=>{this.job={status:'failed',message:error.message};});}
  };
  const options={workspace,tools,log:()=>{},download:async()=>{downloads++;return path.join(workspace,'arduino-cli');},findInstalled:async folders=>{assert(folders.includes('/custom-arduino-data'));return [{root,version:'latest'}];}};
  assert.equal(await setupLoom(options),root);assert.equal(downloads,1);
  assert.deepEqual(calls.filter(args=>args[0]==='core'&&args[1]==='install').map(args=>args[2]),setupCores);
  assert.equal(calls.some(args=>['compile','upload'].includes(args[0])),false);assert(boardIndexURLs.every(url=>tools.config.additionalUrls.includes(url)));
  assert.equal(JSON.parse(await readFile(path.join(workspace,'studio-config.json'),'utf8')).loomRoot,root);
  calls.length=0;tools.json=async args=>{calls.push(args);return args[0]==='core'?{platforms:setupCores.map(id=>({id,installed_version:'kept'}))}:{boards:[{name:'Loomified Feather M0',fqbn:'loom4:samd:feather'}]};};
  await setupLoom(options);assert.equal(downloads,1);assert.equal(calls.filter(args=>args[1]==='install').length,0,'existing cores are not upgraded');
});

test('CLI downloader verifies official archives and supports Windows, macOS and Linux releases',async t=>{
  const temp=await mkdtemp(path.join(os.tmpdir(),'studio-cli-download-'));t.after(()=>rm(temp,{recursive:true,force:true}));
  assert.equal(cliArchiveSuffix('win32','x64'),'Windows_64bit.zip');assert.equal(cliArchiveSuffix('darwin','arm64'),'macOS_ARM64.tar.gz');assert.equal(cliArchiveSuffix('linux','x64'),'Linux_64bit.tar.gz');
  const bytes=Buffer.from('fixture archive'),checksum=createHash('sha256').update(bytes).digest('hex');
  for(const [platform,arch] of [['win32','x64'],['darwin','arm64'],['linux','x64']]) {
    const name='arduino-cli_1.5.0_'+cliArchiveSuffix(platform,arch),base='https://github.com/arduino/arduino-cli/releases/download/v1.5.0/',directory=path.join(temp,platform);let extracted=0;
    const fetcher=async url=>({ok:true,json:async()=>({assets:[{name,browser_download_url:base+name},{name:'checksums.txt',browser_download_url:base+'checksums.txt'}]}),arrayBuffer:async()=>bytes,text:async()=>checksum+'  '+name});
    const executable=await downloadArduinoCLI({directory,platform,arch,fetcher,extract:async()=>{extracted++;await writeFile(path.join(directory,platform==='win32'?'arduino-cli.exe':'arduino-cli'),'fixture');}});
    assert.equal(extracted,1);assert.equal(await readFile(executable,'utf8'),'fixture');
  }
});
