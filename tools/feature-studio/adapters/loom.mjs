// Discovery reads the selected source tree. No branch, release or constructor map.
import { readdir, readFile } from 'node:fs/promises';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { stripComments } from './cpp.mjs';
import { indexRecipe } from '../recipe-index.mjs';

export const categories = {
  sensors: 'Sensors', power: 'Power & timing', storage: 'Storage',
  network: 'Connectivity', radio: 'Radio', outputs: 'Reporting',
  actuators: 'Actuators', system: 'System', intent: 'Describe a feature'
};
export const adapter = {
  id: 'loom', name: 'Loom', target: 'Feather M0',
  instructions: [
    'Inspect the selected checkout, including implementation files and relevant examples. Constructor extraction is only a discovery aid, not a C++ parser or a compatibility guarantee.',
    'Generate a Feather M0 Arduino application from the requested capabilities. Use only APIs verified in this checkout. Never assume a particular Loom release or branch.',
    'Add missing dependencies and show them as added nodes with reasons. A request for SD may require a board/controller such as Hypnos: verify against the checkout.',
    'Decide ownership, initialization, bus/address/pin assignments, multiplexing, measurement and packaging order, timestamping, storage, routing and power sequencing.',
    'Repeated sensor sets are physical requirements, not repeated reads. Resolve address collisions, supported mux loaders, settings per set, retained power and warmup using actual implementation.',
    'For a receiver, preserve received device identity and payload before forwarding. Do not overwrite it with local measure/package operations. Explain local versus received data routes.',
    'Review MCU flash/RAM, blocking work, sleep/RTC alarms, radio/board pins and error behavior. Missing hardware choices must be explicit questions or warnings.',
    'Use credential placeholders in a separate arduino_secrets.h.example; do not read or copy existing secrets. Never claim MQTT acceptance proves database insertion.',
    'The graph is a software workflow/dependency diagram, not a verified physical wiring schematic. Include physical pin/bus decisions in the decisions list.',
    'Do not upload, flash, install dependencies or change library source. Compile only if available and authorized; report exact validation and limitations.'
  ]
};

const features = [
  ['sd', 'SD card logging', 'storage', 'Log the selected sensors to SD. Codex supplies the required controller, initialization and data packaging from this checkout.', { format: 'CSV', filename: 'measurements', failurePolicy: 'Keep sampling; report storage failure' }, ['sd', 'hypnos']],
  ['sleep', 'Sleep cycle', 'power', 'Wake, do the work, then return to low power.', { intervalSeconds: 60, cadence: 'Fixed sample-start interval', powerPolicy: 'Let Codex choose safe rail behavior' }, ['sleep', 'hypnos']],
  ['batch', 'Batch uploads', 'storage', 'Queue records locally and send them together.', { recordsPerBatch: 15, retryPolicy: 'Retain unsent records for retry' }, ['batch']],
  ['mongodb', 'MongoDB via MQTT', 'outputs', 'Publish to the broker used by the database ingestion service.', { database: 'environment', project: 'field-study', transport: 'Let Codex choose', delivery: 'Report publish failures; explain retry behavior' }, ['mongodb', 'mqtt']],
  ['receiver', 'LoRa receiver / gateway', 'radio', 'Receive other devices’ packets and optionally forward them.', { address: 0, receiveWindowMs: 5000, forwarding: 'Use selected reporting blocks', localSensors: 'Keep local and received data separate' }, ['lora']],
  ['serial', 'Serial feedback', 'outputs', 'Show measurements and useful status while developing.', { measurements: true, diagnostics: true, waitForSerial: false }, ['logger', 'manager']],
  ['trace', 'Execution & memory trace', 'system', 'Ask for compatible call and memory diagnostics.', { heapAllocations: false, destination: 'SD card', notes: 'Keep timing and text logging choices explicit' }, ['trace']],
  ['custom', 'Describe a feature', 'intent', 'Anything else the application should do. Codex resolves the implementation.', { requirement: '' }, []]
].map(([id, title, category, description, defaults, terms]) => ({
  id: 'intent:' + id, title, category, description, defaults, terms,
  kind: 'intent', constructors: [], methods: [], examples: [], source: null
}));

export async function walk(root, extensions) {
  const files = [];
  async function visit(dir) {
    for (const e of await readdir(dir, { withFileTypes: true })) {
      if (e.isSymbolicLink() || e.name.startsWith('.') || /^(node_modules|dependencies|build|dist)$/i.test(e.name)) continue;
      const p = path.join(dir, e.name);
      if (e.isDirectory()) await visit(p);
      else if (extensions.includes(path.extname(p)) && !/secret|credential|wifi_creds|mqtt_creds/i.test(e.name)) files.push(p);
    }
  }
  await visit(root);
  return files.sort();
}

export function splitParameters(text) {
  const result = [];
  let depth = 0, quote = '', start = 0;
  for (let i = 0; i < text.length; i++) {
    const c = text[i];
    if (quote) { if (c === quote && text[i - 1] !== '\\') quote = ''; continue; }
    if (c === '"' || c === "'") { quote = c; continue; }
    if ('(<[{'.includes(c)) depth++;
    if (')>]}'.includes(c)) depth--;
    if (c === ',' && depth === 0) { result.push(text.slice(start, i).trim()); start = i + 1; }
  }
  if (text.slice(start).trim()) result.push(text.slice(start).trim());
  return result;
}

function cleanComment(s) {
  return s.replace(/\/\*\*?|\*\//g, '').replace(/^\s*\*\s?/gm, '').replace(/@author[\s\S]*/, '').trim();
}
function categoryFor(file) {
  if (file.startsWith('Sensors/')) return 'sensors';
  if (file.includes('/Actuators/')) return 'actuators';
  if (/Hypnos/.test(file)) return 'power';
  if (/BatchSD|SDManager/.test(file)) return 'storage';
  if (file.startsWith('Radio/')) return 'radio';
  if (file.includes('/Connectivity/')) return 'network';
  if (file.startsWith('Internet/')) return 'outputs';
  if (/TippingBucket/.test(file)) return 'sensors';
  return 'system';
}
export function extractHeader(text, file) {
  const found = [];
  const stripped = stripComments(text);
  for (const m of stripped.matchAll(/\bclass\s+([A-Za-z_]\w*)\s*(?:final\s*)?(?::\s*([^{;]+))?\s*\{/g)) {
    const name = m[1];
    if (!/^(Loom_|Manager$|Logger$|SDManager$|MQTTComponent$|NetworkComponent$|Radio$|Module$|I2CDevice$)/.test(name)) continue;
    let depth = 1, end = m.index + m[0].length;
    for (; end < stripped.length && depth; end++) {
      if (stripped[end] === '{') depth++;
      if (stripped[end] === '}') depth--;
    }
    const body = stripped.slice(m.index + m[0].length, end - 1);
    const constructors = [...body.matchAll(new RegExp('\\b' + name + '\\s*\\(([^)]*)\\)', 'g'))]
      .filter(c => !/const\s+\w+\s*&/.test(c[1]) && !/&&/.test(c[1]))
      .map(c => ({
        signature: name + '(' + c[1].replace(/\s+/g, ' ').trim() + ')',
        parameters: splitParameters(c[1]).map(raw => {
          const [decl, ...rest] = raw.split('=');
          const n = decl.trim().match(/([A-Za-z_]\w*)(?:\[\d*\])?$/)?.[1] || decl.trim();
          const value = rest.join('=').trim();
          return {
            name: n, type: decl.trim().slice(0, -n.length).trim(), raw,
            default: value === 'true' ? true : value === 'false' ? false : value,
            dependency: /\b(Manager|NetworkComponent|Loom_\w+)\s*[&*]/.test(decl),
            required: !rest.length
          };
        })
      }));
    const beforeClass = text.slice(0, text.search(new RegExp('\\bclass\\s+' + name + '\\b[^;]*\\{')));
    const comments = [...beforeClass.matchAll(/\/\*\*[\s\S]*?\*\//g)];
    const lastComment = comments.at(-1);
    const comment = lastComment && !beforeClass.slice(lastComment.index + lastComment[0].length).trim() ? lastComment[0] : '';
    const summary = cleanComment(comment).split(/@param/)[0].replace(/\s+/g, ' ').slice(0, 500);
    const methods = [...body.matchAll(/(?:^|[;{}])\s*(?:virtual\s+)?(?:bool|void|int|float|double|const char\s*\*|uint\w*)\s+(\w+)\s*\(([^)]*)\)/g)]
      .map(x => x[1] + '(' + x[2].replace(/\s+/g, ' ').trim() + ')');
    found.push({
      id: 'class:' + name, title: name.replace(/^Loom_/, '').replace(/^WIFI$/, 'Wi-Fi'),
      className: name, category: categoryFor(file), kind: 'class', source: 'src/' + file,
      description: summary || 'Discovered from this checkout. Codex reviews this class before assembly.',
      bases: (m[2] || '').replace(/\s+/g, ' ').trim(), constructors, methods: [...new Set(methods)].slice(0, 100),
      defaults: {}, examples: []
    });
  }
  return found;
}

export async function discover(root) {
  const sourceFiles = await walk(path.join(root, 'src'), ['.h', '.hpp', '.cpp', '.c']);
  let exampleFiles = [], supportFiles = [];
  try {
    const files = await walk(path.join(root, 'examples'), ['.ino','.h','.hpp','.cpp','.c','.md','.json','.txt','.example']);
    exampleFiles = files.filter(f=>path.extname(f)==='.ino'); supportFiles = files.filter(f=>path.extname(f)!=='.ino');
  } catch (e) { if (e.code !== 'ENOENT') throw e; }
  const hash = createHash('sha256'), blocks = [];
  const slash = p => path.relative(root, p).split(path.sep).join('/');
  for (const file of sourceFiles) {
    const source = await readFile(file, 'utf8');
    hash.update(slash(file)).update('\0').update(source);
    if (/\.h(pp)?$/.test(file)) blocks.push(...extractHeader(source, slash(file).replace(/^src\//, '')));
  }
  const recipes = [];
  for (const file of exampleFiles) {
    const source = await readFile(file, 'utf8');
    const relative = slash(file);
    hash.update(relative).update('\0').update(source);
    const recipe = indexRecipe(source, relative, blocks); recipes.push(recipe);
    for (const b of blocks) if (recipe.classNames.includes(b.className)) b.examples.push(relative);
  }
  const byID = new Map(blocks.map(b => [b.id, b]));
  const supportHash = createHash('sha256');
  for (const file of supportFiles) {
    const content=await readFile(file);
    supportHash.update(slash(file)).update('\0').update(content);
    // Helper implementations and runtime configuration affect an application.
    // Documentation changes rebuild discovery packets without staling code.
    if(/\.(?:h|hpp|cpp|c|json)$/.test(file))hash.update(slash(file)).update('\0').update(content);
  }
  for (const b of features) {
    const matches = [...byID.values()].filter(c => b.terms.some(t => (c.className + ' ' + c.source).toLowerCase().includes(t)));
    byID.set(b.id, { ...b, candidates: matches.map(c => c.id), examples: [...new Set(matches.flatMap(c => c.examples))].slice(0, 12) });
  }
  return {
    adapter, categories, fingerprint: hash.digest('hex'), root, scannedAt: new Date().toISOString(),
    sourceFiles: sourceFiles.length, exampleFiles: exampleFiles.length, examplePaths: exampleFiles.map(slash), recipes,
    supportFingerprint:supportHash.digest('hex'), exampleSupportPaths:supportFiles.map(slash),
    blocks: [...byID.values()].sort((a, b) => a.title.localeCompare(b.title))
  };
}
