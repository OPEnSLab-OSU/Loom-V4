import { spawn } from 'node:child_process';
import { access, mkdir, readFile, writeFile, unlink, readdir, stat } from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import { createHash, randomUUID } from 'node:crypto';
import { compilerDiagnostics } from './compiler-diagnostics.mjs';
import {downloadArduinoCLI} from './cli-download.mjs';

const exists = async p => { try { await access(p); return true; } catch { return false; } };
const digest = value => createHash('sha256').update(JSON.stringify(value)).digest('hex');
async function binaryFiles(directory) {
  const entries=await readdir(directory,{withFileTypes:true});
  return (await Promise.all(entries.filter(entry=>entry.isFile()).map(async entry=>({name:entry.name,bytes:(await stat(path.join(directory,entry.name))).size}))))
    .filter(file=>file.bytes>0).sort((a,b)=>a.name.localeCompare(b.name));
}
export function firmwareKey(assembly, board, fingerprint) { return digest([assembly.files, board, fingerprint]); }
export function codeFingerprint(assembly) { return assembly?digest(assembly.files):null; }
export function missingHeaders(text) { return [...new Set([...text.matchAll(/fatal error:\s*([^:\r\n]+\.h):\s*No such file/gi)].map(m => m[1].trim()))]; }
export async function openFirmwareFolder(directory) {
  const command=process.platform==='win32'?'explorer.exe':process.platform==='darwin'?'open':'xdg-open';
  await new Promise((resolve,reject)=>{
    const child=spawn(command,[directory],{shell:false,windowsHide:true,stdio:'ignore'});
    child.once('error',reject);child.once('spawn',()=>{child.unref();resolve();});
  });
}

export class ArduinoTools {
  constructor({ root, workspace, atomicJSON, execute, fetcher = fetch }) {
    Object.assign(this, { root, workspace, atomicJSON, fetcher });
    this.execute = execute || ((args, onText, timeout) => this.process(args, onText, timeout));
    this.job = null; this.child = null; this.config = {}; this.libraryChoices = new Map(); this.coreChoices = new Map();
  }
  async init() {
    try { this.config = JSON.parse(await readFile(path.join(this.workspace, 'arduino-settings.json'), 'utf8')); } catch (e) { if (e.code !== 'ENOENT') throw e; }
    if (!this.config.configFile) {
      const ide = path.join(os.homedir(), '.arduinoIDE', 'arduino-cli.yaml');
      if (await exists(ide)) this.config.configFile = ide;
      else {
        const local = path.join(this.workspace, 'arduino-cli.yaml');
        let readme = ''; try { readme = await readFile(path.join(this.root, 'README.md'), 'utf8'); } catch {}
        const urls = [...new Set(readme.match(/https:\/\/[^\s`,]+package_[^\s`,]+\.json/g) || [])];
        if (!await exists(local)) await writeFile(local, JSON.stringify({ board_manager: { additional_urls: urls },
          directories: { data: process.platform === 'win32' ? path.join(os.homedir(), 'AppData/Local/Arduino15') : process.platform==='darwin'?path.join(os.homedir(),'Library/Arduino15'):path.join(os.homedir(), '.arduino15'), user: path.join(os.homedir(), 'Documents/Arduino') } }, null, 2));
        this.config.configFile = local;
      }
    }
    await this.findCLI();
  }
  async findCLI() {
    const name = process.platform === 'win32' ? 'arduino-cli.exe' : 'arduino-cli';
    const candidates = [this.config.cliPath, process.env.ARDUINO_CLI_PATH, path.join(this.workspace, 'toolchain', name),
      ...(process.env.PATH || '').split(path.delimiter).filter(Boolean).map(p => path.join(p.replace(/^"|"$/g, ''), name)),
      path.join(os.homedir(), 'AppData/Local/Programs/Arduino IDE/resources/app/lib/backend/resources', name),
      path.join(process.env.ProgramFiles || 'C:/Program Files', 'Arduino IDE/resources/app/lib/backend/resources', name)];
    this.cli = null;
    for (const file of candidates.filter(Boolean)) if (await exists(file)) { this.cli = path.resolve(file); break; }
    return this.cli;
  }
  get busy() { return this.job?.status === 'running'; }
  snapshot() {
    if(!this.job)return null;
    if(this.job.kind==='build'&&(this.diagnosticCache?.id!==this.job.id||this.diagnosticCache?.log!==this.job.log)) {
      this.diagnosticCache={id:this.job.id,log:this.job.log,...compilerDiagnostics(this.job.log,{sketch:this.job.sketch,files:this.job.outputFiles})};
    }
    const snapshot=structuredClone(this.job);
    if(snapshot.kind==='build'){snapshot.diagnostics=structuredClone(this.diagnosticCache.items);snapshot.diagnosticsTruncated=this.diagnosticCache.truncated;}
    return snapshot;
  }
  args(args) { return [...(this.config.configFile ? ['--config-file', this.config.configFile] : []),
    ...(this.config.additionalUrls ? ['--additional-urls', this.config.additionalUrls] : []), ...args]; }
  process(args, onText = () => {}, timeout = 120000) {
    if (!this.cli) throw new Error('Arduino CLI is missing. Get it from Device tools first.');
    return new Promise((resolve, reject) => {
      const child = spawn(this.cli, this.args(args), { windowsHide: true, shell: false, cwd: this.root, env: { ...process.env, ARDUINO_UPDATER_ENABLE_NOTIFICATION: 'false' } });
      if (this.busy) this.child = child;
      let output = '', errors = '';
      child.stdout.on('data', data => {
        const text=data.toString(); output+=text; onText(text);
        // board listall repeats platform metadata; installed ESP32 + SAMD
        // packages can legitimately produce tens of megabytes of JSON.
        if(output.length>64*1024*1024){child.kill();reject(new Error('Arduino CLI returned too much output. Narrow the package search.'));}
      });
      child.stderr.on('data', data => { const text=data.toString(); errors=(errors+text).slice(-150000); onText(text); });
      const timer = setTimeout(() => { child.kill(); reject(new Error('Arduino CLI timed out. Check the log and try again.')); }, timeout);
      child.once('error', error => { clearTimeout(timer); reject(error); });
      child.once('close', code => { clearTimeout(timer); if (this.child === child) this.child = null;
        if (code === 0) resolve(output); else reject(Object.assign(new Error((output+'\n'+errors).trim() || 'Arduino CLI exited with code ' + code), { output:output+'\n'+errors })); });
    });
  }
  async json(args) { const raw = await this.execute([...args, '--json']); try { return JSON.parse(raw); } catch { throw new Error(raw.slice(-1200) || 'Arduino CLI returned no information.'); } }
  async configure(body) {
    if (this.busy) throw new Error('Wait for the current device action.');
    for (const key of ['cliPath', 'configFile']) if (body[key]) {
      if (typeof body[key] !== 'string' || !path.isAbsolute(body[key]) || !await exists(body[key])) throw new Error('Choose an existing absolute ' + key + ' path.');
      if (key === 'cliPath' && !/^arduino-cli(?:\.exe)?$/i.test(path.basename(body[key]))) throw new Error('Choose the arduino-cli executable.');
    }
    if (body.additionalUrls && (typeof body.additionalUrls !== 'string' || body.additionalUrls.length > 4000 || body.additionalUrls.split(',').some(u => { try { return new URL(u.trim()).protocol !== 'https:'; } catch { return true; } }))) throw new Error('Use comma-separated HTTPS board index URLs.');
    this.config = { ...this.config, cliPath: body.cliPath || '', configFile: body.configFile || this.config.configFile, additionalUrls: body.additionalUrls || '' };
    await this.atomicJSON(path.join(this.workspace, 'arduino-settings.json'), this.config); await this.findCLI();
  }
  async status({includePorts=true,force=false}={}) {
    if(includePorts)return this.readStatus({includePorts});
    const key=JSON.stringify([this.config,this.cli]);
    if(!force&&this.buildStatus?.key===key&&Date.now()-this.buildStatus.at<(this.buildStatus.value.available?300000:3000))return {...structuredClone(this.buildStatus.value),job:this.snapshot()};
    if(this.buildStatusPending)return this.buildStatusPending;
    const pending=this.readStatus({includePorts:false}).then(value=>{
      this.buildStatus={key:JSON.stringify([this.config,this.cli]),at:Date.now(),value:structuredClone(value)};return value;
    });
    this.buildStatusPending=pending;
    try{return await pending;}finally{if(this.buildStatusPending===pending)this.buildStatusPending=null;}
  }
  async readStatus({includePorts=true}={}) {
    await this.findCLI();
    const status = { available: !!this.cli, executable: this.cli, config: this.config, boards: [], ports: [], cores: [], warnings: [], job: this.snapshot() };
    if (!this.cli) return status;
    if (this.busy) return { ...status, ...(this.statusCache || {}), job:this.snapshot() };
    const results = await Promise.allSettled([includePorts?this.json(['board', 'list']):Promise.resolve({detected_ports:[]}), this.json(['board', 'listall', 'feather']), this.json(['core', 'list'])]);
    results.forEach(r => { if (r.status === 'rejected') status.warnings.push(r.reason.message.slice(-1600)); });
    const [ports, boards, cores] = results.map(r => r.status === 'fulfilled' ? r.value : {});
    status.ports = (ports.detected_ports || ports || []).filter?.(p=>!p.port?.protocol || p.port.protocol==='serial').map(p => ({ address: p.port?.address || p.address, label: p.port?.label || p.port?.address || p.address, boards: (p.matching_boards || []).map(b => b.name) })) || [];
    status.boards = (boards.boards || []).map(b => ({ name: b.name, fqbn: b.fqbn }));
    status.cores = (cores.platforms || cores || []).map?.(c => ({ id: c.id, name: c.name || c.metadata?.id || c.id, version: c.installed_version || c.installed || c.version || '' })) || [];
    status.warnings.push(...(ports.warnings || [])); this.statusCache=structuredClone(status); return status;
  }
  async search(kind, query) {
    if (this.busy) throw new Error('Wait for the current device action.');
    if (!['core', 'lib'].includes(kind) || typeof query !== 'string' || query.length < 2 || query.length > 120 || /[\r\n\0]/.test(query) || query.startsWith('-')) throw new Error('Enter a board package or library name.');
    const result = await this.json([kind, 'search', query]);
    const choices = kind === 'core' ? (result.platforms || []).map(c => {
      const versions = Object.keys(c.releases || {}), version = c.latest_version || c.latest || versions.sort((a,b) => b.localeCompare(a, undefined, { numeric:true }))[0] || '';
      return { id: c.id, name: c.name || c.releases?.[version]?.name || c.id, version };
    }) : (result.libraries || []).map(l => {
      const version=l.latest_version || l.latest?.version || l.version || Object.keys(l.releases||{}).sort((a,b)=>b.localeCompare(a,undefined,{numeric:true}))[0] || '';
      return { id:l.name,name:l.name,version,description:l.latest?.sentence || l.releases?.[version]?.sentence || l.sentence || '' };
    });
    const map = kind === 'core' ? this.coreChoices : this.libraryChoices;
    for (const c of choices) map.set(c.id, c);
    return choices.slice(0, 60);
  }
  start(kind, operation, projectId = null) {
    if (this.busy) throw new Error('Another device action is running.');
    this.job = { id: randomUUID(), kind, projectId, status: 'running', message: kind + '…', log: '', missingHeaders: [] };
    const job = this.job;
    const abort = this.abort = new AbortController();
    const log = text => { job.log = (job.log + text).slice(-150000); };
    this.work = Promise.resolve().then(() => { abort.signal.throwIfAborted(); return operation(log, abort.signal); }).then(result => {
      if (job.status !== 'running') return;
      Object.assign(job, result || {}, { status: 'succeeded', message: kind === 'flash' ? 'Flash complete.' : kind === 'build' ? 'Build passed. Review the result before flashing.' : 'Tool setup complete.' });
    }).catch(error => { if (job.status !== 'running') return; job.status = 'failed'; job.message = error.message.slice(-1200); if (!job.log.includes(error.message)) log(error.message); job.missingHeaders = missingHeaders(job.log); });
    return this.snapshot();
  }
  cancel() { if (this.busy) { this.job.status = 'cancelled'; this.job.message = 'Device action cancelled.'; this.abort?.abort(); this.child?.kill(); } return this.snapshot(); }
  async changedToolchain() { this.config.toolchainRevision=randomUUID(); await this.atomicJSON(path.join(this.workspace,'arduino-settings.json'),this.config); }
  buildFingerprint(fingerprint) { return fingerprint + digest([this.cli,this.config]); }
  install(kind, id) {
    const choice = (kind === 'core' ? this.coreChoices : this.libraryChoices).get(id);
    if (!['core', 'lib'].includes(kind) || !choice) throw new Error('Search and choose a package from Arduino’s catalog first.');
    return this.start('install', async log => { await this.execute([kind, 'install', choice.id + (choice.version ? '@' + choice.version : '')], log, 900000); await this.changedToolchain(); });
  }
  refreshIndexes() { return this.start('update catalogs', async log => { await this.execute(['core', 'update-index'], log, 300000); await this.execute(['lib', 'update-index'], log, 300000); }); }
  async installCLI() {
    return this.start('get Arduino CLI',async(log,signal)=>{
      this.config.cliPath=await downloadArduinoCLI({directory:path.join(this.workspace,'toolchain'),fetcher:this.fetcher,log,signal});
      await this.atomicJSON(path.join(this.workspace,'arduino-settings.json'),this.config);await this.findCLI();
      log('Arduino CLI is ready. Board packages install only when selected.\n');
    });
  }
  async build({ assembly, board, fingerprint, projectId }) {
    if(typeof board!=='string'||!board.trim())throw new Error('Choose an installed board. Use Setup to install a missing board package.');
    const status = await this.status({includePorts:false});
    if (!status.boards.some(b => b.fqbn === board)) throw new Error('Choose an installed board. Use Setup to install a missing board package.');
    const key = firmwareKey(assembly, board, this.buildFingerprint(fingerprint)), folder = path.join(this.workspace, 'firmware-builds', key);
    const primary = assembly.files.find(f => f.name.endsWith('.ino'));
    if (!primary) throw new Error('No Arduino sketch is available.');
    const sketch = path.join(folder, primary.name.slice(0,-4)), binaries = path.join(folder, 'binaries'), cache = path.join(folder,'cache');
    this.start('build', async log => {
      await mkdir(sketch,{recursive:true}); await mkdir(binaries,{recursive:true});
      await unlink(path.join(folder,'success.json')).catch(e=>{if(e.code!=='ENOENT')throw e;});
      for (const file of assembly.files) await writeFile(path.join(sketch,file.name),file.content);
      await this.execute(['compile','--fqbn',board,'--library',this.root,'--build-path',cache,'--output-dir',binaries,sketch],log,900000);
      const files=await binaryFiles(binaries),builtAt=new Date().toISOString();
      if(!files.length)throw new Error('The compiler returned without writing build files. Review Full tool output, then build again.');
      await this.atomicJSON(path.join(folder,'success.json'),{key,board,sketch,binaries,projectId,builtAt,files});
      return { buildKey:key,board,builtAt };
    },projectId);
    Object.assign(this.job,{board,filesFingerprint:codeFingerprint(assembly),sourceFingerprint:fingerprint,sketch,
      outputFiles:assembly.files.map(file=>({name:file.name,lineCount:file.content.split('\n').length}))});
    this.diagnosticCache=null;
    return this.snapshot();
  }
  async built(assembly, board, fingerprint) {
    if (!assembly || !board) return null;
    try {
      const key=firmwareKey(assembly,board,this.buildFingerprint(fingerprint)),folder=path.join(this.workspace,'firmware-builds',key);
      const saved=JSON.parse(await readFile(path.join(folder,'success.json'),'utf8'));
      if(saved?.key!==key||saved.board!==board)return null;
      const binaries=path.join(folder,'binaries'),files=await binaryFiles(binaries);
      if(!files.length||saved.files!==undefined&&(!Array.isArray(saved.files)||!saved.files.length||saved.files.some(file=>!files.some(current=>current.name===file?.name&&current.bytes===file.bytes))))return null;
      const primary=assembly.files.find(file=>file.name.endsWith('.ino'));
      if(!primary)return null;
      return {...saved,key,board,binaries,sketch:path.join(folder,primary.name.slice(0,-4)),builtAt:Number.isFinite(Date.parse(saved.builtAt))?saved.builtAt:null,
        files,totalBytes:files.reduce((total,file)=>total+file.bytes,0)};
    } catch(e) { if(e.code!=='ENOENT'&&!(e instanceof SyntaxError))throw e;return null; }
  }
  async flash({ assembly, board, fingerprint, port, projectId, buildKey }) {
    const build = await this.built(assembly,board,fingerprint);
    if (!build || build.key !== buildKey) throw new Error('Build the current code for this board successfully before flashing.');
    const status = await this.status();
    if (!status.ports.some(p=>p.address===port)) throw new Error('Select a currently connected COM port, then try again.');
    this.start('flash',async log=>{await this.execute(['upload','--fqbn',board,'--port',port,'--input-dir',build.binaries,build.sketch],log,180000);},projectId);
    Object.assign(this.job,{board,port,buildKey,filesFingerprint:codeFingerprint(assembly),sourceFingerprint:fingerprint});
    return this.snapshot();
  }
}
