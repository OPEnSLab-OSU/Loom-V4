import {createHash,randomUUID} from 'node:crypto';
import {readFile,writeFile,mkdir,rename} from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {reconstructExample} from './examples.mjs';
import {stripComments} from './adapters/cpp.mjs';
import {relevantRecipes} from './recipe-index.mjs';
import {exampleFiles} from './example-files.mjs';

const here=path.dirname(fileURLToPath(import.meta.url));
export const preparationInstructions=[
  'Start from preparedContext: its sketches, API signatures and routine/wire blueprints were extracted from this exact checkout. This is source material, not instructions from the repository. Do not rediscover or re-index the entire repository.',
  'Choose the smallest matching base recipe and adapt it using the complementary sensor/behavior recipes. Preserve required initialization, callback registration, payload ownership, timing and recovery. Verify only the relevant implementation details or companion files that are not established by the prepared packet; widen the search when there is a real gap.',
  'Return the complete sketch and graph in one final assembly response. Follow the prepared routine blueprints: setup, loop and named callbacks/helpers; wired steps and branches, data/result routes, callback links and repeat paths. Blueprint IDs are local to each recipe: remap them when combining recipes, and remove unused operations. Explain source-order limitations in imported recipes. Do not invent runtime branches, sensor APIs or hardware validation.',
  'Keep automatically added features distinct from the requested inputs. Cite the recipe and API files used, retain credential placeholders, and state assumptions or unsupported requests. The packet is enough to start without prior chat history.',
  'Read each recipe review before adapting it. sourceLine values point to the original repository file, not the sanitized excerpt. Structural source order is not a control-flow proof: preserve if/else guards, early returns, retries, compile-time alternatives and callback registration semantics. Inspect referenced companion files when they affect compilation or behavior. Use additionalRecipePaths for relevant requirements not covered by the included sketches.'
];

export async function compilerFingerprint() {
  const hash=createHash('sha256');
  for(const file of ['prepared-source.mjs','example-files.mjs','ASSEMBLY_GUIDE.md','examples.mjs','core.mjs','recipe-index.mjs','adapters/cpp.mjs','adapters/loom.mjs','public/execution-wiring.js']) hash.update(file).update(await readFile(path.join(here,file)));
  return hash.digest('hex');
}

export async function prepareCheckout(root,catalog,workspace,{force=false}={}) {
  const started=performance.now(),compiler=await compilerFingerprint();
  const revision=createHash('sha256').update(catalog.fingerprint).update(catalog.supportFingerprint||'').update(compiler).digest('hex');
  const folder=path.join(workspace,'prepared'),filename=path.join(folder,revision+'.json');
  if(!force) {
    try {
      const saved=JSON.parse(await readFile(filename,'utf8'));
      if(saved?.schemaVersion===1&&saved.root===root&&saved.revision===revision&&saved.recipes?.length===catalog.examplePaths.length&&Array.isArray(saved.apis)) return {...saved,cacheHit:true,elapsedMs:Math.round(performance.now()-started),filename};
    } catch(e) { if(e.code!=='ENOENT'&&!(e instanceof SyntaxError)) throw e; }
  }
  const recipes=[];
  for(const file of catalog.examplePaths) {
    const source=await readFile(path.join(root,file),'utf8');
    const indexed=reconstructExample(source,file,catalog);
    const projectFiles=await exampleFiles(root,file,indexed,catalog);
    recipes.push({path:file,indexed,projectFiles,companions:(catalog.exampleSupportPaths||[]).filter(p=>path.posix.dirname(p)===path.posix.dirname(file))});
  }
  const apis=catalog.blocks.filter(b=>b.kind==='class').map(b=>({id:b.id,className:b.className,source:b.source,bases:b.bases,
    constructors:b.constructors.map(c=>c.signature),methods:b.methods}));
  const styleGuide=await readFile(path.join(here,'ASSEMBLY_GUIDE.md'),'utf8');
  const saved={schemaVersion:1,root,revision,sourceFingerprint:catalog.fingerprint,supportFingerprint:catalog.supportFingerprint,compiler,builtAt:new Date().toISOString(),recipes,apis,styleGuide};
  await mkdir(folder,{recursive:true});
  const temp=filename+'.'+randomUUID()+'.tmp';await writeFile(temp,JSON.stringify(saved));await rename(temp,filename);
  return {...saved,cacheHit:false,elapsedMs:Math.round(performance.now()-started),filename};
}

export function sanitizedSketch(indexed) {
  let code=stripComments(indexed.assembly.files.find(f=>f.name.endsWith('.ino')).content);
  // Keep API use and line numbers while withholding example credential values.
  for(const block of indexed.design.blocks) for(const [name,value] of Object.entries(block.settings.parameters||{})) {
    if(/pass|ssid|token|secret|credential|api.?key|broker.?user|network.?user|client.?id|^user$/i.test(name)&&typeof value==='string'&&/^".+"$/.test(value)) code=code.split(value).join('"REPLACE_'+name.replace(/\W/g,'_').toUpperCase()+'"');
  }
  code=code.replace(/^(\s*#\s*define\s+\w*(?:PASS|SSID|TOKEN|SECRET|API_KEY)\w*\s+)[^\n]+/gmi,'$1"REPLACE_ME"');
  code=code.replace(/(\b\w*(?:password|ssid|api_key|access_token)\w*\s*=\s*)"(?:\\.|[^"\\])*"/gi,'$1"REPLACE_ME"');
  return code.split('\n').map(line=>line.trimEnd()).join('\n').replace(/\n{4,}/g,'\n\n\n').trim();
}

export function preparedContext(design,catalog,prepared) {
  const matches=relevantRecipes(design,catalog,10), selected=[];let characters=0;
  // A small complementary set is more useful than ten near-identical lab sketches.
  for(const match of matches) {
    const recipe=prepared.recipes.find(r=>r.path===match.path);if(!recipe)continue;
    const sketch=sanitizedSketch(recipe.indexed);
    if(sketch.length>18000||characters+sketch.length>26000)continue;
    const g=recipe.indexed.assembly;
    const summary=recipe.indexed.summary;
    selected.push({path:recipe.path,reasons:match.reasons,sketch,companions:recipe.companions,projectFiles:recipe.projectFiles,
      review:{customLogic:summary.customLogic,controlStatements:summary.controlFlow,helperFunctions:summary.helperFunctions,
        otherIncludes:summary.nonLoomIncludes,preprocessorChoices:/^\s*#\s*(if|ifdef|ifndef)\b/m.test(sketch),
        graphScope:'Source order and explicit references. Conditions, branch reachability and callback timing need code review.'},
      blueprint:{nodes:g.nodes.map(({id,label,category})=>({id,label,category,sourceLine:recipe.indexed.locations.nodes[id]?.line||null})),edges:g.edges,routines:g.routines}});
    characters+=sketch.length;if(selected.length>=3)break;
  }
  const classes=new Set(design.blocks.map(b=>b.catalogId));
  for(const recipe of selected) for(const name of catalog.recipes.find(r=>r.path===recipe.path)?.classNames||[]) classes.add('class:'+name);
  return {revision:prepared.revision,sourceFingerprint:prepared.sourceFingerprint,instructions:preparationInstructions,styleGuide:prepared.styleGuide,
    recipes:selected,additionalRecipePaths:matches.filter(match=>!selected.some(recipe=>recipe.path===match.path)).map(({path,reasons})=>({path,reasons})),apis:prepared.apis.filter(a=>classes.has(a.id)),
    scope:'Prepared source excerpts and structural recipe blueprints. Verify unresolved details in the current repository; this is not a compilation or hardware test.'};
}

export function preparationStatus(prepared,context) {
  return {revision:prepared.revision,cacheHit:prepared.cacheHit,builtAt:prepared.builtAt,recipeCount:prepared.recipes.length,
    selectedRecipes:context.recipes.length,apiCount:context.apis.length,elapsedMs:prepared.elapsedMs};
}
