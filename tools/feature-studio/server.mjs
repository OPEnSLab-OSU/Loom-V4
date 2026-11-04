import http from 'node:http';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { mkdir, readFile, writeFile, rename, realpath, unlink } from 'node:fs/promises';
import { discover } from './adapters/loom.mjs';
import { assemblySchema, revisionOf, validateDesign, validateAssembly, makePrompt, assemblyCatalog, blankDesign } from './core.mjs';
import { CodexRunner, findCodex, listModels, DEFAULT_MODEL, DEFAULT_EFFORT } from './codex.mjs';
import { reconstructExample } from './examples.mjs';
import { createHash, randomUUID } from 'node:crypto';
import { relevantRecipes } from './recipe-index.mjs';
import { sourceLocation, openInVSCode } from './source-navigation.mjs';
import {sourceLinks,flowSources} from './source-links.mjs';
import { readBranches, switchBranch } from './branches.mjs';
import { savedFeature, withSavedFeatures } from './custom-features.mjs';
import {prepareCheckout,preparedContext,preparationStatus} from './prepared-source.mjs';
import { projectStore, restoreProject } from './projects.mjs';
import { ArduinoTools, codeFingerprint, openFirmwareFolder } from './arduino.mjs';
import { designHistory } from './history.mjs';
import { exampleOverview } from './example-overview.mjs';
import {resolveLoom} from './find-loom.mjs';
import {CodexAuth} from './codex-auth.mjs';

const here = path.dirname(fileURLToPath(import.meta.url));
export async function atomicJSON(filename, value) {
  if (filename instanceof URL) filename = fileURLToPath(filename);
  const temp = filename + '.tmp';
  await writeFile(temp, JSON.stringify(value, null, 2) + '\n', 'utf8');
  await rename(temp, filename);
}
async function optionalJSON(filename) {
  try { return JSON.parse(await readFile(filename, 'utf8')); }
  catch (e) { if (e.code === 'ENOENT') return null; throw e; }
}
export async function startStudio({ root, workspace = path.join(here, '.local'), port = 4173, codexPath, openEditor = openInVSCode, arduinoExecute, openBuildFolder = openFirmwareFolder, warmTools=false } = {}) {
  root = root ? await realpath(root) : await resolveLoom({workspace});
  await mkdir(workspace, { recursive: true });
  workspace = await realpath(workspace);
  let customFeatures = (await optionalJSON(path.join(workspace, 'custom-features.json')))?.features || [];
  let catalog = withSavedFeatures(await discover(root), customFeatures), lastScan = Date.now(), rescanPromise;
  catalog.git = await readBranches(root);
  let prepared=await prepareCheckout(root,catalog,workspace);
  let design = validateDesign(await optionalJSON(path.join(workspace, 'design.json')) || blankDesign(catalog));
  const projects = await projectStore(workspace, atomicJSON, design);
  const history = await designHistory(workspace,atomicJSON);
  const arduino = new ArduinoTools({ root, workspace, atomicJSON, execute: arduinoExecute }); await arduino.init();
  let previousDesignId = (await optionalJSON(path.join(workspace, 'previous-design.json')))?.id || null;
  let assembly = null, assemblyError = '', lastAssemblyText = '', saving = Promise.resolve();
  let origin = await optionalJSON(path.join(workspace, 'origin.json')), assemblyOrigin = null;
  let lastBuild = await optionalJSON(path.join(workspace, 'last-build.json'));
  let exampleIndex, exampleIndexRevision, modelsPromise;
  const signature = a => createHash('sha256').update(JSON.stringify(a)).digest('hex');
  const queue = fn => { const p = saving.then(fn); saving = p.catch(() => {}); return p; };
  function currentRequest() {
    return {
      schemaVersion: 1, designRevision: revisionOf(design), sourceFingerprint: catalog.fingerprint,
      sourceRoot: root, design: structuredClone(design),
      sourceBranch: catalog.git?.branch || null,
      catalog: assemblyCatalog(design, catalog),
      preparedContext: preparedContext(design,catalog,prepared),
      output: path.join(workspace, 'assembly.json')
    };
  }
  async function saveRequest() {
    const request = currentRequest();
    await atomicJSON(path.join(workspace, 'design.json'), design);
    await projects.rename(design.name,{design,assembly,lastBuild});
    await atomicJSON(path.join(workspace, 'request.json'), request);
    await writeFile(path.join(workspace, 'assembly.schema.json'), JSON.stringify(assemblySchema, null, 2) + '\n');
    await writeFile(path.join(workspace, 'codex-prompt.txt'), makePrompt(design, catalog, workspace));
  }
  async function scan(force = false) {
    if (!force && Date.now() - lastScan < 10000) return;
    if (rescanPromise) return rescanPromise;
    rescanPromise = (async () => {
      const updated = withSavedFeatures(await discover(root), customFeatures);
      updated.git = await readBranches(root);
      catalog.git = updated.git;
      if (updated.fingerprint !== catalog.fingerprint || updated.supportFingerprint !== catalog.supportFingerprint || force) {
        catalog = updated;
        prepared=await prepareCheckout(root,catalog,workspace);
        await saveRequest();
      }
      lastScan = Date.now();
    })();
    try { await rescanPromise; } finally { rescanPromise = null; }
  }
  async function syncAssembly() {
    await scan();
    let content;
    try { content = await readFile(path.join(workspace, 'assembly.json'), 'utf8'); }
    catch (e) { if (e.code !== 'ENOENT') throw e; }
    if (!content) { assembly = null; assemblyError = ''; return; }
    const cacheKey = revisionOf(design) + catalog.fingerprint + content;
    if (lastAssemblyText === cacheKey) return;
    try {
      let parsed = JSON.parse(content);
      // Recover title-only edits saved by older Studio versions. A prior project
      // snapshot must prove that the built behavior still matches exactly.
      if (parsed.designRevision !== revisionOf(design)) {
        const prior = await projects.read(projects.list().activeId).catch(error => { if (error.code !== 'ENOENT') throw error; return null; });
        if (prior && revisionOf({ ...design, name: prior.design.name }) === revisionOf(prior.design)) {
          validateAssembly(parsed, prior.design, catalog);
          const oldSignature = signature(parsed);
          parsed = { ...parsed, designRevision: revisionOf(design) };
          if (origin?.signature === oldSignature) { origin = { ...origin, signature: signature(parsed) }; await atomicJSON(path.join(workspace, 'origin.json'), origin); }
          await atomicJSON(path.join(workspace, 'assembly.json'), parsed);
        }
      }
      let result = validateAssembly(parsed, design, catalog);
      const fromExample = origin?.kind === 'example' && origin.signature === signature(result);
      // Upgrade the displayed wiring of already loaded recipes without changing
      // the requested design, sketch, or saved assembly provenance.
      if (fromExample && !result.edges.some(e => e.kind === 'flow') && catalog.examplePaths.includes(origin.path)) {
        const indexed = reconstructExample(result.files.find(f => f.name.endsWith('.ino')).content, origin.path, catalog).assembly;
        if (indexed.nodes.length === result.nodes.length && indexed.nodes.every(n => result.nodes.some(old => old.id === n.id && old.reason === n.reason))) {
          result = { ...result, routines: indexed.routines, edges: indexed.edges };
        }
      }
      const outputDir = path.join(workspace, 'outputs', revisionOf(design), 'FeatureApplication');
      await mkdir(outputDir, { recursive: true });
      for (const f of result.files) await writeFile(path.join(outputDir, f.name), f.content, 'utf8');
      assembly = result;
      assemblyOrigin = fromExample ? 'example' : origin?.signature === signature(result) ? origin.kind : 'codex';
      lastBuild = { design: structuredClone(design), assembly: result, origin, assemblyOrigin };
      await atomicJSON(path.join(workspace, 'last-build.json'), lastBuild);
      assemblyError = '';
      lastAssemblyText = cacheKey;
    } catch (e) {
      // A partial external write may be retried at the next poll.
      assembly = null;
      assemblyOrigin = null;
      assemblyError = e.message;
      lastAssemblyText = '';
    }
  }
  await saveRequest();
  let executable = await findCodex(codexPath);
  const auth=new CodexAuth(executable);
  const runner = new CodexRunner({
    root, workspace, executable,
    onResult: (result,request,run) => queue(async () => {
      if(run&&(runner.job?.id!==run.id||runner.job.status!=='running'||projects.list().activeId!==run.projectId))throw new Error('This run was cancelled or its project changed before the result was applied.');
      await scan(true);
      if(result.designRevision!==revisionOf(design)&&request?.design&&revisionOf({...design,name:request.design.name})===revisionOf(request.design)) {
        validateAssembly(result,request.design,catalog);result={...result,designRevision:revisionOf(design)};
      }
      validateAssembly(result, design, catalog);
      await atomicJSON(path.join(workspace, 'assembly.json'), result);
      await syncAssembly();
      return {designRevision:revisionOf(design)};
    })
  });
  await runner.restore(projects.list().activeId);
  async function archiveDesign() {
    await syncAssembly();
    const id = randomUUID();
    await mkdir(path.join(workspace, 'design-history'), { recursive: true });
    await atomicJSON(path.join(workspace, 'design-history', id + '.json'), { schemaVersion: 1, savedAt: new Date().toISOString(), sourceRoot: root,
      design, assembly, lastBuild, origin: assemblyOrigin === 'example' && assembly ? { ...origin, signature:signature(assembly) } : origin });
    await atomicJSON(path.join(workspace, 'previous-design.json'), { id });
    previousDesignId = id;
  }
  async function replaceDesign(next, result = null, nextOrigin = null, previousBuild = null) {
    const restored = restoreProject({ design: next, assembly: result, origin: nextOrigin, lastBuild: previousBuild }, catalog);
    design = restored.design; result = restored.assembly; nextOrigin = restored.origin; previousBuild = restored.lastBuild;
    assembly = result; origin = nextOrigin; assemblyOrigin = result ? nextOrigin?.kind || 'codex' : null;
    lastBuild = result ? { design: structuredClone(design), assembly: result, origin, assemblyOrigin } : previousBuild;
    await atomicJSON(path.join(workspace, 'last-build.json'), lastBuild);
    assemblyError = ''; lastAssemblyText = '';await runner.restore(projects.list().activeId);
    await saveRequest();
    await atomicJSON(path.join(workspace, 'origin.json'), origin);
    if (result) await atomicJSON(path.join(workspace, 'assembly.json'), result);
    else await unlink(path.join(workspace, 'assembly.json')).catch(e => { if (e.code !== 'ENOENT') throw e; });
  }
  function assertProjectIdle() {
    if (runner.snapshot()?.status === 'running' || runner.starting) throw new Error('Wait for the assembly to finish or cancel it before changing projects.');
    if (arduino.busy) throw new Error('Wait for the device action to finish or cancel it before changing projects.');
  }
  async function checkpointProject() {
    await syncAssembly();
    await projects.checkpoint({ design, assembly, lastBuild, origin: assemblyOrigin === 'example' && assembly ? { ...origin, signature: signature(assembly) } : origin });
  }
  async function openProject(next, result = null, nextOrigin = null, previousBuild = null) {
    assertProjectIdle();
    validateDesign(next);
    if (next.adapter !== catalog.adapter.id) throw new Error('The saved project uses a different adapter.');
    await checkpointProject();
    await projects.add({ design: next, assembly: result, origin: nextOrigin, lastBuild: previousBuild });
    await replaceDesign(next, result, nextOrigin, previousBuild);
  }
  async function activateProject(id) {
    assertProjectIdle();
    if (id === projects.list().activeId) return;
    const snapshot = await projects.read(id);
    validateDesign(snapshot.design);
    if (snapshot.design.adapter !== catalog.adapter.id) throw new Error('The saved project uses a different adapter.');
    await checkpointProject();
    await projects.activate(id);
    await replaceDesign(snapshot.design, snapshot.assembly, snapshot.origin, snapshot.lastBuild);
  }
  function sourceContext() {
    return assembly ? { root, workspace, catalog, design, assembly, assemblyOrigin, origin } :
      { root, workspace, catalog, design: lastBuild?.design || design, assembly: lastBuild?.assembly || null, assemblyOrigin: lastBuild?.assemblyOrigin, origin: lastBuild?.origin };
  }
  function diagnosticMatches(job,selected) {
    return !!selected?.summary.hasCode&&job?.kind==='build'&&job.projectId===selected.summary.id&&job.filesFingerprint===selected.summary.filesFingerprint&&job.sourceFingerprint===catalog.fingerprint;
  }
  async function deviceProject(id) {
    const index=projects.list();
    if(!index.openIds.includes(id))throw new Error('Choose an open project tab.');
    const snapshot=id===index.activeId?{design,assembly,lastBuild,origin}:await projects.read(id);
    const saved=restoreProject(snapshot,catalog),code=saved.assembly||saved.lastBuild?.assembly||null;
    return {assembly:code,summary:{id,name:saved.design.name,hasCode:!!code,filesFingerprint:codeFingerprint(code),
      files:code?.files.map(file=>({name:file.name,lines:file.content.split('\n').length}))||[]}};
  }
  async function checkedDeviceProject(body) {
    const selected=await deviceProject(body.projectId);
    if(!selected.assembly)throw new Error('This project has no assembled code. Assemble it first.');
    if((body.filesFingerprint!==undefined||body.projectId!==projects.list().activeId)&&body.filesFingerprint!==selected.summary.filesFingerprint)throw new Error('The selected project code changed. Refresh Build and review it before continuing.');
    return selected;
  }
  let boundPort = port;
  const sendJSON = (res, value, code = 200) => {
    res.writeHead(code, { 'Content-Type': 'application/json; charset=utf-8', 'Cache-Control': 'no-store', 'X-Content-Type-Options': 'nosniff' });
    res.end(JSON.stringify(value));
  };
  const server = http.createServer(async (req, res) => {
    try {
      const allowedHosts = ['127.0.0.1:' + boundPort, 'localhost:' + boundPort];
      if (!allowedHosts.includes(req.headers.host)) return sendJSON(res, { error: 'Invalid host.' }, 403);
      const allowedOrigins = allowedHosts.map(h => 'http://' + h);
      if (req.headers.origin && !allowedOrigins.includes(req.headers.origin)) return sendJSON(res, { error: 'This local tool accepts same-origin requests only.' }, 403);
      const url = new URL(req.url, 'http://' + req.headers.host);
      if (req.method === 'POST') {
        if (!String(req.headers['content-type']).startsWith('application/json')) return sendJSON(res, { error: 'Expected JSON.' }, 415);
        let size = 0, parts = [];
        for await (const chunk of req) {
          size += chunk.length;
          if (size > (url.pathname === '/api/projects/import' ? 16_000_000 : 2_000_000)) return sendJSON(res, { error: 'Request is too large.' }, 413);
          parts.push(chunk);
        }
        const body = JSON.parse(Buffer.concat(parts).toString('utf8'));
        if(url.pathname==='/api/codex/login')return sendJSON(res,auth.start());
        if(url.pathname==='/api/codex/login/cancel')return sendJSON(res,auth.cancel());
        return await queue(async () => {
          if (url.pathname.startsWith('/api/history/')) {
            assertProjectIdle();
            const action=url.pathname.split('/').pop();
            if(!['undo','redo','restore'].includes(action))throw new Error('Unknown history action.');
            const target=await history.target(projects.list().activeId,design,action,body.id);
            validateDesign(target.design);await checkpointProject();
            const saved=lastBuild;let result=null,nextOrigin=null;
            if(saved&&revisionOf({...target.design,name:saved.design.name})===revisionOf(saved.design)){
              try{result={...saved.assembly,designRevision:revisionOf(target.design)};validateAssembly(result,target.design,catalog);
                nextOrigin=saved.origin?{...saved.origin,signature:signature(result)}:null;
              }catch{result=null;}
            }
            await replaceDesign(target.design,result,nextOrigin,saved);
            return sendJSON(res,await target.commit());
          }
          if (url.pathname.startsWith('/api/device/')) {
            if (runner.snapshot()?.status === 'running' || runner.starting) throw new Error('Wait for Codex to finish before using device tools.');
            if (url.pathname.endsWith('/configure')) { await arduino.configure(body); return sendJSON(res, { configured:true }); }
            if (url.pathname.endsWith('/get-cli')) return sendJSON(res, await arduino.installCLI(), 202);
            if (url.pathname.endsWith('/update-indexes')) return sendJSON(res, arduino.refreshIndexes(), 202);
            if (url.pathname.endsWith('/install')) return sendJSON(res, arduino.install(body.kind, body.id), 202);
            if (url.pathname.endsWith('/cancel')) return sendJSON(res, arduino.cancel());
            if (url.pathname.endsWith('/open-binaries')) {
              await syncAssembly();const selected=await checkedDeviceProject(body),built=await arduino.built(selected.assembly,body.board,catalog.fingerprint);
              if(!built||body.buildKey!==built.key)throw new Error('Build this project successfully before opening its binaries folder.');
              const base=await realpath(path.join(workspace,'firmware-builds')),directory=await realpath(path.join(base,built.key,'binaries')),relative=path.relative(base,directory);
              if(relative.startsWith('..')||path.isAbsolute(relative))throw new Error('Build folder is outside this workspace.');
              await openBuildFolder(directory);return sendJSON(res,{directory});
            }
            if (url.pathname.endsWith('/build') || url.pathname.endsWith('/flash')) {
              await syncAssembly();
              const selected=await checkedDeviceProject(body);
              const request = { assembly:selected.assembly, board: body.board, fingerprint: catalog.fingerprint, projectId: body.projectId, port: body.port, buildKey: body.buildKey };
              return sendJSON(res, url.pathname.endsWith('/flash') ? await arduino.flash(request) : await arduino.build(request), 202);
            }
            throw new Error('Unknown device action.');
          }
          if (url.pathname === '/api/design/new' || url.pathname === '/api/design/restore') {
            if (runner.snapshot()?.status === 'running' || runner.starting) throw new Error('Wait for the assembly to finish or cancel it before changing designs.');
            let previous;
            if (url.pathname.endsWith('/restore')) {
              if (!previousDesignId || !/^[a-f0-9-]{36}$/.test(previousDesignId)) throw new Error('No previous design is available.');
              previous = await optionalJSON(path.join(workspace, 'design-history', previousDesignId + '.json'));
              if (!previous) throw new Error('The previous design snapshot was not found.');
              validateDesign(previous.design);
              if (previous.design.adapter !== catalog.adapter.id) throw new Error('The previous design uses a different adapter.');
            }
            await archiveDesign();
            await openProject(previous?.design || blankDesign(catalog), previous?.assembly, previous?.origin, previous?.lastBuild);
            return sendJSON(res, { design, previousDesignAvailable: true });
          }
          if (url.pathname === '/api/projects/open') {
            await activateProject(body.id);
            return sendJSON(res, projects.list());
          }
          if (url.pathname === '/api/projects/close' || url.pathname === '/api/projects/delete') {
            assertProjectIdle();
            const index = projects.list(), removing = url.pathname.endsWith('/delete');
            if (!index.items.some(p => p.id === body.id)) throw new Error('That project was not found.');
            if (removing && body.confirm !== true) throw new Error('Confirm before permanently deleting a saved project.');
            if (index.activeId === body.id) {
              const remaining = index.openIds.filter(id => id !== body.id);
              const at = index.openIds.indexOf(body.id);
              if (remaining.length) await activateProject(remaining[Math.min(at, remaining.length - 1)]);
              else await openProject(blankDesign(catalog));
            }
            if (removing) { await projects.delete(body.id); await history.delete(body.id); }
            else await projects.close(body.id);
            return sendJSON(res, projects.list());
          }
          if (url.pathname === '/api/projects/import') {
            const bundle = body.format === 'feature-studio/project';
            const next = validateDesign(bundle ? body.design : body);
            if (bundle && body.schemaVersion !== 1) throw new Error('Unsupported project format.');
            if (bundle && body.assembly && body.source?.fingerprint && body.source.fingerprint !== body.assembly.sourceFingerprint) throw new Error('The project source checkout does not match its saved assembly.');
            const restored = restoreProject(bundle ? body : { design: next }, catalog);
            await openProject(restored.design, restored.assembly, restored.origin, restored.lastBuild);
            return sendJSON(res, projects.list());
          }
          if (url.pathname === '/api/design') {
            const next = body.design || body;
            if (body.projectId && body.projectId !== projects.list().activeId) throw new Error('This project is no longer active. Reload before editing.');
            if (next.adapter !== catalog.adapter.id) throw new Error('This server is using the ' + catalog.adapter.name + ' adapter.');
            validateDesign(next);
            await syncAssembly();
            const titleOnly = revisionOf({ ...next, name: design.name }) === revisionOf(design);
            let assemblyRetained = false;
            if (titleOnly && next.name !== design.name) {
              await syncAssembly();
              if (assembly) {
                const priorRevision=assembly.designRevision;
                const updated = { ...assembly, designRevision: revisionOf(next) };
                if (origin?.signature === signature(assembly)) { origin = { ...origin, signature: signature(updated) }; await atomicJSON(path.join(workspace, 'origin.json'), origin); }
                await atomicJSON(path.join(workspace, 'assembly.json'), updated);
                assembly = updated; lastAssemblyText = ''; assemblyRetained = true;
                if(runner.job?.status==='succeeded'&&runner.job.sourceFingerprint===catalog.fingerprint&&(runner.job.acceptedDesignRevision||runner.job.designRevision)===priorRevision) {
                  runner.job.acceptedDesignRevision=updated.designRevision;await runner.persist(runner.job);
                }
              }
            }
            const historyState=await history.record(projects.list().activeId,design,next);
            design = next;
            await saveRequest();
            // Exact request restoration can reuse the saved result even after a
            // tab switch removed the stale working copy. Recheck this checkout.
            if(!assembly&&lastBuild?.design&&revisionOf({...design,name:lastBuild.design.name})===revisionOf(lastBuild.design)) {
              let restored;
              try{restored=validateAssembly({...lastBuild.assembly,designRevision:revisionOf(design)},design,catalog);}catch{restored=null;}
              if(restored) {
                origin=lastBuild.origin?{...lastBuild.origin,signature:signature(restored)}:null;
                await atomicJSON(path.join(workspace,'origin.json'),origin);
                await atomicJSON(path.join(workspace,'assembly.json'),restored);
                lastAssemblyText='';await syncAssembly();assemblyRetained=!!assembly;
              }
            }
            return sendJSON(res, { designRevision: revisionOf(design), assemblyRetained, history:historyState });
          }
          if (url.pathname === '/api/refresh') {
            await scan(true);
            return sendJSON(res, catalog);
          }
          if(url.pathname==='/api/code') {
            assertProjectIdle();await syncAssembly();
            if(body.projectId!==projects.list().activeId)throw new Error('Return to the project you were editing before saving.');
            const base=assembly||lastBuild?.assembly,inputs=assembly?design:lastBuild?.design;
            const file=base?.files.find(file=>file.name===body.file);
            if(!file||file.content!==body.previousContent)throw new Error('This file changed since editing began. Copy your edits, cancel, and reopen the latest file.');
            if(typeof body.content!=='string'||body.content.length>500000)throw new Error('Use a text file under 500,000 characters.');
            const updated=validateAssembly({...base,files:base.files.map(f=>f.name===file.name?{...f,content:body.content}:f)},inputs,{...catalog,fingerprint:base.sourceFingerprint});
            const folder=path.join(workspace,'code-history',projects.list().activeId);await mkdir(folder,{recursive:true});
            await atomicJSON(path.join(folder,randomUUID()+'.json'),{at:new Date().toISOString(),file:file.name,content:file.content});
            const customOrigin={kind:'custom',signature:signature(updated)};
            if(assembly) {
              origin=customOrigin;await atomicJSON(path.join(workspace,'origin.json'),origin);
              await atomicJSON(path.join(workspace,'assembly.json'),updated);lastAssemblyText='';await syncAssembly();
            } else {
              lastBuild={...lastBuild,assembly:updated,origin:customOrigin,assemblyOrigin:'custom'};
              await atomicJSON(path.join(workspace,'last-build.json'),lastBuild);
            }
            await checkpointProject();
            return sendJSON(res,{saved:true});
          }
          if (url.pathname === '/api/handoff') {
            await scan(true);
            await saveRequest();
            return sendJSON(res, { prompt: makePrompt(design, catalog, workspace), workspace, designRevision: revisionOf(design) });
          }
          if (url.pathname === '/api/assemble') {
            if (arduino.busy) throw new Error('Wait for the device action to finish before assembling.');
            await scan(true);
            await saveRequest();
            if (typeof body.model !== 'string' || typeof body.sandbox !== 'string') throw new Error('Choose a model and project permission mode.');
            const job = await runner.start({ request: currentRequest(), schema: assemblySchema, instructions: catalog.adapter.instructions, model: body.model.trim() || DEFAULT_MODEL, effort: body.effort || DEFAULT_EFFORT, sandbox: body.sandbox,projectId:projects.list().activeId });
            return sendJSON(res, job, 202);
          }
          if (url.pathname === '/api/cancel') return sendJSON(res, runner.cancel());
          if (url.pathname === '/api/custom-features') {
            const block = design.blocks.find(b => b.id === body.blockId);
            if (!block || !(block.catalogId === 'intent:custom' || block.catalogId.startsWith('custom:'))) throw new Error('Choose a custom feature block to save.');
            const feature = savedFeature(block, customFeatures);
            customFeatures = [...customFeatures.filter(f => f.id !== feature.id), feature];
            await atomicJSON(path.join(workspace, 'custom-features.json'), { schemaVersion: 1, features: customFeatures });
            catalog = withSavedFeatures(catalog, customFeatures);
            await saveRequest();
            return sendJSON(res, { feature, catalog });
          }
          if (url.pathname === '/api/branch') {
            if (arduino.busy) throw new Error('Wait for the device action before switching branches.');
            if (runner.snapshot()?.status === 'running') throw new Error('Wait for the current assembly before changing branches.');
            const git = await switchBranch(root, body.branch);
            await scan(true); await syncAssembly();
            return sendJSON(res, git);
          }
          if (url.pathname === '/api/open-source') {
            await syncAssembly();
            const location = await sourceLocation(sourceContext(), body);
            await openEditor(location);
            return sendJSON(res, location);
          }
          if (url.pathname === '/api/example') {
            assertProjectIdle();
            await scan(true);
            if (!catalog.examplePaths.includes(body.path)) throw new Error('Choose an example from this checkout.');
            const reconstructed = structuredClone(prepared.recipes.find(r=>r.path===body.path)?.indexed || reconstructExample(await readFile(path.join(root,body.path),'utf8'),body.path,catalog));
            validateDesign(reconstructed.design);
            validateAssembly(reconstructed.assembly, reconstructed.design, catalog);
            await archiveDesign();
            await openProject(reconstructed.design, reconstructed.assembly, { kind: 'example', path: body.path, signature: signature(reconstructed.assembly) });
            await syncAssembly();
            return sendJSON(res, { design, assembly, assemblyOrigin, designRevision: revisionOf(design) });
          }
          return sendJSON(res, { error: 'Unknown operation.' }, 404);
        });
      }
      if (req.method !== 'GET') return sendJSON(res, { error: 'Method not supported.' }, 405);
      if (url.pathname === '/api/catalog') return sendJSON(res, catalog);
      if (url.pathname === '/api/device/status') {
        const status = await arduino.status({includePorts:url.searchParams.get('mode')!=='build',force:url.searchParams.get('refresh')==='true'});
        const packageMatch = root.replaceAll('\\','/').match(/\/packages\/([^/]+)\/hardware\/([^/]+)\//);
        status.defaultBoard = status.boards.find(b => packageMatch && b.fqbn.startsWith(packageMatch[1]+':'+packageMatch[2]+':') && /feather.*m0/i.test(b.name))?.fqbn || status.boards.find(b=>/feather.*m0/i.test(b.name))?.fqbn || '';
        return sendJSON(res,status);
      }
      if (url.pathname === '/api/device/search') return sendJSON(res, { choices: await arduino.search(url.searchParams.get('kind'),url.searchParams.get('q')) });
      if(url.pathname==='/api/device/projects')return await queue(async()=>{
        await syncAssembly();const index=projects.list(),items=[];
        for(const id of index.openIds)items.push((await deviceProject(id)).summary);
        return sendJSON(res,{projects:items,activeId:index.activeId});
      });
      if (url.pathname === '/api/device/job') return await queue(async()=>{
        await syncAssembly();const selected=await deviceProject(url.searchParams.get('projectId')||projects.list().activeId);
        const build = await arduino.built(selected.assembly,url.searchParams.get('board'),catalog.fingerprint);
        const job=arduino.snapshot();
        const jobMatchesSelected=!!job&&job.projectId===selected.summary.id&&job.filesFingerprint===selected.summary.filesFingerprint&&job.sourceFingerprint===catalog.fingerprint&&job.board===url.searchParams.get('board')&&
          (job.status!=='succeeded'||job.buildKey===build?.key);
        return sendJSON(res,{job,jobMatchesSelected,projectId:projects.list().activeId,selectedProject:selected.summary,diagnosticsMatch:diagnosticMatches(job,selected),diagnosticProjectId:selected.summary.id,
          build:build?{key:build.key,board:build.board,builtAt:build.builtAt,files:build.files,totalBytes:build.totalBytes}:null});
      });
      if (url.pathname === '/api/device/diagnostic') return await queue(async()=>{
        await syncAssembly();
        const job=arduino.snapshot(),index=Number(url.searchParams.get('index'));
        const selected=await deviceProject(url.searchParams.get('projectId'));
        if(url.searchParams.get('jobId')!==job?.id||!diagnosticMatches(job,selected))throw new Error('These compiler messages belong to different project code. Review that build before jumping to a line.');
        if(!url.searchParams.has('index')||!Number.isInteger(index)||index<0||!job.diagnostics[index]?.applicationFile)throw new Error('This compiler message has no verified sketch line. Review its file path in the build log.');
        const item=job.diagnostics[index];
        return sendJSON(res,{file:item.applicationFile,line:item.line,endLine:item.line,projectId:selected.summary.id});
      });
      if (url.pathname === '/api/projects') return sendJSON(res, projects.list());
      if (url.pathname === '/api/history') return await queue(async()=>sendJSON(res,await history.view(projects.list().activeId,design)));
      if (url.pathname === '/api/project-export') return await queue(async () => {
        await syncAssembly();
        return sendJSON(res, { format: 'feature-studio/project', schemaVersion: 1, exportedAt: new Date().toISOString(),
          source: { adapter: catalog.adapter.id, fingerprint: catalog.fingerprint, branch: catalog.git?.branch || null },
          design, assembly, lastBuild: assembly ? null : lastBuild, origin: assemblyOrigin === 'example' && assembly ? { ...origin, signature: signature(assembly) } : origin });
      });
      if (url.pathname === '/api/branches') {
        catalog.git = await readBranches(root);
        return sendJSON(res, catalog.git);
      }
      if (url.pathname === '/api/runtime') return sendJSON(res, { codexAvailable: !!executable, executable: executable?.display || null });
      if (url.pathname === '/api/models') {
        modelsPromise ||= listModels(executable, root).then(models => ({ models, defaultModel: DEFAULT_MODEL, defaultEffort: DEFAULT_EFFORT })).catch(error => { modelsPromise = null; throw error; });
        return sendJSON(res, await modelsPromise);
      }
      if (url.pathname === '/api/examples') {
        await scan();
        if (exampleIndexRevision !== prepared.revision) {
          exampleIndex = prepared.recipes.map(recipe=>exampleOverview(recipe,catalog));
          exampleIndexRevision = prepared.revision;
        }
        return sendJSON(res, { examples: exampleIndex, fingerprint: catalog.fingerprint, matches: relevantRecipes(design, catalog) });
      }
      if(url.pathname==='/api/example-preview') {
        await scan();
        const file=url.searchParams.get('path');
        if(!catalog.examplePaths.includes(file))throw new Error('Choose an example from the current checkout.');
        const recipe=prepared.recipes.find(item=>item.path===file);
        if(!recipe)throw new Error('This example is being indexed. Refresh Examples and try again.');
        return sendJSON(res,{example:exampleOverview(recipe,catalog),fingerprint:catalog.fingerprint,branch:catalog.git?.branch||null});
      }
      if(url.pathname==='/api/source-links')return await queue(async()=>{
        await syncAssembly();
        const request={};for(const key of ['nodeId','routineId'])if(url.searchParams.has(key))request[key]=url.searchParams.get(key);
        if(url.searchParams.has('edgeIndex'))request.edgeIndex=Number(url.searchParams.get('edgeIndex'));
        return sendJSON(res,await sourceLinks(sourceContext(),request));
      });
      if(url.pathname==='/api/flow-sources')return await queue(async()=>{await syncAssembly();return sendJSON(res,await flowSources(sourceContext()));});
      if(url.pathname==='/api/studio')return sendJSON(res,{app:'loom-feature-studio',workspace,root});
      if(url.pathname==='/api/codex/auth') {
        executable=await findCodex(codexPath);auth.executable=executable;runner.executable=executable;
        return sendJSON(res,await auth.check());
      }
      if (url.pathname === '/api/state') return await queue(async () => {
        await syncAssembly();
        return sendJSON(res, {
          design, designRevision: revisionOf(design), sourceFingerprint: catalog.fingerprint, sourceBranch: catalog.git?.branch || null,
          assembly, assemblyOrigin, lastBuild: assembly ? null : lastBuild, assemblyError, workspace, job: runner.snapshot(), codexAvailable: !!executable, previousDesignAvailable: !!previousDesignId,
          projects: projects.list(), history:await history.view(projects.list().activeId,design), deviceJob: arduino.snapshot(), preparation:preparationStatus(prepared,preparedContext(design,catalog,prepared)),
          outputDirectory: assembly ? path.join(workspace, 'outputs', revisionOf(design), 'FeatureApplication') : null
        });
      });
      if (url.pathname === '/api/download') {
        await syncAssembly();
        const downloadable = assembly || (url.searchParams.get('lastBuild') === 'true' ? lastBuild?.assembly : null);
        if (!downloadable) throw new Error('Assemble the current design or load a recipe before downloading its files.');
        const file = downloadable.files.find(f => f.name === url.searchParams.get('file'));
        if (!file) throw new Error('Choose a file from the current result.');
        res.writeHead(200, { 'Content-Type': 'text/plain; charset=utf-8', 'Content-Disposition': 'attachment; filename="' + file.name + '"', 'Cache-Control': 'no-store', 'X-Content-Type-Options': 'nosniff' });
        return res.end(file.content);
      }
      if (url.pathname === '/api/source') {
        const file = url.searchParams.get('file') || '';
        if (!/^(src|examples|docs)\//.test(file) || /secret|credential|creds/i.test(file) || !/\.(h|hpp|cpp|ino|html|md)$/.test(file)) return sendJSON(res, { error: 'Source not available.' }, 403);
        const candidate = path.resolve(root, file);
        const requestedRelative = path.relative(root, candidate);
        if (requestedRelative.startsWith('..') || path.isAbsolute(requestedRelative)) return sendJSON(res, { error: 'Source not available.' }, 403);
        const resolved = await realpath(candidate);
        const relative = path.relative(root, resolved);
        if (relative.startsWith('..') || path.isAbsolute(relative)) return sendJSON(res, { error: 'Source not available.' }, 403);
        res.writeHead(200, { 'Content-Type': 'text/plain; charset=utf-8', 'X-Content-Type-Options': 'nosniff' });
        res.end(await readFile(resolved));
        return;
      }
      const staticFiles = { '/history.js': ['history.js', 'text/javascript'], '/device-tools.js': ['device-tools.js', 'text/javascript'], '/': ['index.html', 'text/html'], '/app.js': ['app.js', 'text/javascript'], '/graph-layout.js': ['graph-layout.js', 'text/javascript'], '/execution-wiring.js': ['execution-wiring.js', 'text/javascript'], '/game-feedback.js': ['game-feedback.js', 'text/javascript'], '/game-audio.js': ['game-audio.js', 'text/javascript'], '/code-view.js': ['code-view.js', 'text/javascript'], '/style.css': ['style.css', 'text/css'] };
      staticFiles['/compatibility.js'] = ['compatibility.js', 'text/javascript'];
      staticFiles['/theme.js'] = ['theme.js', 'text/javascript'];
      staticFiles['/theme.css'] = ['theme.css', 'text/css'];
      staticFiles['/example-browser.js'] = ['example-browser.js', 'text/javascript'];
      staticFiles['/design-changes.js'] = ['design-changes.js', 'text/javascript'];
      staticFiles['/run-activity.js'] = ['run-activity.js', 'text/javascript'];
      staticFiles['/flow-navigation.js'] = ['flow-navigation.js', 'text/javascript'];
      staticFiles['/codex-account.js'] = ['codex-account.js', 'text/javascript'];
      staticFiles['/code-editor.js'] = ['code-editor.js', 'text/javascript'];
      const asset = staticFiles[url.pathname];
      if (!asset) return sendJSON(res, { error: 'Not found.' }, 404);
      res.writeHead(200, {
        'Content-Type': asset[1] + '; charset=utf-8', 'Cache-Control': 'no-cache',
        'X-Content-Type-Options': 'nosniff',
        'Content-Security-Policy': "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; connect-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'self'"
      });
      res.end(await readFile(path.join(here, 'public', asset[0])));
    } catch (e) {
      if (!res.headersSent) sendJSON(res, { error: e.message }, e.code === 'ENOENT' ? 404 : 400);
      else res.end();
    }
  });
  await new Promise((resolve, reject) => { server.once('error', reject); server.listen(port, '127.0.0.1', resolve); });
  boundPort = server.address().port;
  if(warmTools)void arduino.status({includePorts:false}).catch(()=>{});
  server.on('close', () => { runner.cancel('Local app stopped.'); arduino.cancel(); if(auth.child)auth.cancel(); });
  return { server, runner, url: 'http://127.0.0.1:' + boundPort, workspace, catalog };
}

if (process.argv[1] && import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href) {
  const args = process.argv.slice(2);
  const option = name => { const i = args.indexOf(name); return i === -1 ? undefined : args[i + 1]; };
  startStudio({
    root: option('--loom-root') || process.env.LOOM_SOURCE_ROOT,
    workspace: option('--workspace') || path.join(here, '.local'),
    port: Number(option('--port') || process.env.PORT || 4173),
    codexPath: option('--codex'),warmTools:true
  }).then(({ url, workspace, catalog, server }) => {
    console.log('Feature Studio: ' + url);
    console.log('Source: ' + catalog.root);
    console.log('Discovered ' + catalog.blocks.filter(b => b.kind === 'class').length + ' classes and ' + catalog.exampleFiles + ' examples.');
    console.log('Codex Desktop bridge: ' + workspace);
    const stop = () => server.close(() => process.exit(0));
    process.on('SIGINT', stop);
    process.on('SIGTERM', stop);
  }).catch(e => { console.error('Feature Studio could not start: ' + e.message); process.exitCode = 1; });
}
