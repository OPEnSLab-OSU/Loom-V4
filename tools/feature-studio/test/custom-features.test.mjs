import { test } from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import os from 'node:os';
import { mkdtemp, mkdir, rm } from 'node:fs/promises';
import { savedFeature, withSavedFeatures } from '../custom-features.mjs';
import { startStudio } from '../server.mjs';
import { dropSamples } from '../public/game-audio.js';

test('saved custom ingredients retain descriptions and settings without altering the source catalog', () => {
  const block = { label: 'Calibrate readings', count: 3, settings: { requirement: 'Apply a per-sensor calibration before logging.' }, notes: 'Keep raw readings in the packet too.' };
  const saved = savedFeature(block);
  assert.equal(saved.defaults.requirement, block.settings.requirement);
  assert.equal(saved.savedNotes, block.notes); assert.equal(saved.savedCount, 3);
  assert.equal(saved.category, 'custom'); assert.equal(saved.source, null);
  const base = { fingerprint: 'current', categories: {}, blocks: [] };
  const catalog = withSavedFeatures(base, [saved]);
  assert.equal(catalog.fingerprint, base.fingerprint); assert.equal(base.blocks.length, 0);
  assert.equal(savedFeature({ ...block, notes: 'Updated behavior.' }, [saved]).id, saved.id);
  assert.throws(() => savedFeature({ ...block, settings: {}, notes: '' }), /Describe/);
});

test('Custom library persists across local app restarts and keeps the current design separate', async t => {
  const temp = await mkdtemp(path.join(os.tmpdir(), 'studio-custom-test-'));
  t.after(() => rm(temp, { recursive: true, force: true }));
  const root = path.join(temp, 'repo'), workspace = path.join(temp, 'workspace');
  await mkdir(path.join(root, 'src'), { recursive: true });
  let app = await startStudio({ root, workspace, port: 0 });
  const close = () => new Promise(resolve => { app.server.close(resolve); app.server.closeAllConnections(); });
  t.after(close);
  const post = (route, body) => fetch(app.url + route, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
  const design = { schemaVersion: 1, adapter: 'loom', target: 'Feather M0', name: 'Custom test', description: '', blocks: [
    { id: 'custom-request', catalogId: 'intent:custom', label: 'Normalize samples', count: 2, settings: { requirement: 'Normalize each sensor independently.' }, notes: 'Keep the original values.' }
  ] };
  assert.equal((await post('/api/design', design)).status, 200);
  const response = await post('/api/custom-features', { blockId: 'custom-request' });
  assert.equal(response.status, 200);
  const { feature } = await response.json();
  assert.ok(feature.id.startsWith('custom:'));
  assert.equal((await post('/api/custom-features', { blockId: 'missing' })).status, 400);
  await close(); app = await startStudio({ root, workspace, port: 0 });
  const catalog = await (await fetch(app.url + '/api/catalog')).json();
  const restored = catalog.blocks.find(b => b.id === feature.id);
  assert.equal(restored.title, 'Normalize samples'); assert.equal(restored.savedCount, 2);
  const state = await (await fetch(app.url + '/api/state')).json();
  assert.deepEqual(state.design, design);
});

test('element sound families and subtle repeat variations produce distinct quiet waveforms', () => {
  const kinds = ['category', 'node', 'panel', 'tab', 'zoom', 'source', 'recipe', 'assemble'];
  const signatures = new Set();
  for (const kind of kinds) {
    const samples = dropSamples(kind, 22050);
    let peak = 0;
    for (const sample of samples) { assert.ok(Number.isFinite(sample)); peak = Math.max(peak, Math.abs(sample)); }
    assert.ok(peak < .75);
    signatures.add(samples.length + ':' + samples[500].toFixed(6));
  }
  assert.equal(signatures.size, kinds.length);
  assert.notDeepEqual(dropSamples('category', 22050, 0), dropSamples('category', 22050, 1));
});
