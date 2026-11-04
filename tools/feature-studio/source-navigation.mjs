import path from 'node:path';
import { access, readFile, realpath } from 'node:fs/promises';
import { spawn } from 'node:child_process';
import { reconstructExample } from './examples.mjs';
import { revisionOf } from './core.mjs';
import { stripComments } from './adapters/cpp.mjs';
import {sourceLinks} from './source-links.mjs';

export async function sourceLocation({ root, workspace, catalog, design, assembly, assemblyOrigin, origin }, request) {
  let base = root, file, line = 1, exact = true;
  if (request.catalogId || request.source) {
    const block = catalog.blocks.find(b => b.id === request.catalogId);
    file = block?.source || request.source;
    const known = new Set([...catalog.blocks.map(b => b.source), ...catalog.examplePaths]);
    if (!file || !known.has(file)) throw new Error('Choose a source file from this checkout.');
    if (block?.className) {
      const source = stripComments(await readFile(path.join(root, file), 'utf8'));
      const offset = source.search(new RegExp('\\bclass\\s+' + block.className + '\\b'));
      if (offset >= 0) line = source.slice(0, offset).split('\n').length;
    }
  } else {
    if (!assembly) throw new Error('Assemble the application or load a recipe to open its code.');
    if (request.fileName && !assembly.files.some(f => f.name === request.fileName)) throw new Error('Unknown application file.');
    if (request.nodeId && !assembly.nodes.some(n => n.id === request.nodeId)) throw new Error('Unknown graph block.');
    if (request.edgeIndex !== undefined && (!Number.isInteger(request.edgeIndex) || !assembly.edges[request.edgeIndex])) throw new Error('Unknown graph connection.');
    if (!request.nodeId && request.edgeIndex === undefined && !request.routineId && !request.fileName) throw new Error('Choose a block, connection or application file.');
    if(!request.fileName&&(assemblyOrigin!=='example'||request.linkIndex!==undefined)) {
      const result=await sourceLinks({root,workspace,catalog,design,assembly,assemblyOrigin,origin},request);
      const i=request.linkIndex??0;
      if(!Number.isInteger(i)||i<0||!result.links[i])throw new Error('No verified source location is available for this explanation. Open the application file to review its context.');
      const linked=result.links[i];base=linked.scope==='repository'?root:path.join(workspace,'outputs',revisionOf(design),'FeatureApplication');
      file=linked.file;line=linked.line;exact=!['context','candidate'].includes(linked.match);
    } else
    if (assemblyOrigin === 'example' && origin?.path && catalog.examplePaths.includes(origin.path) && (!request.fileName || request.fileName.endsWith('.ino'))) {
      file = origin.path;
      const indexed = reconstructExample(await readFile(path.join(root, file), 'utf8'), file, catalog);
      // Old saved examples numbered only packet operations; match source meaning
      // when the newer index also contains initialization and callback calls.
      const originalNode = assembly.nodes.find(n => n.id === request.nodeId);
      const nodeId = !assembly.routines && originalNode ? indexed.assembly.nodes.find(n => n.reason === originalNode.reason)?.id : request.nodeId;
      const originalEdge = assembly.edges[request.edgeIndex];
      const edgeIndex = !assembly.routines && originalEdge ? indexed.assembly.edges.findIndex(e => e.label === originalEdge.label &&
        indexed.assembly.nodes.find(n => n.id === e.from)?.reason === assembly.nodes.find(n => n.id === originalEdge.from)?.reason &&
        indexed.assembly.nodes.find(n => n.id === e.to)?.reason === assembly.nodes.find(n => n.id === originalEdge.to)?.reason) : request.edgeIndex;
      const location = request.nodeId ? indexed.locations.nodes[nodeId] : indexed.locations.edges[edgeIndex];
      line = location?.line || 1;
    } else {
      const output = request.fileName ? assembly.files.find(f => f.name === request.fileName) : assembly.files.find(f => f.name.endsWith('.ino'));
      if (!output) throw new Error('Unknown application file.');
      base = path.join(workspace, 'outputs', revisionOf(design), 'FeatureApplication');
      file = output.name; exact = !!request.fileName;
    }
  }
  const basePath = await realpath(base), filename = await realpath(path.resolve(basePath, file));
  const relative = path.relative(basePath, filename);
  if (relative.startsWith('..') || path.isAbsolute(relative)) throw new Error('Source is outside the available project files.');
  return { path: filename, line, exact };
}

export async function openInVSCode(location) {
  const candidates = process.platform === 'win32' ? [
    path.join(process.env.LOCALAPPDATA || '', 'Programs', 'Microsoft VS Code', 'Code.exe'),
    path.join(process.env.ProgramFiles || '', 'Microsoft VS Code', 'Code.exe'),
    ...String(process.env.PATH || '').split(path.delimiter).filter(Boolean).flatMap(p => [path.join(p, 'Code.exe'), path.resolve(p, '..', 'Code.exe')])
  ] : String(process.env.PATH || '').split(path.delimiter).filter(Boolean).map(p => path.join(p, 'code'));
  let executable;
  for (const candidate of candidates) { try { await access(candidate); executable = candidate; break; } catch {} }
  if (!executable) throw new Error('VS Code was not found. Install it or add its launcher to PATH.');
  const env = { ...process.env }; delete env.ELECTRON_RUN_AS_NODE;
  await new Promise((resolve, reject) => {
    const child = spawn(executable, ['--reuse-window', '--goto', location.path + ':' + location.line], { shell: false, windowsHide: false, stdio: ['ignore', 'ignore', 'pipe'], env });
    let error = '', timer;
    const complete = () => { clearTimeout(timer); child.stderr.destroy(); child.unref(); resolve(); };
    child.stderr.on('data', chunk => { error = (error + chunk.toString()).slice(-1000); });
    child.once('error', e => { clearTimeout(timer); reject(e); });
    child.once('exit', code => {
      clearTimeout(timer);
      if (code && code !== 0) reject(new Error('VS Code could not open the file. ' + error.trim()));
      else complete();
    });
    child.once('spawn', () => { timer = setTimeout(complete, 1200); });
  });
}
