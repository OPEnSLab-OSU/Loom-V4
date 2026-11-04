import { layoutGraph, workflowPresentation, friendlyNode, graphRoutes, graphBounds, canvasFrame, visibleBlockSpot, supportingFeatures, filterProjectItems, featureBoardPresentation, executionOutline, NODE_W, NODE_H, GRID_X, GRID_Y } from './graph-layout.js';
import { installHoverHints, stretchControl, finishStretch } from './game-feedback.js';
import { renderCode, findCodeMatches, flowMatchesAtLine } from './code-view.js';
import { WaterSounds, controlSound } from './game-audio.js';
import { installDeviceTools } from './device-tools.js';
import { installHistory } from './history.js';
import { compatibilityForNode, compatibilityConcerns, assessmentLabels } from './compatibility.js';
import {filterExamples} from './example-browser.js';
import {designChanges} from './design-changes.js';
import {installRunActivity} from './run-activity.js';
import {stepConnections} from './flow-navigation.js';
import {installCodexAccount} from './codex-account.js';
import {installCodeEditor} from './code-editor.js';
const $ = id => document.getElementById(id);
const colors = { sensors: '#357c70', power: '#b38439', storage: '#8a6baa', network: '#4d7da3', radio: '#bd7554', outputs: '#6e729e', actuators: '#a97786', system: '#758582', intent: '#7c8382', custom: '#a56d83' };
const symbols = { sensors: '◉', power: '◷', storage: '▤', network: '⌁', radio: '◈', outputs: '↗', actuators: '⚙', system: '◇', intent: '✳' };
let canvasZoom = 1, canvasBounds = { x: 0, y: 0, w: 500, h: 350 }, canvasOffset = { x: 0, y: 0 }, zoomMotion, zoomFrame, soundEnabled = false;
const sounds = new WaterSounds();
let catalog, design, state, selected, category = 'all', view = 'requirements', fileIndex = 0;
let contentView = 'canvas';
let codeEditor;
let graph, positions = {}, timer, pendingSaves = Promise.resolve(), editEpoch = 0, pollBusy = false, localDirty = false, dragging = null;
let modelCatalog = [], examples = [], exampleGroup = 'all', lastAdded = null;
let recipeMatches = [];
let exampleSearchTimer,examplePreviewPath=null,examplePreviewRequest=0;
let builtDesign=null,builtDesignKey=null,changesCache=null;
let showConnections = false, flowPresentation;
const collapsedRoutines = new Set();
let branchInfo;
let projectBusy = false, deletingProject = null;
let codeJump = null;
let codeReturn = null;
let buildReturn = null;
let tracedEdge = null;
let outlineState = {key:null,expanded:new Set()};
let codeFindIndex=0,codeFindResults={matches:[],truncated:false},codeFindTimer;
let updateHistory = () => {};
let updateRunActivity = () => {};
const flowSourceCache = new Map();
const flowSourceKeys = new WeakMap();
function flowSourceKey(a) {
  const cached=flowSourceKeys.get(a);
  if(cached?.fingerprint===state.sourceFingerprint)return cached.key;
  const key=JSON.stringify([state.sourceFingerprint,a.files,a.sourceLinks,a.nodes,a.edges,a.routines]);
  flowSourceKeys.set(a,{fingerprint:state.sourceFingerprint,key});return key;
}
function flowSourceData(a=shownAssembly()) { return a && flowSourceCache.get(flowSourceKey(a))?.data; }
function sourceCaption(reference) {
  if(!reference)return '';
  const prefix=reference.match==='candidate'?'≈ ':reference.match==='context'?'Context · ':'';
  return prefix+reference.file.split('/').pop()+':'+reference.line+(reference.endLine>reference.line?'–'+reference.endLine:'');
}
function loadFlowSources(a=shownAssembly()) {
  if(!a)return;
  const key=flowSourceKey(a);if(flowSourceCache.has(key))return flowSourceCache.get(key).pending;
  const entry={};flowSourceCache.set(key,entry);
  entry.pending=api('/api/flow-sources').then(data=>{
    entry.data=data;
    if(shownAssembly()&&flowSourceKey(shownAssembly())===key){renderCanvas();if(view==='flow'&&selected)renderInspector();if($('find-project-dialog').open)renderProjectSearch();if($('flow-outline-dialog').open)renderFlowOutline();}
    while(flowSourceCache.size>4)flowSourceCache.delete(flowSourceCache.keys().next().value);
  }).catch(()=>flowSourceCache.delete(key));
  return entry.pending;
}
function annotatedGraph(a) {
  const data=flowSourceData(a);if(!data)return a;
  return {...a,edges:a.edges.map((edge,i)=>({...edge,sourceReference:data.edges[i],sourceCaption:sourceCaption(data.edges[i])}))};
}
async function jumpConnection(reference,location) {
  try{
    if(!location){const result=await api('/api/source-links?'+new URLSearchParams(reference));location=result.links.find(r=>r.scope==='application'||r.applicationFile)||result.links[0];if(location)location={...location,linkIndex:result.links.indexOf(location)};}
    if(!location)return toast('No source location is linked to this connection yet.');
    if(location.scope==='application'||location.applicationFile)jumpToCode(location);
    else await openSource({...reference,linkIndex:location.linkIndex});
  }catch(error){toast(error.message);}
}
const projectViews = new Map();
const shownAssembly = () => state.assembly || state.lastBuild?.assembly;
const lastBuildChange = () => shownAssembly()?.sourceFingerprint !== state.sourceFingerprint ? 'Loom checkout changed' : 'requests changed';
const shownOrigin = () => state.assembly ? state.assemblyOrigin : state.lastBuild?.assemblyOrigin;
function rememberBuiltDesign(next) {
  const assembly=next.assembly||next.lastBuild?.assembly,baseline=next.assembly?next.design:next.lastBuild?.design;
  const key=assembly?[next.projects?.activeId,assembly.designRevision,assembly.sourceFingerprint].join(':'):null;
  if(key!==builtDesignKey||!builtDesign&&baseline){builtDesignKey=key;builtDesign=baseline?structuredClone(baseline):null;changesCache=null;}
}
function currentDesignChanges() {
  const sourceChanged=!!shownAssembly()&&shownAssembly().sourceFingerprint!==state.sourceFingerprint,key=[editEpoch,builtDesignKey,sourceChanged].join(':');
  if(changesCache?.key===key)return changesCache.value;
  const value=designChanges(design,builtDesign,{sourceChanged});changesCache={key,value};return value;
}
function openBrief() { document.body.classList.add('brief-open'); $('toggle-brief').setAttribute('aria-expanded', 'true'); $('brief').focus(); }

function renderProjectTabs() {
  const projects = state.projects;
  if (!projects) return;
  const busy = projectBusy || state.job?.status === 'running' || state.deviceJob?.status === 'running';
  const key = JSON.stringify([projects.activeId, projects.openIds, projects.items.map(p => [p.id, p.id === projects.activeId ? design.name : p.name]), busy]);
  if (renderProjectTabs.key === key) return;
  renderProjectTabs.key = key;
  $('project-tabs').replaceChildren(...projects.openIds.map(id => {
    const project = projects.items.find(p => p.id === id), active = id === projects.activeId;
    const name = active ? design.name : project.name;
    return el('div', { class: 'project-tab' + (active ? ' active' : '') },
      el('button', { class: 'project-tab-name', 'aria-current': active ? 'page' : null, disabled: busy ? '' : null,
        onclick: () => changeProject('/api/projects/open', { id }) }, name),
      el('button', { class: 'project-tab-close', 'aria-label': 'Close ' + name, 'data-hint': 'Close this tab. The project stays saved in Recipes → My projects.',
        disabled: busy ? '' : null, onclick: () => changeProject('/api/projects/close', { id }) }, '×'));
  }), el('button', { class: 'project-tab-new', 'aria-label': 'New project tab', disabled: busy ? '' : null,
    onclick: () => changeProject('/api/design/new', {}, { blank: true }) }, '+'));
  $('project-tabs').querySelector('.project-tab.active')?.scrollIntoView({ block: 'nearest', inline: 'nearest' });
}
function applyProject(next) {
  state = next; design = next.design;
  rememberBuiltDesign(next);changesCache=null;
  tracedEdge = null;
  if($('code-flow-dialog').open)$('code-flow-dialog').close();
  if($('flow-outline-dialog').open)$('flow-outline-dialog').close();
  if($('design-changes-dialog').open)$('design-changes-dialog').close();
  if($('run-activity-dialog').open)$('run-activity-dialog').close();
  codeJump = null;
  codeReturn = null;
  buildReturn = null;
  clearTimeout(codeFindTimer);codeFindTimer=null;codeFindIndex=0;$('code-query').value='';$('code-search-bar').hidden=true;
  const remembered = projectViews.get(state.projects?.activeId);
  selected = null; view = remembered?.view || (shownAssembly() ? 'flow' : 'requirements');
  showConnections = remembered?.showConnections || false; fileIndex = remembered?.fileIndex || 0;
  collapsedRoutines.clear(); for (const id of remembered?.collapsed || []) collapsedRoutines.add(id);
  localDirty = false; category = 'all'; $('search').value = '';
  $('project-name').value = design.name; $('brief').value = design.description;
  document.body.classList.remove('brief-open'); $('toggle-brief').setAttribute('aria-expanded', 'false');
  setWorkspaceView(remembered?.contentView || 'canvas'); setTray(true);
  renderProjectTabs(); renderLibrary(); renderCanvas(); renderInspector(); renderOutput();
  requestAnimationFrame(() => {
    if(contentView!=='canvas')return;
    if (remembered) {
      canvasZoom = remembered.zoom; renderCanvas();
      $('canvas-scroll').scrollTo(remembered.left, remembered.top);
    } else if (view === 'flow') showFlowStart(); else fitCanvas();
  });
}
async function changeProject(route, body = {}, options = {}) {
  if (projectBusy) return false;
  stopZoom();
  projectBusy = true; renderProjectTabs();
  const workspace = document.querySelector('.workspace'), bar = document.querySelector('.projectbar');
  workspace.inert = true; bar.inert = true;
  try {
    await save();
    projectViews.set(state.projects?.activeId, { view, showConnections, fileIndex, zoom: canvasZoom,
      left: $('canvas-scroll').scrollLeft, top: $('canvas-scroll').scrollTop, collapsed: [...collapsedRoutines], contentView });
    editEpoch++;
    await api(route, body);
    applyProject(await api('/api/state'));
    if (options.blank) requestAnimationFrame(openBrief);
    if (options.message) toast(options.message);
    return true;
  } catch (error) { toast(error.message); return false; }
  finally { projectBusy = false; workspace.inert = false; bar.inert = false; updateStatus(); renderProjectTabs(); }
}

function el(tag, attrs = {}, ...children) {
  const e = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs)) {
    if (k === 'class') e.className = v;
    else if (k === 'text') e.textContent = v;
    else if (k.startsWith('on')) e.addEventListener(k.slice(2).toLowerCase(), v);
    else if (v !== null && v !== undefined) e.setAttribute(k, v);
  }
  for (const c of children.flat()) if (c !== null && c !== undefined) e.append(typeof c === 'string' ? document.createTextNode(c) : c);
  return e;
}
function ingredientIcon(category) {
  const paths = {
    sensors: ['M12 3v3m0 12v3M3 12h3m12 0h3', 'M7 7a7 7 0 1 0 10 0', 'M12 9a3 3 0 1 0 0 6a3 3 0 0 0 0-6'],
    power: ['M15 3a9 9 0 1 0 6 13A8 8 0 0 1 15 3Z', 'M18 4v4m-2-2h4'],
    storage: ['M5 6c0-4 14-4 14 0s-14 4-14 0Z', 'M5 6v12c0 4 14 4 14 0V6', 'M5 12c0 4 14 4 14 0'],
    network: ['M3 8a15 15 0 0 1 18 0', 'M6 12a10 10 0 0 1 12 0', 'M9 16a5 5 0 0 1 6 0', 'M12 20h.01'],
    radio: ['M12 13v8m-4 0h8', 'M9 9a4 4 0 0 1 6 0', 'M6 6a8 8 0 0 0 0 10m12-10a8 8 0 0 1 0 10', 'M12 10h.01'],
    outputs: ['M5 15v4h14v-4', 'M12 15V3m-5 5l5-5l5 5'],
    actuators: ['M9 3h6l1 4l4 2v6l-4 2l-1 4H9l-1-4l-4-2V9l4-2Z', 'M12 9a3 3 0 1 0 0 6a3 3 0 0 0 0-6'],
    system: ['M12 2l9 5v10l-9 5l-9-5V7Z', 'M3 7l9 5l9-5m-9 5v10'],
    intent: ['M12 2l2.5 7.5L22 12l-7.5 2.5L12 22l-2.5-7.5L2 12l7.5-2.5Z'],
    custom: ['M6 3h12v18l-6-4l-6 4Z', 'M9 8h6m-6 4h6']
  };
  const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
  for (const [k, v] of Object.entries({ viewBox: '0 0 24 24', fill: 'none', stroke: 'currentColor', 'stroke-width': 1.7, 'stroke-linecap': 'round', 'stroke-linejoin': 'round', 'aria-hidden': 'true' })) svg.setAttribute(k, v);
  for (const d of paths[category] || paths.system) {
    const p = document.createElementNS(svg.namespaceURI, 'path'); p.setAttribute('d', d); svg.append(p);
  }
  return svg;
}
async function clickSound(kind = 'click') {
  if (!soundEnabled) return;
  try {
    const result = await sounds.play(kind);
    $('sound-toggle').dataset.audioState = result?.state || 'running';
    $('sound-toggle').dataset.lastSound = kind;
    if (kind === 'preview') toast('Playing a water-drop preview. If it is silent, check the app or system volume.');
  } catch (e) { $('sound-toggle').dataset.audioState = 'blocked'; toast(e.message); }
}
function installGameFeedback() {
  try { soundEnabled = localStorage.getItem('feature-studio-sound') === 'on'; } catch {}
  const soundLabel = () => {
    $('sound-toggle').textContent = soundEnabled ? '♪ Sounds on' : '♪ Sounds off';
    $('sound-toggle').setAttribute('aria-pressed', String(soundEnabled));
  };
  soundLabel();
  $('sound-toggle').onclick = () => {
    soundEnabled = !soundEnabled; soundLabel(); if (soundEnabled) clickSound('preview');
    try { localStorage.setItem('feature-studio-sound', soundEnabled ? 'on' : 'off'); } catch {}
  };
  $('sound-preview').onclick = () => { soundEnabled = true; soundLabel(); clickSound('preview'); try { localStorage.setItem('feature-studio-sound', 'on'); } catch {} };
  document.addEventListener('pointerdown', event => {
    const target = event.target.closest('button:not(:disabled), .node, .wire-label, .utility-menu>summary');
    if (!target) return;
    if (!target.matches('#sound-toggle,#sound-preview')) clickSound(controlSound(target));
    if (!target.matches('.wire-label')) stretchControl(target, true);
  }, { capture: true });
}
const meta = b => catalog.blocks.find(c => c.id === b.catalogId);
const fieldLabel = s => s.replace(/_/g, ' ').replace(/([a-z])([A-Z])/g, '$1 $2').replace(/^./, c => c.toUpperCase());
function toast(message) {
  $('toast').textContent = message; $('toast').classList.add('show');
  clearTimeout(toast.timer); toast.timer = setTimeout(() => $('toast').classList.remove('show'), 4500);
}
async function api(url, body) {
  let response;
  try { response = await fetch(url, body === undefined ? { cache: 'no-store' } : { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }); }
  catch { throw new Error('Studio is reconnecting. Try again in a moment.'); }
  const result = await response.json();
  if (!response.ok) throw new Error(result.error || 'Local server request failed.');
  return result;
}
async function openSource(reference) {
  try {
    const location = await api('/api/open-source', reference);
    toast(location.exact ? 'Opened in VS Code · line ' + location.line : 'Opened matching source context in VS Code · line ' + location.line);
  } catch (e) { toast(e.message); }
}
function sourceButton(label, reference, className = 'text-button source-button') {
  return el('button', { class: className, 'data-hint-title': 'Open in VS Code', 'data-hint': label,
    onclick: e => { e.stopPropagation(); openSource(reference); } }, label);
}
function jumpToCode(link) {
  const name=link.applicationFile || link.file, assembly=shownAssembly();
  const index=assembly?.files.findIndex(f=>f.name===name);
  if(index===undefined||index<0)return toast('This reference belongs to the library. Expand Under the hood to read it.');
  if(contentView==='canvas') {
    stopZoom();const area=$('canvas-scroll');
    area.scrollTo({left:area.scrollLeft,top:area.scrollTop,behavior:'instant'});
    codeReturn={projectId:state.projects?.activeId,view,selected,showConnections,zoom:canvasZoom,
      worldLeft:(area.scrollLeft-canvasOffset.x)/canvasZoom,worldTop:(area.scrollTop-canvasOffset.y)/canvasZoom};
  }
  fileIndex=index;
  codeJump={projectId:state.projects?.activeId,file:name,line:link.line,endLine:link.endLine};
  if($('source-dialog').open)$('source-dialog').close();
  setWorkspaceView('code');
  renderOutput();
  requestAnimationFrame(()=>{
    const code=$('code'),target=code.querySelector('[data-line="'+link.line+'"]');
    if(!target)return;
    code.focus({preventScroll:true});
    code.scrollTo({top:code.scrollTop+target.getBoundingClientRect().top-code.getBoundingClientRect().top-code.clientHeight*.25,left:0,
      behavior:matchMedia('(prefers-reduced-motion: reduce)').matches?'auto':'smooth'});
  });
}
async function renderSourceLinks(container,reference,showJump=true) {
  container.replaceChildren(el('p',{class:'small muted'},'Finding linked code…'));
  try {
    const result=await api('/api/source-links?'+new URLSearchParams(reference));
    if(!container.isConnected)return;
    container.replaceChildren();
    if(!result.links.length)container.append(el('p',{class:'small'},'No exact code reference could be matched. Review the application file; future assemblies include explicit source anchors.'));
    const inSketch=link=>link.scope==='application'||!!link.applicationFile;
    const entries=result.links.map((link,i)=>({link,i})),sketch=entries.filter(e=>inSketch(e.link)),implementation=entries.filter(e=>!inSketch(e.link));
    if(showJump&&sketch.length)container.append(el('button',{class:'quiet jump-to-code',onclick:()=>jumpToCode(sketch[0].link)},
      sketch[0].link.match==='candidate'||sketch[0].link.match==='context'?'Jump to suggested sketch code':'Jump to sketch code'));
    const card=({link,i},open=false)=>{
      const name=link.file.split('/').pop(),range=link.line===link.endLine?link.line:link.line+'–'+link.endLine;
      const code=el('pre',{class:'source-excerpt'});renderCode(code,link.excerpt,link.file);
      const details=el('details',{class:'source-reference'},el('summary',{},
        el('span',{class:'node-badge'},link.scope==='application'?'SKETCH':link.file.startsWith('examples/')?'EXAMPLE':'UNDER THE HOOD'),el('strong',{},link.symbol?link.symbol+'()':name),el('small',{},name+':'+range)),
        el('p',{class:'small'},link.explanation),el('div',{class:'path small'},link.file),
        !['mapped','matched'].includes(link.match)?el('p',{class:'small muted'},link.match==='candidate'?'Suggested location · inferred from the current code.':'Routine context · exact operation not identified.'):null,
        code,el('div',{class:'source-ref-actions'},inSketch(link)?el('button',{class:'quiet compact',onclick:()=>jumpToCode(link)},'Jump to these lines'):null,
          sourceButton('Open in VS Code ↗',{...reference,linkIndex:i})));
      details.open=open;return details;
    };
    sketch.forEach((entry,i)=>container.append(card(entry,i===0)));
    if(implementation.length)container.append(el('details',{class:'under-the-hood'},el('summary',{},'Under the hood · '+implementation.length+' source references'),
      el('p',{class:'small muted'},'Library implementations that explain delegated behavior.'),...implementation.map(entry=>card(entry))));
    for(const issue of result.issues)container.append(el('p',{class:'small muted'},'Unresolved reference: '+issue));
  } catch(e){if(container.isConnected)container.replaceChildren(el('p',{class:'small muted'},e.message),el('button',{class:'quiet',onclick:()=>renderSourceLinks(container,reference)},'Retry linked code'));}
}
function showSourceDetails(reference,title) {
  $('source-dialog-title').textContent=title;
  $('source-dialog').showModal();renderSourceLinks($('source-dialog-content'),reference);
}
function changed(invalidateAssembly = true) {
  editEpoch++; localDirty = true; $('save-status').textContent = 'Saving…';
  if (invalidateAssembly) {
    if (state.assembly) state.lastBuild = { design:builtDesign?structuredClone(builtDesign):null,assembly: state.assembly, assemblyOrigin: state.assemblyOrigin };
    state.assembly = null; state.assemblyOrigin = null;
    $('inspector').querySelectorAll('.compatibility-card').forEach(card=>card.remove());
    $('brief-review').replaceChildren();
    $('toggle-brief').classList.remove('compatibility-conflict','compatibility-question');
    $('toggle-brief').querySelector('small').textContent='Edit';
  }
  updateStatus(); renderOutput(); renderProjectTabs();
  clearTimeout(timer); timer = setTimeout(() => save().catch(e => toast(e.message)), 400);
  if (view === 'flow' && invalidateAssembly) renderCanvas();
}
function save() {
  clearTimeout(timer);
  const snapshot = structuredClone(design), epoch = editEpoch, projectId = state.projects?.activeId;
  pendingSaves = pendingSaves.catch(() => {}).then(async () => {
    const result = await api('/api/design', projectId ? { projectId, design: snapshot } : snapshot);
    if (epoch === editEpoch) {
      state.designRevision = result.designRevision;
      if (result.assemblyRetained && shownAssembly()) shownAssembly().designRevision = result.designRevision;
      localDirty = false;
      $('save-status').textContent = 'Saved locally';
      state.history=result.history;updateHistory();
    }
  }).catch(e => { $('save-status').textContent = 'Save failed'; localDirty = true; throw e; });
  return pendingSaves;
}
function updateSource() {
  $('source-state').textContent = catalog.blocks.filter(c => c.kind === 'class').length + ' classes · live checkout';
  $('library-count').textContent = catalog.blocks.length;
  $('info-content').replaceChildren(
    el('p', {}, 'Feature Studio reads the selected source tree each time it starts, when you rescan, and periodically while open. It has no fixed Loom release or branch map.'),
    el('div', { class: 'path' }, catalog.root),
    el('h3', {}, 'Discovered now'),
    el('p', {}, catalog.sourceFiles + ' source files · ' + catalog.exampleFiles + ' examples · ' + catalog.blocks.filter(c => c.kind === 'class').length + ' classes'),
    el('div', { class: 'path' }, 'Source fingerprint: ' + catalog.fingerprint),
    el('h3', {}, 'Discovery is an index, not a compatibility claim'),
    el('p', {}, 'Header extraction exposes constructor choices and settings. Codex still reviews the implementations and examples to resolve hardware, APIs, power, data routes and missing dependencies.'),
    el('h3', {}, 'Application-neutral document'),
    el('p', {}, 'The canvas stores features, settings, instance counts and a brief. The Loom adapter supplies source discovery and Feather M0 guidance. Another source adapter can use the same graph and file handoff contract.'),
    el('h3', {}, 'Local Codex connection'),
    el('div', { class: 'path' }, state.workspace),
    el('p', {}, 'The assembly button launches your installed Codex CLI in this repository with the selected model and permission mode. The app waits for a structured result and displays it automatically. An optional file handoff can use an existing Desktop chat instead.')
  );
}
async function loadBranches() {
  branchInfo = await api('/api/branches');
  const select = $('source-branch');
  const current = branchInfo.branch || (branchInfo.available ? 'Detached · ' + branchInfo.revision : 'No Git checkout');
  select.replaceChildren(...[...new Set([current, ...branchInfo.branches])].map(name => el('option', { value: name }, name)));
  select.value = current; select.disabled = !branchInfo.canSwitch;
  $('switch-branch').hidden = true;
  select.dataset.hint = branchInfo.canSwitch ? 'Current Loom branch. Choose a local branch, then press Switch. Git protects conflicting local edits.' : 'Current source checkout branch.';
}
function renderLibrary() {
  $('library-count').textContent = catalog.blocks.length;
  const counts = {};
  for (const c of catalog.blocks) counts[c.category] = (counts[c.category] || 0) + 1;
  const categoryScroll = $('category-list').scrollLeft;
  $('category-list').replaceChildren(...[['all', 'All'], ...Object.entries(catalog.categories).filter(([id]) => counts[id] || id === 'custom')].map(([id, label]) =>
    el('button', { class: category === id ? 'active' : '', 'data-hint-title': id === 'intent' ? label : label + ' ingredients', 'data-hint': id === 'intent' ? 'Add a new feature described in your own words.' : 'Browse ' + label.toLowerCase() + ' ingredients.', 'data-hint-category': id, onclick: () => { if (id === 'intent') return addBlock('intent:custom'); category = id; renderLibrary(); } }, ingredientIcon(id === 'all' ? 'intent' : id), el('span', {}, label))
  ));
  $('category-list').scrollLeft = categoryScroll;
  const query = $('search').value.toLowerCase().trim();
  const blocks = catalog.blocks.filter(c => (category === 'all' || c.category === category) && (c.title + ' ' + (c.className || '') + ' ' + c.description).toLowerCase().includes(query));
  blocks.sort((a, b) => (a.kind === 'intent' ? 0 : 1) - (b.kind === 'intent' ? 0 : 1) || a.title.localeCompare(b.title));
  $('catalog').replaceChildren(...blocks.map(c => {
    const card = el('button', { class: 'catalog-card', draggable: 'true', 'data-hint-title': c.title, 'data-hint': c.description, 'data-hint-category': c.category, 'data-hint-action': 'Click to add · Or drag onto the canvas', 'aria-label': 'Add ' + c.title, onclick: () => addBlock(c.id) },
      el('span', { class: 'block-icon' }, ingredientIcon(c.category)),
      el('span', {}, el('strong', {}, c.title), el('small', {}, c.saved ? 'Saved behavior' : c.kind === 'intent' ? 'Desired capability' : c.className))
    );
    card.style.setProperty('--block-color', colors[c.category] || colors.system);
    card.addEventListener('dragstart', event => {
      event.dataTransfer.setData('application/x-feature-studio', c.id);
      event.dataTransfer.effectAllowed = 'copy';
    });
    return card;
  }));
  const quick = Object.entries(catalog.categories).filter(([id]) => counts[id] || id === 'custom');
  $('quick-add').replaceChildren(...quick.map(([id, label]) => {
    const button = el('button', { class: 'quick-ingredient', 'data-hint-title': label,
      'data-hint': id === 'intent' ? 'Add a new feature described in your own words.' : id === 'custom' ? 'Reuse your saved feature descriptions.' : 'Browse ' + counts[id] + ' ' + label.toLowerCase() + ' ingredients from this checkout.',
      'data-hint-category': id, 'data-hint-action': id === 'intent' ? 'Describe behavior · Codex fills in the implementation' : 'Open category · Choose an ingredient to add',
      'aria-label': id === 'intent' ? 'Describe a feature' : 'Browse ' + label, onclick: () => {
        if (id === 'intent') return addBlock('intent:custom');
        category = id; $('search').value = ''; setTray(false); renderLibrary();
      } }, ingredientIcon(id));
    button.style.setProperty('--block-color', colors[id]);
    return button;
  }));
  if (!blocks.length) $('catalog').append(el('p', { class: 'small muted' }, category === 'custom' ? 'No saved features yet. Describe a feature, then choose Save to Custom in its settings.' : 'No matching blocks. Describe a feature to add what is missing.'));
}
function isFeatureNote(block){return !!block&&Object.hasOwn(block.settings||{},'requirement');}
function noteStorageKey(block){return 'feature-studio:note:'+state.projects?.activeId+':'+block.id;}
function noteIsOpen(block){try{return isFeatureNote(block)&&localStorage.getItem(noteStorageKey(block))!=='closed';}catch{return isFeatureNote(block);}}
function blockSize(block){return noteIsOpen(block)?{w:300,h:230}:{w:NODE_W,h:NODE_H};}
function addBlock(catalogId, position) {
  const c = catalog.blocks.find(b => b.id === catalogId);
  if (!c) return;
  setWorkspaceView('canvas');
  const cols = Math.max(1, Math.floor(($('canvas-scroll').clientWidth / canvasZoom - 50) / GRID_X)), i = design.blocks.length;
  if (!position) {
    for (let slot = 0; slot < (i + 1) * cols + 1; slot++) {
      const candidate = { x: 35 + (slot % cols) * GRID_X, y: 80 + Math.floor(slot / cols) * GRID_Y };
      const nextSize=Object.hasOwn(c.defaults||{},'requirement')?{w:300,h:230}:{w:NODE_W,h:NODE_H};
      const overlaps = design.blocks.some(b => {const p=b.position,size=blockSize(b);return p&&p.x<candidate.x+nextSize.w+20&&p.x+size.w+20>candidate.x&&p.y<candidate.y+nextSize.h+20&&p.y+size.h+20>candidate.y;});
      if (!overlaps) { position = candidate; break; }
    }
  }
  const b = {
    id: crypto.randomUUID(), catalogId, label: c.title, count: c.savedCount || 1,
    settings: structuredClone(c.defaults || {}), notes: c.savedNotes || '',
    position: position || { x: 35 + (i % cols) * GRID_X, y: 80 + Math.floor(i / cols) * GRID_Y }
  };
  design.blocks.push(b); selected = b.id; lastAdded = b.id; view = 'requirements'; changed(); renderCanvas(); renderInspector();
  requestAnimationFrame(() => revealNewBlock(b.id));
}
function revealNewBlock(id) {
  if (!positions[id]) return;
  stopZoom();
  const area = $('canvas-scroll'),size=blockSize(design.blocks.find(block=>block.id===id));
  const rect = element => { const b=element.getBoundingClientRect(); return {x:b.left,y:b.top,w:b.width,h:b.height}; };
  const spot = () => {
    const viewport=rect(area), p=positions[id];
    const obstacles=[document.querySelector('.library'),document.querySelector('.brief-box'),document.querySelector('.canvas-controls'),document.querySelector('.inspector')].map(rect);
    return visibleBlockSpot(viewport,obstacles,{w:size.w*canvasZoom+16,h:size.h*canvasZoom+22},
      {x:viewport.x+canvasOffset.x+p.x*canvasZoom-area.scrollLeft-8,y:viewport.y+canvasOffset.y+p.y*canvasZoom-area.scrollTop-10});
  };
  applyZoom();
  document.querySelector('.main-pane').style.setProperty('--canvas-top', area.offsetTop + 'px');
  let target=spot();
  if(!target) { setTray(true); target=spot(); }
  if(!target) {
    canvasZoom=Math.max(.2,Math.min(canvasZoom,1,(area.clientWidth-50)/size.w,(area.clientHeight-100)/size.h));
    applyZoom(); target=spot();
  }
  if(!target) {
    document.body.classList.remove('brief-open'); $('toggle-brief').setAttribute('aria-expanded','false'); target=spot();
  }
  if(!target) return;
  const viewport=rect(area), p=positions[id];
  area.scrollTo({left:canvasOffset.x+p.x*canvasZoom-(target.x+8-viewport.x),
    top:canvasOffset.y+p.y*canvasZoom-(target.y+10-viewport.y),
    behavior:matchMedia('(prefers-reduced-motion: reduce)').matches?'instant':'smooth'});
}
function briefSummary(b) {
  const c = meta(b), entries = Object.entries(b.settings).filter(([k]) => !['constructorIndex', 'parameters'].includes(k));
  if (b.notes.trim()) return b.notes;
  if (b.settings.parameters && Object.keys(b.settings.parameters).length) return Object.entries(b.settings.parameters).map(([k, v]) => fieldLabel(k) + ': ' + v).slice(0, 2).join(' · ');
  if (entries.length) return entries.slice(0, 2).map(([k, v]) => fieldLabel(k) + ': ' + v).join(' · ');
  return c?.description || 'Missing from current source; Codex must resolve this feature.';
}
function projectSearchItems() {
  const assembly=shownAssembly(),supplied=supportingFeatures(design,assembly),features=[...design.blocks,...supplied];
  const featureIds=new Set(features.map(feature=>feature.id)),steps=new Set((assembly?.routines||[]).flatMap(r=>r.nodeIds));
  const items=features.map(feature=>{
    const definition=feature.catalogId?meta(feature):null;
    return {id:feature.id,kind:'feature',label:feature.label,category:feature.category||definition?.category||'intent',
      context:feature.added?'Added component':'Requested feature',description:feature.reason||briefSummary(feature),
      searchText:definition?.className||''};
  });
  for(const node of assembly?.nodes||[]) {
    if(!steps.has(node.id)&&(assembly.routines?.length||featureIds.has(node.id)))continue;
    const appearance=friendlyNode(node),routine=assembly.routines?.find(r=>r.nodeIds.includes(node.id));
    const owners=flowSourceData()?.nodes[node.id]?.owners||[];
    items.push({id:node.id,kind:'step',label:appearance.label,description:appearance.reason,category:appearance.category,
      context:(!state.assembly?'Previous assembly · ':'')+(routine?routine.name+'() · step '+(routine.nodeIds.indexOf(node.id)+1):'Workflow step'),
      searchText:[node.label,node.reason,...owners.map(owner=>owner.object+' '+owner.className+' '+(owner.method||''))].join(' ')});
  }
  return items;
}
function openProjectResult(item) {
  if(item.kind==='feature') {
    if(![...design.blocks,...supportingFeatures(design,shownAssembly())].some(feature=>feature.id===item.id))return renderProjectSearch();
    view='requirements';
  } else {
    const assembly=shownAssembly();if(!assembly?.nodes.some(node=>node.id===item.id))return renderProjectSearch();
    view='flow';showConnections=false;
    const routine=assembly.routines?.find(r=>r.nodeIds.includes(item.id));if(routine)collapsedRoutines.delete(routine.id);
  }
  $('find-project-dialog').close();selected=item.id;setWorkspaceView('canvas');renderCanvas();renderInspector();
  requestAnimationFrame(()=>{
    canvasZoom=Math.max(.9,canvasZoom);applyZoom();revealNewBlock(item.id);
    [...$('nodes').children].find(node=>node.dataset.id===item.id)?.focus({preventScroll:true});
  });
}
function renderProjectSearch() {
  const matches=filterProjectItems(projectSearchItems(),$('project-query').value),list=$('project-find-results');list.replaceChildren();
  $('project-find-count').textContent=matches.length>50?'Showing 50 of '+matches.length+' matches · narrow the search':matches.length+' '+(matches.length===1?'result':'results');
  for(const item of matches.slice(0,50))list.append(el('li',{},el('button',{class:'project-result','data-hint-category':item.category,onclick:()=>openProjectResult(item)},
    el('span',{class:'block-icon','aria-hidden':'true'},ingredientIcon(item.category)),el('span',{},el('strong',{},item.label),el('small',{},item.context),el('span',{class:'project-result-description'},item.description)))));
  if(!matches.length)list.append(el('li',{class:'small muted'},design.blocks.length?'No matching feature or step. Try a name, class or API call.':'Add a feature or open an example to search this project.'));
}
function openProjectSearch() {
  loadFlowSources();
  $('project-query').value='';renderProjectSearch();$('find-project-dialog').showModal();$('project-query').focus();
}
function draftGraph() {
  const nodes = design.blocks.map(b => ({ id: b.id, label: b.label, category: meta(b)?.category || 'intent', added: false, reason: briefSummary(b), requestIds: [b.id] }));
  const edges = [];
  const sensing = nodes.filter(n => n.category === 'sensors');
  const receivers = design.blocks.filter(b => b.catalogId === 'intent:receiver').map(b => nodes.find(n => n.id === b.id));
  const storage = nodes.filter(n => n.category === 'storage');
  const reports = nodes.filter(n => n.category === 'outputs');
  const sources = [...sensing, ...receivers];
  if (sources.length) {
    nodes.push({ id: 'preview-packet', label: 'Collect & package', category: 'system', added: false, suggested: true, reason: 'Intent preview only. Codex determines packet ownership, identity and sequencing.', requestIds: [] });
    for (const n of sources) edges.push({ from: n.id, to: 'preview-packet', kind: 'data', label: receivers.includes(n) ? 'received payload' : 'measurements' });
    for (const n of [...storage, ...reports]) edges.push({ from: 'preview-packet', to: n.id, kind: 'data', label: 'requested route' });
  }
  for (const n of nodes.filter(n => n.category === 'network')) for (const r of reports.filter(r => !design.blocks.find(b => b.id === r.id)?.catalogId.includes('serial'))) edges.push({ from: n.id, to: r.id, kind: 'dependency', label: 'transport candidate' });
  for (const n of nodes.filter(n => n.category === 'power')) for (const s of sources.slice(0, 1)) edges.push({ from: n.id, to: s.id, kind: 'cycle', label: 'requested cadence' });
  return { nodes, edges, preview: true };
}
function setCanvasSize(nodes, pos, routes = [], sections = []) {
  const before = canvasOffset;
  canvasBounds = graphBounds(pos, routes, sections);
  if(view==='requirements')for(const block of design.blocks)if(pos[block.id]) {
    const size=blockSize(block),p=pos[block.id];
    canvasBounds.w=Math.max(canvasBounds.w,p.x+size.w+28-canvasBounds.x);
    canvasBounds.h=Math.max(canvasBounds.h,p.y+size.h+28-canvasBounds.y);
  }
  $('canvas').style.width = Math.max(300, canvasBounds.x + canvasBounds.w) + 'px';
  $('canvas').style.height = Math.max(220, canvasBounds.y + canvasBounds.h) + 'px';
  applyZoom();
  if (canvasOffset.x !== before.x) $('canvas-scroll').scrollLeft += canvasOffset.x - before.x;
  if (canvasOffset.y !== before.y) $('canvas-scroll').scrollTop += canvasOffset.y - before.y;
}
function applyZoom() {
  if(contentView!=='canvas')return;
  const area = $('canvas-scroll');
  // Screen-space slack remains generous even when the graph is zoomed out.
  // A whole viewport below the content lets every label clear an open dock.
  const frame = canvasFrame(canvasBounds, canvasZoom, area.clientWidth, area.clientHeight);
  canvasOffset = { x: frame.x, y: frame.y };
  $('canvas').style.transform = `translate(${canvasOffset.x}px,${canvasOffset.y}px) scale(${canvasZoom})`;
  $('canvas-sizer').style.width = frame.width + 'px';
  $('canvas-sizer').style.height = frame.height + 'px';
  $('zoom-value').textContent = Math.round(canvasZoom * 100) + '%';
}
function stopZoom() { cancelAnimationFrame(zoomFrame); zoomMotion = null; }
function fitCanvas() {
  stopZoom();
  const scroll = $('canvas-scroll'), area = scroll.getBoundingClientRect();
  const dock = document.querySelector('.library').getBoundingClientRect();
  const top = 58, bottom = Math.max(80, area.bottom - dock.top + 20);
  const height = Math.max(80, area.height - top - bottom);
  canvasZoom = Math.max(.08, Math.min(1, (area.width - 48) / canvasBounds.w, height / canvasBounds.h));
  applyZoom();
  scroll.scrollTo({ left: canvasOffset.x + canvasBounds.x * canvasZoom - (area.width - canvasBounds.w * canvasZoom) / 2,
    top: canvasOffset.y + canvasBounds.y * canvasZoom - top - (height - canvasBounds.h * canvasZoom) / 2, behavior: 'smooth' });
}
function showFlowStart(sectionId) {
  if (!graph?.routines?.length || showConnections || view !== 'flow') return fitCanvas();
  const section = flowPresentation.sections.find(s=>s.id===sectionId)||flowPresentation.sections[0], area = $('canvas-scroll');
  stopZoom();
  canvasZoom = Math.min(1, Math.max(.75, (area.clientWidth - 52) / section.w));
  applyZoom();
  const inset=Math.max(26,(area.clientWidth-section.w*canvasZoom)/2);
  area.scrollTo({ left: canvasOffset.x + section.x * canvasZoom - inset,
    top: canvasOffset.y + section.y * canvasZoom - 58, behavior: 'smooth' });
}
function zoomAt(value, point) {
  const scroll = $('canvas-scroll'), area = scroll.getBoundingClientRect();
  scroll.scrollTo({ left: scroll.scrollLeft, top: scroll.scrollTop, behavior: 'instant' });
  const x = point?.clientX == null ? area.width / 2 : point.clientX - area.left;
  const y = point?.clientY == null ? area.height / 2 : point.clientY - area.top;
  zoomMotion = { target: Math.max(.08, Math.min(3, value)), x, y,
    worldX: (scroll.scrollLeft + x - canvasOffset.x) / canvasZoom,
    worldY: (scroll.scrollTop + y - canvasOffset.y) / canvasZoom };
  cancelAnimationFrame(zoomFrame);
  let previous;
  const step = time => {
    if (!zoomMotion) return;
    const m = zoomMotion, dt = previous == null ? 16 : Math.min(48, time - previous); previous = time;
    const done = Math.abs(m.target - canvasZoom) < .0005 || matchMedia('(prefers-reduced-motion: reduce)').matches;
    canvasZoom = done ? m.target : canvasZoom + (m.target - canvasZoom) * (1 - Math.exp(-dt / 65));
    applyZoom();
    scroll.scrollLeft = canvasOffset.x + m.worldX * canvasZoom - m.x;
    scroll.scrollTop = canvasOffset.y + m.worldY * canvasZoom - m.y;
    if (done) zoomMotion = null;
    else zoomFrame = requestAnimationFrame(step);
  };
  zoomFrame = requestAnimationFrame(step);
}
function syncWorkspaceTabs() {
  const editor=contentView!=='canvas';
  document.body.classList.toggle('editor-view',editor);
  document.body.classList.toggle('notes-view',contentView==='notes');
  document.querySelector('.output').hidden=!editor;
  $('code-panel').hidden=contentView!=='code';$('decisions-panel').hidden=contentView!=='notes';
  document.querySelector('.output-view-title').textContent=contentView==='notes'?'Assembly notes':'Application code';
  for(const [id,active]of [['requirements-tab',!editor&&view==='requirements'],['flow-tab',!editor&&view==='flow'],['code-tab',contentView==='code'],['decisions-tab',contentView==='notes']]){
    if($(id).classList.contains('active')!==active)finishStretch($(id));
    $(id).classList.toggle('active',active);$(id).setAttribute('aria-selected',String(active));$(id).tabIndex=active?0:-1;
  }
  $('canvas-scroll').setAttribute('aria-labelledby',view==='flow'?'flow-tab':'requirements-tab');
}
function setWorkspaceView(next) {
  if(next!=='code'){clearTimeout(codeFindTimer);codeFindTimer=null;}
  contentView=next;syncWorkspaceTabs();
  if(next==='canvas')requestAnimationFrame(applyZoom);
}
function renderCanvas() {
  syncWorkspaceTabs();
  updateStatus();
  $('block-count').textContent=design.blocks.length+supportingFeatures(design,shownAssembly()).length;
  const briefReview=state.assembly?.featureAssessments?.find(r=>r.requestId==='$brief'&&r.status!=='supported');
  $('toggle-brief').classList.toggle('compatibility-conflict',!!briefReview&&briefReview.status!=='needs-info');
  $('toggle-brief').classList.toggle('compatibility-question',briefReview?.status==='needs-info');
  $('toggle-brief').querySelector('small').textContent=briefReview?assessmentLabels[briefReview.status]:'Edit';
  $('brief-review').replaceChildren(...(briefReview?[assessmentCard(briefReview)]:[]));
  if(contentView!=='canvas')return;
  loadFlowSources();
  $('canvas-caption').hidden = view === 'requirements' && design.blocks.length < 2;
  document.body.classList.toggle('workflow-simple', view === 'flow' && !showConnections);
  const supplied = supportingFeatures(design,shownAssembly());
  $('flow-outline').hidden=view!=='flow'||!shownAssembly()?.routines?.length;
  $('block-count').textContent = design.blocks.length + supplied.length;
  $('block-count').dataset.hint = design.blocks.length+' requested · '+supplied.length+' supporting features added by Codex';
  $('flow-detail').hidden = view !== 'flow';
  $('flow-detail').setAttribute('aria-pressed', String(showConnections));
  $('flow-detail').querySelector('.control-label').textContent = showConnections ? 'Workflow only' : 'All connections';
  $('empty').hidden = design.blocks.length > 0 || !!shownAssembly()?.nodes.length;
  $('nodes').replaceChildren(); $('connections').replaceChildren();
  $('canvas').querySelectorAll('.layer-heading,.flow-section').forEach(e => e.remove());
  if (view === 'requirements') {
    $('canvas-caption').replaceChildren(el('span', { class: 'eyebrow' }, '01 / CHOOSE CAPABILITIES'), el('span', {}, 'Choose the features. Codex fills in the rest.'));
    positions = Object.fromEntries(design.blocks.map((b, i) => [b.id, b.position || { x: 35 + (i % 3) * GRID_X, y: 80 + Math.floor(i / 3) * GRID_Y }]));
    for(const n of supplied) {
      for(let slot=0;slot<1000;slot++) {
        const candidate={x:35+slot%3*GRID_X,y:80+Math.floor(slot/3)*GRID_Y};
        if(!Object.values(positions).some(p=>Math.abs(p.x-candidate.x)<NODE_W+16&&Math.abs(p.y-candidate.y)<NODE_H+16)) {positions[n.id]=candidate;break;}
      }
    }
    if(supplied.length) $('canvas-caption').replaceChildren(el('span',{class:'eyebrow'},'01 / FEATURES IN USE'),el('span',{},design.blocks.length+' requested · '+supplied.length+' supporting features added by Codex'));
    $('canvas-caption').hidden = design.blocks.length+supplied.length<2;
    setCanvasSize([...design.blocks,...supplied], positions);
    for (const b of design.blocks) appendNode({ ...b, category: meta(b)?.category || 'intent', reason: briefSummary(b) }, false);
    for(const n of supplied) appendNode({...n,assemblyFeature:true},true);
  } else {
    graph = shownAssembly() ? annotatedGraph(shownAssembly()) : draftGraph();
    flowPresentation = workflowPresentation(graph, showConnections, collapsedRoutines);
    positions = flowPresentation.positions;
    flowPresentation.sections.forEach(section => {
      const heading = el('section', { class: 'flow-section' + (section.w ? ' routine-section ' + section.kind : ''), 'aria-label': section.title },
        section.w ? el('div', { class: 'routine-heading' }, el('button', { class: 'routine-toggle', 'aria-expanded': String(!section.collapsed),
          'data-hint-title': section.title, 'data-hint': section.reason,
          onclick: () => { if (collapsedRoutines.has(section.id)) collapsedRoutines.delete(section.id); else collapsedRoutines.add(section.id); renderCanvas(); } },
          el('strong', {}, (section.collapsed ? '+ ' : '− ') + section.title), el('span', {}, section.description + ' · ' + section.count + (section.kind === 'components' ? ' blocks' : section.count === 1 ? ' step' : ' steps'))),
          el('small', {}, section.kind === 'components' ? 'Shared by the routines above' : 'Numbered in source order · conditions remain in the code')) : el('strong', {}, section.title),
        section.w ? null : el('span', {}, section.description));
      if (section.bindings?.length && !section.collapsed) heading.append(el('div', { class: 'routine-bindings' }, ...section.bindings.map(b => el('button', {
        class: 'routine-binding', onclick: () => focusFlowNode(b.nodeId), 'data-hint-title': b.kind === 'registration' ? 'Callback registration' : 'Routine call', 'data-hint': b.label
      }, (b.kind === 'registration' ? '↪ Registered via ' : '↪ Called by ') + b.label))));
      heading.style.left = section.x + 'px'; heading.style.top = section.y + 'px';
      if (section.w) { heading.style.width = section.w + 'px'; heading.style.height = section.h + 'px'; }
      $('canvas').append(heading);
    });
    $('canvas-caption').replaceChildren(el('span', { class: 'eyebrow' }, graph.preview ? '02 / INTENT PREVIEW' : shownOrigin()==='custom'?'02 / ORIGINAL ASSEMBLY':shownOrigin() === 'example' ? '02 / SOURCE RECONSTRUCTION' : '02 / CODEX ASSEMBLY'), el('span', {}, graph.preview ? 'Unreviewed · compatibility and dependencies await Codex.' : graph.summary));
    const routes = graphRoutes(flowPresentation.graph, positions, flowPresentation.sections);
    setCanvasSize(flowPresentation.graph.nodes, positions, routes, flowPresentation.sections);
    drawConnections(graph, routes);
    for (const n of flowPresentation.graph.nodes) appendNode(n, true, routes);
  }
}
function appendNode(n, flow, routes = []) {
  const appearance = flow && !showConnections ? friendlyNode(n) : n;
  const b = design.blocks.find(b => b.id === n.id), c = b && meta(b), color = colors[appearance.category] || colors.system;
  const owner = b || design.blocks.find(b => n.requestIds?.includes(b.id)), sourceClass = owner && meta(owner);
  const sourceRef = flow && shownAssembly() ? { nodeId: n.id } : sourceClass?.source ? { catalogId: sourceClass.id } : null;
  const isSelected = selected === n.id;
  const review=compatibilityForNode(state.assembly,n.id);
  const concern=review&&review.status!=='supported';
  const owners=flowSourceData()?.nodes[n.id]?.owners||[];
  const location=flowSourceData()?.nodes[n.id]?.reference;
  const sketchLocation=location&&(location.scope==='application'||location.applicationFile);
  const approximate=location&&['candidate','context'].includes(location.match);
  const codeAction=flow&&shownAssembly()?el('button',{class:'node-source source-button node-code',
    'aria-label':sketchLocation?'View '+n.label+' in '+(approximate?'suggested ':'')+'application code, line '+location.line:location?'Open library code for '+n.label:'Find code for '+n.label,
    'data-hint-title':sketchLocation?'Jump to application code':location?'Open library implementation':'Find linked code',
    'data-hint':location?(approximate?'Suggested source location · ':'')+sourceCaption(location):'Find the source location linked to this step.',
    onclick:event=>{event.stopPropagation();if(sketchLocation||!location)selected=n.id;else selectNode(n.id);jumpConnection({nodeId:n.id},location);}},
    sketchLocation?'Line: '+(approximate?'≈ ':'')+location.line:location?'Library ↗':'Code ↗'):null;
  const node = el('article', { class: 'node' + (isSelected ? ' selected' : '') + (n.added ? ' added' : '') + (flow ? ' readonly' : ''), tabindex: '0', role: 'button', 'aria-pressed':String(isSelected), 'aria-label': n.label + (n.added ? ', added by Codex' : ''), 'data-id': n.id },
    el('span', { class: 'grip', 'aria-hidden': 'true' }, flow ? '↗' : '⠿'),
    el('div', { class: 'node-heading' }, el('span', { class: 'block-icon' }, ingredientIcon(appearance.category)), el('div',{class:'node-heading-text'},el('h3', {}, appearance.label),
      flow&&owners.length?el('small',{class:'node-owner','data-hint-title':'Class association','data-hint':owners.map(o=>o.object+' is declared as '+o.className+' in '+o.file+':'+o.line+(o.method?' · calls '+o.method+'()':'')).join(' · ')},owners.map(o=>o.object+' · '+o.className).join(' / ')):null)),
    el('div', { class: 'node-sub', 'data-hint-title': n.label, 'data-hint': n.reason, 'data-hint-category': appearance.category }, appearance.reason),
    el('div', { class: 'node-footer' }, codeAction || (sourceRef ? sourceButton((c?.className || 'Source') + ' ↗', sourceRef, 'node-source source-button') : el('span', {}, 'Desired capability')),
      el('span', { class: 'node-badge' }, n.assemblyFeature ? '+ ADDED' : flow && !showConnections ? n.added ? '+ AUTO' : flowPresentation.roles[n.id] === 'components' ? 'COMPONENT' : 'STEP' : flow && shownOrigin() === 'example' ? n.category === 'intent' ? 'CUSTOM' : 'SOURCE' : n.added ? '+ CODEX' : n.suggested ? 'PREVIEW' : b?.count > 1 ? '× ' + b.count + ' SETS' : catalog.categories[n.category] || n.category))
  );
  node.classList.toggle('has-class-owner',flow&&owners.length>0);
  node.dataset.category=appearance.category;
  if(!flow&&isFeatureNote(b)) {
    const expanded=noteIsOpen(b);
    node.classList.add('feature-note');node.classList.toggle('note-expanded',expanded);
    const toggle=el('button',{class:'note-toggle','aria-expanded':String(expanded),'aria-label':expanded?'Collapse feature note':'Expand feature note',onclick:event=>{
      event.stopPropagation();try{localStorage.setItem(noteStorageKey(b),expanded?'closed':'open');}catch{}
      renderCanvas();requestAnimationFrame(()=>{document.querySelector('[data-id="'+b.id+'"] .note-toggle')?.focus({preventScroll:true});});
    }},expanded?'−':'+');
    const editor=el('textarea',{class:'feature-notepad','aria-label':'Describe feature requirement',placeholder:'What should this feature do? For example: log this sensor every 5 minutes.',maxlength:'12000'});
    editor.value=b.settings.requirement||'';
    editor.addEventListener('input',()=>{b.settings.requirement=editor.value;changed();});
    editor.addEventListener('blur',()=>{if(selected===b.id)renderInspector();});
    node.append(toggle,editor);editor.hidden=!expanded;
  }
  if(concern) {
    node.classList.add(review.status==='needs-info'?'compatibility-question':'compatibility-conflict');
    node.setAttribute('aria-label',n.label+' · '+assessmentLabels[review.status]+': '+review.summary);
    const description=node.querySelector('.node-sub');
    description.textContent=review.summary;description.dataset.hintTitle=assessmentLabels[review.status];
    description.dataset.hint=review.issues.map(issue=>issue.requirement+': '+issue.reason).join(' · ');
    node.querySelector('.node-badge').textContent=assessmentLabels[review.status];
  }
  const step = flow && !n.assemblyFeature && !showConnections && flowPresentation?.steps?.[n.id];
  if (step && step.routine.kind !== 'components') {
    const badge=node.querySelector('.node-badge'), detail=badge.textContent;
    badge.classList.add('step-badge');
    badge.replaceChildren(...[el('span',{class:'step-number','aria-label':'Source step '+step.number},'Step '+step.number),
      concern||n.added?el('span',{class:'step-detail'},concern?detail:'Auto'):null].filter(Boolean));
  }
  if(!flow) {
    const remove = el('button', {
    class: 'node-remove', 'aria-label': 'Remove ' + n.label,
    'data-hint-title': 'Remove this feature',
    'data-hint': 'Remove ' + n.label + ' from your project.',
    onclick: e => { e.stopPropagation(); removeSelected(n.id); }
    }, '×');
    remove.addEventListener('pointerdown', e => e.stopPropagation());
    node.append(remove);
  }
  node.style.setProperty('--block-color', color);
  if (!flow && b?.notes) node.classList.add('has-behavior');
  if (n.id === lastAdded) { node.classList.add('just-added'); node.append(el('span', { class: 'spawn-ring', 'aria-hidden': 'true' })); lastAdded = null; }
  node.style.left = positions[n.id].x + 'px'; node.style.top = positions[n.id].y + 'px';
  if (flow && !n.assemblyFeature) node.append(el('span', { class: 'port in' }), el('span', { class: 'port out' }));
  if (flow) for (const point of routes.flatMap(r => r.edge.from === n.id ? [r.start] : r.edge.to === n.id ? [r.end] : []).filter(p => p.y !== positions[n.id].y + NODE_H / 2)) {
    const port = el('span', {class:'port control-port'});
    port.style.left = (point.x - positions[n.id].x - 4) + 'px'; port.style.top = (point.y - positions[n.id].y - 4) + 'px'; node.append(port);
  }
  node.addEventListener('keydown', e => {
    if (e.target.closest('button,a,input,textarea,select')) return;
    if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); selectNode(n.id); }
    if (!flow && e.key === 'Delete') removeSelected(n.id);
    if (!flow && ['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown'].includes(e.key)) {
      e.preventDefault();
      b.position = { x: Math.max(0, positions[n.id].x + (e.key === 'ArrowRight' ? 20 : e.key === 'ArrowLeft' ? -20 : 0)), y: Math.max(65, positions[n.id].y + (e.key === 'ArrowDown' ? 20 : e.key === 'ArrowUp' ? -20 : 0)) };
      changed(); renderCanvas(); [...$('nodes').children].find(c => c.dataset.id === n.id)?.focus();
    }
  });
  node.addEventListener('pointerdown', e => {
    if (e.button !== 0 || e.target.closest('button,a,input,textarea,select')) return;
    e.preventDefault();
    node.focus({preventScroll:true});
    selectNode(n.id);
    if (flow) return;
    dragging = { id: n.id, startX: e.clientX, startY: e.clientY, x: positions[n.id].x, y: positions[n.id].y, node, moved: false };
    node.setPointerCapture(e.pointerId);
  });
  node.addEventListener('pointermove', e => {
    if (!dragging || dragging.id !== n.id) return;
    const dx = (e.clientX - dragging.startX) / canvasZoom, dy = (e.clientY - dragging.startY) / canvasZoom;
    if (Math.abs(dx) + Math.abs(dy) < 4 && !dragging.moved) return;
    dragging.moved = true; node.classList.add('dragging');
    b.position = { x: Math.min(20000, Math.max(5, dragging.x + dx)), y: Math.min(20000, Math.max(65, dragging.y + dy)) };
    positions[b.id] = b.position;
    if (!dragging.frame) dragging.frame = requestAnimationFrame(() => {
      if (!dragging || dragging.id !== n.id) return;
      dragging.frame = null;
      node.style.left = b.position.x + 'px'; node.style.top = b.position.y + 'px';
    });
  });
  const end = () => {
    if (!dragging || dragging.id !== n.id) return;
    const moved = dragging.moved; cancelAnimationFrame(dragging.frame);
    if (moved) { node.style.left = b.position.x + 'px'; node.style.top = b.position.y + 'px'; }
    dragging = null; node.classList.remove('dragging');
    if (moved) { changed(false); setCanvasSize(design.blocks, positions); }
  };
  node.addEventListener('pointerup', end); node.addEventListener('pointercancel', end);
  $('nodes').append(node);
  if (node.classList.contains('just-added')) requestAnimationFrame(() => stretchControl(node, true));
}
function selectNode(id) {
  selected = id;
  tracedEdge = null;
  $('connections').querySelectorAll('.wire-traced').forEach(wire=>wire.classList.remove('wire-traced'));
  for (const node of $('nodes').children) {
    node.classList.toggle('selected', node.dataset.id === id);
    node.setAttribute('aria-pressed',String(node.dataset.id===id));
  }
  renderInspector();
  for(const wire of $('connections').querySelectorAll('[data-from]')) wire.classList.toggle('wire-muted',!!selected&&wire.dataset.from!==selected&&wire.dataset.to!==selected);
  if(id&&view==='flow'&&contentView==='canvas')requestAnimationFrame(()=>{if(selected===id&&view==='flow'&&contentView==='canvas')revealNewBlock(id);});
}
function clearSelection() {
  if(!selected&&tracedEdge===null)return;
  const restoreFocus=document.activeElement?.closest('.node,.inspector');
  selectNode(null);
  $('connections').querySelectorAll('.wire-traced').forEach(wire=>wire.classList.remove('wire-traced'));
  if(restoreFocus)$('canvas').focus({preventScroll:true});
}
function focusFlowNode(id) {
  const routine = graph?.routines?.find(r => r.nodeIds.includes(id));
  if (routine && collapsedRoutines.delete(routine.id)) renderCanvas();
  if(view==='flow'&&!showConnections&&flowPresentation.roles[id]==='components') {
    const node=graph.nodes.find(n=>n.id===id);
    const feature=design.blocks.find(b=>b.id===id)||supportingFeatures(design,shownAssembly()).find(n=>n.id===id)||design.blocks.find(b=>node?.requestIds.includes(b.id));
    if(feature){id=feature.id;view='requirements';renderCanvas();}
  }
  selectNode(id);
  if(positions[id])requestAnimationFrame(()=>{
    canvasZoom=Math.max(.9,canvasZoom);applyZoom();revealNewBlock(id);
    [...$('nodes').children].find(node=>node.dataset.id===id)?.focus({preventScroll:true});
  });
}
function outlineIsCurrent(key) {
  if(shownAssembly()&&flowSourceKey(shownAssembly())===key)return true;
  renderFlowOutline();toast('The application changed. Choose a step from the updated outline.');return false;
}
function revealOutlineNode(id,key) {
  if(!outlineIsCurrent(key))return;
  $('flow-outline-dialog').close();view='flow';showConnections=false;setWorkspaceView('canvas');renderCanvas();focusFlowNode(id);
}
function renderFlowOutline() {
  const assembly=shownAssembly(),list=$('flow-outline-list'),oldScroll=list.scrollTop,focused=document.activeElement?.dataset.outlineFocus;
  const groups=executionOutline(assembly,flowSourceData(assembly)),key=assembly?flowSourceKey(assembly):null;
  if(outlineState.key!==key)outlineState={key,expanded:new Set(groups.filter(r=>['setup','loop'].includes(r.kind)||r.steps.some(step=>step.id===selected)).map(r=>r.id))};
  const count=groups.reduce((sum,r)=>sum+r.steps.length,0);
  $('outline-status').textContent=groups.length+' routines · '+count+' steps'+(assembly&&!state.assembly?' · previous assembly':'');
  $('outline-expand').disabled=$('outline-collapse').disabled=!groups.length;
  list.replaceChildren();
  if(!groups.length)list.append(el('p',{class:'notice'},'No routine outline is available for this result. Use the flow or open Code to review it.'));
  const names={setup:'Startup',loop:'Repeats',callback:'Callback',helper:'Helper'};
  for(const group of groups) {
    const section=el('details',{class:'outline-routine outline-'+group.kind,open:outlineState.expanded.has(group.id)?'':null});
    section.addEventListener('toggle',()=>{if(!section.isConnected||outlineState.key!==key)return;if(section.open)outlineState.expanded.add(group.id);else outlineState.expanded.delete(group.id);});
    const summary=el('summary',{'data-outline-focus':'routine-'+group.id,'data-hint-title':group.name+'()','data-hint':group.reason},
      el('strong',{},group.name+'()'),el('span',{class:'outline-kind'},names[group.kind]||'Routine'),el('span',{class:'outline-count'},group.steps.length+' '+(group.steps.length===1?'step':'steps')));
    const actions=el('div',{class:'outline-routine-actions'},el('button',{class:'quiet compact','data-outline-focus':'code-'+group.id,
      'data-hint':'Review the function body and its conditions.',onclick:()=>{if(!outlineIsCurrent(key))return;$('flow-outline-dialog').close();showSourceDetails({routineId:group.id},group.name+'()');}},'Routine code ↗'));
    for(const binding of group.bindings)actions.append(el('button',{class:'text-button outline-caller','data-outline-focus':'caller-'+group.id+'-'+binding.nodeId,
      'data-hint-title':binding.kind==='registration'?'Callback registration':'Routine call',
      'data-hint':binding.kind==='registration'?'Reveal where this callback is registered. Registration does not execute the callback body.':'Reveal the step that calls this function.',
      onclick:()=>revealOutlineNode(binding.nodeId,key)},(binding.kind==='registration'?'Registered via ':'Called by ')+binding.label));
    if(!group.bindings.length&&['callback','helper'].includes(group.kind))actions.append(el('span',{class:'small muted'},'No caller links recorded · review the routine code.'));
    const steps=el('ol',{class:'outline-steps','aria-label':group.name+' steps'});
    for(const step of group.steps) {
      const owners=step.owners.map(owner=>owner.object+' · '+owner.className).join(' / '),reference=step.reference;
      steps.append(el('li',{},el('button',{class:'outline-step','aria-current':step.id===selected?'step':null,'data-outline-focus':'step-'+step.id,
        'data-hint-title':step.label,'data-hint':step.reason,'data-hint-category':step.category,onclick:()=>revealOutlineNode(step.id,key)},
        el('span',{class:'outline-step-number','aria-hidden':'true'},String(step.number)),
        el('span',{class:'outline-step-text'},el('strong',{},step.label),owners?el('small',{},owners):null),
        reference?el('small',{class:'outline-step-location'},sourceCaption(reference)):null)));
    }
    section.append(summary,actions,steps);
    if(!group.steps.length)section.append(el('p',{class:'small outline-empty'},'No steps indexed. Open the routine code to inspect its body.'));
    list.append(section);
  }
  list.scrollTop=oldScroll;
  if(focused)[...list.querySelectorAll('[data-outline-focus]')].find(item=>item.dataset.outlineFocus===focused)?.focus({preventScroll:true});
}
function openFlowOutline() {
  renderFlowOutline();$('flow-outline-dialog').showModal();loadFlowSources();
}
function drawConnections(g, routes) {
  const ns = 'http://www.w3.org/2000/svg', svg = $('connections');
  const defs = document.createElementNS(ns, 'defs');
  for (const [kind, color] of Object.entries({ data: '#b6dfad', dependency: '#adb8ee', cycle: '#efc576', flow:'#9fd8ff', callback:'#d5b2ff' })) {
    const marker = document.createElementNS(ns, 'marker');
    for (const [k, v] of Object.entries({ id: 'arrow-' + kind, viewBox: '0 0 8 8', refX: '7', refY: '4', markerWidth: '5', markerHeight: '5', orient: 'auto-start-reverse' })) marker.setAttribute(k, v);
    const arrow = document.createElementNS(ns, 'path'); arrow.setAttribute('d', 'M0,0 L8,4 L0,8 Z'); arrow.style.fill = color; arrow.style.strokeWidth = 0;
    marker.append(arrow); defs.append(marker);
  }
  const wires = document.createElementNS(ns, 'g'), labels = document.createElementNS(ns, 'g');
  svg.append(defs, wires, labels);
  routes.forEach(route => {
    const edge = route.edge;
    if (!route) return;
    const sourceIndex = edge.sourceIndex ?? g.edges.indexOf(edge);
    const line = document.createElementNS(ns, 'path');
    line.setAttribute('d', route.d);
    line.setAttribute('stroke', { data: '#b6dfad', dependency: '#adb8ee', cycle: '#efc576', flow:'#9fd8ff', callback:'#d5b2ff' }[edge.kind]);
    line.setAttribute('class','wire-' + edge.kind);
    line.dataset.from=edge.from;line.dataset.to=edge.to;
    line.dataset.edgeIndex=String(sourceIndex);line.classList.toggle('wire-traced',sourceIndex===tracedEdge);
    line.classList.toggle('wire-muted',tracedEdge!==null?sourceIndex!==tracedEdge:!!selected&&edge.from!==selected&&edge.to!==selected);
    if (!['data','flow'].includes(edge.kind)) line.setAttribute('stroke-dasharray', edge.kind === 'cycle' ? '3 4' : '6 4');
    line.setAttribute('marker-end', 'url(#arrow-' + edge.kind + ')');
    const title = document.createElementNS(ns, 'title'); title.textContent = (edge.carriesOrder ? 'Step + data' : edge.kind) + ': ' + edge.label; line.append(title);
    wires.append(line);
    if (route.leader) {
      const link = document.createElementNS(ns, 'path');
      link.setAttribute('d', 'M' + route.leader.from.x + ',' + route.leader.from.y + ' L' + route.leader.to.x + ',' + route.leader.to.y);
      link.setAttribute('stroke', line.getAttribute('stroke')); link.setAttribute('class', 'label-leader');link.dataset.from=edge.from;link.dataset.to=edge.to;link.dataset.edgeIndex=String(sourceIndex);link.classList.toggle('wire-muted',tracedEdge!==null?sourceIndex!==tracedEdge:!!selected&&edge.from!==selected&&edge.to!==selected); wires.append(link);
    }
    if (route.label) {
      const tag = document.createElementNS(ns, 'g'), rect = document.createElementNS(ns, 'rect'), text = document.createElementNS(ns, 'text');
      const box = route.label, from = g.nodes.find(n => n.id === edge.from), to = g.nodes.find(n => n.id === edge.to);
      tag.setAttribute('class', 'wire-label'); tag.setAttribute('tabindex', '0');
      tag.dataset.from=edge.from;tag.dataset.to=edge.to;
      tag.dataset.edgeIndex=String(sourceIndex);tag.classList.toggle('wire-traced',sourceIndex===tracedEdge);
      tag.classList.toggle('wire-muted',tracedEdge!==null?sourceIndex!==tracedEdge:!!selected&&edge.from!==selected&&edge.to!==selected);
      tag.setAttribute('role', 'button'); tag.setAttribute('aria-label', edge.label + (edge.sourceCaption?' · '+edge.sourceCaption:'') + ' · Isolate connection, then view explanation');
      const sourceRef = sourceIndex < 0 ? {nodeId:edge.navigationNodeId || edge.from} : { edgeIndex: sourceIndex };
      let clickTimer;
      const isolateOrExplain=()=>{
        if(tracedEdge===sourceIndex){showSourceDetails(sourceRef,edge.label);return;}
        selectNode(null);tracedEdge=sourceIndex;
        for(const wire of $('connections').querySelectorAll('[data-from]')) {
          const match=wire.dataset.edgeIndex===String(sourceIndex);
          wire.classList.toggle('wire-muted',!match);wire.classList.toggle('wire-traced',match);
        }
      };
      tag.addEventListener('click', () => {clearTimeout(clickTimer);if(tracedEdge!==sourceIndex)isolateOrExplain();else clickTimer=setTimeout(()=>{if(tag.isConnected)isolateOrExplain();},250);});
      tag.addEventListener('dblclick', e => {e.preventDefault();clearTimeout(clickTimer);jumpConnection(sourceRef,edge.sourceReference);});
      tag.addEventListener('keydown', e => {
        if(e.key==='Enter'&&e.shiftKey){e.preventDefault();jumpConnection(sourceRef,edge.sourceReference);}
        else if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); isolateOrExplain(); }
      });
      tag.style.setProperty('--wire-color', line.getAttribute('stroke'));
      tag.setAttribute('data-hint-title', edge.label);
      tag.setAttribute('data-hint', from.label + ' → ' + to.label + ' · ' + (edge.detailLabel || edge.kind)+(edge.sourceReference?' · '+edge.sourceReference.file+':'+edge.sourceReference.line:''));
      tag.setAttribute('data-hint-action', 'Click to isolate · Click again for explanation · Double-click to jump to code');
      for (const [k, v] of Object.entries({ x: box.x, y: box.y, width: box.w, height: box.h, rx: 7 })) rect.setAttribute(k, v);
      text.setAttribute('class', 'flow-label'); text.setAttribute('text-anchor', 'middle');
      box.lines.forEach((s, i) => {
        const span = document.createElementNS(ns, 'tspan');
        span.setAttribute('x', box.x + box.w / 2); span.setAttribute('y', box.y + 13 + i * 12);
        if(i>=box.sourceLineStart)span.setAttribute('class','flow-source-location');
        span.textContent = s; text.append(span);
      });
      tag.append(rect, text); labels.append(tag);
    }
  });
}
function inputField(label, value, onChange, { help, multiline = false, number = false, min, max, placeholder = '', bool = false } = {}) {
  const wrap = el('div', { class: 'field' + (bool ? ' check' : '') });
  const id = 'setting-' + crypto.randomUUID();
  const input = multiline ? el('textarea', { id, rows: '3' }) : el('input', { id, type: bool ? 'checkbox' : number ? 'number' : 'text', placeholder });
  if (bool) input.checked = Boolean(value); else input.value = value ?? '';
  if (min !== undefined) input.min = min;
  if (max !== undefined) input.max = max;
  input.addEventListener('input', () => {
    if (!input.checkValidity()) return;
    onChange(bool ? input.checked : number ? Number(input.value) : input.value);
  });
  if (bool) wrap.append(input, el('label', { for: id }, label)); else wrap.append(el('label', { for: id }, label), input);
  if (help) wrap.append(el('small', {}, help));
  return wrap;
}
function assessmentCard(review,navigate=false) {
  const inspect=()=>{
    view='requirements';setWorkspaceView('canvas');selected=null;renderCanvas();renderInspector();
    if(review.requestId==='$brief'){openBrief();return;}
    if(design.blocks.some(block=>block.id===review.requestId)) {
      selectNode(review.requestId);requestAnimationFrame(()=>revealNewBlock(review.requestId));
    }
  };
  return el('section',{class:'compatibility-card '+(review.status==='needs-info'?'compatibility-question':'compatibility-conflict')},
    el('strong',{},assessmentLabels[review.status]),el('p',{},review.summary),
    ...review.issues.map(issue=>el('div',{class:'compatibility-issue'},el('strong',{},issue.requirement),el('p',{},issue.reason),
      el('details',{},el('summary',{},'Review evidence'),el('p',{},issue.evidence)))),
    review.suggestion?el('p',{class:'compatibility-suggestion'},'Suggested change: '+review.suggestion):null,
    navigate?el('button',{class:'quiet',onclick:inspect},review.requestId==='$brief'?'Review behavior brief':'Review feature'):null);
}
function renderInspector() {
  const panel = $('inspector'); panel.replaceChildren();
  if (panel.dataset.selected !== String(selected)) {
    panel.scrollTop = 0; panel.dataset.selected = String(selected);
  }
  const suppliedNode = view==='requirements'&&supportingFeatures(design,shownAssembly()).find(n=>n.id===selected);
  if(suppliedNode) graph=shownAssembly();
  const flowNode = suppliedNode || view === 'flow' && graph?.nodes.find(n => n.id === selected);
  $('inspector-title').textContent = flowNode ? 'Flow details' : 'Block settings';
  $('inspector-tag').textContent = flowNode ? 'VIEW' : 'EDIT';
  let b = design.blocks.find(b => b.id === selected);
  if (flowNode && !b) b = design.blocks.find(b => flowNode.requestIds.includes(b.id));
  document.body.classList.toggle('has-selection', !!(b || flowNode));
  const review=compatibilityForNode(state.assembly,selected);
  if(review&&review.status!=='supported')panel.append(assessmentCard(review));
  if (flowNode) {
    const appearance = showConnections ? flowNode : friendlyNode(flowNode);
    panel.append(el('span', { class: 'node-badge' }, shownOrigin() === 'example' ? 'EXAMPLE SOURCE' : flowNode.added ? 'ADDED BY CODEX' : graph.preview ? 'INTENT PREVIEW' : 'REQUESTED FEATURE'), el('h3', {}, appearance.label), el('p', {}, appearance.reason));
    const owners=flowSourceData()?.nodes[flowNode.id]?.owners||[];
    if(owners.length)panel.append(el('p',{class:'step-owner-summary'},owners.map(owner=>owner.object+' · '+owner.className).join(' / ')));
    const stepLocation=flowSourceData()?.nodes[flowNode.id]?.reference,stepInSketch=stepLocation&&(stepLocation.scope==='application'||stepLocation.applicationFile);
    if(shownAssembly())panel.append(el('button',{class:'quiet jump-to-code step-code-action',
      'data-hint-title':stepInSketch?'Jump to application code':stepLocation?'Open library code in VS Code':'Find linked code',
      'data-hint':stepLocation?sourceCaption(stepLocation):'Find the code linked to this step.',
      onclick:()=>jumpConnection({nodeId:flowNode.id},stepLocation)},stepInSketch?'Jump to sketch code':stepLocation?'Open library code ↗':'Find step code ↗'));
    const membership = graph.routines?.find(r => r.nodeIds.includes(flowNode.id));
    if (membership) panel.append(el('div', { class: 'notice' }, membership.name + '() · ' + (membership.kind === 'callback' ? 'Registered callback' : membership.kind === 'setup' ? 'Runs once at startup' : membership.kind === 'loop' ? 'Repeating routine' : 'Called helper') + ' · source step ' + (membership.nodeIds.indexOf(flowNode.id) + 1)));
    const connections=stepConnections(graph,flowNode.id,flowSourceData());
    const connectionSection=group=>{
      const section=el('section',{class:'step-connections','aria-label':group.title},el('h4',{},group.title));
      if(group.kind==='flow')section.append(el('p',{class:'small step-connection-help'},group.rows.some(row=>row.links.some(link=>link.derived||/^Source order/.test(link.label)))?'Source order only; check the linked code for conditions.':'Choose a connected step, or open its connection in code.'));
      for(const row of group.rows) {
        const onBoard=view==='flow'&&!showConnections&&flowPresentation.roles[row.otherId]==='components';
        const card=el('article',{class:'step-connection','data-kind':group.kind},
          el('span',{class:'step-connection-direction'},row.direction==='self'?'Returns to this step':row.direction==='outgoing'?'Outgoing →':'← Incoming'),
          el('button',{class:'step-connection-target','data-target-node':row.otherId,onclick:()=>focusFlowNode(row.otherId),
            'data-hint-title':'Inspect '+friendlyNode(row.node).label,'data-hint':onBoard?'View this component in Your features.':'Select this connected step in the flow.'},
            el('strong',{},friendlyNode(row.node).label),el('small',{},onBoard?'Your features':row.routine?row.routine.name+'() · step '+row.routine.step:'Supporting block')));
        const locationKey=link=>link.reference?JSON.stringify([link.reference.scope,link.reference.file,link.reference.applicationFile,link.reference.line,link.reference.endLine,link.reference.match]):null;
        for(const [linkIndex,link] of row.links.entries()) {
          const location=link.reference,inSketch=location&&(location.scope==='application'||location.applicationFile);
          const sharedLocation=location&&row.links.slice(linkIndex+1).some(next=>locationKey(next)===locationKey(link));
          card.append(el('div',{class:'step-connection-relation'},el('p',{},row.links.length>1&&link.kind==='data'?'Data · '+link.label:link.label||({flow:'Step transition',data:'Data connection',callback:'Routine connection',cycle:'Repeat route',dependency:'Supporting connection'})[link.kind]),
            shownAssembly()&&!sharedLocation?el('button',{class:'text-button step-connection-code','data-hint-title':inSketch?'Jump to application code':location?'Open library code in VS Code':'Find linked code',
              'data-hint':location?location.file+':'+location.line:'Review the code linked to this connection.',
              'aria-label':(inSketch?'View connection in application code':location?'Open connection library code':'Find connection code')+(location?' · '+sourceCaption(location):''),
              onclick:()=>jumpConnection(link.source,location)},location?(inSketch?'Code · ':'Library · ')+sourceCaption(location)+' ↗':'Find linked code ↗'):null));
        }
        section.append(card);
      }
      return section;
    };
    for(const group of connections.filter(group=>group.kind!=='dependency'))panel.append(connectionSection(group));
    if (membership?.bindings.length) panel.append(el('div', { class: 'connection-list' }, ...membership.bindings.map(binding =>
      el('button', { class: 'connection-link', onclick: () => focusFlowNode(binding.nodeId) }, el('strong', {}, binding.kind === 'registration' ? 'Registered via' : 'Called by'), el('small', {}, binding.label)))));
    const related = (graph.routines || []).flatMap(r => {
      const binding = r.bindings.find(b => b.nodeId === flowNode.id);
      const ownerSteps = !membership && flowNode.requestIds.length ? r.nodeIds.filter(id => graph.nodes.find(n => n.id === id)?.requestIds.some(id => flowNode.requestIds.includes(id))) : [];
      if (!binding && !ownerSteps.length) return [];
      return [el('button', { class: 'connection-link', onclick: () => focusFlowNode(ownerSteps[0] || r.nodeIds[0]) },
        el('strong', {}, (binding?.kind === 'registration' ? 'Registers ' : '') + r.name + '()'),
        el('small', {}, r.kind + (binding ? ' · ' + binding.label : ' · ' + ownerSteps.length + ' related steps')))];
    });
    if (related.length) panel.append(el('div', { class: 'settings-divider' }, 'RELATED ROUTINES & CALLBACKS'), el('div', { class: 'connection-list' }, ...related));
    if (appearance.reason !== flowNode.reason) panel.append(el('details', { class: 'source-detail' }, el('summary', {}, 'Exact source details'), el('p', {}, flowNode.reason)));
    if (shownAssembly()) {
      const sources=el('div',{class:'source-references'});
      panel.append(el('div',{class:'settings-divider'},'EXPLANATION & CODE'),sources);
      renderSourceLinks(sources,{nodeId:flowNode.id},false);
    }
    if(owners.length)panel.append(el('details',{class:'source-detail object-class-details'},el('summary',{},'Objects & classes · '+owners.length),
      el('div',{class:'class-associations'},...owners.map(owner=>el('div',{},el('strong',{},owner.object+' · '+owner.className),
        el('small',{},(owner.method?'Calls '+owner.method+'() · ':'')+'Object declared in '+owner.file+':'+owner.line),owner.catalogId?sourceButton('Class definition ↗',{catalogId:owner.catalogId}):null)))));
    const namedClass = catalog.blocks.find(c=>c.className && (new RegExp('^'+c.className+'\\b').test(flowNode.label)||flowNode.reason.includes('Source declaration: '+c.className+' ')));
    const definition = namedClass || (!flowNode.added && b && meta(b));
    if (definition?.source&&!owners.some(owner=>owner.catalogId===definition.id)) panel.append(sourceButton('Class definition ↗', { catalogId: definition.id }));
    const support=connections.find(group=>group.kind==='dependency');
    if(support)panel.append(el('details',{class:'source-detail',open:showConnections?'':null},el('summary',{},'Supporting connections · '+support.rows.length),connectionSection(support)));
    if(!connections.length)panel.append(el('p',{class:'small'},'No connections are recorded for this block.'));
    if (b) panel.append(el('button', { class: 'quiet', onclick: () => { view = 'requirements'; selected = b.id; renderCanvas(); renderInspector(); } }, 'Edit requested settings'));
    else panel.append(el('div', { class: 'notice' }, graph.preview ? 'This summarizes intent. Codex has not reviewed this component.' : 'Codex added this component to support your requested features. Describe changes in the brief and reassemble.'));
    return;
  }
  if (!b) {
    panel.append(el('div', { class: 'empty-inspector' }, el('span', { class: 'spark' }, '◇'), el('h3', {}, 'Select a feature'), el('p', {}, 'Choose a block to edit its behavior, instance count or constructor settings.')));
    return;
  }
  const c = meta(b);
  panel.append(el('span', { class: 'node-badge' }, c?.kind === 'class' ? 'DISCOVERED CLASS' : 'DESIRED CAPABILITY'), el('h3', {}, b.label), el('p', {}, c?.description || 'This block is no longer present in the current source. Ask Codex to resolve or replace it.'));
  if (c?.source) panel.append(sourceButton('Class definition in VS Code ↗', { catalogId: c.id }));
  const update = () => { changed(); renderCanvas(); };
  panel.append(inputField('Block label', b.label, v => { b.label = v || c?.title || 'Feature'; update(); }));

  for (const [key, fallback] of Object.entries(c?.defaults || {})) {
    const value = Object.hasOwn(b.settings, key) ? b.settings[key] : fallback;
    panel.append(inputField(fieldLabel(key), value, v => { b.settings[key] = v; update(); }, { bool: typeof fallback === 'boolean', number: typeof fallback === 'number', min: typeof fallback === 'number' ? 1 : undefined, multiline: ['requirement', 'notes', 'failurePolicy', 'retryPolicy', 'delivery'].includes(key) }));
  }
  panel.append(inputField('Behavior & per-set settings', b.notes, v => { b.notes = v; update(); }, { multiline: true, help: 'Use plain language, e.g. “Sets 1–3 on separate mux ports; PM enabled, number concentrations off.”' }));
  panel.append(el('details',{class:'feature-more-settings'},el('summary',{},'More settings · device count'),
    inputField(c?.kind==='class'?'Number of devices':'Number of copies',b.count,v=>{b.count=v;update();},{number:true,min:1,max:64,
      help:c?.kind==='class'?'How many physical copies of this device? Leave 1 for a single sensor. Codex checks the connections for multiple copies.':'Leave 1 for one feature. Increase this only when you need it repeated across devices.'})));
  if(!state.assembly?.featureAssessments)panel.append(el('p',{class:'small'},'Assemble with Codex to check this behavior against the object, Loom and Arduino hardware.'));
  if (b.catalogId === 'intent:custom' || c?.saved) {
    panel.append(el('button', { class: 'quiet save-custom', onclick: async event => {
      const button = event.currentTarget; button.disabled = true;
      try {
        await save();
        const result = await api('/api/custom-features', { blockId: b.id });
        catalog = result.catalog; renderLibrary();
        toast('Saved “' + result.feature.title + '” to Custom. Add it again from the ingredient dock.');
      } catch (e) { toast(e.message); }
      finally { button.disabled = false; }
    } }, 'Save to Custom'), el('p', { class: 'save-custom-help' }, 'Uses the block label as its name. Saves the description and settings locally for reuse.'));
  }
  if (c?.constructors.length) {
    const details = el('details', {}, el('summary', {}, 'Source constructor options'));
    details.append(el('p', { class: 'small' }, 'Optional overrides, discovered from this checkout. Leave values blank to let Codex select compatible settings. Reference and pointer dependencies are resolved during assembly.'));
    let idx = Math.min(b.settings.constructorIndex || 0, c.constructors.length - 1);
    const select = el('select', { 'aria-label': 'Constructor variant' });
    c.constructors.forEach((ctor, i) => select.append(el('option', { value: i }, 'Variant ' + (i + 1) + ' · ' + ctor.parameters.length + ' parameters')));
    select.value = idx;
    select.addEventListener('change', () => {
      const next = Number(select.value); if (next === idx) return;
      b.settings.constructorIndex = next; b.settings.parameters = {}; update(); renderInspector();
    });
    details.append(select, el('div', { class: 'notice' }, el('code', {}, c.constructors[idx].signature)));
    for (const p of c.constructors[idx].parameters.filter(p => !p.dependency)) {
      const overrides = b.settings.parameters || {};
      const value = Object.hasOwn(overrides, p.name) ? overrides[p.name] : '';
      const field = inputField(fieldLabel(p.name), value, v => {
        b.settings.parameters ||= {};
        if (v === '') delete b.settings.parameters[p.name]; else b.settings.parameters[p.name] = v;
        update();
      }, { placeholder: p.default === '' ? 'Let Codex choose' : String(p.default), help: p.type + (p.required ? ' · required by constructor; Codex resolves' : ' · source default: ' + p.default) });
      details.append(field);
    }
    panel.append(details);
  }
  const links = el('details', { class: 'source-links' }, el('summary', {}, 'Source & related examples'));
  if (c?.source) links.append(sourceButton(c.source + ' ↗', { catalogId: c.id }));
  for (const source of (c?.examples || []).slice(0, 8)) links.append(sourceButton(source + ' ↗', { source }));
  if (!c?.source && !c?.examples?.length) links.append(el('p', {}, 'This is an intent block. Codex discovers its implementation in the selected source.'));
  panel.append(links);
  if (b.catalogId === 'class:Loom_WIFI' || c?.category === 'network' || c?.category === 'outputs') panel.append(el('div', { class: 'notice' }, 'Describe the connection and credential placeholders here. Keep real passwords and tokens out of the design.'));
  panel.append(el('div', { class: 'inspector-actions' }, el('button', { class: 'text-button', onclick: () => {
    const copy = structuredClone(b); copy.id = crypto.randomUUID(); copy.position = { x: (b.position?.x || 0) + 25, y: (b.position?.y || 80) + 25 };
    design.blocks.push(copy); selected = copy.id; changed(); renderCanvas(); renderInspector();
    requestAnimationFrame(() => revealNewBlock(copy.id));
  } }, 'Duplicate block'), el('button', { class: 'danger', onclick: () => removeSelected(b.id) }, 'Remove')));
}
function removeSelected(id) {
  design.blocks = design.blocks.filter(b => b.id !== id); selected = null; changed(); renderCanvas(); renderInspector();
}
function updateStatus() {
  const a = shownAssembly(), stale = !!a && !state.assembly;
  $('flow-dot').textContent = stale ? 'PREVIOUS' : a ? 'READY' : 'PREVIEW';
  const delta=stale?currentDesignChanges():null;
  $('review-changes').hidden=!stale;$('design-change-count').textContent=delta?.total?String(delta.total):'';
  $('canvas-status').textContent = stale ? 'Previous assembly · '+lastBuildChange()+' · review Changes' : a ? shownOrigin() === 'example' ? 'Example source · structural graph · ready to adapt' : 'Codex assembly · ' + supportingFeatures(design,a).length + ' supporting features added · support links in All connections' :
    localDirty ? 'Saving requirements · assembly pending' : state.assemblyError ? 'Assembly needs review · see notes' : 'Unreviewed requirements · ready for Codex';
  $('warning-count').textContent = (a?.warnings.length||0)+compatibilityConcerns(a).length || (state.assemblyError ? '1' : '');
  $('decisions-tab').classList.toggle('has-concerns',!!state.assembly&&compatibilityConcerns(a).length>0);
  $('download-file').disabled = !a?.files.length;
  $('open-code').disabled = !a?.files.length;
  $('find-code').disabled = !a?.files.length;$('copy-code').disabled = !a?.files.length;
  $('download-svg').disabled = !a?.nodes.length && !design.blocks.length;
  $('download-svg').textContent=view==='requirements'?'Export features (SVG) ↓':'Export flow (SVG) ↓';
  $('download-svg').setAttribute('aria-label',view==='requirements'?'Download feature board':'Download flow graph');
  $('file-action-status').textContent = stale ? 'Previous assembly · '+lastBuildChange() : a ? '' : state.job?.status === 'running' ? 'Creating files…' : state.assemblyError ? 'Design changed · assemble for new files' : 'Assemble for app files';
  $('open-code').dataset.hint = a ? 'Open the selected result file in VS Code.' : 'Assemble this design or load a recipe to create an application file. Source links on individual blocks still open their library code.';
  $('download-file').dataset.hint = a ? 'Download the selected result file.' : 'There is no application file for this design yet. Assemble it or load a recipe first.';
  const job = state.job, running = job?.status === 'running';
  $('runbar').hidden = !job;
  $('runbar').classList.toggle('failed', ['failed','interrupted'].includes(job?.status));
  $('run-message').textContent = job ? (running ? '◌ ' : job.status === 'succeeded' ? '✓ ' : '• ') + job.message : '';
  $('cancel-run').hidden = !running;
  $('run-assembly').disabled = running || state.deviceJob?.status === 'running' || !state.codexAvailable;
  $('new-design').disabled = running || state.deviceJob?.status === 'running';
  $('restore-design').disabled = running || state.deviceJob?.status === 'running' || !state.previousDesignAvailable;
  renderProjectTabs();
  updateHistory();
  updateRunActivity();
}
function reviewChangedFeature(id,brief=false) {
  $('design-changes-dialog').close();view='requirements';selected=null;setWorkspaceView('canvas');renderCanvas();renderInspector();
  if(brief){requestAnimationFrame(fitCanvas);return openBrief();}
  if(id&&design.blocks.some(block=>block.id===id)){selectNode(id);requestAnimationFrame(()=>revealNewBlock(id));}
  else requestAnimationFrame(fitCanvas);
}
function changeFieldLabel(path) {
  const labels={description:'Behavior brief',target:'Target board',adapter:'Source adapter',order:'Feature order',label:'Block name',catalogId:'Feature class',count:'Instances / sets',notes:'Behavior description'};
  if(path[0]!=='settings')return labels[path[0]]||fieldLabel(path[0]);
  if(path[1]==='constructorIndex')return 'Constructor choice';
  return (path[1]==='parameters'?'Constructor · ':'Setting · ')+path.slice(path[1]==='parameters'?2:1).map(fieldLabel).join(' / ');
}
function requestValue(value,path) {
  if(value===undefined||value===null||value==='')return 'Not set';
  if(path[0]==='catalogId')return catalog.blocks.find(block=>block.id===value)?.title||String(value).replace(/^class:/,'');
  if(path[1]==='constructorIndex'&&Number.isInteger(value))return 'Choice '+(value+1);
  return typeof value==='object'?JSON.stringify(value,null,2):String(value);
}
function changeValue(change) {
  const label=changeFieldLabel(change.path);
  return el('section',{class:'request-field-change'},el('strong',{},label),el('div',{class:'request-change-values'},
    ...[['Previous request',change.before],['Current request',change.after]].map(([title,value])=>el('label',{},el('span',{},title),
      el('textarea',{readonly:'',rows:'3','aria-label':label+' · '+title},requestValue(value,change.path))))));
}
function renderDesignChanges() {
  const delta=currentDesignChanges(),list=$('design-changes-list'),scroll=list.scrollTop;
  const opened=new Set([...list.querySelectorAll('details[open][data-change-id]')].map(group=>group.dataset.changeId));
  const summary=[delta.added.length?delta.added.length+' added':null,delta.removed.length?delta.removed.length+' removed':null,
    delta.edited.length?delta.edited.length+' edited':null,delta.general.length?delta.general.length+' project '+(delta.general.length===1?'change':'changes'):null,
    delta.sourceChanged?'Loom source changed':null].filter(Boolean);
  $('design-changes-summary').textContent=summary.join(' · ')||(delta.hasBaseline?'No feature or behavior changes found.':'Previous request snapshot unavailable.');list.replaceChildren();
  const stale=!!shownAssembly()&&!state.assembly;
  $('changes-assemble').disabled=!stale||state.job?.status==='running'||state.deviceJob?.status==='running';
  if(!stale)list.append(el('p',{class:'notice'},'The assembled result matches the current requests.'));
  else if(!delta.hasBaseline)list.append(el('p',{class:'notice'},'The original requests were not saved with this result. Review the current features, then assemble again.'));
  if(delta.sourceChanged)list.append(el('p',{class:'notice warning'},'The Loom source changed since this result. Assemble again to review the current APIs and examples.'));
  const group=(id,title,changes,review)=>{
    const details=el('details',{class:'request-change-group change-edited','data-change-id':id,open:opened.has(id)||id==='project'||delta.edited.length===1?'':null},
      el('summary',{},el('span',{class:'change-kind'},'Edited'),el('strong',{},title),el('small',{},changes.length+' '+(changes.length===1?'edit':'edits'))));
    let loaded=false;
    const fill=()=>{if(loaded||!details.open)return;loaded=true;
      const body=el('div',{class:'request-change-body'},...changes.slice(0,50).map(changeValue));
      if(changes.length>50)body.append(el('p',{class:'small'},'Showing the first 50 fields. Review the feature settings for all values.'));
      if(review)body.append(el('button',{class:'quiet compact',onclick:review},id==='project'?'Review behavior brief':'Review feature'));
      details.append(body);
    };
    details.addEventListener('toggle',fill);fill();list.append(details);
  };
  if(delta.general.length)group('project','Project requests',delta.general,delta.general.some(change=>change.path[0]==='description')?()=>reviewChangedFeature(null,true):null);
  for(const [kind,blocks]of [['Added',delta.added],['Removed',delta.removed]])for(const block of blocks)list.append(el('article',{class:'request-change-card change-'+kind.toLowerCase()},
    el('div',{class:'request-change-heading'},el('span',{class:'change-kind'},kind),el('strong',{},block.label),el('small',{},block.count+' '+(block.count===1?'instance':'instances'))),
    block.notes?el('p',{class:'request-change-description'},block.notes):null,
    kind==='Added'?el('button',{class:'quiet compact',onclick:()=>reviewChangedFeature(block.id)},'Review feature'):null));
  for(const block of delta.edited)group(block.id,block.label,block.changes,()=>reviewChangedFeature(block.id));
  if(stale&&delta.hasBaseline&&!delta.total)list.append(el('p',{class:'notice'},'No request differences were found in the saved comparison. Assemble again to review this result.'));
  list.scrollTop=scroll;
}
function updateCodeSearch() {
  const matches=codeFindResults.matches;
  codeFindIndex=matches.length?Math.max(0,Math.min(codeFindIndex,matches.length-1)):0;
  $('code-search-count').textContent=!$('code-query').value?'Enter: next · Shift+Enter: previous':matches.length?(codeFindIndex+1)+' of '+matches.length+(codeFindResults.truncated?'+':''):'No matches';
  $('code-search-prev').disabled=$('code-search-next').disabled=!matches.length;
  $('code').querySelectorAll('mark[data-match-index]').forEach(mark=>mark.classList.toggle('current-code-match',Number(mark.dataset.matchIndex)===codeFindIndex));
}
function revealCodeMatch() {
  const match=codeFindResults.matches[codeFindIndex];if(!match)return;
  const code=$('code'),target=code.querySelector('mark[data-match-index="'+codeFindIndex+'"]')||code.querySelector('[data-line="'+match.line+'"]');
  if(!target)return;
  const area=code.getBoundingClientRect(),rect=target.getBoundingClientRect();
  const left=rect.left<area.left+70?code.scrollLeft+rect.left-area.left-75:rect.right>area.right-15?code.scrollLeft+rect.right-area.right+20:code.scrollLeft;
  code.scrollTo({top:code.scrollTop+rect.top-area.top-code.clientHeight*.3,left:Math.max(0,left),behavior:'instant'});
}
function openCodeSearch() {
  if(!shownAssembly()?.files[fileIndex])return;
  $('code-search-bar').hidden=false;updateCodeSearch();$('code-query').focus({preventScroll:true});$('code-query').select();
}
function closeCodeSearch() {
  clearTimeout(codeFindTimer);codeFindTimer=null;$('code-query').value='';$('code-search-bar').hidden=true;codeFindIndex=0;renderOutput();$('code').focus({preventScroll:true});
}
function moveCodeMatch(direction) {
  const pending=!!codeFindTimer;clearTimeout(codeFindTimer);codeFindTimer=null;renderOutput();const count=codeFindResults.matches.length;if(!count)return;
  codeFindIndex=pending?(direction<0?count-1:0):(codeFindIndex+direction+count)%count;updateCodeSearch();revealCodeMatch();
  selectCodeLine(codeFindResults.matches[codeFindIndex].line);
}
function selectCodeLine(line,scroll=false) {
  const file=shownAssembly()?.files[fileIndex];if(!file)return;
  line=Math.max(1,Math.min(file.content.split('\n').length,Math.trunc(Number(line))||1));
  codeJump={projectId:state.projects?.activeId,file:file.name,line,endLine:line};renderOutput();
  if(scroll) {
    const code=$('code'),target=code.querySelector('[data-line="'+line+'"]');
    if(target)code.scrollTo({top:code.scrollTop+target.getBoundingClientRect().top-code.getBoundingClientRect().top-code.clientHeight*.3,behavior:'instant'});
  }
}
function openCodeFlowResult(item) {
  const assembly=shownAssembly();if(!assembly)return;
  const edge=item.kind==='edge'?assembly.edges[item.id]:null,nodeId=edge?.to||item.id;
  const node=assembly.nodes.find(n=>n.id===nodeId);if(!node)return;
  const routine=assembly.routines?.find(r=>r.nodeIds.includes(nodeId));
  view='flow';showConnections=!!edge||!!assembly.routines?.length&&!routine;
  if(routine)collapsedRoutines.delete(routine.id);
  if(edge)for(const r of assembly.routines||[])if(r.nodeIds.includes(edge.from))collapsedRoutines.delete(r.id);
  $('code-flow-dialog').close();setWorkspaceView('canvas');renderCanvas();selectNode(nodeId);
  if(edge) {
    tracedEdge=item.id;
    $('connections').querySelectorAll('[data-edge-index="'+item.id+'"]').forEach(wire=>wire.classList.add('wire-traced'));
  }
  requestAnimationFrame(()=>{
    canvasZoom=Math.max(.9,canvasZoom);applyZoom();revealNewBlock(nodeId);
    const target=edge?$('connections').querySelector('.wire-label[data-edge-index="'+item.id+'"]'):[...$('nodes').children].find(n=>n.dataset.id===nodeId);
    target?.focus({preventScroll:true});
  });
}
async function showCodeInFlow() {
  const assembly=shownAssembly(),file=assembly?.files[fileIndex];if(!file)return;
  selectCodeLine($('code-line').value);
  const line=codeJump.line,projectId=state.projects?.activeId,key=flowSourceKey(assembly);
  const results=$('code-flow-results');
  $('code-flow-location').textContent=file.name+':'+line+(!state.assembly?' · previous assembly':'');
  results.replaceChildren(el('p',{},'Finding linked steps and connections…'));$('code-flow-dialog').showModal();
  await loadFlowSources(assembly);
  if(!$('code-flow-dialog').open)return;
  if(state.projects?.activeId!==projectId||!shownAssembly()||flowSourceKey(shownAssembly())!==key){results.replaceChildren(el('p',{class:'notice'},'The application changed. Close this window and select a line from the updated file.'));return;}
  const sources=flowSourceData(assembly);
  if(!sources){results.replaceChildren(el('p',{class:'notice warning'},'Could not load code references. Close this window and try again.'));return;}
  const matches=flowMatchesAtLine(assembly,sources,file.name,line);
  if(!matches.length){results.replaceChildren(el('p',{class:'notice'},'No linked step or connection for this line. Includes, comments and helper details may not have their own flow block.'));return;}
  const steps=el('ul',{class:'project-find-results','aria-label':'Linked steps'}),connections=el('ul',{class:'project-find-results','aria-label':'Linked connections'});
  for(const item of matches) {
    const edge=item.kind==='edge'?assembly.edges[item.id]:null,node=assembly.nodes.find(n=>n.id===(edge?.to||item.id));
    const routine=assembly.routines?.find(r=>r.nodeIds.includes(node.id));
    const confidence=item.reference.match==='context'?'Routine context':item.reference.match==='candidate'?'Suggested match':'Linked code';
    const context=edge?'Connection · '+(assembly.nodes.find(n=>n.id===edge.from)?.label||'')+' → '+node.label:routine?'Step · '+routine.name+'()':'Supporting block';
    (edge?connections:steps).append(el('li',{},el('button',{class:'project-result','data-hint-title':edge?'Trace this connection':'Reveal this step',
      'data-hint':edge?'Open the flow and highlight this wire. Press Escape to clear it.':'Open the flow at this block'+(routine?' in '+routine.name+'().':'.'),
      'data-hint-category':node.category,onclick:()=>openCodeFlowResult(item)},
      el('span',{class:'block-icon','aria-hidden':'true'},edge?'→':ingredientIcon(node.category)),
      el('span',{},el('strong',{},edge?edge.label:friendlyNode(node).label),el('small',{},context),el('span',{class:'project-result-description'},confidence+' · '+sourceCaption(item.reference))))));
  }
  results.replaceChildren(el('p',{class:'small'},'Choose a linked step to view it in the flow.'));
  if(steps.children.length)results.append(steps);
  if(connections.children.length)results.append(el('details',{class:'code-flow-connections',open:steps.children.length?null:''},el('summary',{},'Connections ('+connections.children.length+')'),connections));
}
function renderOutput() {
  const a = shownAssembly();
  $('return-to-build').hidden=!buildReturn||buildReturn.projectId!==state.projects?.activeId||contentView!=='code';
  $('return-to-flow').hidden=!codeReturn||codeReturn.projectId!==state.projects?.activeId||contentView!=='code';
  $('return-to-flow').textContent=codeReturn?.view==='requirements'?'← Back to features':'← Back to flow';
  $('file-tabs').replaceChildren();
  $('custom-code-notice').hidden=shownOrigin()!=='custom';
  if (a) {
    fileIndex = Math.min(fileIndex, a.files.length - 1);
    a.files.forEach((f, i) => $('file-tabs').append(el('button', { class: i === fileIndex ? 'active' : '', onclick: () => { fileIndex = i;codeFindIndex=0;renderOutput();if($('code-query').value)revealCodeMatch(); } }, f.name)));
    $('file-tabs').append(el('span', { class: 'muted' }, shownOrigin()==='custom'?'Customized code · save before building':shownOrigin() === 'example' ? 'Original example · source reconstruction' : 'Codex output · review validation'));
    codeFindResults=findCodeMatches(a.files[fileIndex]?.content||'',$('code-query').value);
    renderCode($('code'), a.files[fileIndex]?.content || '', a.files[fileIndex]?.name || '',codeFindResults.matches);
    const highlighted=codeJump?.projectId===state.projects?.activeId&&codeJump.file===a.files[fileIndex]?.name;
    $('code').querySelectorAll('.code-line').forEach(line=>line.classList.toggle('code-jump-highlight',highlighted&&Number(line.dataset.line)>=codeJump.line&&Number(line.dataset.line)<=codeJump.endLine));
    if(highlighted)$('file-tabs').append(el('span',{class:'code-jump-location',role:'status'},'Lines '+codeJump.line+(codeJump.endLine>codeJump.line?'–'+codeJump.endLine:'')));
  } else {
    codeFindResults={matches:[],truncated:false};
    $('file-tabs').append(el('span', {}, 'No application file yet'), el('span', { class: 'muted' }, 'Assemble to generate one'));
    renderCode($('code'), '// Choose your features, then select “Assemble with Codex”.\n//\n// Codex reviews the selected repository and examples,\n// resolves compatible components, and writes the application.\n// Its files and organized graph appear here automatically.\n//\n// The instant diagram preview only summarizes your intent.\n// It does not invent APIs or claim hardware compatibility.', 'FeatureApplication.ino');
  }
  updateCodeSearch();
  const currentFile=a?.files[fileIndex],lineCount=currentFile?.content.split('\n').length||1;
  $('code-line').disabled=$('code-show-flow').disabled=!currentFile;
  $('code-line').max=String(lineCount);
  $('code-line').value=String(codeJump?.projectId===state.projects?.activeId&&codeJump.file===currentFile?.name?Math.min(codeJump.line,lineCount):1);
  codeEditor?.render();
  const notes = $('decisions-panel'); notes.replaceChildren();
  if (!a) {
    notes.append(el('div', { class: 'notice' + (state.assemblyError ? ' warning' : '') }, state.assemblyError || 'No reviewed assembly yet. Codex will explain supporting components, compatibility choices, data routes and validation here.'));
    const missing = design.blocks.filter(b => !meta(b));
    if (missing.length) notes.append(el('div', { class: 'notice warning' }, 'Missing from current checkout: ' + missing.map(b => b.label).join(', ') + '. Codex must resolve these blocks.'));
    if (state.job) notes.append(el('h3', {}, 'Assembly run'), el('div', { class: 'notice' + (['failed','interrupted'].includes(state.job.status) ? ' warning' : '') }, state.job.message),
      el('button',{class:'quiet',onclick:()=>$('run-details').click()},'View activity'));
    return;
  }
  if(!state.assembly)notes.append(el('div',{class:'notice previous-assembly-notice'},el('p',{},'These notes describe the previous assembly. Review the changed requests before assembling again.'),
    el('button',{class:'quiet compact',onclick:()=>{$('review-changes').click();}},'Review Changes')));
  notes.append(el('p', {}, a.summary), el('h3', {}, 'Assembly decisions'), el('ul', {}, ...a.decisions.map(d => el('li', {}, d))));
  for(const review of compatibilityConcerns(a)) {
    notes.append(el('h3',{},review.requestId==='$brief'?'Behavior brief':design.blocks.find(b=>b.id===review.requestId)?.label||'Feature review'),assessmentCard(review,!!state.assembly));
  }
  if (a.nodes.some(n => n.added)) notes.append(el('h3', {}, 'Added to support your features'), el('ul', {}, ...a.nodes.filter(n => n.added).map(n => el('li', {}, n.label + ' — ' + n.reason))));
  if (a.warnings.length) notes.append(el('h3', {}, 'Compatibility & open questions'), el('ul', {}, ...a.warnings.map(w => el('li', {}, w))));
  notes.append(el('h3', {}, 'Validation'), el('div', { class: 'notice' }, a.validation));
  if (state.outputDirectory) notes.append(el('h3', {}, 'Generated files on disk'), el('div', { class: 'path' }, state.outputDirectory));
  if (['failed','interrupted'].includes(state.job?.status)) notes.append(el('h3', {}, 'Latest assembly run'), el('div', { class: 'notice warning' }, state.job.message),
    el('button',{class:'quiet',onclick:()=>$('run-details').click()},'View activity'));
}
function download(name, content, type) {
  const url = URL.createObjectURL(new Blob([content], { type }));
  const a = el('a', { href: url, download: name }); document.body.append(a); a.click(); a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}
function escapeXML(s) { return String(s).replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&apos;' }[c])); }
function diagramSVG() {
  const features=view==='requirements',assembly=shownAssembly();
  const featureNodes=features?[...design.blocks.map(block=>({...block,category:meta(block)?.category||'intent',reason:briefSummary(block)})),...supportingFeatures(design,assembly)]:[];
  const presentation = features?featureBoardPresentation(featureNodes,positions):workflowPresentation(assembly ? annotatedGraph(assembly) : draftGraph(), showConnections, collapsedRoutines), g = presentation.graph, p = presentation.positions;
  const routed = graphRoutes(g, p, presentation.sections), bounds = graphBounds(p, routed, presentation.sections);
  const x = bounds.x-16, y = bounds.y-64;
  const width = Math.max(700,bounds.w+32), height = Math.max(300,bounds.h+84);
  let s = '<svg xmlns="http://www.w3.org/2000/svg" width="' + width + '" height="' + height + '" viewBox="' + x + ' ' + y + ' ' + width + ' ' + height + '"><defs>';
  for (const [kind, color] of Object.entries({ data: '#67937f', dependency: '#9b83af', cycle: '#bb9a61', flow:'#397da8',callback:'#9c62ad' })) s += '<marker id="export-' + kind + '" viewBox="0 0 8 8" refX="7" refY="4" markerWidth="5" markerHeight="5" orient="auto"><path d="M0,0 L8,4 L0,8 Z" fill="' + color + '"/></marker>';
  const context=features?(!state.assembly&&assembly?'Feature board · requested features; supporting components from previous assembly':'Feature board · requested features and supporting components'):!state.assembly&&assembly?'Assembled flow · previous assembly · '+lastBuildChange():g.preview?'Intent preview · unreviewed':'Assembled flow · blue: step order · green: data · purple: callback/dependency · gold: repeat';
  s += '</defs><title>'+escapeXML(design.name+' · '+(features?'Feature board':'Assembled flow'))+'</title><rect x="' + x + '" y="' + y + '" width="100%" height="100%" fill="#f7f8f4"/><text x="'+(x+24)+'" y="'+(y+28)+'" font-family="sans-serif" font-size="20" fill="#183b38">' + escapeXML(design.name) + '</text><text x="'+(x+24)+'" y="'+(y+46)+'" font-family="sans-serif" font-size="10" fill="#53645c">' + escapeXML(context) + '</text>';
  for (const section of presentation.sections) {
    if (section.w) s += '<rect x="' + section.x + '" y="' + section.y + '" width="' + section.w + '" height="' + section.h + '" rx="22" fill="#edf3e6" stroke="#b8ccb1"/>';
    s += '<text x="' + (section.x + 14) + '" y="' + (section.y + 22) + '" font-family="sans-serif" font-size="14" font-weight="bold" fill="#3d624b">' + escapeXML(section.title) + '</text>';
    if (section.w) s += '<text x="' + (section.x + 14) + '" y="' + (section.y + 42) + '" font-family="sans-serif" font-size="10" fill="#53645c">' + escapeXML(section.description + ' · ' + section.count + ' steps') + '</text>';
    if (!section.collapsed && section.bindings?.length) s += '<text x="' + (section.x + 14) + '" y="' + (section.y + 67) + '" font-family="sans-serif" font-size="10" fill="#66537d">' + escapeXML(section.bindings.map(b => (b.kind === 'registration' ? 'Registered via ' : 'Called by ') + b.label).join(' · ')) + '</text>';
  }
  for (const route of routed) {
    const e = route.edge, color = { data: '#67937f', dependency: '#9b83af', cycle: '#bb9a61', flow:'#397da8',callback:'#9c62ad' }[e.kind];
    s += '<path d="' + route.d + '" fill="none" stroke="' + color + '" stroke-width="2" marker-end="url(#export-' + e.kind + ')"' + (['data','flow'].includes(e.kind) ? '' : ' stroke-dasharray="6 4"') + '><title>' + escapeXML(e.label) + '</title></path>';
    if (route.leader) s += '<path d="M' + route.leader.from.x + ',' + route.leader.from.y + ' L' + route.leader.to.x + ',' + route.leader.to.y + '" stroke="' + color + '" stroke-width=".8" fill="none"/>';
  }
  for (const route of routed.filter(r => r.label)) {
    const b = route.label;
    s += '<rect x="' + b.x + '" y="' + b.y + '" width="' + b.w + '" height="' + b.h + '" rx="7" fill="#f7f8f4" stroke="#d9e2d0"/>';
    b.lines.forEach((line, i) => { s += '<text x="' + (b.x + b.w / 2) + '" y="' + (b.y + 13 + i * 12) + '" text-anchor="middle" font-family="sans-serif" font-size="10" fill="#5c7560">' + escapeXML(line) + '</text>'; });
  }
  for (const n of g.nodes) {
    const { x, y } = p[n.id];
    const appearance = features||showConnections ? n : friendlyNode(n),review=compatibilityForNode(state.assembly,n.id);
    const concern=review&&review.status!=='supported',warning=review?.status==='needs-info';
    const color = concern?(warning?'#8a661e':'#b3453f'):colors[appearance.category] || colors.system;
    const description=concern?review.summary:n.reason,reference=features?null:flowSourceData()?.nodes[n.id]?.reference;
    s += '<g><title>' + escapeXML(n.label+' · '+description+(reference?' · '+reference.file+':'+reference.line:'')) + '</title><rect x="' + x + '" y="' + y + '" width="' + NODE_W + '" height="' + NODE_H + '" rx="28" fill="' + (concern?warning?'#fff6dc':'#fff0eb':n.added ? '#f0f9f0' : '#fff') + '" stroke="' + color + '"' + (n.added ? ' stroke-dasharray="5 3"' : '') + '/>';
    const words = appearance.label.match(/.{1,27}(?:\s|$)|.{1,27}/g) || [appearance.label];
    const owners=features?[]:flowSourceData()?.nodes[n.id]?.owners||[];
    const step = presentation.steps?.[n.id];
    if (step && step.routine.kind !== 'components') s += '<text x="' + (x + NODE_W - 14) + '" text-anchor="end" y="' + (y + 101) + '" font-family="sans-serif" font-size="10" fill="#53645c">Source step ' + step.number + '</text>';
    words.slice(0, features||owners.length?2:3).forEach((line, i) => { s += '<text x="' + (x + 14) + '" y="' + (y + 29 + i * 18) + '" font-family="sans-serif" font-size="12" fill="#183b38">' + escapeXML(line.trim()) + '</text>'; });
    if(features){const lines=String(description).match(/.{1,33}(?:\s|$)|.{1,33}/g)||[];lines.slice(0,3).forEach((line,i)=>{s+='<text x="'+(x+14)+'" y="'+(y+63+i*11)+'" font-family="sans-serif" font-size="8.5" fill="#465d4d">'+escapeXML(line.trim())+'</text>';});}
    if(owners.length){const label=owners.map(o=>o.object+' · '+o.className).join(' / ');s+='<text x="'+(x+14)+'" y="'+(y+67)+'" font-family="monospace" font-size="8" fill="#42614e"><title>'+escapeXML(label)+'</title>'+escapeXML(label.length>34?label.slice(0,33)+'…':label)+'</text>';}
    const filename=reference?.file.split('/').pop(),sourceFooter=filename?(['candidate','context'].includes(reference.match)?'≈ ':'')+((reference.scope==='application'||reference.applicationFile)?'Line: '+reference.line:(filename.length>25?filename.slice(0,24)+'…':filename)+':'+reference.line):'';
    const footer=concern?assessmentLabels[review.status]:features?n.added?'+ Added by Codex':'Requested · '+n.count+(n.count===1?' instance':' instances'):sourceFooter|| (n.added?'+ Added by Codex':step?'Routine step':'Requested feature');
    s += '<text x="' + (x + 14) + '" y="' + (y + 101) + '" font-family="sans-serif" font-size="8.5" fill="' + color + '">' + escapeXML(footer) + '</text></g>';
  }
  return s + '</svg>';
}
async function poll() {
  if (pollBusy || projectBusy || localDirty || dragging || document.hidden) return;
  pollBusy = true;
  try {
    const epoch = editEpoch, next = await api('/api/state');
    if (editEpoch !== epoch || projectBusy || localDirty) return;
    if (next.projects?.activeId !== state.projects?.activeId) { editEpoch++; applyProject(next); return; }
    const sourceChanged = next.sourceFingerprint !== state.sourceFingerprint;
    const branchChanged = next.sourceBranch !== state.sourceBranch;
    const assemblyChanged=JSON.stringify(next.assembly||next.lastBuild?.assembly)!==JSON.stringify(shownAssembly());
    const becameCurrent=!!next.assembly&&!state.assembly;
    const resultChanged = !!next.assembly!==!!state.assembly||assemblyChanged || next.assemblyError !== state.assemblyError;
    const jobChanged = JSON.stringify(next.job) !== JSON.stringify(state.job);
    const deviceChanged = JSON.stringify(next.deviceJob) !== JSON.stringify(state.deviceJob);
    const externalDesign = next.designRevision !== state.designRevision;
    state = next;
    rememberBuiltDesign(next);
    loadFlowSources();
    renderProjectTabs();
    updateHistory();
    if (branchChanged) loadBranches().catch(() => {});
    if (sourceChanged) { catalog = await api('/api/catalog'); updateSource(); renderLibrary(); toast('Source changed. Codex must review the current checkout.'); }
    if (externalDesign) { design = next.design;changesCache=null; $('project-name').value = design.name; $('brief').value = design.description; }
    if (resultChanged || sourceChanged || externalDesign) {
      if (next.assembly&&assemblyChanged) {
        if(contentView==='canvas')view = 'flow';
        toast(next.assemblyOrigin === 'example' ? 'Example opened with feature blocks, code and a flow graph.' : 'Codex assembly received. Review the flow, files and notes.');
      }
      else if(becameCurrent)toast('Current requests match the previous assembly.');
      renderCanvas(); renderInspector(); renderOutput();
      if($('flow-outline-dialog').open)renderFlowOutline();
      if($('design-changes-dialog').open)renderDesignChanges();
    }
    if (jobChanged) { updateStatus(); renderOutput(); }
    if (deviceChanged) updateStatus();
    $('save-status').textContent = 'Saved locally';
  } catch (e) { $('save-status').textContent = 'Server disconnected'; }
  finally { pollBusy = false; }
}

async function init() {
  [catalog, state] = await Promise.all([api('/api/catalog'), api('/api/state')]);
  design = state.design; selected = null;
  rememberBuiltDesign(state);
  $('project-name').value = design.name; $('brief').value = design.description;
  $('save-status').textContent = 'Saved locally';
  try { $('model').value = localStorage.getItem('feature-studio-model') || 'gpt-6.1-sol'; } catch {}
  if (shownAssembly()) view = 'flow';
  updateSource(); renderLibrary(); renderCanvas(); renderInspector(); renderOutput();
  renderProjectTabs();
  setTray(true);
  requestAnimationFrame(showFlowStart);
  installGameFeedback();
  installDeviceTools({ api, el, toast, getState: () => state, save, reviewCode:async (projectId,refresh=false)=>{
    if(projectId&&(projectId!==state.projects.activeId||refresh)&&!await changeProject('/api/projects/open',{id:projectId}))return false;
    setWorkspaceView('code');renderOutput();return true;},
    showCodeLocation:(location,returnToBuild)=>{buildReturn={projectId:location.projectId,open:returnToBuild};jumpToCode(location);},onJob: job => { state.deviceJob=job; updateStatus(); } });
  $('return-to-build').onclick=()=>buildReturn?.open();
  updateRunActivity=installRunActivity({el,toast,getState:()=>state,
    reviewCode:()=>{setWorkspaceView('code');renderOutput();},
    reviewRequests:()=>{if(!$('review-changes').hidden)$('review-changes').click();else reviewChangedFeature();},
    cancelRun:()=>$('cancel-run').click(),
    reviewAndSend:job=>{
      if(job?.model&&job.model!=='Configured default')$('model').value=job.model;
      if(job?.effort)$('reasoning-effort').replaceChildren(el('option',{value:job.effort},fieldLabel(job.effort)));
      if(['read-only','workspace-write'].includes(job?.sandbox))$('permission-mode').value=job.sandbox;
      $('permission-mode').onchange?.();$('handoff').click();
    }});
  updateHistory=installHistory({ api,el,getState:()=>state,isDirty:()=>localDirty,isBusy:()=>projectBusy||state.job?.status==='running'||state.deviceJob?.status==='running',save,changeProject,toast });
  installHoverHints({ icon: ingredientIcon, isDragging: () => !!dragging });
  document.addEventListener('click',event=>{
    if(event.target.closest('#theme-toggle'))document.querySelectorAll('.elastic-surface').forEach(skin=>finishStretch(skin.parentElement));
  },true);
  $('toggle-brief').onclick = () => {
    const open = document.body.classList.toggle('brief-open');
    $('toggle-brief').setAttribute('aria-expanded', String(open));
    if (open) $('brief').focus();
  };
  $('close-brief').onclick = () => {
    document.body.classList.remove('brief-open'); $('toggle-brief').setAttribute('aria-expanded', 'false');
    $('toggle-brief').focus({ preventScroll: true });
  };
  $('source-branch').onchange = () => { $('switch-branch').hidden = $('source-branch').value === branchInfo?.branch; };
  $('switch-branch').onclick = async () => {
    $('switch-branch').disabled = true;
    try {
      await save();
      const result = await api('/api/branch', { branch: $('source-branch').value });
      catalog = await api('/api/catalog'); state = await api('/api/state');
      updateSource(); renderLibrary(); renderCanvas(); renderInspector(); renderOutput(); await loadBranches();
      toast('Using ' + result.branch + '. Source rescanned; review and assemble against this checkout.');
    } catch (e) { toast(e.message); await loadBranches().catch(() => {}); }
    finally { $('switch-branch').disabled = false; }
  };
  $('open-code').onclick = () => {
    const file = shownAssembly()?.files[fileIndex];
    if (file) openSource({ fileName: file.name });
  };
  $('find-code').onclick=openCodeSearch;
  $('copy-code').onclick=async()=>{
    const file=shownAssembly()?.files[fileIndex];if(!file)return;
    try{await navigator.clipboard.writeText(file.content);toast('Copied '+file.name+(!state.assembly?' · previous assembly':''));}
    catch{toast('Clipboard access is unavailable. Select the code and copy it manually.');}
  };
  $('code-query').addEventListener('input',()=>{
    clearTimeout(codeFindTimer);codeFindIndex=0;codeFindTimer=setTimeout(()=>{codeFindTimer=null;renderOutput();revealCodeMatch();},90);
  });
  $('code-query').addEventListener('keydown',event=>{
    if(event.key==='Enter'){event.preventDefault();moveCodeMatch(event.shiftKey?-1:1);}
  });
  $('code-search-next').onclick=()=>moveCodeMatch(1);$('code-search-prev').onclick=()=>moveCodeMatch(-1);
  $('close-code-search').onclick=closeCodeSearch;
  $('code-search-bar').addEventListener('keydown',event=>{
    if(event.key==='Escape'){event.preventDefault();event.stopPropagation();closeCodeSearch();}
  });
  document.addEventListener('keydown',event=>{
    if(contentView==='code'&&(event.ctrlKey||event.metaKey)&&event.key.toLowerCase()==='f'&&!document.querySelector('dialog[open]')&&shownAssembly()?.files[fileIndex]){event.preventDefault();openCodeSearch();}
  });
  $('zoom-out').onclick = () => zoomAt((zoomMotion?.target || canvasZoom) / 1.16);
  $('zoom-in').onclick = () => zoomAt((zoomMotion?.target || canvasZoom) * 1.16);
  $('zoom-fit').onclick = fitCanvas;
  $('canvas-scroll').addEventListener('wheel', e => {
    if (e.shiftKey || dragging || e.target.closest('textarea,input')) return;
    e.preventDefault();
    const delta = e.deltaY * (e.deltaMode === 1 ? 16 : e.deltaMode === 2 ? $('canvas-scroll').clientHeight : 1);
    zoomAt((zoomMotion?.target || canvasZoom) * Math.exp(-delta * .0009), e);
  }, { passive: false });
  let pan, panFrame;
  $('canvas-scroll').addEventListener('pointerdown', e => {
    if (![0, 1].includes(e.button) || e.target.closest('.node,.wire-label,button,a,input,select,textarea')) return;
    const area = $('canvas-scroll');
    stopZoom(); area.scrollTo({ left: area.scrollLeft, top: area.scrollTop, behavior: 'instant' });
    pan = { x: e.clientX, y: e.clientY, left: area.scrollLeft, top: area.scrollTop };
    area.setPointerCapture(e.pointerId); area.classList.add('panning'); e.preventDefault();
  });
  $('canvas-scroll').addEventListener('pointermove', e => {
    if (!pan) return;
    pan.nextLeft = pan.left - (e.clientX - pan.x); pan.nextTop = pan.top - (e.clientY - pan.y);
    if (!panFrame) panFrame = requestAnimationFrame(() => {
      panFrame = null;
      if (!pan) return;
      $('canvas-scroll').scrollLeft = pan.nextLeft; $('canvas-scroll').scrollTop = pan.nextTop;
    });
  });
  const stopPan = e => {
    cancelAnimationFrame(panFrame); panFrame = null;
    const backgroundClick=pan&&e.type==='pointerup'&&e.button===0&&Math.hypot(e.clientX-pan.x,e.clientY-pan.y)<5;
    if (pan?.nextLeft != null) { $('canvas-scroll').scrollLeft = pan.nextLeft; $('canvas-scroll').scrollTop = pan.nextTop; }
    pan = null; $('canvas-scroll').classList.remove('panning');
    if(backgroundClick)clearSelection();
  };
  $('canvas-scroll').addEventListener('pointerup', stopPan);
  $('canvas-scroll').addEventListener('pointercancel', stopPan);
  new ResizeObserver(() => {
    if(contentView!=='canvas')return;
    document.querySelector('.main-pane').style.setProperty('--canvas-top', $('canvas-scroll').offsetTop + 'px');
    const before = canvasOffset; applyZoom();
    if (canvasOffset.x !== before.x) $('canvas-scroll').scrollLeft += canvasOffset.x - before.x;
    if (canvasOffset.y !== before.y) $('canvas-scroll').scrollTop += canvasOffset.y - before.y;
  }).observe($('canvas-scroll'));
  $('toggle-tray').onclick = () => setTray(!document.body.classList.contains('tray-collapsed'));
  $('close-tray').onclick = () => { setTray(true); $('toggle-tray').focus({ preventScroll: true }); };
  $('category-list').addEventListener('wheel', e => {
    const bar = $('category-list');
    if (bar.scrollWidth <= bar.clientWidth || Math.abs(e.deltaX) >= Math.abs(e.deltaY)) return;
    e.preventDefault();
    const unit = e.deltaMode === 1 ? 16 : e.deltaMode === 2 ? bar.clientWidth : 1;
    bar.scrollBy({ left: e.deltaY * unit, behavior: matchMedia('(prefers-reduced-motion: reduce)').matches ? 'instant' : 'smooth' });
  }, { passive: false });
  $('search').addEventListener('input', renderLibrary);
  $('project-name').addEventListener('input', () => { design.name = $('project-name').value; changed(false); });
  $('brief').addEventListener('input', () => { design.description = $('brief').value; changed(); });
  $('requirements-tab').onclick = () => { view = 'requirements'; setWorkspaceView('canvas');renderCanvas(); renderInspector(); requestAnimationFrame(fitCanvas); };
  $('flow-tab').onclick = () => { view = 'flow';setWorkspaceView('canvas');renderCanvas(); renderInspector(); showFlowStart(); };
  $('flow-detail').onclick = () => { showConnections = !showConnections; renderCanvas(); fitCanvas(); };
  $('flow-outline').onclick=openFlowOutline;
  $('outline-expand').onclick=()=>{outlineState.expanded=new Set((shownAssembly()?.routines||[]).map(r=>r.id));renderFlowOutline();};
  $('outline-collapse').onclick=()=>{outlineState.expanded.clear();renderFlowOutline();};
  $('code-tab').onclick = () => {setWorkspaceView('code');renderOutput();};
  $('code-line').addEventListener('change',()=>selectCodeLine($('code-line').value,true));
  $('code-line').addEventListener('keydown',event=>{if(event.key==='Enter'){event.preventDefault();selectCodeLine($('code-line').value,true);showCodeInFlow();}});
  $('code-show-flow').onclick=showCodeInFlow;
  $('code').addEventListener('click',event=>{
    const line=event.target.closest('.code-line');
    if(line&&window.getSelection()?.isCollapsed!==false)selectCodeLine(Number(line.dataset.line));
  });
  $('return-to-flow').onclick=()=>{
    if(!codeReturn||codeReturn.projectId!==state.projects?.activeId)return;
    const saved=codeReturn;view=saved.view;selected=saved.selected;showConnections=saved.showConnections;
    setWorkspaceView('canvas');renderCanvas();renderInspector();
    requestAnimationFrame(()=>{canvasZoom=saved.zoom;applyZoom();$('canvas-scroll').scrollTo({left:canvasOffset.x+saved.worldLeft*canvasZoom,top:canvasOffset.y+saved.worldTop*canvasZoom,behavior:'instant'});
      ([...$('nodes').children].find(node=>node.dataset.id===selected)||$('canvas')).focus({preventScroll:true});});
  };
  $('decisions-tab').onclick = () => {setWorkspaceView('notes');renderOutput();};
  const workspaceTabs=['requirements-tab','flow-tab','code-tab','decisions-tab'];
  for(const id of workspaceTabs)$(id).addEventListener('keydown',event=>{
    if(!['ArrowLeft','ArrowRight','Home','End'].includes(event.key))return;
    event.preventDefault();const i=workspaceTabs.indexOf(id),next=event.key==='Home'?0:event.key==='End'?3:(i+(event.key==='ArrowRight'?1:3))%4;
    $(workspaceTabs[next]).click();$(workspaceTabs[next]).focus();
  });
  $('add-custom').onclick = () => addBlock('intent:custom');
  $('find-project').onclick=openProjectSearch;
  $('review-changes').onclick=()=>{$('design-changes-list').replaceChildren();renderDesignChanges();$('design-changes-dialog').showModal();$('design-changes-list').scrollTop=0;};
  $('changes-review-features').onclick=()=>reviewChangedFeature();
  $('changes-assemble').onclick=()=>{$('design-changes-dialog').close();$('handoff').click();};
  $('project-query').addEventListener('input',renderProjectSearch);
  $('project-query').addEventListener('keydown',event=>{
    const first=$('project-find-results').querySelector('button');
    if(first&&['Enter','ArrowDown'].includes(event.key)){event.preventDefault();if(event.key==='Enter')first.click();else first.focus();}
  });
  $('project-find-results').addEventListener('keydown',event=>{
    if(!['ArrowDown','ArrowUp','Home','End'].includes(event.key))return;
    const buttons=[...$('project-find-results').querySelectorAll('button')],index=buttons.indexOf(event.target);if(index<0)return;
    event.preventDefault();
    if(event.key==='ArrowUp'&&index===0){$('project-query').focus();return;}
    const next=event.key==='Home'?0:event.key==='End'?buttons.length-1:(index+(event.key==='ArrowDown'?1:buttons.length-1))%buttons.length;
    buttons[next].focus();
  });
  document.addEventListener('keydown',event=>{
    if((event.ctrlKey||event.metaKey)&&event.key.toLowerCase()==='k'&&!document.querySelector('dialog[open]')){event.preventDefault();openProjectSearch();}
  });
  $('close-inspector').onclick = clearSelection;
  document.addEventListener('keydown',e=>{
    if(e.key!=='Escape'||e.defaultPrevented||document.querySelector('dialog[open]'))return;
    const menus=document.querySelectorAll('.utility-menu[open]');
    if(menus.length){menus.forEach(menu=>menu.open=false);e.preventDefault();return;}
    if(selected||tracedEdge!==null){e.preventDefault();clearSelection();}
  });
  document.addEventListener('pointerdown',event=>document.querySelectorAll('.utility-menu[open]').forEach(menu=>{
    if(!menu.contains(event.target))menu.open=false;
  }));
  document.querySelector('.canvas-menu').addEventListener('click',event=>{
    if(event.target.closest('button'))event.currentTarget.open=false;
  });
  $('empty-brief').onclick = openBrief;
  $('empty-sensors').onclick = () => { category = 'sensors'; $('search').value = ''; setTray(false); renderLibrary(); $('search').focus(); };
  async function changeDesign(action) {
    try {
      $('new-design').disabled = true; $('restore-design').disabled = true;
      await changeProject('/api/design/' + action, {}, { blank: action === 'new' });
      document.querySelector('.utility-menu').open = false;
    } catch (e) { toast(e.message); }
    finally { updateStatus(); }
  }
  $('new-design').onclick = () => changeDesign('new');
  $('restore-design').onclick = () => changeDesign('restore');
  $('arrange').onclick = () => {
    if (view === 'flow') return renderCanvas();
    const gridX=Math.max(GRID_X,...design.blocks.map(b=>blockSize(b).w+50)),gridY=Math.max(GRID_Y,...design.blocks.map(b=>blockSize(b).h+40));
    const cols = Math.max(1, Math.floor(($('canvas-scroll').clientWidth / canvasZoom - 50) / gridX));
    design.blocks.forEach((b, i) => { b.position = { x: 35 + (i % cols) * gridX, y: 80 + Math.floor(i / cols) * gridY }; });
    changed(false); renderCanvas();
  };
  $('clear').onclick = () => {
    if (!design.blocks.length) return;
    design.blocks = []; selected = null; changed(); renderCanvas(); renderInspector();
    toast('Canvas cleared. Undo with Ctrl+Z.');
  };
  $('canvas').addEventListener('dragover', e => { if (e.dataTransfer.types.includes('application/x-feature-studio')) { e.preventDefault(); $('canvas').classList.add('dragover'); e.dataTransfer.dropEffect = 'copy'; } });
  $('canvas').addEventListener('dragleave', e => { if (!$('canvas').contains(e.relatedTarget)) $('canvas').classList.remove('dragover'); });
  $('canvas').addEventListener('drop', e => {
    e.preventDefault(); $('canvas').classList.remove('dragover');
    const bounds = $('canvas').getBoundingClientRect(), id = e.dataTransfer.getData('application/x-feature-studio');
    addBlock(id, { x: Math.max(5, (e.clientX - bounds.left) / canvasZoom - NODE_W / 2), y: Math.max(65, (e.clientY - bounds.top) / canvasZoom - 40) });
  });
  $('refresh').onclick = async () => {
    try { await save(); catalog = await api('/api/refresh', {}); await poll(); updateSource(); renderLibrary(); renderCanvas(); renderInspector(); toast('Rescanned source and examples.'); } catch (e) { toast(e.message); }
  };
  $('export').onclick = async () => {
    try {
      await save(); const bundle = await api('/api/project-export');
      download((design.name.replace(/[^a-z0-9_-]+/gi, '-') || 'feature') + '.studio.json', JSON.stringify(bundle, null, 2), 'application/json');
      toast(bundle.assembly || bundle.lastBuild ? 'Exported feature blocks, flow, source files and notes.' : 'Exported project inputs. Assemble first to include a completed flow and source files.');
    } catch (e) { toast(e.message); }
  };
  $('import').addEventListener('change', async e => {
    const file = e.target.files[0]; if (!file) return;
    try {
      if (file.size > 16_000_000) throw new Error('Project file is too large.');
      const imported = JSON.parse(await file.text());
      await changeProject('/api/projects/import', imported, { message: 'Project imported in a new tab.' });
    } catch (error) { toast('Import failed: ' + error.message); }
    e.target.value = '';
  });
  $('handoff').onclick = () => {
    $('cli-availability').textContent = state.codexAvailable ? 'Codex CLI available · uses its own sign-in. Repository and feature settings included.' : 'Codex CLI was not found. Choose Codex sign-in / setup below to get started.';
    if(state.preparation&&state.codexAvailable) $('cli-availability').textContent += ' Prepared '+state.preparation.selectedRecipes+' matching recipes and '+state.preparation.apiCount+' API references from this checkout.';
    $('run-dialog').showModal();
    loadModels();
  };
  installCodexAccount({api,toast,onAvailable:available=>{state.codexAvailable=available;updateStatus();}});
  codeEditor=installCodeEditor({api,toast,getContext:()=>({projectId:state.projects?.activeId,file:shownAssembly()?.files[fileIndex],busy:state.job?.status==='running'||state.deviceJob?.status==='running'}),onSaved:async()=>{state=await api('/api/state');rememberBuiltDesign(state);renderCanvas();renderOutput();}});
  codeEditor.render();
  $('permission-mode').onchange = () => {
    $('permission-help').textContent = $('permission-mode').value === 'read-only' ? 'Codex can inspect the repository with its tools. The app saves the generated files after validating the response.' : 'Allows file writes within the selected repository for validation. The prompt still restricts changes to the task and prohibits changing Loom library source or flashing hardware.';
  };
  $('run-assembly').onclick = async () => {
    try {
      $('run-assembly').disabled = true;
      await save();
      try { localStorage.setItem('feature-studio-model', $('model').value.trim()); } catch {}
      state.job = await api('/api/assemble', { model: $('model').value.trim() || 'gpt-6.1-sol', effort: $('reasoning-effort').value, sandbox: $('permission-mode').value });
      $('run-dialog').close(); updateStatus(); renderOutput();
      toast('Sent to Codex. Progress and the result will appear here.');
    } catch (e) { toast(e.message); updateStatus(); }
  };
  $('cancel-run').onclick = async () => {
    try { state.job = await api('/api/cancel', {}); updateStatus(); renderOutput(); } catch (e) { toast(e.message); }
  };
  $('use-desktop').onclick = async () => {
    try {
      $('run-dialog').close();
      await save(); const result = await api('/api/handoff', {});
      $('handoff-prompt').value = result.prompt; $('bridge-location').textContent = result.workspace; $('bridge-dialog').showModal();
    } catch (e) { toast(e.message); }
  };
  $('copy-prompt').onclick = async () => {
    try { await navigator.clipboard.writeText($('handoff-prompt').value); $('copy-prompt').textContent = 'Copied ✓'; setTimeout(() => $('copy-prompt').textContent = 'Copy handoff', 2000); }
    catch { $('handoff-prompt').select(); toast('Select and copy the highlighted handoff.'); }
  };
  $('source-info').onclick = () => $('info-dialog').showModal();
  $('examples-button').onclick = async () => {
    $('examples-dialog').showModal();
    try {
      await save(); const [result, projects] = await Promise.all([api('/api/examples'), api('/api/projects')]);
      examples = result.examples; recipeMatches = result.matches || []; state.projects = projects; renderExamples();
    }
    catch (e) { $('example-list').textContent = e.message; }
  };
  $('example-search').addEventListener('input',()=>{clearTimeout(exampleSearchTimer);exampleSearchTimer=setTimeout(renderExamples,90);});
  $('example-search').addEventListener('keydown',event=>{
    if(!['Enter','ArrowDown'].includes(event.key))return;
    event.preventDefault();renderExamples();const first=$('example-list').querySelector(exampleGroup==='projects'?'.example-card':'.example-preview');
    if(event.key==='Enter')first?.click();else first?.focus();
  });
  $('open-preview-example').onclick=async()=>{
    if(!examplePreviewPath)return;
    $('open-preview-example').disabled=true;
    try {
      if(await changeProject('/api/example',{path:examplePreviewPath},{message:'Example opened in a new tab. Edit the features, then ask Codex to adapt it.'})){
        $('example-preview-dialog').close();$('examples-dialog').close();
      }
    }catch(error){toast(error.message);}
    finally{$('open-preview-example').disabled=false;}
  };
  $('cancel-delete-project').onclick = () => $('delete-project-dialog').close();
  $('confirm-delete-project').onclick = async () => {
    if (!deletingProject) return;
    $('confirm-delete-project').disabled = true;
    try {
      if (await changeProject('/api/projects/delete', { id: deletingProject, confirm: true }, { message: 'Project deleted.' })) {
        projectViews.delete(deletingProject); deletingProject = null;
        $('delete-project-dialog').close(); renderExamples();
      }
    } finally { $('confirm-delete-project').disabled = false; }
  };
  $('model').addEventListener('input', () => updateEfforts());
  for (const button of document.querySelectorAll('.close-dialog')) button.onclick = () => button.closest('dialog').close();
  for(const dialog of document.querySelectorAll('dialog')) {
    let backdropDown=false;
    const outside=event=>{const rect=dialog.getBoundingClientRect();return event.target===dialog&&(event.clientX<rect.left||event.clientX>rect.right||event.clientY<rect.top||event.clientY>rect.bottom);};
    dialog.addEventListener('pointerdown',event=>{backdropDown=outside(event);});
    dialog.addEventListener('click',event=>{if(backdropDown&&outside(event))dialog.close();backdropDown=false;});
  }
  $('download-file').onclick = () => {
    const f = shownAssembly()?.files[fileIndex];
    if (!f) return toast('Assemble the current design or load a recipe to create files.');
    const a = el('a', { href: '/api/download?lastBuild=true&file=' + encodeURIComponent(f.name), download: f.name });
    document.body.append(a); a.click(); a.remove(); toast('Downloading ' + f.name);
  };
  $('download-svg').onclick = async () => {
    try {
      if (!shownAssembly() && !design.blocks.length) return toast('Add a feature to create a graph.');
      if(view==='flow'&&shownAssembly()) {
        const assembly=shownAssembly(),projectId=state.projects?.activeId,key=flowSourceKey(assembly);
        await loadFlowSources(assembly);
        if(view!=='flow'||state.projects?.activeId!==projectId||!shownAssembly()||flowSourceKey(shownAssembly())!==key)throw new Error('The project or view changed. Export again from the view you want.');
        if(!flowSourceData(assembly))throw new Error('Could not load code references. Try exporting again.');
      }
      download(view==='requirements'?'application-features.svg':'application-flow.svg', diagramSVG(), 'image/svg+xml'); toast(view==='requirements'?'Downloading the feature board.':'Downloading the flow graph.');
    } catch (e) { toast('Could not export the graph: ' + e.message); }
  };
  loadBranches().catch(() => { $('source-branch').replaceChildren(el('option', {}, 'Branch unavailable')); });
  setInterval(poll, 1800);
  document.addEventListener('visibilitychange', () => { if (!document.hidden) poll(); });
}
async function loadModels() {
  try {
    const result = await api('/api/models');
    modelCatalog = result.models;
    $('model-options').replaceChildren(...modelCatalog.map(m => el('option', { value: m.id, label: m.name })));
    $('model-help').textContent = modelCatalog.length + ' models reported by your local Codex. Default: GPT-6.1 Sol · high.';
    updateEfforts();
  } catch (e) {
    $('model-help').textContent = 'Model list unavailable. Enter an exact model ID. Default: gpt-6.1-sol.';
    updateEfforts();
  }
}
function updateEfforts() {
  const previous = $('reasoning-effort').value || 'high';
  const model = modelCatalog.find(m => m.id === $('model').value.trim());
  const choices = model?.efforts?.length ? model.efforts : ['low', 'medium', 'high'];
  $('reasoning-effort').replaceChildren(...choices.map(e => el('option', { value: e }, fieldLabel(e))));
  $('reasoning-effort').value = choices.includes(previous) ? previous : choices.includes('high') ? 'high' : model?.defaultEffort || choices[0];
}
function renderExamples() {
  clearTimeout(exampleSearchTimer);exampleSearchTimer=null;
  if (exampleGroup === 'suggested' && !recipeMatches.length) exampleGroup = 'all';
  const groups = [...new Set(examples.map(e => e.group))].sort();
  const categoryScroll=$('example-filters').scrollLeft;
  $('example-filters').replaceChildren(...[['all', 'Checkout examples ' + examples.length], ['projects', 'My projects ' + (state.projects?.items.filter(project=>!project.draft).length || 0)], ...(recipeMatches.length ? [['suggested', 'For this design ' + recipeMatches.length]] : []), ...groups.map(g => [g, g])].map(([id, label]) => el('button', {
    class: exampleGroup === id ? 'active' : '','aria-pressed':String(exampleGroup===id), onclick: () => { exampleGroup = id; renderExamples(); }
  }, label)));
  $('example-filters').scrollLeft=categoryScroll;
  const query = $('example-search').value.toLowerCase().trim();
  if (exampleGroup === 'projects') {
    const recent=[...(state.projects?.items||[])].filter(project=>!project.draft).sort((a,b)=>Number(b.id===state.projects.activeId)-Number(a.id===state.projects.activeId)||(Date.parse(b.updatedAt)||0)-(Date.parse(a.updatedAt)||0));
    const projects=filterProjectItems(recent.map(project=>({...project,label:project.name})),query);
    $('example-result-count').textContent=projects.length+' '+(projects.length===1?'project':'projects');
    $('example-list').replaceChildren(...projects.map(project => el('div', { class: 'saved-project-card' },
      el('button', { class: 'example-card', onclick: async () => {
        if (await changeProject('/api/projects/open', { id: project.id })) $('examples-dialog').close();
      } }, el('span', { class: 'example-icon' }, '◈'), el('strong', {}, project.name),
      el('small', {}, project.id===state.projects.activeId?'Current project':state.projects.openIds.includes(project.id) ? 'Open in a tab' : 'Saved locally'), el('span', { class: 'example-badge' }, 'YOUR PROJECT')),
      el('button', { class: 'saved-project-delete', 'aria-label': 'Delete ' + project.name, 'data-hint': 'Permanently delete this saved project. You will be asked to confirm.', onclick: () => {
        deletingProject = project.id;
        $('delete-project-message').textContent = '“' + project.name + '” and its saved blocks, flow and generated files will be removed from Studio. Repository examples are kept.';
        $('delete-project-dialog').showModal(); $('cancel-delete-project').focus();
      } }, '×'))));
    if (!projects.length) $('example-list').append(el('p', { class: 'small muted' }, 'No projects match this search.'));
    $('recipe-help').textContent = 'Projects with work save automatically. Empty starter drafts stay out of Recipes. Closing a saved project keeps it here; × permanently deletes it.';
    return;
  }
  $('recipe-help').textContent = 'Examples open in a new project tab. Source reconstruction does not establish firmware compatibility; use Assemble with Codex to review it.';
  const matches = new Map(recipeMatches.map((r, i) => [r.path, { ...r, rank: i }]));
  const filtered=filterExamples(examples.filter(e=>exampleGroup==='all'||exampleGroup==='suggested'&&matches.has(e.path)||e.group===exampleGroup),query);
  if (exampleGroup === 'suggested') filtered.sort((a, b) => matches.get(a.path).rank - matches.get(b.path).rank);
  $('example-result-count').textContent=filtered.length+' '+(filtered.length===1?'example':'examples')+' · Enter previews the first result';
  $('example-list').replaceChildren(...filtered.map(example => el('article',{class:'example-entry'},el('button', { class: 'example-card','aria-label':'Open '+example.title,'data-hint-title':'Open '+example.title,'data-hint':'Open the source reconstruction in a new project tab. Use Preview to inspect its classes and routines first.',onclick: async () => {
    try {
      if (await changeProject('/api/example', { path: example.path }, { message: 'Example opened in a new tab. Edit the features, then ask Codex to adapt it.' })) $('examples-dialog').close();
    } catch (e) { toast(e.message); }
  } }, el('span', { class: 'example-icon','aria-hidden':'true' },ingredientIcon(example.classes?.find(c=>c.category!=='system')?.category||'system')),
    el('div',{class:'example-card-title'},el('strong', {}, example.title), el('small', {}, example.group + ' · ' + example.lines + ' lines')),
    el('span',{class:'example-capabilities'},...(example.capabilities?.length?example.capabilities.slice(0,3):(example.classes||[]).filter(c=>c.category!=='system').slice(0,3).map(c=>c.label)).map(label=>el('span',{},label))),
    el('span', { class: 'example-badge' + (example.customLogic ? ' custom' : '') }, example.customLogic ? 'Review custom logic' : 'Source example'),
    matches.has(example.path) ? el('span', { class: 'recipe-match', 'data-hint-title': 'Relevant to this design', 'data-hint': matches.get(example.path).reasons.join(' · ') }, matches.get(example.path).reasons.slice(0, 2).join(' · ') || 'Matches your behavior prompt') : null,
    ),el('div',{class:'example-entry-footer'},el('small',{},example.classNames.length+' classes · '+(example.routines?.length||0)+' routines'),
      el('button',{class:'quiet compact example-preview','aria-label':'Preview '+example.title,'data-hint':'Inspect referenced classes, routines and custom logic without opening a project.',onclick:()=>previewExample(example.path)},'Preview')))));
  if(!filtered.length)$('example-list').append(el('div',{class:'example-empty'},el('strong',{},'No examples match this search.'),el('p',{},'Try a class, API call or behavior such as logToSD, sleep or WiFi.'),
    el('button',{class:'quiet',onclick:()=>{$('example-search').value='';renderExamples();$('example-search').focus();}},'Clear search')));
}
async function previewExample(file) {
  const request=++examplePreviewRequest;examplePreviewPath=null;
  $('example-preview-title').textContent='Example preview';$('example-preview-content').replaceChildren(el('p',{},'Reading the current source index…'));
  $('open-preview-example').disabled=true;$('example-preview-dialog').showModal();
  try {
    const result=await api('/api/example-preview?'+new URLSearchParams({path:file}));
    if(request!==examplePreviewRequest||!$('example-preview-dialog').open)return;
    const example=result.example;examplePreviewPath=example.path;
    $('example-preview-title').textContent=example.title;
    const content=$('example-preview-content');content.replaceChildren(
      el('p',{class:'small'},example.lines+' lines · '+example.classNames.length+' classes · '+example.routines.length+' routines'+(result.branch?' · '+result.branch:'')),
      el('p',{class:'notice'},'This preview indexes the source. Review conditions, wiring and board compatibility when adapting it.'),
      el('h3',{},'Classes referenced'),el('div',{class:'preview-classes'},...example.classes.map(c=>el('span',{'data-hint-title':c.label,'data-hint':c.className,'data-hint-category':c.category},el('strong',{},c.label),el('code',{},c.className)))),
      el('h3',{},'Routine structure'),el('ul',{class:'preview-routines'},...example.routines.map(r=>el('li',{},el('strong',{},r.name+'()'),el('span',{},({setup:'Startup',loop:'Repeats',callback:'Callback',helper:'Helper'}[r.kind]||'Routine')+' · '+r.steps+' '+(r.steps===1?'step':'steps')),
        ...r.bindings.map(b=>el('small',{},(b.kind==='registration'?'Registered via ':'Called by ')+b.label))))));
    const flags=[example.controlFlow?example.controlFlow+' conditional or loop '+(example.controlFlow===1?'statement':'statements'):null,
      example.helperFunctions.length?'Helper functions: '+example.helperFunctions.join(', '):null,
      example.preprocessorChoices?'Compile-time alternatives':null,
      example.nonLoomIncludes.length?'Other includes: '+example.nonLoomIncludes.join(', '):null].filter(Boolean);
    if(flags.length)content.append(el('h3',{},'Custom logic to review'),el('ul',{},...flags.map(flag=>el('li',{},flag))));
    if(!example.classes.length)content.querySelector('.preview-classes').append(el('p',{class:'small'},'No Loom class references indexed. Review the custom code and includes.'));
    if(!example.routines.length)content.querySelector('.preview-routines').append(el('li',{},'No routine steps indexed. Open the sketch to inspect its structure.'));
    const included=example.projectFiles.filter(file=>file.included),other=example.projectFiles.filter(file=>!file.included);
    content.append(el('h3',{},'Included with the project'),el('ul',{class:'preview-files'},...included.map(file=>el('li',{},el('code',{},file.name),el('small',{},file.kind==='sketch'?'Original example sketch':'Helper source · copied without changes')))));
    if(other.length)content.append(el('h3',{},'Other example files'),el('ul',{class:'preview-files'},...other.map(file=>el('li',{},el('code',{},file.source),el('small',{},file.reason+' · not included')))));
    content.append(sourceButton('Open sketch in VS Code ↗',{source:example.path}));
    $('open-preview-example').disabled=false;
  }catch(error){if(request===examplePreviewRequest&&$('example-preview-dialog').open)$('example-preview-content').replaceChildren(el('p',{class:'notice warning'},error.message));}
}
function setTray(collapsed) {
  document.body.classList.toggle('tray-collapsed', collapsed);
  $('tray-toggle-label').textContent = 'Ingredients';
  $('toggle-tray').setAttribute('aria-expanded', String(!collapsed));
}
init().catch(e => { $('save-status').textContent = 'Could not connect'; toast(e.message); });
