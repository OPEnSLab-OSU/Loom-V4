'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {main, traceProperties, choosePort, parseArgs, compilerPreprocessArgs} = require('../tools/sketch-launcher/loom-build.cjs');

function prepare(args) {
  const build = args[args.indexOf('--build-path') + 1];
  const cpp = path.join(build, 'LoomLauncherBuild.ino.cpp');
  fs.writeFileSync(cpp, fs.readFileSync(path.join(args.at(-1), 'LoomLauncherBuild.ino')));
  fs.writeFileSync(path.join(build, 'compile_commands.json'), JSON.stringify([{file: cpp, directory: build,
    arguments: ['fake-cxx', '-c', '-MMD', '-o', cpp + '.o', cpp]}]));
  return {status: 0};
}
function evaluated(args, mode) {
  assert.ok(args.includes('-E'));
  assert.ok(!args.includes('-o') && !args.includes('-c') && !args.includes('-MMD'));
  const cpp = args.find(x => x.endsWith('.ino.cpp'));
  const marker = fs.readFileSync(cpp, 'utf8').match(/loomLauncherProbe_[a-f0-9]+/)[0];
  return {status: 0, stdout: `const char* const ${marker} = "${mode}";`};
}
assert.deepEqual(compilerPreprocessArgs({arguments: ['cxx', '-c', '-o', 'a.o', '-MMD', '-MF', 'a.d', '-Iwith spaces', 'a.cpp']}), ['-Iwith spaces', 'a.cpp', '-E']);

assert.equal(traceProperties('off').length, 1);
assert.match(traceProperties('off')[0], /LOOM_TRACE=0.*LOOM_TRACE_HEAP=0.*HOOKS=0/);
assert.equal(traceProperties('calls').length, 1);
assert.match(traceProperties('heap')[1], /--wrap=_malloc_r/);
assert.throws(() => parseArgs(['--port']), /Missing value/);
assert.throws(() => choosePort([{port: {address: 'COM5'}}, {port: {address: 'COM6'}}], 'loom4:samd:adafruit_feather_m0'), /ambiguous/);
assert.equal(choosePort([{port: {address: 'COM5'}, matching_boards: [{fqbn: 'loom4:samd:adafruit_feather_m0'}]}, {port: {address: 'COM6'}}], 'loom4:samd:adafruit_feather_m0:debug=off'), 'COM5');

const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'loom launcher tests '));
const source = path.join(temp, 'Sketch With Spaces');
fs.mkdirSync(source);
const sketch = '#define LOOM_TRACE (1)\n#define LOOM_TRACE_HEAP LOOM_TRACE\nvoid setup() {}\nvoid loop() {}\n';
fs.writeFileSync(path.join(source, 'Sketch With Spaces.ino'), sketch);
fs.writeFileSync(path.join(source, 'loom-build.json'), JSON.stringify({fqbn: 'loom4:samd:adafruit_feather_m0', arduinoCli: 'CLI with spaces', port: 'COM5'}));
let count = 0;
for (const mode of ['off', 'calls', 'heap']) {
  const calls = [];
  const result = main(['--sketch', source], {run(exe, args) {
    calls.push(args);
    if (exe === 'fake-cxx') return evaluated(args, mode);
    assert.equal(exe, 'CLI with spaces');
    if (args[0] === 'version') return {status: 0, stdout: 'fake CLI'};
    if (args.includes('--only-compilation-database')) return prepare(args);
    if (args[0] === 'compile') {
      assert.equal(fs.readFileSync(path.join(args.at(-1), 'LoomLauncherBuild.ino'), 'utf8'), sketch);
      assert.equal(args.some(x => x.includes('--wrap=')), mode === 'heap');
      for (const property of traceProperties(mode)) assert.ok(args.includes(property));
      return {status: 0, stdout: 'Sketch uses 123 bytes'};
    }
    assert.equal(args[0], 'upload');
    assert.equal(args[args.indexOf('--input-dir') + 1], calls.find(x => x[0] === 'compile' && !x.includes('--only-compilation-database'))[calls.find(x => x[0] === 'compile' && !x.includes('--only-compilation-database')).indexOf('--build-path') + 1]);
    return {status: 0, stdout: 'uploaded'};
  }});
  assert.equal(result.traceMode, mode);
  assert.equal(result.uploaded, true);
  assert.equal(fs.readFileSync(path.join(source, 'Sketch With Spaces.ino'), 'utf8'), sketch);
  ++count;
}
for (const failAt of ['prepare', 'preprocess', 'compile']) {
  let uploads = 0;
  assert.throws(() => main(['--sketch', source], {run(exe, args) {
    if (args[0] === 'version') return {status: 0};
    if (args[0] === 'upload') ++uploads;
    if (args.includes('--only-compilation-database') && failAt !== 'prepare') return prepare(args);
    if (exe === 'fake-cxx' && failAt === 'compile') return evaluated(args, 'heap');
    return {status: 1, stderr: 'Missing.h: No such file or directory'};
  }}), /No firmware was uploaded/);
  assert.equal(uploads, 0);
  ++count;
}
assert.throws(() => main(['--sketch', source, '--check'], {run: () => ({status: 1})}), /docs.arduino.cc\/arduino-cli\/installation/);
let uploads = 0;
main(['--sketch', source, '--no-upload'], {run(exe, args) {
  if (args[0] === 'version') return {status: 0};
  assert.notEqual(args[0], 'board'); // Compile-only never needs a connected board.
  if (args[0] === 'upload') ++uploads;
  if (args.includes('--only-compilation-database')) return prepare(args);
  if (exe === 'fake-cxx') return evaluated(args, 'off');
  return {status: 0, stdout: 'Sketch uses 123 bytes'};
}});
assert.equal(uploads, 0);
fs.writeFileSync(path.join(source, 'loom-build.json'), JSON.stringify({fqbn: 'loom4:samd:adafruit_feather_m0', arduinoCli: 'CLI with spaces', port: 'auto'}));
assert.throws(() => main(['--sketch', source], {run(exe, args) {
  if (args[0] === 'version') return {status: 0};
  assert.equal(args[0], 'board');
  return {status: 0, stdout: JSON.stringify({detected_ports: [{port: {address: 'COM5'}}, {port: {address: 'COM6'}}]})};
}}), /No firmware was uploaded/);
for (const file of ['build-upload.bat', 'build-upload.sh', 'loom-build.cjs']) {
  assert.equal(fs.readFileSync(path.join(__dirname, '../tools/sketch-launcher', file), 'utf8'),
    fs.readFileSync(path.join(__dirname, '../examples/Lab Examples/Wisp/WispV2_Deploy_2026_debug', file), 'utf8'), `Source sketch launcher drifted: ${file}`);
}
console.log(`Launcher tests passed: ${count + 2} build/upload scenarios plus argument, port, dependency and bundle checks. Artifacts: ${temp}`);
