import { stripComments, maskStrings } from './adapters/cpp.mjs';
import { splitParameters } from './adapters/loom.mjs';
import { revisionOf } from './core.mjs';
import { routineConnections } from './public/execution-wiring.js';

const operations = {
  beginSerial: ['Start serial', 'setup'], initialize: ['Initialize components', 'setup'], enable: ['Enable hardware', 'setup'],
  registerInterrupt: ['Register interrupt callback', 'setup'], wakeup: ['Handle wake-up', 'setup'], reattachRTCInterrupt: ['Reattach RTC interrupt', 'setup'],
  measure: ['Read sensors', 'data'], package: ['Package measurements', 'data'],
  display_data: ['Show packet on Serial', 'data'], logToSD: ['Save packet to SD', 'data'],
  publish: ['Publish packet', 'data'], send: ['Transmit packet', 'data'],
  sendBatch: ['Transmit batch', 'data'], receive: ['Receive packet', 'data'],
  receiveBatch: ['Receive batch', 'data'], sleep: ['Sleep / wake cycle', 'cycle'],
  setSampleInterval: ['Sample-start cadence', 'cycle'], setInterruptDuration: ['Schedule RTC wake', 'cycle'],
  pause: ['Wait between work', 'cycle']
};
function callEnd(code, open) {
  let depth = 1, quote = '', end = open + 1;
  for (; end < code.length && depth; end++) {
    const c = code[end];
    if (quote) { if (c === '\\') end++; else if (c === quote) quote = ''; }
    else if (c === '"' || c === "'") quote = c;
    else if (c === '(') depth++;
    else if (c === ')') depth--;
  }
  return end;
}
function callArguments(code, open) { return code.slice(open + 1, callEnd(code, open) - 1).replace(/\s+/g, ' ').trim(); }
function scopesIn(code) {
  return [...code.matchAll(/(?:^|\n)\s*(?:static\s+)?(?:inline\s+)?(?:void|bool|float|double|int|uint\w*|String|char\s*\*)\s+(\w+)\s*\([^;{}]*\)\s*\{/g)].map(m => {
    let depth = 1, end = m.index + m[0].length, quote = '';
    for (; end < code.length && depth; end++) {
      const c = code[end];
      if (quote) { if (c === '\\') end++; else if (c === quote) quote = ''; continue; }
      if (c === '"' || c === "'") quote = c;
      else if (c === '{') depth++;
      else if (c === '}') depth--;
    }
    return { name: m[1], start: m.index, end, key: m[1] + '@' + m.index };
  });
}
function functionsIn(code) {
  return scopesIn(code).map(s => s.name);
}
export function summarizeExample(source, file, blocks) {
  const code = stripComments(source);
  const symbols = maskStrings(code.replace(/^\s*#\s*include[^\n]*/gm, ''));
  const classes = blocks.filter(b => b.className && new RegExp('\\b' + b.className + '\\b').test(symbols));
  const helpers = functionsIn(maskStrings(code)).filter(n => !['setup', 'loop'].includes(n));
  const controlFlow = [...maskStrings(code).matchAll(/\b(if|for|while|switch)\s*\(/g)].length;
  const nonLoomIncludes = [...code.matchAll(/^\s*#include\s*[<"]([^>"]+)[>"]/gm)].map(m => m[1]).filter(n => !/Loom|Logger|arduino_secrets/i.test(n));
  return {
    path: file, title: file.split('/').at(-1).replace(/\.ino$/, '').replace(/_/g, ' '),
    group: file.split('/')[1] || 'Examples',
    classNames: classes.map(c => c.className), helperFunctions: [...new Set(helpers)],
    controlFlow, nonLoomIncludes, preprocessorChoices:/^\s*#\s*(?:if|ifdef|ifndef)\b/m.test(code), customLogic: helpers.length > 0 || nonLoomIncludes.length > 0 || controlFlow > 0 || /#if|#ifdef|#ifndef/.test(code) || !classes.length,
    lines: source.split('\n').length
  };
}

export function reconstructExample(source, file, catalog) {
  const code = stripComments(source), summary = summarizeExample(source, file, catalog.blocks);
  const symbols = maskStrings(code);
  const scopes = scopesIn(symbols);
  const declarations = [];
  for (const c of catalog.blocks.filter(c => c.className)) {
    const re = new RegExp('(?:^|[;\\n])\\s*(?:static\\s+)?' + c.className + '\\s+([A-Za-z_]\\w*)\\s*\\(([^;]*?)\\)\\s*;', 'g');
    for (const m of symbols.matchAll(re)) {
      const offset = m.index + m[0].indexOf(c.className);
      const expression = callArguments(code, code.indexOf('(', offset + c.className.length));
      declarations.push({ c, variable: m[1], args: splitParameters(expression), expression, offset });
    }
  }
  declarations.sort((a, b) => a.offset - b.offset);
  const unique = new Map();
  for (const d of declarations) if (!unique.has(d.variable)) unique.set(d.variable, d);
  const objects = [...unique.values()];
  const calls = [];
  for (const d of objects) {
    const re = new RegExp('\\b' + d.variable + '\\s*(?:\\.|->)\\s*(\\w+)\\s*\\(', 'g');
    for (const m of symbols.matchAll(re)) calls.push({
      variable: d.variable, method: m[1], args: callArguments(code, m.index + m[0].length - 1), offset: m.index, end: callEnd(code, m.index + m[0].length - 1), owner: d,
      scope: scopes.find(s => s.start < m.index && s.end > m.index) || { name: 'global', key: 'global' }
    });
  }
  calls.sort((a, b) => a.offset - b.offset);
  const blocks = objects.map((d, i) => {
    const ctor = d.c.constructors.findIndex(c => c.parameters.length >= d.args.length && c.parameters.filter(p => p.required).length <= d.args.length);
    const chosen = d.c.constructors[Math.max(0, ctor)];
    const parameters = {};
    chosen?.parameters.forEach((p, j) => { if (!p.dependency && d.args[j] !== undefined) parameters[p.name] = d.args[j]; });
    const objectCalls = calls.filter(c => c.variable === d.variable && !operations[c.method] && !['initialize', 'beginSerial'].includes(c.method));
    return {
      id: 'example-' + (i + 1), catalogId: d.c.id, label: d.c.title + ' · ' + d.variable, count: 1,
      settings: { constructorIndex: Math.max(0, ctor), parameters, exampleConstructor: d.expression },
      notes: objectCalls.length ? 'Behavior in source: ' + [...new Set(objectCalls.map(c => c.method + '(' + c.args + ')'))].slice(0, 10).join('; ') : '',
      position: { x: 35 + (i % 3) * 218, y: 80 + Math.floor(i / 3) * 148 }
    };
  });
  const declarationMap = new Map(objects.map((d, i) => [d.variable, { ...d, block: blocks[i] }]));
  const nodes = blocks.map((b, i) => ({
    id: b.id, label: b.label, category: objects[i].c.category, added: false,
    reason: 'Source declaration: ' + objects[i].c.className + ' ' + objects[i].variable + '(' + objects[i].expression + '). ' + b.notes,
    requestIds: [b.id]
  }));
  const nodeOffsets = Object.fromEntries(objects.map(d => [declarationMap.get(d.variable).block.id, d.offset]));
  const location = offset => ({ file, line: source.slice(0, offset || 0).split('\n').length });
  const edgeLocations = [], edges = [], seen = new Set();
  function edge(from, to, kind, label, offset) {
    const key = [from, to, kind, label].join('|');
    if (from !== to && !seen.has(key)) {
      seen.add(key); edges.push({ from, to, kind, label });
      edgeLocations.push(location(offset ?? nodeOffsets[to] ?? nodeOffsets[from]));
    }
  }
  for (const d of objects) {
    const current = declarationMap.get(d.variable);
    for (const arg of d.args) {
      const dependency = declarationMap.get(arg.replace(/^[&*]\s*/, '').trim());
      if (dependency) edge(dependency.block.id, current.block.id, 'dependency', 'constructor dependency');
    }
  }
  for (const call of calls.filter(c => /^(set|attach|use)/.test(c.method))) {
    for (const arg of splitParameters(call.args)) {
      const dependency = declarationMap.get(arg.replace(/^[&*]\s*/, '').trim());
      if (dependency) edge(dependency.block.id, declarationMap.get(call.variable).block.id, 'dependency', call.method + ' reference', call.offset);
    }
  }
  // Setup, loop and helpers remain distinct. A setup-time package must never
  // masquerade as the producer of a packet in loop or in a different helper.
  const opMap = new Map();
  for (const call of calls.filter(c => c.scope.name !== 'global')) {
    const key = call.variable + '.' + call.method + '@' + call.offset;
    const owner = declarationMap.get(call.variable);
    const op = { id: 'operation-' + (opMap.size + 1), label: (operations[call.method]?.[0] || call.variable + '.' + call.method + '()') + (call.scope.name === 'loop' ? '' : ' · ' + call.scope.name), category: operations[call.method]?.[1] === 'cycle' ? 'power' : owner.c.category,
      added: false, reason: 'Source call in ' + call.scope.name + ': ' + call.variable + '.' + call.method + '(' + call.args + '). Conditional execution and timing require review of the original code.',
      requestIds: [owner.block.id], call };
    nodes.push(op); opMap.set(key, op);
    nodeOffsets[op.id] = call.offset;
    edge(owner.block.id, op.id, 'dependency', 'called on ' + call.variable);
  }
  const opList = [...opMap.values()];
  const measures = opList.filter(n => n.call.method === 'measure'), packets = opList.filter(n => n.call.method === 'package');
  for (const packet of packets) {
    const measure = measures.filter(n => n.call.variable === packet.call.variable && n.call.scope.key === packet.call.scope.key && n.call.offset < packet.call.offset).at(-1);
    if (measure) edge(measure.id, packet.id, 'data', 'measure → package in ' + packet.call.scope.name);
  }
  function managerOf(variable, visited = new Set()) {
    if (visited.has(variable)) return null;
    visited.add(variable);
    const obj = declarationMap.get(variable);
    if (!obj) return null;
    if (obj.c.className === 'Manager') return variable;
    for (const arg of obj.args) {
      const result = managerOf(arg.replace(/^[&*]\s*/, '').trim(), visited);
      if (result) return result;
    }
    return null;
  }
  const receives = opList.filter(n => /^receive/.test(n.call.method));
  const outputs = opList.filter(n => ['logToSD', 'publish', 'send', 'sendBatch', 'display_data'].includes(n.call.method));
  for (const out of outputs) {
    const args = splitParameters(out.call.args);
    const batch = args.map(a => declarationMap.get(a.trim())).find(d => d?.c.className === 'Loom_BatchSD');
    if (batch) { edge(batch.block.id, out.id, 'data', 'explicit batch argument'); continue; }
    const sameContext = n => n.call.scope.key === out.call.scope.key && n.call.offset < out.call.offset && managerOf(n.call.variable) === managerOf(out.call.variable) && managerOf(out.call.variable);
    const packet = packets.filter(sameContext).at(-1);
    const receive = receives.filter(n => sameContext(n) && n.call.method === 'receive' && splitParameters(n.call.args).at(-1) === 'true').at(-1);
    const producer = receive && (!packet || receive.call.offset > packet.call.offset) ? receive : packet;
    if (producer) edge(producer.id, out.id, 'data', receive === producer ? 'explicit proxy route · ' + out.call.scope.name : 'packet route · ' + out.call.scope.name);
  }
  for (const cycle of opList.filter(n => ['sleep', 'pause'].includes(n.call.method) && n.call.scope.name === 'loop')) {
    const target = opList.find(n => n.call.scope.key === cycle.call.scope.key && n.call.offset < cycle.call.offset);
    if (target) edge(cycle.id, target.id, 'cycle', 'loop cadence; review conditions', cycle.call.offset);
  }
  const extras = [];
  if (summary.helperFunctions.length) extras.push('Helper functions: ' + summary.helperFunctions.join(', '));
  if (summary.nonLoomIncludes.length) extras.push('Other includes: ' + summary.nonLoomIncludes.join(', '));
  if (summary.controlFlow) extras.push(summary.controlFlow + ' conditional/loop/switch statements; preserve their semantics.');
  if (/#if|#ifdef|#ifndef/.test(code)) extras.push('Preprocessor choices appear in this sketch. Constructor declarations may belong to alternative configurations.');
  if (receives.length && packets.length) extras.push('Both local packaging and radio receiving occur: review payload ownership and forwarding order.');
  if (!objects.length) extras.push('No direct Loom object declarations were extracted. This example includes non-Loom or indirect/custom implementation.');
  if (extras.length || summary.customLogic) {
    const b = { id: 'example-custom', catalogId: 'intent:custom', label: 'Custom behavior · preserve source logic', count: 1,
      settings: { requirement: 'Preserve the custom behavior in ' + file + '. ' + extras.join(' ') },
      notes: 'Review the original source before changing this behavior. Settings and callbacks may not be exposed by Loom classes.',
      position: { x: 35, y: 80 + Math.ceil(blocks.length / 3) * 148 } };
    blocks.push(b);
    nodeOffsets[b.id] = scopes.find(s => !['setup', 'loop'].includes(s.name))?.start || 0;
    nodes.push({ id: b.id, label: b.label, category: 'intent', added: false, reason: extras.join(' ') || 'Custom application logic is present in the source.', requestIds: [b.id] });
    for (const call of calls) {
      const callbackReference = summary.helperFunctions.some(name => new RegExp('\\b' + name + '\\b').test(call.args));
      const customHelperUse = !['setup', 'loop', 'global'].includes(call.scope.name);
      if (callbackReference || customHelperUse) edge(b.id, declarationMap.get(call.variable).block.id, 'dependency', callbackReference ? 'callback behavior reference' : 'used by custom helper', call.offset);
    }
  }
  if (!blocks.length) throw new Error('Could not reconstruct this example.');
  // Function membership and explicit callback/call references form a hierarchy,
  // separate from payload routes. Source order does not flatten C++ conditions.
  const routineSteps = [...opMap.values()];
  for (const scope of scopes) {
    const body = code.slice(scope.start, scope.end), bodyStart = scope.start + body.indexOf('{') + 1;
    const freeCalls = /\b([A-Za-z_]\w*)\s*\(/g;
    for (const m of symbols.slice(bodyStart, scope.end - 1).matchAll(freeCalls)) {
      const offset = bodyStart + m.index, name = m[1];
      if (!summary.helperFunctions.includes(name) && name !== 'attachInterrupt') continue;
      if (/[.>\w]/.test(code[offset - 1] || '')) continue;
      const args = callArguments(code, offset + m[0].length - 1);
      const step = { id: 'routine-call-' + routineSteps.length, label: name + '()', category: 'intent', added: false,
        reason: 'Source function call in ' + scope.name + ': ' + name + '(' + args + '). Review conditions in the original code.',
        requestIds: blocks.some(b => b.id === 'example-custom') ? ['example-custom'] : [],
        call: { method: name, variable: '', args, scope, offset, end: callEnd(code, offset + m[0].length - 1), free: true } };
      routineSteps.push(step); nodes.push(step); nodeOffsets[step.id] = offset;
    }
  }
  const routines = scopes.map((scope, index) => {
    const bindings = [];
    for (const step of routineSteps) {
      const call = step.call;
      const reference = splitParameters(call.args).some(arg => arg.replace(/^&\s*/, '').trim() === scope.name);
      const invocation = call.free && call.method === scope.name;
      if (reference || invocation) bindings.push({ nodeId: step.id, kind: invocation ? 'call' : 'registration',
        label: (call.variable ? call.variable + '.' : '') + call.method + '() in ' + call.scope.name + '()' });
    }
    const kind = ['setup', 'loop'].includes(scope.name) ? scope.name : bindings.some(b => b.kind === 'registration') ? 'callback' : 'helper';
    const localSteps = routineSteps.filter(n => n.call.scope.key === scope.key);
    const children = n => localSteps.filter(other => other !== n && other.call.offset > n.call.offset && other.call.end <= n.call.end &&
      !localSteps.some(mid => mid !== n && mid !== other && mid.call.offset > n.call.offset && mid.call.end <= n.call.end && other.call.offset > mid.call.offset && other.call.end <= mid.call.end));
    const steps = [], visited = new Set();
    const visit = n => { if (visited.has(n.id)) return; visited.add(n.id); children(n).sort((a,b)=>a.call.offset-b.call.offset).forEach(child => { visit(child); edge(child.id,n.id,'data','Argument result',n.call.offset); }); steps.push(n); };
    localSteps.sort((a,b)=>a.call.offset-b.call.offset).forEach(visit);
    if (!steps.length && !['setup', 'loop'].includes(kind)) {
      const id = 'routine-body-' + index;
      nodes.push({ id, label: 'Custom code · ' + scope.name, category: 'intent', added: false,
        reason: 'Custom implementation in ' + scope.name + '(). Open the source to inspect its statements and conditions.',
        requestIds: blocks.some(b => b.id === 'example-custom') ? ['example-custom'] : [] });
      nodeOffsets[id] = scope.start; steps.push({ id });
    }
    return { id: 'routine-' + index, name: scope.name, kind, nodeIds: steps.map(n => n.id), bindings,
      reason: 'Source order only. Branches, conditions, nested calls and event timing must be reviewed in the sketch.' };
  });
  const design = {
    schemaVersion: 1, adapter: catalog.adapter.id, target: catalog.adapter.target,
    name: summary.title.slice(0, 100),
    description: 'Reproduce ' + file + ' using the selected checkout. Preserve its original behavior, custom logic, settings and data routes. Review compatibility against the current source before modifying it. The imported graph is a structural source reconstruction, not a validation of the sketch.',
    blocks
  };
  const graphNodes = nodes.map(({ call, ...n }) => n);
  for (const e of routineConnections({nodes:graphNodes,edges,routines}).filter(e=>e.derived)) edge(e.from,e.to,e.kind,e.label,nodeOffsets[e.navigationNodeId]);
  return {
    summary, design,
    locations: { nodes: Object.fromEntries(Object.entries(nodeOffsets).map(([id, offset]) => [id, location(offset)])), edges: edgeLocations },
    assembly: {
      schemaVersion: 1, designRevision: revisionOf(design), sourceFingerprint: catalog.fingerprint,
      summary: 'Source reconstruction of ' + summary.title + ': ' + objects.length + ' objects, ' + opList.length + ' workflow operations' + (extras.length ? ', plus custom behavior.' : '.'),
      nodes: graphNodes, edges, routines,
      files: [{ name: 'FeatureApplication.ino', content: source }],
      decisions: [
        'Imported directly from ' + file + '. The displayed .ino is the original example source, with the download filename normalized.',
        'Class blocks come from actual object declarations. Constructor settings and object-specific behavior calls are preserved in the blocks.',
        'Solid routes require source order, the same function scope and matching Manager context, or an explicit batch argument. Dashed dependencies come from constructor/configuration references and method ownership.',
        'Sensor registration and arbitrary helper-to-helper payload routing are not inferred from class categories. Custom callbacks attach only to components referenced by the source.',
        'The source index does not evaluate preprocessor branches, callbacks, aliasing or C++ control flow. Graph connections are structural hints for Codex to review.',
        ...extras
      ],
      warnings: [
        'Original examples can depend on older APIs, hardware assumptions or companion files. Importing one does not establish that it compiles on the current checkout.',
        'Only the listed project files are included. Review companion headers, configuration files and credential placeholders from the original example folder when validating it.',
        ...(extras.length ? ['Custom behavior is represented by a Describe a feature block and per-block behavior text. Review it in the source before adapting the application.'] : [])
      ],
      validation: 'Source indexed and graph references reconstructed automatically. No LLM review, firmware compilation or hardware test is implied. Use Assemble with Codex to review and adapt this example.'
    }
  };
}
