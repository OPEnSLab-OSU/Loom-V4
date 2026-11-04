import { stripComments, maskStrings } from './adapters/cpp.mjs';

const words = s => new Set(String(s).replace(/([a-z])([A-Z])/g, '$1 $2').toLowerCase().split(/[^a-z0-9]+/).filter(w => w.length > 2));
const signals = [
  ['Local logging', /\b(sd|log(?:ging)?|record(?:ing)?|save|csv|storage)\b/i, /logToSD|BatchSD|SDManager|(?:^|[_/])SD(?:[_/.]|$)/i],
  ['Sampling cadence', /\b(interval|minutes?|seconds?|periodic|cadence|every)\b/i, /pause|setSampleInterval|setInterruptDuration/i],
  ['Sleep / wake', /\b(sleep|wake|low power)\b/i, /(?:^|\s)(?:sleep|standby|deepSleep)(?=\s|$)/i],
  ['Wi-Fi', /\b(wi-?fi|wifi)\b/i, /Loom_WIFI/i],
  ['LTE', /\b(lte|cellular)\b/i, /Loom_LTE/i],
  ['MQTT / database', /\b(mqtt|mongo(?:db)?|database|broker)\b/i, /MongoDB|MQTT/i],
  ['LoRa / receiver', /\b(lora|radio|receiver|gateway)\b/i, /LoRa|receiveBatch|\.receive\b/i],
  ['Batch uploads', /\b(batch|buffer|queue)\b/i, /BatchSD|sendBatch|receiveBatch/i]
];

export function indexRecipe(source, file, blocks) {
  const code = stripComments(source).replace(/^\s*#\s*include[^\n]*/gm, ''), searchable = maskStrings(code);
  const classNames = blocks.filter(b => b.className && new RegExp('\\b' + b.className + '\\b').test(searchable)).map(b => b.className);
  const methods = [...new Set([...searchable.matchAll(/\b\w+\s*(?:\.|->)\s*(\w+)\s*\(/g)].map(m => m[1]))];
  return { path: file, title: file.split('/').at(-1).replace(/\.ino$/, '').replace(/_/g, ' '), classNames, methods, lines: source.split('\n').length };
}

export function recipeCapabilities(recipe) {
  const text=[...recipe.classNames,...recipe.methods].join(' ');
  return signals.filter(([, , present])=>present.test(text)).map(([label])=>label);
}

export function relevantRecipes(design, catalog, limit = 10) {
  const chosen = catalog.blocks.filter(b => design.blocks.some(s => s.catalogId === b.id));
  const demands = [design.description, ...design.blocks.flatMap(b => [b.label, b.notes, JSON.stringify(b.settings)]), ...chosen.map(b => b.title)].join(' ');
  const query = words(demands), classNames = chosen.filter(b => b.className).map(b => b.className);
  const wanted = signals.filter(([, request]) => request.test(demands));
  const ranked = (catalog.recipes || []).map(recipe => {
    const reasons = [], text = [recipe.path, ...recipe.classNames, ...recipe.methods].join(' ');
    let score = 0;
    for (const name of classNames) if (recipe.classNames.includes(name)) { score += 24; reasons.push('Uses ' + name); }
    for (const [label, , present] of wanted) if (present.test(text)) { score += 14; reasons.push(label); }
    const tokens = words(text);
    for (const word of query) if (tokens.has(word) && !['loom', 'manager', 'feature', 'source', 'selected', 'sensor', 'sensors', 'example', 'class'].includes(word)) score += 1;
    score -= Math.max(0, recipe.classNames.length - Math.max(2, classNames.length + 1)) * 4;
    return { ...recipe, score, reasons };
  }).filter(r => r.reasons.length || r.score >= 3).sort((a, b) => b.score - a.score || a.path.length - b.path.length || a.path.localeCompare(b.path));
  // Include small complementary examples before large combinations. Ten variants
  // of a lab sketch must not crowd out the basic sensor and logging references.
  const simplest = list => [...list].sort((a, b) => a.classNames.length - b.classNames.length || (a.lines || 0) - (b.lines || 0) || b.score - a.score)[0];
  const selected = new Map();
  const add = recipe => { if (recipe && selected.size < limit) selected.set(recipe.path, recipe); };
  for (const name of classNames) add(simplest(ranked.filter(r => r.classNames.includes(name))));
  for (const [label] of wanted) if (![...selected.values()].some(r => r.reasons.includes(label))) add(simplest(ranked.filter(r => r.reasons.includes(label))));
  ranked.forEach(add);
  return [...selected.values()];
}
