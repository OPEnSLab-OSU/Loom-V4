import {test} from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,mkdir,readFile,writeFile,rm} from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import {ArduinoTools,firmwareKey,missingHeaders} from '../arduino.mjs';
import {atomicJSON,startStudio} from '../server.mjs';
import {revisionOf} from '../core.mjs';

test('device tools discover without compiling; explicit builds and uploads use matching code, board and connected port',async t=>{
  const workspace=await mkdtemp(path.join(os.tmpdir(),'studio-device-'));t.after(()=>rm(workspace,{recursive:true,force:true}));
  const calls=[],board='test:samd:feather_m0';let failBuild=false;
  const execute=async(args,log=()=>{})=>{
    calls.push(args);
    if(args[0]==='board'&&args[1]==='list')return JSON.stringify({detected_ports:[{port:{address:'COM8',protocol:'serial'},matching_boards:[{name:'Test Feather'}]}]});
    if(args[0]==='board'&&args[1]==='listall')return JSON.stringify({boards:[{name:'Test Feather M0',fqbn:board}]});
    if(args[0]==='core'&&args[1]==='list')return JSON.stringify({platforms:[{id:'test:samd',installed_version:'1.0'}]});
    if(args[1]==='search')return JSON.stringify(args[0]==='lib'?{libraries:[{name:'Example library',releases:{'1.2.0':{},'1.2.3':{sentence:'A test library'}}}]}:{platforms:[{id:'test:samd',latest_version:'1.0'}]});
    if(args[0]==='compile'&&failBuild)throw new Error('fatal error: MissingSensor.h: No such file or directory');
    if(args[0]==='compile')await writeFile(path.join(args[args.indexOf('--output-dir')+1],'firmware.bin'),'simulated firmware');
    log('Finished '+args[0]+'\n');return '';
  };
  const tools=new ArduinoTools({root:workspace,workspace,atomicJSON,execute});await tools.init();
  const fakeCLI=path.join(workspace,process.platform==='win32'?'arduino-cli.exe':'arduino-cli');await writeFile(fakeCLI,'test executable placeholder');
  await tools.configure({cliPath:fakeCLI});
  const status=await tools.status();assert.equal(status.ports[0].address,'COM8');assert.equal(status.boards[0].fqbn,board);
  assert.deepEqual(calls.find(args=>args[1]==='listall'),['board','listall','feather','--json'],'board lookup stays focused and supports installed CLI generations');
  assert.ok(calls.every(args=>['board','core'].includes(args[0])),'discovery never compiles, installs or uploads');
  const assembly={files:[{name:'MySketch.ino',content:'void setup(){}\nvoid loop(){}'},{name:'settings.h',content:'#define EXAMPLE 1'}]},fingerprint='current';
  await assert.rejects(tools.flash({assembly,board,fingerprint,port:'COM8',buildKey:'guessed'}),/Build the current code/);
  await tools.build({assembly,board,fingerprint,projectId:'project-a'});await tools.work;
  assert.equal(tools.job.status,'succeeded');const key=tools.job.buildKey;
  const compile=calls.find(args=>args[0]==='compile');
  assert.ok(!compile.includes('--upload')&&!compile.includes('-u'),'build never uploads');
  assert.equal(await readFile(path.join(compile.at(-1),'MySketch.ino'),'utf8'),assembly.files[0].content);
  assert.equal(await readFile(path.join(compile.at(-1),'settings.h'),'utf8'),assembly.files[1].content);
  await assert.rejects(tools.flash({assembly,board,fingerprint,port:'COM999',buildKey:key}),/connected COM/);
  await assert.rejects(tools.flash({assembly:{files:[{name:'MySketch.ino',content:'changed'}]},board,fingerprint,port:'COM8',buildKey:key}),/Build the current code/);
  await tools.flash({assembly,board,fingerprint,port:'COM8',buildKey:key,projectId:'project-a'});await tools.work;
  const upload=calls.find(args=>args[0]==='upload');assert.ok(upload.includes('--input-dir'));assert.ok(upload.includes('COM8'));assert.ok(!upload.includes('compile'));
  failBuild=true;await tools.build({assembly,board,fingerprint});await tools.work;
  assert.equal(tools.job.status,'failed');assert.deepEqual(tools.job.missingHeaders,['MissingSensor.h']);
  assert.equal(await tools.built(assembly,board,fingerprint),null,'failed rebuild cannot flash an older binary');
  assert.throws(()=>tools.install('lib','--git-url'),/Search and choose/);
  await tools.search('lib','provides:MissingSensor.h');tools.install('lib','Example library');await tools.work;
  assert.deepEqual(calls.find(args=>args[0]==='lib'&&args[1]==='install'),['lib','install','Example library@1.2.3']);
  await tools.search('core','samd');tools.install('core','test:samd');await tools.work;
  assert.deepEqual(calls.find(args=>args[0]==='core'&&args[1]==='install'),['core','install','test:samd@1.0']);
  assert.deepEqual(missingHeaders('fatal error: one.h: No such file\nfatal error: one.h: No such file'),['one.h']);
  assert.notEqual(firmwareKey(assembly,board,fingerprint),firmwareKey(assembly,'other:board:target',fingerprint));
});

test('official CLI getter rejects a bad checksum before extracting or executing a download',async t=>{
  if(process.platform!=='win32'||process.arch!=='x64')return;
  const workspace=await mkdtemp(path.join(os.tmpdir(),'studio-cli-get-'));t.after(()=>rm(workspace,{recursive:true,force:true}));
  const urls=[],base='https://github.com/arduino/arduino-cli/releases/download/v1.0/';
  const fetcher=async url=>{urls.push(url);return{ok:true,json:async()=>({assets:[{name:'arduino-cli_1.0.0_Windows_64bit.zip',browser_download_url:base+'cli.zip'},{name:'checksums.txt',browser_download_url:base+'checksums.txt'}]}),arrayBuffer:async()=>new TextEncoder().encode('not an archive').buffer,text:async()=> '0'.repeat(64)+'  arduino-cli_1.0.0_Windows_64bit.zip'};};
  const tools=new ArduinoTools({root:workspace,workspace,atomicJSON,fetcher});
  await tools.installCLI();await tools.work;
  assert.equal(tools.job.status,'failed');assert.match(tools.job.message,/checksum verification/);assert.equal(urls.length,3);
});

test('compiler messages navigate only to matching project code and retain old-build review without running uploads',async t=>{
  const temp=await mkdtemp(path.join(os.tmpdir(),'studio-diagnostics-http-')),root=path.join(temp,'repo'),workspace=path.join(temp,'local');
  await mkdir(path.join(root,'src'),{recursive:true});await mkdir(path.join(root,'examples/Test'),{recursive:true});
  await writeFile(path.join(root,'src/Test.h'),'class Manager {public:Manager(const char *name,int id);};');
  await writeFile(path.join(root,'examples/Test/Test.ino'),'#include <Test.h>\nManager manager("Device",1);\nvoid setup(){manager.initialize();}\nvoid loop(){manager.measure();}\n');
  const calls=[],board='test:samd:feather';
  const app=await startStudio({root,workspace,port:0,arduinoExecute:async(args,log=()=>{})=>{
    calls.push(args);
    if(args[0]==='board'&&args[1]==='listall')return JSON.stringify({boards:[{name:'Feather M0',fqbn:board}]});
    if(args[0]==='board')return JSON.stringify({detected_ports:[]});
    if(args[0]==='core')return JSON.stringify({platforms:[]});
    if(args[0]==='compile') {
      const filename=path.join(args.at(-1),'FeatureApplication.ino');
      log(filename+':4:14: error: test compiler failure\n'+path.join(root,'src','Test.h')+':1:5: note: test library context\n');
      throw new Error('Compilation failed (test fixture).');
    }
    throw new Error('Unexpected command '+args[0]);
  }});
  t.after(async()=>{await new Promise(resolve=>{app.server.close(resolve);app.server.closeAllConnections();});await rm(temp,{recursive:true,force:true});});
  const get=async(route,status=200)=>{const response=await fetch(app.url+route),body=await response.json();assert.equal(response.status,status,JSON.stringify(body));return body;};
  const post=async(route,body)=>{const response=await fetch(app.url+route,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body||{})});assert.ok(response.ok,await response.text());};
  const cli=path.join(temp,process.platform==='win32'?'arduino-cli.exe':'arduino-cli');await writeFile(cli,'test placeholder');
  await post('/api/device/configure',{cliPath:cli});await post('/api/example',{path:'examples/Test/Test.ino'});
  let state=await get('/api/state');const projectId=state.projects.activeId;
  await post('/api/device/build',{projectId,board});
  let result;
  for(let i=0;i<100;i++){result=await get('/api/device/job?board='+board);if(result.job.status!=='running')break;await new Promise(resolve=>setTimeout(resolve,5));}
  assert.equal(result.job.status,'failed');assert.equal(result.diagnosticsMatch,true);assert.equal(result.job.diagnostics.length,2);
  assert.equal(result.job.diagnostics[0].applicationFile,'FeatureApplication.ino');assert.equal(result.job.diagnostics[1].applicationFile,null);
  const route='/api/device/diagnostic?'+new URLSearchParams({jobId:result.job.id,projectId,index:'0'});
  assert.deepEqual(await get(route),{file:'FeatureApplication.ino',line:4,endLine:4,projectId});
  await get(route.replace('index=0','index=1'),400);await get(route.replace('index=0','index=-1'),400);await get(route.replace(result.job.id,'old-job'),400);
  await post('/api/design',{projectId,design:{...state.design,description:'Changed requirements; keep the old code for review'}});
  assert.equal((await get('/api/state')).assembly,null);
  assert.equal((await get(route)).line,4,'the matching old sketch remains reviewable');
  await post('/api/design/new');const otherTab=(await get('/api/state')).projects.activeId;
  assert.equal((await get(route)).projectId,projectId,'a verified message can target another open tab');
  assert.equal((await get('/api/state')).projects.activeId,otherTab,'checking a diagnostic never switches the active tab');
  await post('/api/projects/open',{id:projectId});state=await get('/api/state');
  const replacement=structuredClone(state.lastBuild.assembly);
  replacement.designRevision=revisionOf(state.design);replacement.files[0].content+='// new application code\n';
  await writeFile(path.join(workspace,'assembly.json'),JSON.stringify(replacement));
  await get('/api/state');assert.equal((await get('/api/device/job?board='+board)).diagnosticsMatch,false);
  await get(route,400);
  assert.equal(calls.filter(args=>args[0]==='compile').length,1);assert.equal(calls.filter(args=>args[0]==='upload').length,0);
});

test('device HTTP actions require a current project and explicit requests; large CLI JSON stays intact',async t=>{
  const workspace=await mkdtemp(path.join(os.tmpdir(),'studio-device-http-'));t.after(()=>rm(workspace,{recursive:true,force:true}));
  const root=path.join(workspace,'repo');await mkdir(path.join(root,'src'),{recursive:true});await mkdir(path.join(root,'examples/Test'),{recursive:true});
  await writeFile(path.join(root,'src/Test.h'),'class Manager {public: Manager(const char *name,int id);};');
  await writeFile(path.join(root,'examples/Test/Test.ino'),'Manager manager("Device",1);\nvoid setup(){manager.initialize();}\nvoid loop(){manager.measure();}');
  const calls=[],board='test:samd:feather';
  const app=await startStudio({root,workspace:path.join(workspace,'local'),port:0,arduinoExecute:async(args)=>{
    calls.push(args);
    if(args[0]==='board'&&args[1]==='listall')return JSON.stringify({boards:[{name:'Feather M0',fqbn:board}]});
    if(args[0]==='board')return JSON.stringify({detected_ports:[{port:{address:'COM8',protocol:'serial'},matching_boards:[]}]});
    if(args[0]==='core')return JSON.stringify({platforms:[]});
    if(args[0]==='compile')await writeFile(path.join(args[args.indexOf('--output-dir')+1],'firmware.bin'),'simulated firmware');
    return '';
  }});t.after(()=>new Promise(resolve=>{app.server.close(resolve);app.server.closeAllConnections();}));
  const get=async route=>(await fetch(app.url+route)).json();
  const post=async(route,body,status=200)=>{const r=await fetch(app.url+route,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});const v=await r.json();assert.equal(r.status,status,JSON.stringify(v));return v;};
  assert.equal(calls.length,0,'startup does not invoke Arduino CLI');
  const fakeCLI=path.join(workspace,process.platform==='win32'?'arduino-cli.exe':'arduino-cli');await writeFile(fakeCLI,'placeholder');
  await post('/api/device/configure',{cliPath:fakeCLI});
  await get('/api/device/status');assert.ok(calls.every(c=>!['compile','upload','install'].includes(c[0])));
  await post('/api/device/build',{projectId:'wrong',board},400);
  await post('/api/example',{path:'examples/Test/Test.ino'});const state=await get('/api/state');
  await post('/api/device/build',{projectId:'wrong',board},400);
  await post('/api/device/build',{projectId:state.projects.activeId,board},202);
  let result;for(let i=0;i<100;i++){result=await get('/api/device/job?board='+board);if(result.job.status!=='running')break;await new Promise(r=>setTimeout(r,5));}
  assert.equal(result.job.status,'succeeded');assert.ok(result.build.key);assert.equal(calls.filter(c=>c[0]==='compile').length,1);assert.equal(calls.filter(c=>c[0]==='upload').length,0);
  await post('/api/device/flash',{projectId:state.projects.activeId,board,port:'COM8',buildKey:'wrong'},400);
  await post('/api/device/flash',{projectId:state.projects.activeId,board,port:'COM8',buildKey:result.build.key},202);
  const tools=new ArduinoTools({root,workspace,atomicJSON});tools.cli=process.execPath;
  const output=await tools.process(['-e','process.stdout.write(JSON.stringify({data:"x".repeat(600000)}));process.stderr.write("a warning");']);
  assert.equal(JSON.parse(output).data.length,600000,'valid CLI JSON is not truncated or mixed with stderr');
});
