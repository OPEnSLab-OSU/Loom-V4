import {checkout} from './checkout.mjs';
import { test } from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import os from 'node:os';
import { mkdtemp, mkdir, writeFile, readFile, rm } from 'node:fs/promises';
import { splitParameters, extractHeader, discover } from '../adapters/loom.mjs';
import { revisionOf, validateDesign, validateAssembly } from '../core.mjs';
import { startStudio, atomicJSON } from '../server.mjs';
import { cliArguments, findCodex, listModels } from '../codex.mjs';
import { fileURLToPath } from 'node:url';
import { reconstructExample } from '../examples.mjs';
import { stripComments } from '../adapters/cpp.mjs';

const baseDesign = () => ({
  schemaVersion: 1, adapter: 'loom', target: 'Feather M0', name: 'Station', description: 'Make a station',
  blocks: [{ id: 'sensor-1', catalogId: 'class:Loom_Future', label: 'Future sensor', count: 3, settings: {}, notes: '', position: { x: 20, y: 80 } }]
});
const catalog = { fingerprint: 'current-source' };
const result = d => ({
  schemaVersion: 1, designRevision: revisionOf(d), sourceFingerprint: catalog.fingerprint,
  summary: 'Three sensors', nodes: [{ id: 'sensor', label: 'Sensors', category: 'sensors', added: false, reason: 'Requested', requestIds: ['sensor-1'] }],
  edges: [], files: [{ name: 'FeatureApplication.ino', content: 'void setup() {}\nvoid loop() {}\n' }], decisions: [], warnings: [], validation: 'Envelope only'
});

test('discovery supports multiline constructors, dependency parameters and nested defaults', () => {
  assert.deepEqual(splitParameters('Manager &man, std::array<int, 2> values = {1, 2}, const char *s = "a,b"'), ['Manager &man', 'std::array<int, 2> values = {1, 2}', 'const char *s = "a,b"']);
  const blocks = extractHeader('/** Future sensor. */\nclass Loom_Future : public Module {\npublic:\nLoom_Future(Manager &man,\n int addr = 0x44, bool enabled = true);\nvoid setRate(int rate);\n};', 'Sensors/I2C/Loom_Future.h');
  assert.equal(blocks[0].className, 'Loom_Future');
  assert.equal(blocks[0].constructors[0].parameters[0].dependency, true);
  assert.equal(blocks[0].constructors[0].parameters[2].default, true);
  assert.equal(blocks[0].category, 'sensors');
  assert.match(blocks[0].description, /Future sensor/);
});
test('design revision changes for behavior but ignores block movement', () => {
  const d = baseDesign(), rev = revisionOf(d);
  d.blocks[0].position.x += 100;
  assert.equal(revisionOf(d), rev);
  d.blocks[0].count++;
  assert.notEqual(revisionOf(d), rev);
});
test('design rejects duplicate IDs, impossible instance counts and invalid positions', () => {
  const d = baseDesign(); d.blocks.push(structuredClone(d.blocks[0]));
  assert.throws(() => validateDesign(d), /unique/);
  d.blocks.pop(); d.blocks[0].count = 0;
  assert.throws(() => validateDesign(d), /1–64/);
  d.blocks[0].count = 1; d.blocks[0].position.x = -1;
  assert.throws(() => validateDesign(d), /position/);
});
test('assembly validates current design and source, never stale results', () => {
  const d = baseDesign(), a = result(d);
  assert.equal(validateAssembly(a, d, catalog), a);
  d.description += ' and sleep';
  assert.throws(() => validateAssembly(a, d, catalog), /different design/);
  a.designRevision = revisionOf(d);
  assert.throws(() => validateAssembly(a, d, { fingerprint: 'new-source' }), /different source/);
});
test('assembly cannot omit a request, invent references or add unexplained components', () => {
  const d = baseDesign(), a = result(d);
  a.nodes[0].requestIds = [];
  assert.throws(() => validateAssembly(a, d, catalog), /omitted/);
  a.nodes[0].requestIds = ['sensor-1']; a.nodes[0].added = true; a.nodes[0].reason = '';
  assert.throws(() => validateAssembly(a, d, catalog), /explanation/);
  a.nodes[0].reason = 'Needed'; a.edges.push({ from: 'sensor', to: 'missing', kind: 'data', label: '' });
  assert.throws(() => validateAssembly(a, d, catalog), /connection/);
});
test('assembly refuses path traversal and case-insensitive filename collisions', () => {
  const d = baseDesign(), a = result(d);
  a.files.push({ name: '../../outside.h', content: 'bad' });
  assert.throws(() => validateAssembly(a, d, catalog), /filename/);
  a.files[1].name = 'featureapplication.ino';
  assert.throws(() => validateAssembly(a, d, catalog), /filename/);
});
test('CLI arguments preserve paths and model as arguments without a shell', () => {
  const args = cliArguments({ root: 'C:/repo with spaces', model: 'my-model', sandbox: 'read-only', schemaFile: 'C:/schema file.json', resultFile: 'C:/result file.json' });
  assert.equal(args[args.indexOf('--cd') + 1], 'C:/repo with spaces');
  assert.equal(args[args.indexOf('--model') + 1], 'my-model');
  assert.ok(!args.some(a => a.includes('dangerously')));
  assert.throws(() => cliArguments({ model: '--bad; injected', root: 'repo' }), /model ID/);
  assert.throws(() => cliArguments({ sandbox: 'danger-full-access', root: 'repo' }), /permission/);
  const defaults = cliArguments({ root: 'repo', schemaFile: 'schema', resultFile: 'result' });
  assert.equal(defaults[defaults.indexOf('--model') + 1], 'gpt-6.1-sol');
  assert.ok(defaults.includes('model_reasoning_effort="high"'));
});
test('available models and their reasoning choices come from the CLI RPC', async () => {
  const exe = await findCodex(fileURLToPath(new URL('fixtures/fake-models.mjs', import.meta.url)));
  const models = await listModels(exe, process.cwd());
  assert.equal(models[0].id, 'gpt-6.1-sol');
  assert.deepEqual(models[0].efforts, ['medium', 'high']);
  assert.deepEqual(models[1].efforts, ['low']);
});
test('example reconstruction preserves custom behavior and excludes commented alternatives', () => {
  const header = 'class Loom_Test { public: Loom_Test(Manager &man, int address = 4); void publish(); };';
  const c = { adapter: { id: 'loom', target: 'Feather M0' }, fingerprint: 'test', blocks: extractHeader(header, 'Sensors/Test.h') };
  const source = '// Loom_Test inactive(manager, 5);\nLoom_Test sensor(manager, 4);\nfloat calibrate(float value) { return value * 2; }\nvoid setup() {}\nvoid loop() { sensor.publish(); }\n';
  const r = reconstructExample(source, 'examples/Test/Test.ino', c);
  assert.equal(r.design.blocks.filter(b => b.catalogId === 'class:Loom_Test').length, 1);
  assert.ok(r.design.blocks.some(b => b.catalogId === 'intent:custom' && b.settings.requirement.includes('calibrate')));
  assert.equal(r.assembly.files[0].content, source);
  assert.ok(r.assembly.nodes.some(n => n.label === 'Publish packet'));
  validateAssembly(r.assembly, r.design, c);
  assert.equal(stripComments('const char *url = "https://example.test"; // comment\n'), 'const char *url = "https://example.test";           \n');
});
test('every example in the selected Loom checkout reconstructs a covered graph', async () => {
  const root = checkout;
  const c = await discover(root);
  assert.ok(c.examplePaths.length > 0);
  for (const file of c.examplePaths) {
    const r = reconstructExample(await readFile(path.join(root, file), 'utf8'), file, c);
    validateDesign(r.design);
    validateAssembly(r.assembly, r.design, c);
    assert.equal(r.assembly.files[0].content, await readFile(path.join(root, file), 'utf8'), file);
    const totalLines=r.assembly.files[0].content.split('\n').length;
    for(const [id,location] of Object.entries(r.locations.nodes))assert.ok(Number.isInteger(location.line)&&location.line>0&&location.line<=totalLines,file+' · '+id+' source location');
    for(const routine of r.assembly.routines)assert.equal(new Set(routine.nodeIds).size,routine.nodeIds.length,file+' · '+routine.name+' unique source steps');
  }
});
test('fresh checkout discovery and the complete Desktop file bridge', async t => {
  const temp = await mkdtemp(path.join(os.tmpdir(), 'feature-studio-test-'));
  // All cleanup targets are direct descendants of this freshly created temp folder.
  t.after(() => rm(temp, { recursive: true, force: true }));
  const root = path.join(temp, 'repo'), workspace = path.join(temp, 'bridge');
  await mkdir(path.join(root, 'src', 'Sensors'), { recursive: true });
  await mkdir(path.join(root, 'examples', 'Future'), { recursive: true });
  const header = path.join(root, 'src', 'Sensors', 'Future.h');
  await writeFile(header, '/** Future branch sensor. */\nclass Loom_Future : public Module { public: Loom_Future(Manager &man, int samples = 7); };');
  await writeFile(path.join(root, 'examples', 'Future', 'Future.ino'), 'Loom_Future sensor(manager);');
  await writeFile(path.join(root, 'examples', 'Future', 'arduino_secrets.h'), 'secret value must not be scanned');
  const first = await discover(root);
  assert.equal(first.blocks.find(b => b.id === 'class:Loom_Future').examples.length, 1);
  assert.equal(first.sourceFiles, 1);
  const opened = [];
  const { server, url } = await startStudio({ root, workspace, port: 0, openEditor: async location => opened.push(location) });
  t.after(() => new Promise(resolve => { server.close(resolve); server.closeAllConnections(); }));
  const get = async route => (await fetch(url + route)).json();
  const post = async (route, body, extra = {}) => fetch(url + route, { method: 'POST', headers: { 'Content-Type': 'application/json', ...extra }, body: JSON.stringify(body) });
  assert.equal((await fetch(url)).status, 200);
  const d = baseDesign();
  assert.equal((await post('/api/design', d)).status, 200);
  const handoff = await (await post('/api/handoff', {})).json();
  assert.match(handoff.prompt, /Codex Desktop/);
  const saved = JSON.parse(await readFile(path.join(workspace, 'request.json'), 'utf8'));
  assert.equal(saved.designRevision, revisionOf(d));
  const a = result(d); a.sourceFingerprint = saved.sourceFingerprint;
  // Simulate Codex's external file write; this is not an LLM call.
  await atomicJSON(path.join(workspace, 'assembly.json'), a);
  let state = await get('/api/state');
  assert.equal(state.assembly.summary, 'Three sensors');
  assert.equal(await readFile(path.join(state.outputDirectory, 'FeatureApplication.ino'), 'utf8'), a.files[0].content);
  const attachment = await fetch(url + '/api/download?file=FeatureApplication.ino');
  assert.match(attachment.headers.get('content-disposition'), /attachment; filename="FeatureApplication.ino"/);
  assert.equal(await attachment.text(), a.files[0].content);
  assert.equal((await fetch(url + '/api/download?file=../outside.ino')).status, 400);
  const branches = await get('/api/branches');
  assert.equal(branches.canSwitch, false);
  assert.equal((await post('/api/branch', { branch: '--bad' })).status, 400);
  assert.equal((await post('/api/design', d, { Origin: 'https://unrelated.example' })).status, 403);
  assert.equal((await fetch(url + '/api/source?file=src/../../outside.h')).status, 403);
  assert.equal((await fetch(url + '/api/source?file=examples/Future/arduino_secrets.h')).status, 403);
  const gallery = await get('/api/examples');
  assert.equal(gallery.examples.length, 1);
  const imported = await (await post('/api/example', { path: 'examples/Future/Future.ino' })).json();
  assert.equal(imported.assemblyOrigin, 'example');
  assert.ok(imported.design.blocks.some(b => b.catalogId === 'class:Loom_Future'));
  const sourceNode = imported.assembly.nodes.find(n => n.id !== 'example-custom');
  const open = await post('/api/open-source', { nodeId: sourceNode.id });
  assert.equal(open.status, 200);
  assert.equal(opened.at(-1).path, await (await import('node:fs/promises')).realpath(path.join(root, 'examples', 'Future', 'Future.ino')));
  assert.equal(opened.at(-1).line, 1);
  assert.equal((await post('/api/open-source', { source: '../outside.h' })).status, 400);
  assert.equal((await post('/api/open-source', { nodeId: 'missing' })).status, 400);
  assert.equal((await post('/api/open-source', { fileName: '../outside.ino' })).status, 400);
  assert.equal((await post('/api/open-source', { nodeId: sourceNode.id }, { Origin: 'https://unrelated.example' })).status, 403);
  assert.equal(opened.length, 1);
  assert.equal((await fetch(url + '/code-view.js')).status, 200);
  assert.equal((await fetch(url + '/compatibility.js')).status, 200);
  assert.equal((await fetch(url + '/theme.js')).status, 200);
  assert.equal((await fetch(url + '/theme.css')).status, 200);
  // A brand-new API discovered on an uncommitted edit invalidates the old result.
  await writeFile(header, (await readFile(header, 'utf8')).replace('samples = 7', 'samples = 11'));
  await post('/api/refresh', {});
  state = await get('/api/state');
  assert.equal(state.assembly, null);
  assert.match(state.assemblyError, /different source/);
  const refreshed = await get('/api/catalog');
  assert.equal(refreshed.blocks.find(b => b.className === 'Loom_Future').constructors[0].parameters[1].default, '11');
});
test('button-triggered CLI run returns progress and files; stale results fail; cancellation works', async t => {
  const temp = await mkdtemp(path.join(os.tmpdir(), 'feature-cli-test-'));
  const root = path.join(temp, 'repo'), workspace = path.join(temp, 'bridge');
  await mkdir(path.join(root, 'src'), { recursive: true });
  await writeFile(path.join(root, 'src', 'Future.h'), 'class Loom_Future { public: Loom_Future(Manager &man); };');
  const codexPath = fileURLToPath(new URL('fixtures/fake-codex.mjs', import.meta.url));
  assert.ok(await findCodex(codexPath));
  const { server, runner, url } = await startStudio({ root, workspace, port: 0, codexPath });
  t.after(async () => {
    await runner.stop();
    await new Promise(resolve => { server.close(resolve); server.closeAllConnections(); });
    await rm(temp, { recursive: true, force: true });
  });
  const get = async route => (await fetch(url + route)).json();
  const post = async (route, body) => fetch(url + route, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
  const d = baseDesign();
  await post('/api/design', d);
  assert.equal((await get('/api/runtime')).codexAvailable, true);
  let response = await post('/api/assemble', { model: 'test-model', sandbox: 'read-only' });
  assert.equal(response.status, 202);
  let job = await response.json();
  assert.equal(job.status, 'running');
  const wait = async status => {
    for (let i = 0; i < 100; i++) {
      const s = await get('/api/state');
      if (s.job.status === status) return s;
      if (['failed', 'cancelled'].includes(s.job.status) && s.job.status !== status) assert.fail(s.job.message);
      await new Promise(resolve => setTimeout(resolve, 50));
    }
    assert.fail('Timed out waiting for CLI fixture.');
  };
  let state = await wait('succeeded');
  assert.equal(state.assembly.summary, 'Fixture assembly');
  assert.equal(state.job.sessionId, 'fixture-session');
  const validation = JSON.parse(state.assembly.validation);
  assert.equal(path.resolve(validation.cwd), path.resolve(root));
  assert.equal(validation.receivedDescription, d.description);
  assert.ok(validation.args.includes('test-model'));
  assert.equal(validation.args[validation.args.indexOf('--sandbox') + 1], 'read-only');
  response = await post('/api/assemble', { model: 'test-stale', sandbox: 'read-only' });
  assert.equal(response.status, 202);
  state = await wait('failed');
  assert.match(state.job.error, /different source/);
  assert.equal(state.assembly.summary, 'Fixture assembly'); // Failed run cannot replace good output.
  await post('/api/assemble', { model: 'test-wait', sandbox: 'read-only' });
  const concurrent = await post('/api/assemble', { model: 'test-model', sandbox: 'read-only' });
  assert.equal(concurrent.status, 400);
  response = await post('/api/cancel', {});
  assert.equal((await response.json()).status, 'cancelled');
});
