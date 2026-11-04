import path from 'node:path';
import { mkdir, readFile, unlink } from 'node:fs/promises';
import { randomUUID } from 'node:crypto';
import { validateDesign, validateAssembly } from './core.mjs';

// A saved result can be inspected without claiming it was reviewed against the
// current checkout. Still validate its design, graph, filenames and references.
export function restoreProject(snapshot, catalog) {
  const design = validateDesign(snapshot.design);
  if (design.adapter !== catalog.adapter.id) throw new Error('The saved project uses a different adapter.');
  const validateSaved = (assembly, inputs) => {
    validateDesign(inputs);
    if (inputs.adapter !== design.adapter || typeof assembly?.sourceFingerprint !== 'string' || !assembly.sourceFingerprint.trim()) throw new Error('The saved build is missing its source checkout.');
    return validateAssembly(assembly, inputs, { ...catalog, fingerprint: assembly.sourceFingerprint });
  };
  let assembly = snapshot.assembly || null, origin = snapshot.origin || null;
  let lastBuild = snapshot.lastBuild || null;
  if (lastBuild) validateSaved(lastBuild.assembly, lastBuild.design);
  if (assembly) {
    validateSaved(assembly, design);
    if (assembly.sourceFingerprint !== catalog.fingerprint) {
      lastBuild = { design: structuredClone(design), assembly, origin, assemblyOrigin: origin?.kind || 'codex' };
      assembly = null; origin = null;
    }
  }
  return { design, assembly, origin, lastBuild };
}

export function emptyProjectDraft(snapshot) {
  const design=snapshot?.design;
  return !!design&&design.name==='Untitled application'&&!design.description.trim()&&!design.blocks.length&&!snapshot.assembly&&!snapshot.lastBuild;
}

// The active project's working files remain the Codex file bridge. Other tabs
// keep independent snapshots, including their generated files and provenance.
export async function projectStore(workspace, atomicJSON, design) {
  const directory = path.join(workspace, 'projects');
  const manifest = path.join(directory, 'index.json');
  await mkdir(directory, { recursive: true });
  let index;
  try { index = JSON.parse(await readFile(manifest, 'utf8')); }
  catch (error) { if (error.code !== 'ENOENT') throw error; }
  const save = () => atomicJSON(manifest, index);
  if (!index) {
    const id = randomUUID();
    index = { schemaVersion: 1, activeId: id, openIds: [id], items: [{ id, name: design.name, draft:emptyProjectDraft({design}), updatedAt: new Date().toISOString() }] };
    await save();
  }
  // Backfill older manifests. Keep snapshot files for recovery; an untouched
  // starter is a workspace draft, not a saved recipe.
  for(const project of index.items) {
    try{project.draft=emptyProjectDraft(JSON.parse(await readFile(path.join(directory,project.id+'.json'),'utf8')));}
    catch(error){if(error.code!=='ENOENT')throw error;if(project.id===index.activeId)project.draft=emptyProjectDraft({design});}
  }
  index.items=index.items.filter(project=>!project.draft||index.openIds.includes(project.id));await save();
  function item(id) {
    if (typeof id !== 'string' || !/^[a-f0-9-]{36}$/.test(id) || !index.items.some(p => p.id === id)) throw new Error('That project was not found.');
    return index.items.find(p => p.id === id);
  }
  return {
    list: () => structuredClone(index),
    async rename(name,snapshot) { const p = item(index.activeId),draft=snapshot?emptyProjectDraft(snapshot):p.draft; if (p.name !== name||p.draft!==draft) { p.name = name;p.draft=draft; await save(); } },
    async checkpoint(snapshot) {
      const p = item(index.activeId);
      await atomicJSON(path.join(directory, p.id + '.json'), snapshot);
      p.name = snapshot.design.name; p.draft=emptyProjectDraft(snapshot);p.updatedAt = new Date().toISOString(); await save();
    },
    async read(id) { item(id); return JSON.parse(await readFile(path.join(directory, id + '.json'), 'utf8')); },
    async add(snapshot) {
      const id = randomUUID();
      await atomicJSON(path.join(directory, id + '.json'), snapshot);
      index.items.push({ id, name: snapshot.design.name,draft:emptyProjectDraft(snapshot),updatedAt: new Date().toISOString() });
      index.openIds.push(id); index.activeId = id; await save(); return id;
    },
    async activate(id) { item(id); if (!index.openIds.includes(id)) index.openIds.push(id); index.activeId = id; await save(); },
    async close(id) { const project=item(id);index.openIds = index.openIds.filter(p => p !== id);if(project.draft)index.items=index.items.filter(p=>p.id!==id);await save(); },
    async delete(id) {
      item(id);
      if (id === index.activeId) throw new Error('Switch away from the project before deleting it.');
      index.items = index.items.filter(p => p.id !== id); index.openIds = index.openIds.filter(p => p !== id);
      await save();
      await unlink(path.join(directory, id + '.json')).catch(e => { if (e.code !== 'ENOENT') throw e; });
    }
  };
}
