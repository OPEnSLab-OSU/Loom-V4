import {test} from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,mkdir,writeFile,rm} from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import {EventEmitter} from 'node:events';
import {PassThrough} from 'node:stream';
import http from 'node:http';
import {execFile} from 'node:child_process';
import {promisify} from 'node:util';
import {fileURLToPath} from 'node:url';
import {installedLooms,resolveLoom,arduinoDataFolders} from '../find-loom.mjs';
import {CodexAuth} from '../codex-auth.mjs';

test('standalone discovery finds vendor/version independent Loom installs and remembers a valid selection',async t=>{
  const temp=await mkdtemp(path.join(os.tmpdir(),'studio-discovery-'));t.after(()=>rm(temp,{recursive:true,force:true}));
  const install=async(version,vendor='different-vendor')=>{const root=path.join(temp,'packages',vendor,'hardware','samd',version,'libraries','Loom');await mkdir(path.join(root,'src'),{recursive:true});await mkdir(path.join(root,'examples'));await writeFile(path.join(root,'library.properties'),'name=Loom\n');return root;};
  const older=await install('4.9'),newer=await install('4.10'),workspace=path.join(temp,'studio');await mkdir(workspace);
  await install('99','invalid');await writeFile(path.join(temp,'packages','invalid','hardware','samd','99','libraries','Loom','library.properties'),'name=Other\n');
  assert.deepEqual((await installedLooms([temp])).map(item=>item.version),['4.10','4.9']);
  const options={env:{},workspace,dataFolders:[temp],sourceTree:path.join(temp,'absent')};
  assert.equal(await resolveLoom(options),newer);
  await writeFile(path.join(workspace,'studio-config.json'),JSON.stringify({loomRoot:older}));
  assert.equal(await resolveLoom(options),older);
  assert.equal(await resolveLoom({...options,explicit:newer}),newer);
  await assert.rejects(resolveLoom({...options,explicit:path.join(temp,'missing')}),/Loom was not found/);
  await writeFile(path.join(workspace,'studio-config.json'),'{}');
  await assert.rejects(resolveLoom({...options,dataFolders:[]}),/No installed Loom/);
  assert(arduinoDataFolders({env:{ARDUINO_DIRECTORIES_DATA:temp},home:temp,platform:'linux'}).includes(temp));
});

test('Codex sign-in uses CLI-owned auth, redacts output, supports cancellation and rechecking',async()=>{
  let signedIn=false,child,args;
  const auth=new CodexAuth({command:'fake',prefix:['entry.js']},{execute:(command,argv,options,callback)=>{assert.deepEqual(argv,['entry.js','login','status']);callback(signedIn?null:new Error('signed out'),'Logged in using API key: SECRET_VALUE','');},spawnProcess:(command,argv,options)=>{args=argv;assert.equal(options.shell,false);child=new EventEmitter();child.stdout=new PassThrough();child.stderr=new PassThrough();child.kill=()=>{child.killed=true;};return child;}});
  assert.equal((await auth.check()).signedIn,false);
  assert.equal(auth.start().status,'signing-in');assert.deepEqual(args,['entry.js','login']);
  child.stdout.write('https://auth.example/?secret=SECRET_VALUE\n');
  assert.equal((await auth.check()).status,'signing-in');
  auth.cancel();assert(child.killed);assert.equal(auth.snapshot().status,'cancelled');
  signedIn=true;assert.equal((await auth.check()).signedIn,true);
  assert(!JSON.stringify(auth.snapshot()).includes('SECRET_VALUE'));
  auth.start();child.emit('close',0);await new Promise(resolve=>setImmediate(resolve));assert.equal(auth.snapshot().status,'signed-in');
  assert.equal((await new CodexAuth(null).check()).status,'missing');
});

test('launcher works from an unrelated folder and reuses the same workspace without starting another server',async t=>{
  const temp=await mkdtemp(path.join(os.tmpdir(),'studio-launch-')),workspace=path.join(temp,'workspace'),root=path.join(temp,'Loom');
  await mkdir(workspace);await mkdir(path.join(root,'src'),{recursive:true});await mkdir(path.join(root,'examples'));await writeFile(path.join(root,'library.properties'),'name=Loom\n');
  await writeFile(path.join(workspace,'studio-config.json'),JSON.stringify({loomRoot:root}));
  let requests=0;const server=http.createServer((req,res)=>{requests++;assert.equal(req.url,'/api/studio');res.setHeader('Content-Type','application/json');res.end(JSON.stringify({app:'loom-feature-studio',workspace,root}));});
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  t.after(async()=>{await new Promise(resolve=>server.close(resolve));await rm(temp,{recursive:true,force:true});});
  const result=await promisify(execFile)(process.execPath,[fileURLToPath(new URL('../launch.mjs',import.meta.url)),'--workspace',workspace,'--port',String(server.address().port),'--no-open'],{cwd:temp,timeout:10000,env:{...process.env,LOOM_SOURCE_ROOT:''}});
  assert.match(result.stdout,/already running/);assert.equal(requests,1);assert.equal(result.stderr,'');
});
