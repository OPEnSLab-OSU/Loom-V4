import {recipeCapabilities,indexRecipe} from './recipe-index.mjs';

// Everything shown by the browser comes from the prepared current-checkout
// reconstruction. This is an index of source, not a compatibility assessment.
export function exampleOverview(recipe,catalog) {
  const indexed=recipe.indexed,summary=indexed.summary,assembly=indexed.assembly;
  const phaseOrder={setup:0,loop:1,callback:2,helper:3};
  const sourceRecipe=catalog.recipes?.find(item=>item.path===recipe.path)||indexRecipe(assembly.files.find(file=>file.name.endsWith('.ino')).content,recipe.path,catalog.blocks);
  const classes=summary.classNames.map(className=>{
    const block=catalog.blocks.find(item=>item.className===className);
    return {className,label:block?.title||className.replace(/^Loom_/,''),category:block?.category||'system'};
  });
  return {...summary,methods:sourceRecipe.methods,capabilities:recipeCapabilities(sourceRecipe),classes,
    routines:[...(assembly.routines||[])].sort((a,b)=>phaseOrder[a.kind]-phaseOrder[b.kind]).map(routine=>({id:routine.id,name:routine.name,kind:routine.kind,steps:routine.nodeIds.length,
      bindings:routine.bindings.map(({kind,label})=>({kind,label}))})),
    files:[recipe.path,...(recipe.companions||[])],
    projectFiles:recipe.projectFiles||[{source:recipe.path,name:assembly.files[0].name,kind:'sketch',included:true,reason:'Main example sketch'}],
    operations:[...new Set((assembly.routines||[]).flatMap(r=>r.nodeIds).map(id=>assembly.nodes.find(node=>node.id===id)?.label).filter(Boolean))]
  };
}
