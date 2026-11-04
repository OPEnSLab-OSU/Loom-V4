import { test } from 'node:test';
import assert from 'node:assert/strict';
import { assemblyCatalog, completionInstructions, makePrompt } from '../core.mjs';
import { cliPrompt } from '../codex.mjs';
import { extractHeader } from '../adapters/loom.mjs';

test('a sparse sensor and log request discovers transitive support from the current API without adding requested blocks', () => {
  const classes = [
    ...extractHeader('class Loom_Host { public: Loom_Host(); };', 'Host.h'),
    ...extractHeader('class Loom_Probe { public: Loom_Probe(Loom_Host &host); };', 'Sensors/Probe.h'),
    ...extractHeader('class Loom_Archive { public: Loom_Archive(Loom_Host &host); };', 'Storage/Archive.h'),
    ...extractHeader('class Loom_Unrelated { public: Loom_Unrelated(); };', 'Radio/Unrelated.h')
  ];
  const log = { id: 'intent:log', title: 'Log records', kind: 'intent', category: 'storage', candidates: ['class:Loom_Archive'], constructors: [], examples: [] };
  const catalog = { adapter: { id: 'loom', instructions: [] }, blocks: [...classes, log] };
  const design = { blocks: [{ id: 'sensor', catalogId: 'class:Loom_Probe' }, { id: 'log', catalogId: 'intent:log' }] };
  const original = JSON.stringify(design);
  const context = assemblyCatalog(design, catalog);
  assert.deepEqual(context.blocks.map(b => b.id).sort(), ['class:Loom_Probe', 'intent:log']);
  assert.deepEqual(context.supportCandidates.map(b => b.id).sort(), ['class:Loom_Archive', 'class:Loom_Host']);
  assert.ok(context.availableClasses.some(c => c.id === 'class:Loom_Unrelated'));
  assert.equal(JSON.stringify(design), original);
  assert.ok(context.supportCandidates.every(c => c.source && c.constructors.length));
});

test('both assembly paths ask for completed behavior and explain inferred support, without imposing extra feature selections', () => {
  const design = { schemaVersion: 1, adapter: 'loom', target: 'test', name: 'Sparse', description: 'Read and store', blocks: [] };
  const catalog = { root: '/checkout', fingerprint: 'source', adapter: { instructions: [] } };
  const desktop = makePrompt(design, catalog, '/workspace');
  const cli = cliPrompt({ design }, [], 'read-only');
  for (const instruction of completionInstructions) {
    assert.ok(desktop.includes(instruction)); assert.ok(cli.includes(instruction));
  }
  assert.match(cli, /added:true/);
  assert.match(cli, /sensor plus an SD logging capability/);
  assert.match(desktop, /minimum supporting objects/);
});
