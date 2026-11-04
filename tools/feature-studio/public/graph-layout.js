import { routineConnections } from './execution-wiring.js';
export const NODE_W = 184, NODE_H = 118, GRID_X = 218, GRID_Y = 148;

// Navigation follows routine membership, never an imagined execution through
// branches. Callback registrations stay separate from ordinary helper calls.
export function executionOutline(graph, sources) {
  if(!graph)return [];
  const nodes=new Map(graph.nodes.map(node=>[node.id,node])),order={setup:0,loop:1,callback:2,helper:3};
  return [...(graph.routines||[])].sort((a,b)=>(order[a.kind]??4)-(order[b.kind]??4)).map(routine=>({
    id:routine.id,name:routine.name,kind:routine.kind,reason:routine.reason,
    steps:routine.nodeIds.flatMap((id,index)=>{
      const node=nodes.get(id);if(!node)return [];
      const appearance=friendlyNode(node),source=sources?.nodes?.[id];
      return [{id,label:appearance.label,category:appearance.category,reason:appearance.reason,number:index+1,owners:source?.owners||[],reference:source?.reference||null}];
    }),
    bindings:(routine.bindings||[]).filter(binding=>nodes.has(binding.nodeId)).map(binding=>({
      ...binding,callerRoutine:graph.routines.find(other=>other.nodeIds.includes(binding.nodeId))?.name||null
    }))
  }));
}

export function filterProjectItems(items,query) {
  const normalize=text=>String(text||'').toLowerCase().normalize('NFKC');
  const text=normalize(query).trim(),terms=text.split(/\s+/).filter(Boolean);
  return items.map((item,index)=>{
    const label=normalize(item.label),haystack=normalize([item.label,item.context,item.description,item.searchText].join(' '));
    const score=label===text?0:label.startsWith(text)?1:terms.every(term=>label.includes(term))?2:3;
    return {item,index,score,match:terms.every(term=>haystack.includes(term))};
  }).filter(entry=>entry.match).sort((a,b)=>a.score-b.score||a.index-b.index).map(entry=>entry.item);
}

export function supportingFeatures(design, assembly) {
  if (!assembly) return [];
  const requested = new Set(design.blocks.map(b=>b.id)), operations = new Set((assembly.routines||[]).flatMap(r=>r.nodeIds));
  return assembly.nodes.filter(n=>n.added&&!requested.has(n.id)&&!operations.has(n.id)&&!/^Source (?:call|function call)/.test(n.reason));
}

// The feature board is a list of capabilities, not executable wiring. Export
// its current positions and descriptions without silently substituting the flow.
export function featureBoardPresentation(nodes, positions) {
  const placed=Object.fromEntries(nodes.map((node,index)=>[node.id,positions[node.id]||{x:35+index%3*GRID_X,y:80+Math.floor(index/3)*GRID_Y}]));
  return {graph:{nodes,edges:[]},positions:placed,sections:[],roles:{}};
}

// Collapse strongly connected groups before ranking. Timing loops never force
// dependency levels, and mutually dependent components remain in the same tier.
export function layoutGraph(graph) {
  const nodes = graph.nodes, ids = new Set(nodes.map(n => n.id));
  const edges = graph.edges.filter(e => e.kind !== 'cycle' && ids.has(e.from) && ids.has(e.to));
  const next = new Map(nodes.map(n => [n.id, []]));
  edges.forEach(e => next.get(e.from).push(e.to));
  let index = 0;
  const indices = new Map(), low = new Map(), stack = [], active = new Set(), groups = [];
  function visit(id) {
    indices.set(id, index); low.set(id, index++); stack.push(id); active.add(id);
    for (const to of next.get(id)) {
      if (!indices.has(to)) { visit(to); low.set(id, Math.min(low.get(id), low.get(to))); }
      else if (active.has(to)) low.set(id, Math.min(low.get(id), indices.get(to)));
    }
    if (low.get(id) === indices.get(id)) {
      const group = []; let item;
      do { item = stack.pop(); active.delete(item); group.push(item); } while (item !== id);
      groups.push(group);
    }
  }
  nodes.forEach(n => { if (!indices.has(n.id)) visit(n.id); });
  const groupOf = new Map();
  groups.forEach((g, i) => g.forEach(id => groupOf.set(id, i)));
  const incoming = groups.map(() => 0), ranks = groups.map(() => 0), successors = groups.map(() => new Set());
  for (const e of edges) {
    const a = groupOf.get(e.from), b = groupOf.get(e.to);
    if (a !== b && !successors[a].has(b)) { successors[a].add(b); incoming[b]++; }
  }
  const queue = groups.map((_, i) => i).filter(i => !incoming[i]);
  while (queue.length) {
    const from = queue.shift();
    for (const to of successors[from]) {
      ranks[to] = Math.max(ranks[to], ranks[from] + 1);
      if (--incoming[to] === 0) queue.push(to);
    }
  }
  // A terminal cadence block belongs after the work it repeats. Its return
  // arrow still does not participate in dependency ranking (or create a cycle).
  for (const edge of graph.edges.filter(e => e.kind === 'cycle' && ids.has(e.from) && ids.has(e.to))) {
    const source = groupOf.get(edge.from), target = groupOf.get(edge.to);
    if (source === target || successors[source].size) continue;
    const seen = new Set([source]);
    let last = ranks[target];
    const pending = [target];
    while (pending.length) {
      const group = pending.pop();
      if (seen.has(group)) continue;
      seen.add(group); last = Math.max(last, ranks[group]);
      pending.push(...successors[group]);
    }
    ranks[source] = Math.max(ranks[source], last + 1);
  }
  const columns = new Map(), order = new Map(nodes.map((n, i) => [n.id, i]));
  for (const n of nodes) {
    const col = ranks[groupOf.get(n.id)];
    if (!columns.has(col)) columns.set(col, []);
    columns.get(col).push(n.id);
  }
  const row = new Map();
  for (const column of columns.values()) column.forEach((id, i) => row.set(id, i));
  const colIDs = [...columns.keys()].sort((a, b) => a - b);
  // Barycentric sweeps reduce crossings while retaining a stable source order.
  for (let pass = 0; pass < 4; pass++) {
    const reverse = pass % 2;
    for (const col of reverse ? [...colIDs].reverse() : colIDs) {
      const score = id => {
        const neighbors = edges.filter(e => reverse ? e.from === id : e.to === id);
        let sum = 0, weight = 0;
        for (const e of neighbors) {
          const other = reverse ? e.to : e.from;
          if (groupOf.get(other) === groupOf.get(id)) continue;
          const w = e.kind === 'data' ? 3 : 1;
          sum += row.get(other) * w; weight += w;
        }
        return weight ? sum / weight : row.get(id);
      };
      columns.get(col).sort((a, b) => score(a) - score(b) || order.get(a) - order.get(b));
      columns.get(col).forEach((id, i) => row.set(id, i));
    }
  }
  const positions = Object.create(null);
  for (const [col, column] of columns) column.forEach((id, i) => { positions[id] = { x: 35 + col * 384, y: 90 + i * 224 }; });
  return positions;
}

export function friendlyNode(node) {
  if (node.id === 'example-custom') return { label: 'Source notes', reason: node.reason, category: 'intent' };
  const call = node.reason.match(/^Source call in (\w+):\s*([\w]+)\.(\w+)\(([\s\S]*?)\)\. Conditional/);
  const declaration = /^Source declaration:/.test(node.reason);
  const label = node.label.replace(call || declaration ? / · \w+$/ : /$^/, '');
  const instance = declaration && node.label.match(/ · (\w+)$/)?.[1];
  if (!call) return { label, reason: declaration ? (instance ? 'Instance “' + instance + '”. ' : '') + 'Configured for use by the workflow.' : node.reason, category: node.category };
  const [, , owner, method, args] = call;
  const descriptions = {
    beginSerial: ['Start the Serial connection.', 'outputs'], initialize: ['Initialize the configured components.', 'system'],
    enable: ['Enable the configured hardware.', node.category], registerInterrupt: ['Register ' + args + ' as the interrupt callback.', 'power'],
    wakeup: ['Handle the hardware wake-up event.', 'power'], reattachRTCInterrupt: ['Re-enable the RTC interrupt for the next wake-up.', 'power'],
    measure: ['Read the configured sensors.', 'sensors'],
    package: ['Prepare a packet from the latest measurements.', 'system'],
    display_data: ['Print the current packet to Serial.', 'outputs'],
    logToSD: ['Save the current packet to the SD card.', 'storage'],
    publish: ['Publish the current packet using ' + owner + '.', 'outputs'],
    send: ['Transmit the current packet using ' + owner + '.', 'radio'],
    sendBatch: ['Transmit the saved batch using ' + owner + '.', 'radio'],
    receive: ['Receive an incoming packet.', 'radio'],
    receiveBatch: ['Receive a batch of packets.', 'radio'],
    sleep: ['Sleep until the configured wake condition.', 'power'],
    pause: [/^\d+$/.test(args) ? 'Wait ' + (Number(args) >= 1000 ? Number(args) / 1000 + ' s' : args + ' ms') + ' before continuing.' : 'Wait for the configured interval.', 'power']
  };
  const [reason, category] = descriptions[method] || ['Run ' + owner + '.' + method + '(). Select for source arguments and conditions.', node.category];
  return { label, reason, category };
}

// Presentation only: the workflow shows execution. Component declarations stay
// in the feature board, original graph and inspector (or explicit detail view).
export function workflowPresentation(graph, details = false, collapsed = new Set()) {
  if (details) return { graph, positions: layoutGraph(graph), sections: [], roles: {} };
  if (graph.routines?.length) return executionPresentation(graph, collapsed);
  const flowing = new Set(graph.edges.filter(e => e.kind !== 'dependency').flatMap(e => [e.from, e.to]));
  const groups = { workflow: [], setup: [], helpers: [], components: [] }, roles = {};
  for (const node of graph.nodes) {
    const scope = node.reason.match(/^Source call in (\w+):/)?.[1];
    const group = /^Source declaration:/.test(node.reason) ? 'components' : scope === 'setup' ? 'setup' : node.id === 'example-custom' || scope && scope !== 'loop' ? 'helpers' :
      scope || flowing.has(node.id) || (!/^Source declaration:/.test(node.reason) && ['sensors', 'storage', 'outputs', 'radio', 'power', 'intent', 'custom'].includes(node.category)) ? 'workflow' : 'components';
    groups[group].push(node); roles[node.id] = group;
  }
  const labels = label => label.replace(/^measure → package in \w+$/, 'Measurements').replace(/^packet route · \w+$/, 'Packet')
    .replace(/^explicit proxy route · \w+$/, 'Received packet').replace(/^explicit batch argument$/, 'Saved batch')
    .replace(/^loop cadence; review conditions$/, 'Next cycle · review conditions');
  const edges = graph.edges.flatMap((edge, sourceIndex) => edge.kind === 'dependency'||roles[edge.from]==='components'||roles[edge.to]==='components' ? [] : [{ ...edge, sourceIndex, detailLabel: edge.label, label: labels(edge.label) }]);
  const positions = Object.create(null), sections = [];
  let top = 90;
  const names = { workflow: ['Workflow', 'Data and repeat paths · select a step for details'], setup: ['Run once', 'Calls in setup()'], helpers: ['Other routines', 'Review invocation and conditions in the sketch'], components: ['Components', 'Setup and ownership connections are available in All connections'] };
  for (const [group, nodes] of Object.entries(groups)) {
    if (!nodes.length || group==='components') continue;
    const ids = new Set(nodes.map(n => n.id)), localEdges = edges.filter(e => ids.has(e.from) && ids.has(e.to));
    const local = group === 'components' ? Object.fromEntries(nodes.map((n, i) => [n.id, { x: 35 + i % 4 * 256, y: 90 + Math.floor(i / 4) * 176 }])) : layoutGraph({ nodes, edges: localEdges });
    for (const node of nodes) positions[node.id] = { x: 35 + (local[node.id].x - 35) * 352 / 384, y: top + local[node.id].y - 90 };
    const bottom = Math.max(...nodes.map(n => positions[n.id].y + NODE_H));
    localEdges.filter(e => e.kind === 'cycle').forEach((edge, i) => { edge.railY = bottom + 48 + i * 22; });
    sections.push({ id: group, title: names[group][0], description: names[group][1], x: 35, y: top - 34 });
    top = bottom + (localEdges.some(e => e.kind === 'cycle') ? 148 + localEdges.filter(e => e.kind === 'cycle').length * 22 : 100);
  }
  return { graph: { ...graph, nodes:graph.nodes.filter(n=>roles[n.id]!=='components'),edges }, positions, sections, roles };
}

function executionPresentation(graph, collapsed) {
  const positions = Object.create(null), roles = {}, steps = {}, sections = [], assigned = new Set(), hidden = new Set();
  const byId = new Map(graph.nodes.map(n => [n.id, n]));
  const order = { setup: 0, loop: 1, callback: 2, helper: 3 };
  const routines = [...graph.routines].sort((a, b) => order[a.kind] - order[b.kind]);
  let top = 66;
  const groups = routines.map(r => ({ ...r, nodes: r.nodeIds.map(id => byId.get(id)).filter(Boolean) }));
  const members = new Set(routines.flatMap(r => r.nodeIds));
  const remaining = graph.nodes.filter(n => !members.has(n.id));
  for(const node of remaining){hidden.add(node.id);roles[node.id]='components';}
  for (const r of groups) {
    const isClosed = collapsed.has(r.id), nodes = r.nodes.filter(n => !assigned.has(n.id));
    nodes.forEach((n, i) => { assigned.add(n.id); roles[n.id] = r.kind === 'components' ? 'components' : r.kind === 'loop' ? 'workflow' : r.kind; steps[n.id] = { routine: r, number: i + 1 }; });
    let width = 650;
    const headerHeight = r.bindings.length ? 106 : 76;
    const y = top + headerHeight;
    let bodyHeight=30;
    if (isClosed) nodes.forEach(n => hidden.add(n.id));
    else if(nodes.length) {
      const ids=new Set(nodes.map(n=>n.id)),sourceOrder=new Map(nodes.map((n,i)=>[n.id,i]));
      // Back jumps remain visible as return wires. Including them in ranking
      // collapses an entire while-loop into one very tall column.
      const local=layoutGraph({nodes,edges:routineConnections(graph).filter(e=>e.kind==='flow'&&ids.has(e.from)&&ids.has(e.to)&&sourceOrder.get(e.from)<sourceOrder.get(e.to))});
      const xs=[...new Set(nodes.map(n=>local[n.id].x))].sort((a,b)=>a-b);
      const columns=xs.map(x=>nodes.filter(n=>local[n.id].x===x).sort((a,b)=>local[a.id].y-local[b.id].y));
      // Follow execution from left to right. Only real branches need another
      // row; wrapping every four columns creates needless vertical detours.
      // The 272px gap reserves room for complete multi-line wire labels.
      columns.forEach((column,i)=>column.forEach((n,row)=>{positions[n.id]={x:55+i*456,y:y+row*240};}));
    }
    if(!isClosed&&nodes.length) {
      bodyHeight=Math.max(...nodes.map(n=>positions[n.id].y+NODE_H))-y;
      width=Math.max(width,...nodes.map(n=>positions[n.id].x+NODE_W));
    }
    const localIds = new Set(nodes.map(n => n.id));
    const cycles = graph.edges.filter(e => e.kind === 'cycle' && localIds.has(e.from) && localIds.has(e.to));
    const height = isClosed ? 58 : headerHeight + bodyHeight + 36 + (cycles.length ? 62 : 0);
    sections.push({ id: r.id, title: r.kind === 'components' ? r.name : r.name + '()',
      description: ({ setup: 'Run once at startup', loop: 'Repeats while the device runs', callback: 'Runs when invoked, not during registration', helper: 'Runs only when called', components: r.reason })[r.kind],
      reason: r.reason, kind: r.kind, bindings: r.bindings, count: nodes.length, collapsed: isClosed, x: 29, y: top, w: width, h: height });
    top += height + 34;
  }
  const labels = label => label.replace(/^measure → package in \w+$/, 'Measurements').replace(/^packet route · \w+$/, 'Packet').replace(/^loop cadence; review conditions$/, 'Next cycle · review conditions');
  let edges = routineConnections(graph).flatMap(e => e.kind === 'dependency' || e.kind==='data'&&(!members.has(e.from)||!members.has(e.to)) || hidden.has(e.from) || hidden.has(e.to) ? [] : [{ ...e,
    ...(graph.edges.includes(e) ? { sourceIndex: graph.edges.indexOf(e) } : {}), detailLabel: e.label, label: labels(e.label) }]);
  // One route can carry both the step transition and its data. Keep the original
  // edges in the inspector, but avoid drawing parallel duplicates in the overview.
  const carriesOrder = new Set(edges.filter(e=>e.kind==='data'&&edges.some(f=>f.kind==='flow'&&f.from===e.from&&f.to===e.to)).map(e=>e.from+'\0'+e.to));
  edges=edges.filter(e=>e.kind!=='flow'||!carriesOrder.has(e.from+'\0'+e.to)).map(e=>carriesOrder.has(e.from+'\0'+e.to)&&e.kind==='data'?{...e,carriesOrder:true}:e);
  for (const section of sections.filter(s => !s.collapsed)) {
    const ids = new Set(groups.find(g => g.id === section.id).nodeIds || groups.find(g => g.id === section.id).nodes.map(n => n.id));
    edges.filter(e => e.kind === 'cycle' && ids.has(e.from) && ids.has(e.to)).forEach(e => { e.railY = section.y + section.h - 24; });
  }
  return { graph: { ...graph, nodes: graph.nodes.filter(n => !hidden.has(n.id)), edges }, positions, sections, roles, steps };
}

function segmentHits(a, b, r) {
  const pad = 9;
  if (a.x === b.x) return a.x > r.x - pad && a.x < r.x + NODE_W + pad && Math.max(a.y, b.y) > r.y - pad && Math.min(a.y, b.y) < r.y + NODE_H + pad;
  return a.y > r.y - pad && a.y < r.y + NODE_H + pad && Math.max(a.x, b.x) > r.x - pad && Math.min(a.x, b.x) < r.x + NODE_W + pad;
}
function roundedPath(points, radius = 12) {
  let d = 'M' + points[0].x + ',' + points[0].y;
  for (let i = 1; i < points.length - 1; i++) {
    const a = points[i - 1], b = points[i], c = points[i + 1];
    const before = Math.min(radius, Math.hypot(b.x - a.x, b.y - a.y) / 2);
    const after = Math.min(radius, Math.hypot(c.x - b.x, c.y - b.y) / 2);
    const lenA = Math.hypot(b.x - a.x, b.y - a.y) || 1, lenB = Math.hypot(c.x - b.x, c.y - b.y) || 1;
    const p = { x: b.x - (b.x - a.x) / lenA * before, y: b.y - (b.y - a.y) / lenA * before };
    const q = { x: b.x + (c.x - b.x) / lenB * after, y: b.y + (c.y - b.y) / lenB * after };
    d += ' L' + p.x + ',' + p.y + ' Q' + b.x + ',' + b.y + ' ' + q.x + ',' + q.y;
  }
  const last = points.at(-1);
  return d + ' L' + last.x + ',' + last.y;
}

export function routeEdge(edge, positions, index = 0) {
  const a = positions[edge.from], b = positions[edge.to];
  if (!a || !b) return null;
  if (edge.kind === 'flow' && b.x <= a.x && b.y > a.y && b.y - a.y <= 450) {
    const start = { x: a.x + NODE_W / 2, y: a.y + NODE_H }, end = { x: b.x + NODE_W / 2, y: b.y };
    const rail = start.y + (end.y - start.y) / 2;
    const points = [start, { x:start.x,y:rail }, { x:end.x,y:rail }, end];
    return { d:roundedPath(points), start,end,points,labelX:(start.x+end.x)/2,labelY:rail-8 };
  }
  if (['flow','data'].includes(edge.kind) && b.y === a.y && a.x > b.x && a.x - b.x < 400) {
    const start = { x:a.x,y:a.y+NODE_H/2-(edge.parallel?12:0) },end={x:b.x+NODE_W,y:b.y+NODE_H/2-(edge.parallel?12:0)};
    return { d:'M'+start.x+','+start.y+' L'+end.x+','+end.y,start,end,labelX:(start.x+end.x)/2,labelY:start.y-8,direct:true };
  }
  if (edge.kind === 'callback') {
    const start={x:a.x+NODE_W/2,y:a.y},end={x:b.x+NODE_W/2,y:b.y};
    const railX=Math.max(...Object.values(positions).map(p=>p.x+NODE_W))+70;
    const points=[start,{x:start.x,y:a.y-24},{x:railX,y:a.y-24},{x:railX,y:b.y-32},{x:end.x,y:b.y-32},end];
    return {d:roundedPath(points),start,end,points,labelX:railX,labelY:(a.y+b.y)/2};
  }
  const start = { x: a.x + NODE_W, y: a.y + NODE_H / 2 };
  const end = { x: b.x, y: b.y + NODE_H / 2 };
  if (edge.kind === 'flow' && edge.parallel && a.y === b.y) { start.y -= 12; end.y -= 12; }
  const obstacles = Object.entries(positions).filter(([id]) => id !== edge.from && id !== edge.to).map(([, p]) => p);
  if (edge.kind !== 'cycle' && end.x - start.x >= 35) {
    const delta = Math.max(20, (end.x - start.x) * .48);
    const p1 = { x: start.x + delta, y: start.y }, p2 = { x: end.x - delta, y: end.y };
    let clear = true;
    for (let i = 1; i < 32 && clear; i++) {
      const t = i / 32, u = 1 - t;
      const x = u ** 3 * start.x + 3 * u ** 2 * t * p1.x + 3 * u * t ** 2 * p2.x + t ** 3 * end.x;
      const y = u ** 3 * start.y + 3 * u ** 2 * t * p1.y + 3 * u * t ** 2 * p2.y + t ** 3 * end.y;
      if (obstacles.some(r => x > r.x - 12 && x < r.x + NODE_W + 12 && y > r.y - 12 && y < r.y + NODE_H + 12)) clear = false;
    }
    if (clear) return {
      d: 'M' + start.x + ',' + start.y + ' C' + p1.x + ',' + p1.y + ' ' + p2.x + ',' + p2.y + ' ' + end.x + ',' + end.y,
      start, end, labelX: (start.x + end.x) / 2, labelY: (start.y + end.y) / 2 - 8, direct: true
    };
  }
  const all = Object.values(positions), lane = index % 8 * 10;
  const bottom = Math.max(...all.map(p => p.y + NODE_H)) + 25 + lane;
  const rails = edge.kind === 'cycle' ? [...new Set([edge.railY ?? bottom, bottom])] : [...new Set(all.flatMap(p => [p.y - 22 - lane, p.y + NODE_H + 22 + lane]))].filter(y => y >= 18);
  const candidates = rails.map(y => {
    const points = [start, { x: start.x + 20, y: start.y }, { x: start.x + 20, y }, { x: end.x - 20, y }, { x: end.x - 20, y: end.y }, end];
    const hits = points.slice(1).reduce((count, p, i) => count + obstacles.filter(r => segmentHits(points[i], p, r)).length, 0);
    return { points, y, hits, length: Math.abs(start.y - y) + Math.abs(end.y - y) };
  }).sort((x, y) => x.hits - y.hits || x.length - y.length);
  const chosen = candidates[0];
  return {
    d: roundedPath(chosen.points), start, end, points: chosen.points,
    labelX: (start.x + end.x) / 2, labelY: chosen.y - 7, direct: false
  };
}

// Name tags get their own clear space, and retain the complete connection name.
// This placement is shared by the live canvas and the standalone SVG export.
export function graphRoutes(graph, positions, sections = []) {
  const occupied = [], cards = Object.values(positions).map(p => ({ x: p.x - 14, y: p.y - 18, w: NODE_W + 28, h: NODE_H + 40 }));
  const headers=sections.filter(s=>s.w).map(s=>({x:s.x,y:s.y,w:s.w,h:s.collapsed?s.h:s.bindings?.length?94:64}));
  const obstacles=[...cards,...headers];
  const overlap = (a, b) => a.x < b.x + b.w + 5 && a.x + a.w + 5 > b.x && a.y < b.y + b.h + 4 && a.y + a.h + 4 > b.y;
  return graph.edges.map((edge, i) => {
    const route = routeEdge({ ...edge, ...(edge.carriesOrder?{kind:'flow'}:{}), parallel: edge.kind === 'flow' && graph.edges.some(other => other.kind === 'data' && other.from === edge.from && other.to === edge.to) }, positions, i);
    if (!route) return null;
    if (!edge.label || edge.kind === 'flow' && /^(?:Source order|Next$|Next setup operation$|Begin active cycle$)/.test(edge.label)) return { ...route, edge };
    const lines = [];
    let line = '';
    for (const word of edge.label.split(/\s+/)) {
      const parts = word.match(/.{1,32}/g) || [''];
      for (const part of parts) {
        if (line && line.length + part.length + 1 > 32) { lines.push(line); line = ''; }
        line += (line ? ' ' : '') + part;
      }
    }
    if (line) lines.push(line);
    const sourceLineStart=lines.length;
    if(edge.sourceCaption)lines.push(...(edge.sourceCaption.match(/.{1,32}/g)||[]));
    const w = Math.max(55, Math.max(...lines.map(s => s.length)) * 6.2 + 18), h = lines.length * 12 + 8;
    let chosen;
    for (const dy of [0, -20, 20, -40, 40, -60, 60, -80, 80]) {
      for (const dx of [0, -40, 40, -80, 80]) {
        const box = { x: route.labelX - w / 2 + dx, y: route.labelY - 11 + dy, w, h };
        if (box.x < 6 || box.y < 6 || [...obstacles, ...occupied].some(r => overlap(box, r))) continue;
        chosen = box; break;
      }
      if (chosen) break;
    }
    if(!chosen) {
      // A crowded junction must never fall back onto a card. Search nearby
      // clear rows first, then use an outside label lane with a leader line.
      const near=obstacles.flatMap(b=>[b.y-h-12,b.y+b.h+12]);
      near.sort((a,b)=>Math.abs(a-route.labelY)-Math.abs(b-route.labelY));
      for(const y of near) {
        const box={x:Math.max(6,route.labelX-w/2),y,w,h};
        if(y>=6&&![...obstacles,...occupied].some(r=>overlap(box,r))) {chosen=box;break;}
      }
    }
    if(!chosen) {
      const x=Math.max(6,...obstacles.map(b=>b.x+b.w))+30;
      let y=Math.max(6,route.labelY-11),box;
      while(true) {
        box={x,y,w,h};const hits=occupied.filter(r=>overlap(box,r));
        if(!hits.length)break;
        y=Math.max(...hits.map(r=>r.y+r.h))+12;
      }
      chosen=box;
    }
    occupied.push(chosen);
    const anchor = { x: route.labelX, y: route.labelY + 8 };
    const closest = { x: Math.max(chosen.x + 3, Math.min(chosen.x + chosen.w - 3, anchor.x)), y: Math.max(chosen.y + 3, Math.min(chosen.y + chosen.h - 3, anchor.y)) };
    const leader = Math.hypot(anchor.x - closest.x, anchor.y - closest.y) > 7 ? { from: anchor, to: closest } : null;
    return { ...route, edge, leader, label: { ...chosen, lines, sourceLineStart } };
  }).filter(Boolean);
}

// Include outside return rails and complete labels, not only the card boxes.
export function graphBounds(positions, routes = [], sections = []) {
  const boxes = Object.values(positions).map(p => ({ x: p.x - 12, y: p.y - 18, w: NODE_W + 24, h: NODE_H + 38 }));
  for (const section of sections) if (section.w) boxes.push({ x: section.x, y: section.y, w: section.w, h: section.h });
  for (const route of routes) {
    if (route.label) boxes.push(route.label);
    for (const point of route.points || [route.start, route.end]) boxes.push({ ...point, w: 1, h: 1 });
  }
  if (!boxes.length) return { x: 0, y: 0, w: 300, h: 220 };
  const x = Math.min(...boxes.map(b => b.x)) - 16, y = Math.min(...boxes.map(b => b.y)) - 16;
  return { x, y, w: Math.max(...boxes.map(b => b.x + b.w)) + 16 - x, h: Math.max(...boxes.map(b => b.y + b.h)) + 16 - y };
}

export function canvasFrame(bounds, zoom, viewportWidth, viewportHeight) {
  const marginX = Math.max(180, viewportWidth), marginY = Math.max(280, viewportHeight);
  return { x: marginX - bounds.x * zoom, y: marginY - bounds.y * zoom,
    width: bounds.w * zoom + marginX * 2, height: bounds.h * zoom + marginY * 2 };
}

// Find clear screen space for a block without moving its world position.
export function visibleBlockSpot(viewport, obstacles, size, preferred, padding = 14) {
  const left = viewport.x + padding, top = viewport.y + padding;
  const right = viewport.x + viewport.w - padding, bottom = viewport.y + viewport.h - padding;
  if (size.w > right - left || size.h > bottom - top) return null;
  const overlap = (a,b) => a.x < b.x+b.w+padding && a.x+a.w+padding > b.x && a.y < b.y+b.h+padding && a.y+a.h+padding > b.y;
  const blocks = obstacles.filter(b => b.w > 0 && b.h > 0 && b.x < right && b.x+b.w > left && b.y < bottom && b.y+b.h > top);
  const target = preferred || {x:(left+right-size.w)/2,y:(top+bottom-size.h)/2};
  const xs = [target.x,left,right-size.w,(left+right-size.w)/2,...blocks.flatMap(b=>[b.x+b.w+padding,b.x-size.w-padding])];
  const ys = [target.y,top,bottom-size.h,(top+bottom-size.h)/2,...blocks.flatMap(b=>[b.y+b.h+padding,b.y-size.h-padding])];
  const candidates=[];
  for(const x of xs) for(const y of ys) {
    const box={x,y,...size};
    if(x<left || y<top || x+size.w>right || y+size.h>bottom || blocks.some(b=>overlap(box,b))) continue;
    candidates.push(box);
  }
  candidates.sort((a,b)=>Math.hypot(a.x-target.x,a.y-target.y)-Math.hypot(b.x-target.x,b.y-target.y));
  return candidates[0] || null;
}
