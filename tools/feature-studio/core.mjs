// Application-neutral document and assembly contract. The source adapter owns
// discovery and target-specific guidance; the canvas never generates driver calls.
import { createHash } from 'node:crypto';
import { relevantRecipes } from './recipe-index.mjs';

export function blankDesign(catalog) {
  return { schemaVersion: 1, adapter: catalog.adapter.id, target: catalog.adapter.target, name: 'Untitled application', description: '', blocks: [] };
}

export const completionInstructions = [
  'Use plain development-tool language for labels, summaries and decisions. Explain what runs, when or under which actual condition, and what data is read or written. Prefer names such as Read temperature, Save reading to SD, Wait 5 minutes, and Register wake callback. Avoid vague orchestration language or playful metaphors. Keep explanations short and tied to code. Follow each prepared recipe review: source order does not prove branch reachability; preserve conditions, early returns, compile-time alternatives and callback timing. Do not copy deployment diagnostics or recovery frameworks unless the request needs them.',
  'Review feasibility in featureAssessments for every requested block and, when present, the application brief (requestId "$brief"). Use supported, needs-info, partial or unsupported. Check the selected object, board/hardware limits, current Loom implementations and the wider Arduino ecosystem: absent Loom settings do not mean impossible; custom Arduino code and compatible external libraries are allowed. Mark partial/unsupported only for a verified conflict, with the exact requested clause, a brief concrete reason and the evidence checked. Missing hardware details or unverified external APIs are needs-info, not proven incompatibility. Offer a small feasible alternative. Associate nodeIds only with nodes whose own behavior is affected, not every supporting component. Preserve infeasible requests as non-executable graph nodes outside routines; never draw them as working operations or silently invent an implementation. Implement only the feasible subset, clearly documenting omissions in the sketch and warnings. If nothing is feasible, return an explicitly nonfunctional review sketch with no fake behavior. Do not claim a fully working application while requirements remain infeasible or unresolved.',
  'Implementation style: use the smallest working pattern established by the current repository examples and implementations. Make the fewest changes needed. Prefer direct SDK calls and simple constants. Do not introduce configuration parsers, generalized frameworks/wrappers, retry engines, extra state machines, diagnostics or fallback services unless explicitly requested or required by verified hardware/API behavior. Preserve essential initialization, timing, power sequencing, data ownership and error behavior. Follow tools/feature-studio/ASSEMBLY_GUIDE.md when present.',
  'If preparedContext is present, begin with its current-checkout source excerpts, verified API signatures and reference routine graphs. Use these instead of a broad repository crawl. Inspect only unresolved relevant implementation details or companion guidance, and widen the search only when the prepared packet has a genuine gap.',
  'Complete the application from capabilities: the selected blocks are desired behaviors, not an exhaustive list of implementation components. A sparse design is normal. Do not require the user to add support blocks manually or connect ports.',
  'Work from this request and the current repository; assume no prior chat context or remembered Loom APIs. Start with catalog.relevantRecipes, read the smallest relevant example sketches and their companion README/configuration templates, then verify the classes and implementations they use. Search additional current examples if the shortlist is incomplete. Never copy credentials or assume an example matches the selected branch APIs. Cite the actual recipe/source paths you used in decisions, and explain adaptations or conflicts.',
  'The brief and block descriptions can supply capabilities not present as blocks: for example, one sensor plus “log every 5 minutes” is a complete request. Supply logging, cadence and required infrastructure in the assembled features, routines and application. Decide whether sleep is appropriate from the request and hardware; do not equate a sampling interval with permission to power down a sensor or choose network services the user did not ask for.',
  'Discover and add the minimum supporting objects, initialization, measurement/read calls, data packaging, storage/reporting calls and required scheduling or error handling from the actual source and examples. For example, a sensor plus an SD logging capability should produce the complete read-to-record workflow, including its required manager/controller and storage setup.',
  'Choose ordinary implementation details from verified repository examples and compatible defaults. Honor explicit settings and exclusions. Explain unresolved hardware assumptions or unsupported combinations in warnings; do not invent APIs, claim validation you did not perform, or add unrelated capabilities.',
  'Show automatically supplied components AND workflow operations in the returned graph with added:true and a concrete reason. Associate them with the requested feature IDs they support. Preserve requested nodes as added:false. Connect both data routes and real prerequisites so the graph agrees with the generated code.',
  'Wire the graph automatically; the user never connects ports. Include flow edges between executable steps, with labeled conditional paths and loop boundaries matching the actual code. Include callback edges for registration/call relationships (registration is not immediate invocation), data edges for payloads and returned argument values, dependency edges for supporting components, and cycle edges for repetition. Do not leave setup/configuration steps as an unconnected list. Evaluate nested argument calls before the operation consuming their result.',
  'Provide sourceLinks for every graph node and connection explanation. Each reference has targetKind node/edge/routine, targetId (node/routine ID or zero-based edge index as a string), scope application/repository, file, anchor, and explanation. The anchor must be a short exact unique excerpt of actual code in that file, not a comment or line-number guess. Link executable steps and branch conditions to the final generated application. Also link the relevant current library implementations when behavior is delegated or implicit, explaining the caller-to-implementation relationship. Reuse references for related edges; do not add extra sketch code merely to create anchors. If an exact relationship is unverified, state that limitation instead of inventing a source link.',
  'Describe execution hierarchy in routines: include setup and loop, plus each callback or helper relevant to the features. List its operation nodeIds in source order and explain conditions in reason. Include initialization/configuration and callback registration calls as operation nodes, not just packet operations. Keep component declarations outside routines. Each binding references the actual registration/calling operation nodeId, labels the object/event or caller, and uses kind registration or call. Never imply that registering a callback executes its body immediately. Represent custom callback logic explicitly. Data edges are payload routes, not execution order or proof that branches run unconditionally.',
  'Return a complete application with required includes, declarations, setup and loop behavior, rather than stubs or instructions to wire the missing pieces. Keep automatic additions in the assembly result; do not change the requested design to manufacture extra requirements.'
];

export function assemblyCatalog(design, catalog) {
  const selected = new Set(design.blocks.map(b => b.catalogId));
  const classes = catalog.blocks.filter(b => b.className);
  const indexed = new Map(catalog.blocks.map(b => [b.id, b]));
  const candidates = new Set(), visited = new Set(), pending = [...selected];
  // These are discovery hints for Codex, not a generated dependency graph. All
  // constructor variants and intent matches remain candidates until reviewed.
  while (pending.length) {
    const id = pending.pop();
    if (visited.has(id)) continue;
    visited.add(id);
    const block = indexed.get(id);
    if (!block) continue;
    const words = new Set([block.bases || '', ...(block.constructors || []).flatMap(c => c.parameters.map(p => p.type))].join(' ').split(/\W+/));
    const references = [...(block.candidates || []), ...classes.filter(c => words.has(c.className)).map(c => c.id)];
    for (const reference of references) {
      if (!selected.has(reference) && indexed.has(reference)) candidates.add(reference);
      if (!visited.has(reference)) pending.push(reference);
    }
  }
  return {
    adapter: catalog.adapter,
    blocks: catalog.blocks.filter(b => selected.has(b.id)),
    supportCandidates: catalog.blocks.filter(b => candidates.has(b.id)),
    relevantRecipes: relevantRecipes(design, catalog),
    availableClasses: classes.map(b => ({ id: b.id, className: b.className, category: b.category, source: b.source,
      constructors: b.constructors.map(c => c.signature), examples: b.examples.slice(0, 3) })),
    guidance: 'Support candidates and available classes come from this checkout. They are not required blocks or confirmed compatibility. Review source and add only what the requested behavior needs.'
  };
}

export const assemblySchema = {
  type: 'object', additionalProperties: false,
  required: ['schemaVersion', 'designRevision', 'sourceFingerprint', 'summary', 'nodes', 'edges', 'routines', 'sourceLinks', 'featureAssessments', 'files', 'decisions', 'warnings', 'validation'],
  properties: {
    schemaVersion: { type: 'integer', enum: [1] },
    designRevision: { type: 'string' }, sourceFingerprint: { type: 'string' },
    summary: { type: 'string' },
    featureAssessments: {type:'array',items:{type:'object',additionalProperties:false,
      required:['requestId','status','summary','nodeIds','issues','suggestion'],properties:{
        requestId:{type:'string'},status:{enum:['supported','needs-info','partial','unsupported']},summary:{type:'string'},
        nodeIds:{type:'array',items:{type:'string'}},suggestion:{type:'string'},
        issues:{type:'array',items:{type:'object',additionalProperties:false,required:['requirement','reason','evidence'],properties:{requirement:{type:'string'},reason:{type:'string'},evidence:{type:'string'}}}}
      }}},
    sourceLinks: {type:'array',items:{type:'object',additionalProperties:false,
      required:['targetKind','targetId','scope','file','anchor','explanation'],properties:{
        targetKind:{enum:['node','edge','routine']},targetId:{type:'string'},scope:{enum:['application','repository']},
        file:{type:'string'},anchor:{type:'string'},explanation:{type:'string'}
      }}},
    nodes: { type: 'array', items: {
      type: 'object', additionalProperties: false,
      required: ['id', 'label', 'category', 'added', 'reason', 'requestIds'],
      properties: {
        id: { type: 'string' }, label: { type: 'string' }, category: { type: 'string' },
        added: { type: 'boolean' }, reason: { type: 'string' },
        requestIds: { type: 'array', items: { type: 'string' } }
      }
    }},
    edges: { type: 'array', items: {
      type: 'object', additionalProperties: false, required: ['from', 'to', 'kind', 'label'],
      properties: { from: { type: 'string' }, to: { type: 'string' }, kind: { enum: ['data', 'dependency', 'cycle', 'flow', 'callback'] }, label: { type: 'string' } }
    }},
    routines: { type: 'array', items: {
      type: 'object', additionalProperties: false, required: ['id', 'name', 'kind', 'nodeIds', 'bindings', 'reason'],
      properties: {
        id: { type: 'string' }, name: { type: 'string' }, kind: { enum: ['setup', 'loop', 'callback', 'helper'] }, reason: { type: 'string' },
        nodeIds: { type: 'array', items: { type: 'string' } },
        bindings: { type: 'array', items: { type: 'object', additionalProperties: false, required: ['nodeId', 'kind', 'label'],
          properties: { nodeId: { type: 'string' }, kind: { enum: ['registration', 'call'] }, label: { type: 'string' } } } }
      }
    }},
    files: { type: 'array', items: {
      type: 'object', additionalProperties: false, required: ['name', 'content'],
      properties: { name: { type: 'string' }, content: { type: 'string' } }
    }},
    decisions: { type: 'array', items: { type: 'string' } },
    warnings: { type: 'array', items: { type: 'string' } },
    validation: { type: 'string' }
  }
};

export function revisionOf(design) {
  // Position-only changes do not invalidate an assembly. All behavior/config does.
  const content = {
    schemaVersion: design.schemaVersion, adapter: design.adapter, target: design.target,
    name: design.name, description: design.description,
    blocks: design.blocks.map(({ position, ...b }) => b)
  };
  return createHash('sha256').update(JSON.stringify(content)).digest('hex');
}
export function validateDesign(d) {
  if (!d || d.schemaVersion !== 1 || typeof d.adapter !== 'string' || !/^[\w-]{1,60}$/.test(d.adapter)) throw new Error('Unsupported design format or adapter.');
  if (typeof d.name !== 'string' || !d.name.trim() || d.name.length > 100) throw new Error('Give the design a name (up to 100 characters).');
  if (typeof d.target !== 'string' || typeof d.description !== 'string' || d.description.length > 20000) throw new Error('Invalid application brief.');
  if (!Array.isArray(d.blocks) || d.blocks.length > 100) throw new Error('Use at most 100 feature blocks.');
  const ids = new Set();
  for (const b of d.blocks) {
    if (!b || typeof b.id !== 'string' || !/^[\w-]{1,80}$/.test(b.id) || ids.has(b.id)) throw new Error('Block IDs must be unique.');
    ids.add(b.id);
    if (typeof b.catalogId !== 'string' || typeof b.label !== 'string' || typeof b.notes !== 'string') throw new Error('Invalid feature block.');
    if (!b.settings || typeof b.settings !== 'object' || Array.isArray(b.settings)) throw new Error('Invalid block settings.');
    if (Object.hasOwn(b.settings, 'constructorIndex') && (!Number.isInteger(b.settings.constructorIndex) || b.settings.constructorIndex < 0 || b.settings.constructorIndex > 1000)) throw new Error('Invalid constructor selection.');
    if (Object.hasOwn(b.settings, 'parameters') && (!b.settings.parameters || typeof b.settings.parameters !== 'object' || Array.isArray(b.settings.parameters))) throw new Error('Invalid constructor settings.');
    if (!Number.isInteger(b.count) || b.count < 1 || b.count > 64) throw new Error('Each block needs 1–64 instances.');
    if (b.position && (!Number.isFinite(b.position.x) || !Number.isFinite(b.position.y) || b.position.x < 0 || b.position.y < 0 || b.position.x > 20000 || b.position.y > 20000)) throw new Error('Invalid canvas position.');
  }
  return d;
}

export function validateAssembly(a, design, catalog) {
  if (!a || a.schemaVersion !== 1 || a.designRevision !== revisionOf(design)) throw new Error('Assembly belongs to a different design revision.');
  if (a.sourceFingerprint !== catalog.fingerprint) throw new Error('Assembly belongs to a different source checkout. Ask Codex to review the current source.');
  for (const k of ['summary', 'validation']) if (typeof a[k] !== 'string') throw new Error('Assembly is missing ' + k + '.');
  for (const k of ['nodes', 'edges', 'files', 'decisions', 'warnings']) if (!Array.isArray(a[k])) throw new Error('Assembly is missing ' + k + '.');
  if (a.nodes.length > 250 || a.edges.length > 1000 || a.files.length > 40) throw new Error('Assembly exceeds the prototype limits.');
  const ids = new Set(), requestIDs = new Set(design.blocks.map(b => b.id)), covered = new Set();
  for (const n of a.nodes) {
    if (!n || typeof n.id !== 'string' || ids.has(n.id) || !n.id || typeof n.label !== 'string' || typeof n.category !== 'string' || typeof n.added !== 'boolean' || typeof n.reason !== 'string' || !Array.isArray(n.requestIds)) throw new Error('Invalid assembly node.');
    ids.add(n.id);
    if (n.added && !n.reason.trim()) throw new Error('Added nodes need an explanation.');
    for (const id of n.requestIds) {
      if (!requestIDs.has(id)) throw new Error('Assembly references an unknown requested block.');
      covered.add(id);
    }
  }
  for (const id of requestIDs) if (!covered.has(id)) throw new Error('Assembly omitted requested block ' + id + '.');
  // Legacy recipes remain readable; new model responses review all requests.
  if (a.featureAssessments !== undefined) {
    if (!Array.isArray(a.featureAssessments) || a.featureAssessments.length > 101) throw new Error('Invalid feature assessments.');
    const reviewed = new Set();
    for (const review of a.featureAssessments) {
      if (!review || (!requestIDs.has(review.requestId) && !(review.requestId === '$brief' && design.description.trim())) || reviewed.has(review.requestId) ||
        !['supported','needs-info','partial','unsupported'].includes(review.status) || typeof review.summary !== 'string' || !review.summary.trim() ||
        typeof review.suggestion !== 'string' || !Array.isArray(review.nodeIds) || review.nodeIds.some(id => !ids.has(id)) ||
        !Array.isArray(review.issues) || review.issues.length > 50 || review.issues.some(issue => !issue || ['requirement','reason','evidence'].some(key => typeof issue[key] !== 'string' || !issue[key].trim())) ||
        (review.status !== 'supported' && !review.issues.length)) throw new Error('Invalid feature assessment or missing feasibility explanation.');
      reviewed.add(review.requestId);
    }
    if ([...requestIDs, ...(design.description.trim() ? ['$brief'] : [])].some(id => !reviewed.has(id))) throw new Error('Feature assessments omitted a request or application brief.');
  }
  for (const e of a.edges) if (!e || !ids.has(e.from) || !ids.has(e.to) || !['data', 'dependency', 'cycle', 'flow', 'callback'].includes(e.kind) || typeof e.label !== 'string') throw new Error('Invalid graph connection.');
  // Older saved assemblies remain readable; new generations use the hierarchy.
  if (a.routines !== undefined) {
    if (!Array.isArray(a.routines) || a.routines.length > 100) throw new Error('Invalid execution routines.');
    const routines = new Set(), members = new Set();
    for (const r of a.routines) {
      if (!r || typeof r.id !== 'string' || !r.id || routines.has(r.id) || typeof r.name !== 'string' || !r.name || typeof r.reason !== 'string' || !['setup', 'loop', 'callback', 'helper'].includes(r.kind) || !Array.isArray(r.nodeIds) || !Array.isArray(r.bindings)) throw new Error('Invalid execution routine.');
      routines.add(r.id);
      for (const id of r.nodeIds) { if (!ids.has(id) || members.has(id)) throw new Error('Invalid or repeated routine step.'); members.add(id); }
      for (const b of r.bindings) if (!b || !ids.has(b.nodeId) || !['registration', 'call'].includes(b.kind) || typeof b.label !== 'string') throw new Error('Invalid routine binding.');
    }
  }
  const filenames = new Set();
  for (const f of a.files) {
    // Flat output folder deliberately avoids arbitrary paths and overwrites.
    if (!f || typeof f.name !== 'string' || !/^[A-Za-z0-9][A-Za-z0-9_.-]{0,100}$/.test(f.name) || /^(con|nul|aux|prn|com\d|lpt\d)(\.|$)/i.test(f.name) || !/\.(ino|h|hpp|cpp|c|json|txt|md|example)$/.test(f.name) || filenames.has(f.name.toLowerCase()) || typeof f.content !== 'string' || f.content.length > 500000) throw new Error('Invalid or duplicate output filename.');
    filenames.add(f.name.toLowerCase());
  }
  if (!a.files.some(f => f.name.endsWith('.ino'))) throw new Error('Assembly must include an Arduino sketch.');
  if(a.sourceLinks!==undefined) {
    if(!Array.isArray(a.sourceLinks)||a.sourceLinks.length>4000)throw new Error('Invalid source links.');
    for(const ref of a.sourceLinks) {
      if(!ref||!['node','edge','routine'].includes(ref.targetKind)||typeof ref.targetId!=='string'||
        !(ref.targetKind==='node'?ids.has(ref.targetId):ref.targetKind==='edge'?/^(0|[1-9]\d*)$/.test(ref.targetId)&&!!a.edges[Number(ref.targetId)]:a.routines?.some(r=>r.id===ref.targetId))||
        !['application','repository'].includes(ref.scope)||typeof ref.file!=='string'||typeof ref.anchor!=='string'||!ref.anchor.trim()||ref.anchor.length>10000||typeof ref.explanation!=='string'||
        (ref.scope==='application'?!filenames.has(ref.file.toLowerCase()):! /^(src|examples)\/[\w./ -]+\.(ino|h|hpp|cpp|c)$/.test(ref.file)||ref.file.split('/').includes('..')||/secret|credential|creds/i.test(ref.file)))throw new Error('Invalid source link target or code anchor.');
    }
  }
  if (![...a.decisions, ...a.warnings].every(s => typeof s === 'string')) throw new Error('Invalid assembly explanations.');
  return a;
}

export function makePrompt(design, catalog, workspace) {
  return [
    'Assemble the saved Feature Studio design in this Codex Desktop chat.',
    'Read ' + workspace + '/request.json and ' + workspace + '/assembly.schema.json.',
    'Source checkout: ' + catalog.root,
    'The request contains desired features, not a manually wired circuit. Review the actual source and examples, fill in required supporting components, and generate both the application files and the organized workflow graph.',
    ...completionInstructions,
    ...catalog.adapter.instructions,
    'Honor EVERY requested block, count, setting and the free-text brief. If a request is incompatible or underspecified, represent it in the graph and explain the issue in warnings; never silently omit it or pretend it works.',
    'Return JSON matching the schema by writing ' + workspace + '/assembly.json. The running web app watches that file and shows the result. Write atomically via a temporary file and rename to avoid partial reads.',
    'Copy designRevision and sourceFingerprint EXACTLY from request.json. These identify the design and source you reviewed.',
    'Graph IDs must be unique; each requested block ID must appear in at least one node requestIds array. Added dependencies have added:true and a reason. Edges must reference graph nodes. Use flow for step execution and labeled branches, callback for registration/call relationships, data for payload and argument results, dependency for required components, and cycle for scheduling.',
    'Name the primary sketch FeatureApplication.ino. Extra files must use simple filenames without folders. Keep credentials as placeholders. Include honest validation results and remaining limitations.',
    'Do not edit the app, its design, or library source. Only write assembly.json and an optional temporary assembly file in the workspace. The app materializes reviewed output files after validation.',
    'Design revision: ' + revisionOf(design),
    'Source fingerprint: ' + catalog.fingerprint
  ].join('\n\n');
}
