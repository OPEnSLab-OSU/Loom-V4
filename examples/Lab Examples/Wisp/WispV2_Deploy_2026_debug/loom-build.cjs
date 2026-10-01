#!/usr/bin/env node
'use strict';
// No npm dependencies. Pass arguments directly to executables, never through a shell.
const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const crypto = require('node:crypto');
const {spawnSync} = require('node:child_process');
const CLI_URL = 'https://docs.arduino.cc/arduino-cli/installation/';
const LOOM_URL = 'https://github.com/OPEnSLab-OSU/Loom-V4#install';
const WRAPS = ['malloc', 'calloc', 'realloc', 'free', '_malloc_r', '_calloc_r', '_realloc_r', '_free_r'];

function traceProperties(mode) {
  const trace = mode === 'off' ? 0 : 1;
  const heap = mode === 'heap' ? 1 : 0;
  const properties = [`compiler.cpp.extra_flags=-DLOOM_TRACE=${trace} -DLOOM_TRACE_HEAP=${heap} -DLOOM_TRACE_LINKER_HEAP_HOOKS=${heap}`];
  if (heap) properties.push('compiler.c.elf.extra_flags=-Wl,' + WRAPS.map(x => '--wrap=' + x).join(','));
  return properties;
}

function parseArgs(args) {
  const result = {};
  for (let i = 0; i < args.length; ++i) {
    const arg = args[i];
    if (['--no-upload', '--build-only', '--check', '--help', '--upload'].includes(arg)) result[arg.slice(2)] = true;
    else if (['--sketch', '--config', '--fqbn', '--port', '--cli'].includes(arg)) {
      if (!args[i + 1] || args[i + 1].startsWith('--')) throw new Error(`Missing value for ${arg}`);
      result[arg.slice(2)] = args[++i];
    } else throw new Error(`Unknown option: ${arg}. Use --help.`);
  }
  return result;
}

function help() {
  console.log(`Loom build and upload: reads LOOM_TRACE / LOOM_TRACE_HEAP from your sketch.
  --no-upload / --build-only  Compile and keep binaries; do not flash
  --check                    Check tools and settings without compiling/flashing
  --sketch DIRECTORY         Sketch folder (default: folder beside this script)
  --config FILE              Settings JSON (default: sketch/loom-build.json)
  --fqbn BOARD               Override configured board
  --port COM5                Override configured upload port
  --cli PATH                 Arduino CLI executable
  --upload                   Override upload:false in settings
  --help                     Show this help
Without --no-upload, a successful build is uploaded unless settings say upload:false.
Install Node.js: https://nodejs.org/en/download
Install Arduino CLI: ${CLI_URL}
Loom board and dependencies: ${LOOM_URL}`);
}

function findCli(explicit, run) {
  const candidates = [explicit, process.env.ARDUINO_CLI, 'arduino-cli'];
  if (process.platform === 'win32') candidates.push(path.join(process.env.LOCALAPPDATA || '', 'Programs/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe'));
  candidates.push('/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli',
    path.join(os.homedir(), 'Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli'),
    '/opt/arduino-ide/resources/app/lib/backend/resources/arduino-cli');
  for (const executable of candidates.filter(Boolean)) {
    const r = run(executable, ['version']);
    if (!r.error && r.status === 0) return executable;
    if (explicit) break; // Do not silently replace a configured tool.
  }
  throw new Error(`Arduino CLI was not found. Install it from ${CLI_URL}\nArduino IDE also includes it: https://www.arduino.cc/en/software\nIf needed, set "arduinoCli" in loom-build.json to its executable path.`);
}

function choosePort(ports, fqbn) {
  const boardId = fqbn.split(':')[2];
  const matches = ports.filter(p => (p.matching_boards || []).some(b => b.fqbn === fqbn || b.fqbn?.split(':')[2] === boardId));
  if (matches.length === 1) return matches[0].port.address;
  if (matches.length === 0 && ports.length === 1) return ports[0].port.address;
  throw new Error(`Upload port is ambiguous or absent. Set "port" in loom-build.json or use --port.\nDetected ports: ${ports.map(p => p.port.address).join(', ') || 'none'}. No firmware was uploaded.`);
}

function stageSketch(source, stage) {
  const main = path.join(source, path.basename(source) + '.ino');
  if (!fs.existsSync(main)) throw new Error(`Missing main sketch: ${main}`);
  fs.mkdirSync(stage, {recursive: true});
  for (const item of fs.readdirSync(source, {withFileTypes: true})) {
    if (item.isFile() && /\.(ino|cpp|c|h|hpp|hh|S|s|inc|tpp)$/i.test(item.name)) {
      fs.copyFileSync(path.join(source, item.name), path.join(stage, path.join(source, item.name) === main ? 'LoomLauncherBuild.ino' : item.name));
    } else if (item.isDirectory() && ['src', 'data'].includes(item.name)) fs.cpSync(path.join(source, item.name), path.join(stage, item.name), {recursive: true});
  }
  return path.join(stage, 'LoomLauncherBuild.ino');
}

function buildError(label, result, log) {
  const tail = ((result.stdout || '') + '\n' + (result.stderr || '')).slice(-6000);
  return new Error(`${label} failed (exit ${result.status}). ${result.error?.message || ''}\n${tail}\nFull log: ${log}\nMissing board or library? Loom installation: ${LOOM_URL}\nUse Loom's supplied/patched dependencies. Library Manager instructions: https://docs.arduino.cc/software/ide-v2/tutorials/ide-v2-installing-a-library/\nNo firmware was uploaded.`);
}

function compilerPreprocessArgs(entry) {
  if (!Array.isArray(entry.arguments) || !entry.arguments.length) throw new Error('Arduino CLI did not provide compiler argument arrays. Update Arduino CLI: ' + CLI_URL);
  const args = [];
  for (let i = 1; i < entry.arguments.length; ++i) {
    const arg = entry.arguments[i];
    if (['-o', '-MF', '-MT', '-MQ'].includes(arg)) { ++i; continue; }
    if (['-c', '-MMD', '-MD', '-MP'].includes(arg) || /^-M[FQT].+/.test(arg)) continue;
    args.push(arg);
  }
  args.push('-E');
  return args;
}

function main(argv = process.argv.slice(2), options = {}) {
  const run = options.run || ((exe, args, extra = {}) => spawnSync(exe, args, {encoding: 'utf8', maxBuffer: 128 * 1024 * 1024, windowsHide: true, ...extra}));
  const args = parseArgs(argv);
  if (args.help) { help(); return; }
  if (Number(process.versions.node.split('.')[0]) < 18) throw new Error('Node.js 18 or newer is required: https://nodejs.org/en/download');
  const source = path.resolve(args.sketch || __dirname);
  const configFile = path.resolve(args.config || path.join(source, 'loom-build.json'));
  const configDir = path.dirname(configFile);
  const config = fs.existsSync(configFile) ? JSON.parse(fs.readFileSync(configFile, 'utf8')) : {};
  const fqbn = args.fqbn || config.fqbn;
  if (!fqbn) throw new Error(`Choose a board: set "fqbn" in ${configFile} or use --fqbn.\nFor Wisp: loom4:samd:adafruit_feather_m0:usbstack=arduino,debug=off\nBoard installation: ${LOOM_URL}`);
  const cli = findCli(args.cli || config.arduinoCli, run);
  const upload = !args['no-upload'] && !args['build-only'] && (args.upload || config.upload !== false);
  let port = args.port || config.port || 'auto';
  if (upload && !args.check && port === 'auto') {
    const detected = run(cli, ['board', 'list', '--format', 'json']);
    if (detected.status !== 0) throw new Error('Could not list upload ports. Set "port" in loom-build.json. No firmware was uploaded.');
    port = choosePort(JSON.parse(detected.stdout).detected_ports || [], fqbn);
  }
  console.log(`Sketch: ${source}\nBoard: ${fqbn}\nArduino CLI: ${cli}\nUpload: ${upload ? port : 'disabled (compile only)'}`);
  if (args.check) { console.log('Tool/settings check complete. No compile or upload was performed.'); return; }
  // Installed library examples have long paths; the older SAMD GCC cannot open
  // dependency files beneath those paths even when Windows long paths are enabled.
  const outputBase = config.outputDirectory ? path.resolve(configDir, config.outputDirectory) : path.join(os.tmpdir(), 'loom-build');
  const id = new Date().toISOString().replace(/[:.]/g, '-') + '-' + crypto.randomBytes(4).toString('hex');
  const root = path.join(outputBase, id);
  const stage = path.join(root, 'LoomLauncherBuild');
  const build = path.join(root, 'build');
  fs.mkdirSync(build, {recursive: true});
  console.log(`Logs and matching ELF/binaries: ${root}`);
  const stagedMain = stageSketch(source, stage);
  const original = fs.readFileSync(stagedMain);
  const marker = 'loomLauncherProbe_' + crypto.randomBytes(8).toString('hex');
  const probe = `\n#if defined(LOOM_TRACE) && LOOM_TRACE\n#if defined(LOOM_TRACE_HEAP) && LOOM_TRACE_HEAP\nconst char* const ${marker} = "heap";\n#else\nconst char* const ${marker} = "calls";\n#endif\n#else\nconst char* const ${marker} = "off";\n#endif\n`;
  const common = ['compile', '--fqbn', fqbn, '--build-path', build, '--jobs', String(config.jobs || 4)];
  const libraries = config.libraryPaths || [];
  const userLibraries = path.join(os.homedir(), 'Documents/Arduino/libraries');
  if (fs.existsSync(userLibraries)) libraries.push(userLibraries);
  for (const library of new Set(libraries.map(x => path.resolve(configDir, x)))) common.push('--libraries', library);
  if (config.additionalUrls?.length) common.push('--additional-urls', config.additionalUrls.join(','));
  let preprocessed;
  console.log('Reading trace flags with the board compiler...');
  try {
    fs.appendFileSync(stagedMain, probe);
    const prepared = run(cli, [...common, '--only-compilation-database', stage]);
    const prepareLog = path.join(root, 'prepare.log');
    fs.writeFileSync(prepareLog, (prepared.stdout || '') + '\n' + (prepared.stderr || ''));
    if (prepared.status !== 0) throw buildError('Sketch preparation', prepared, prepareLog);
    const databasePath = path.join(build, 'compile_commands.json');
    if (!fs.existsSync(databasePath)) throw new Error(`Missing compiler-command database. Update Arduino CLI: ${CLI_URL}. No firmware was uploaded.`);
    const database = JSON.parse(fs.readFileSync(databasePath, 'utf8'));
    const entry = database.find(x => /LoomLauncherBuild\.ino\.cpp$/i.test(x.file));
    if (!entry) throw new Error('Cannot find sketch compiler command. No firmware was uploaded.');
    preprocessed = run(entry.arguments[0], compilerPreprocessArgs(entry), {cwd: entry.directory});
  } finally { fs.writeFileSync(stagedMain, original); }
  const probeLog = path.join(root, 'preprocess.log');
  // Do not save the expanded source: included private configuration can contain credentials.
  fs.writeFileSync(probeLog, preprocessed.stderr || '');
  if (preprocessed.status !== 0) throw buildError('Sketch preprocessing', preprocessed, probeLog);
  const modes = [...(preprocessed.stdout || '').matchAll(new RegExp(`${marker}\\s*=\\s*"(heap|calls|off)"`, 'g'))];
  if (modes.length !== 1) throw new Error(`Cannot determine sketch trace flags from compiler output. See ${probeLog}. No firmware was uploaded.`);
  const mode = modes[0][1];
  fs.appendFileSync(probeLog, '\nEvaluated sketch trace mode: ' + mode + '\n');
  const compileArgs = [...common, '--warnings', 'all'];
  for (const property of traceProperties(mode)) compileArgs.push('--build-property', property);
  compileArgs.push(stage);
  const manifest = {sketch: source, fqbn, port: upload ? port : null, traceMode: mode, arduinoCli: cli, arguments: compileArgs, buildDirectory: build, uploaded: false};
  const manifestPath = path.join(root, 'loom-build.json');
  fs.writeFileSync(manifestPath, JSON.stringify(manifest, null, 2));
  console.log(`Trace mode: ${mode}${mode === 'heap' ? ' (allocation linker hooks enabled)' : ''}. Building...`);
  const compiled = run(cli, compileArgs);
  const compileLog = path.join(root, 'compile.log');
  fs.writeFileSync(compileLog, (compiled.stdout || '') + '\n' + (compiled.stderr || ''));
  if (compiled.status !== 0) throw buildError('Compilation', compiled, compileLog);
  const summaries = ((compiled.stdout || '') + '\n' + (compiled.stderr || '')).split(/\r?\n/).filter(x => /Sketch uses|Global variables use/.test(x));
  console.log(summaries.join('\n') || 'Compilation succeeded.');
  fs.writeFileSync(path.join(outputBase, 'latest.json'), JSON.stringify({...manifest, manifestPath}, null, 2));
  if (upload) {
    console.log(`Uploading the compiled binary to ${port}...`);
    const flashed = run(cli, ['upload', '--fqbn', fqbn, '--port', port, '--input-dir', build]);
    const uploadLog = path.join(root, 'upload.log');
    fs.writeFileSync(uploadLog, (flashed.stdout || '') + '\n' + (flashed.stderr || ''));
    if (flashed.status !== 0) throw new Error(`Upload failed. See ${uploadLog}. ${flashed.error?.message || flashed.stderr || ''}`);
    manifest.uploaded = true;
    fs.writeFileSync(manifestPath, JSON.stringify(manifest, null, 2));
    fs.writeFileSync(path.join(outputBase, 'latest.json'), JSON.stringify({...manifest, manifestPath}, null, 2));
    console.log('Upload complete.');
  } else console.log('Compile only: no firmware was uploaded.');
  return manifest;
}

module.exports = {main, traceProperties, choosePort, parseArgs, stageSketch, compilerPreprocessArgs};
if (require.main === module) {
  try { main(); } catch (error) { console.error(error.message); process.exitCode = 1; }
}
