import {test} from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,writeFile,readFile,rm} from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import {ArduinoTools} from '../arduino.mjs';
import {atomicJSON} from '../server.mjs';

test('build records describe existing output, survive restart and stop matching when files disappear or change size',async t=>{
  const workspace=await mkdtemp(path.join(os.tmpdir(),'studio-build-result-'));t.after(()=>rm(workspace,{recursive:true,force:true}));
  const board='test:samd:feather',assembly={files:[{name:'App.ino',content:'void setup(){}\nvoid loop(){}'}]},fingerprint='source';
  let produce=true;const calls=[];
  const execute=async args=>{
    calls.push(args);
    if(args[0]==='board'&&args[1]==='listall')return JSON.stringify({boards:[{name:'Loomified Feather M0',fqbn:board}]});
    if(args[0]==='core')return JSON.stringify({platforms:[]});
    if(args[0]==='compile'&&produce){const out=args[args.indexOf('--output-dir')+1];await writeFile(path.join(out,'App.bin'),'0123456789abcdef');await writeFile(path.join(out,'App.elf'),'debug');}
    return '';
  };
  const tools=new ArduinoTools({root:workspace,workspace,atomicJSON,execute});await tools.init();
  const cli=path.join(workspace,'arduino-cli.exe');await writeFile(cli,'not executed');await tools.configure({cliPath:cli});
  await tools.build({assembly,board,fingerprint,projectId:'project'});await tools.work;
  assert.equal(tools.job.status,'succeeded');let result=await tools.built(assembly,board,fingerprint);
  assert.deepEqual(result.files,[{name:'App.bin',bytes:16},{name:'App.elf',bytes:5}]);assert.equal(result.totalBytes,21);assert.ok(Number.isFinite(Date.parse(result.builtAt)));
  const restarted=new ArduinoTools({root:workspace,workspace,atomicJSON,execute});await restarted.init();await restarted.findCLI();
  assert.deepEqual(await restarted.built(assembly,board,fingerprint),result);
  await rm(path.join(result.binaries,'App.elf'));assert.equal(await restarted.built(assembly,board,fingerprint),null);
  await assert.rejects(restarted.flash({assembly,board,fingerprint,port:'COM1',buildKey:result.key}),/Build the current code/);
  await writeFile(path.join(result.binaries,'App.elf'),'debug');await writeFile(path.join(result.binaries,'App.bin'),'changed');
  assert.equal(await restarted.built(assembly,board,fingerprint),null);
  await writeFile(path.join(result.binaries,'App.bin'),'0123456789abcdef');assert.ok(await restarted.built(assembly,board,fingerprint));
  const marker=path.join(workspace,'firmware-builds',result.key,'success.json'),saved=JSON.parse(await readFile(marker,'utf8'));
  await atomicJSON(marker,{...saved,binaries:'outside',sketch:'outside'});const bounded=await restarted.built(assembly,board,fingerprint);assert.equal(bounded.binaries,result.binaries);assert.equal(bounded.sketch,result.sketch);
  await writeFile(marker,'{broken');assert.equal(await restarted.built(assembly,board,fingerprint),null,'damaged metadata offers a rebuild instead of breaking the menu');
  delete saved.files;delete saved.builtAt;await atomicJSON(marker,saved);
  const legacy=await restarted.built(assembly,board,fingerprint);assert.equal(legacy.builtAt,null);assert.equal(legacy.files.length,2,'existing builds stay usable when their files exist');
  produce=false;const empty={files:[{...assembly.files[0],content:assembly.files[0].content+'\n// new code'}]};
  await restarted.build({assembly:empty,board,fingerprint,projectId:'project'});await restarted.work;
  assert.equal(restarted.job.status,'failed');assert.match(restarted.job.message,/without writing build files/);assert.equal(await restarted.built(empty,board,fingerprint),null);
  assert.ok(!calls.some(args=>args[0]==='upload'),'inspecting and recovering builds never uploads');
});
