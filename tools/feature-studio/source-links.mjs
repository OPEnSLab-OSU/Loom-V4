import path from 'node:path';
import {readFile,realpath,readdir} from 'node:fs/promises';
import {stripComments,maskStrings} from './adapters/cpp.mjs';
import {reconstructExample} from './examples.mjs';

const cache=new WeakMap();
const flowCache=new WeakMap();
const escape=s=>s.replace(/[.*+?^${}()|[\]\\]/g,'\\$&');
const lineAt=(s,i)=>s.slice(0,i).split('\n').length;
const words=s=>new Set(s.replace(/([a-z])([A-Z])/g,'$1 $2').toLowerCase().split(/[^a-z0-9]+/).filter(w=>w.length>2).map(w=>w.replace(/(?:ing|s|e)$/,'')));
function closing(code,start,open='{',close='}') {
  let depth=1,i=start+1;for(;i<code.length&&depth;i++){if(code[i]===open)depth++;if(code[i]===close)depth--;}
  return i;
}
function functions(source) {
  const code=maskStrings(stripComments(source)),found=[];
  for(const m of code.matchAll(/\b(?:(\w+)::)?(\w+)\s*\(/g)) {
    if(['if','for','while','switch','catch','sizeof','decltype'].includes(m[2]))continue;
    const after=closing(code,m.index+m[0].lastIndexOf('('),'(',')');
    const tail=code.slice(after).match(/^\s*(?:const\s*)?(?:noexcept\s*)?(?:override\s*)?(?:\:[^;{]*?)?\{/);
    if(!tail)continue;
    const body=after+tail[0].lastIndexOf('{'),end=closing(code,body);
    found.push({className:m[1]||'',method:m[2],start:m.index,body,end,line:lineAt(source,m.index),endLine:lineAt(source,end),code:code.slice(body,end)});
  }
  return found;
}
async function repositoryFile(root,file) {
  if(typeof file!=='string'||! /^(?:src|examples)\/[\w./ -]+\.(?:ino|h|hpp|cpp|c)$/.test(file)||file.split('/').includes('..')||/secret|credential|creds/i.test(file))throw new Error('Source reference must use a code file in the selected checkout.');
  const base=await realpath(root),resolved=await realpath(path.join(base,file)),relative=path.relative(base,resolved);
  if(relative.startsWith('..')||path.isAbsolute(relative))throw new Error('Source reference is outside the selected checkout.');
  return {path:resolved,source:await readFile(resolved,'utf8')};
}
async function libraryIndex(ctx) {
  const result={functions:[],members:new Map(),bases:new Map()};
  async function walk(folder) {
    for(const entry of await readdir(folder,{withFileTypes:true})) {
      if(entry.isDirectory())await walk(path.join(folder,entry.name));
      else if(entry.isFile()&&/\.(?:h|hpp|cpp|c)$/.test(entry.name)&&!/secret|credential|creds/i.test(entry.name)) {
        const file=path.relative(ctx.root,path.join(folder,entry.name)).split(path.sep).join('/');
        const loaded=await repositoryFile(ctx.root,file);
        for(const f of functions(loaded.source))if(f.className)result.functions.push({...f,file,source:loaded.source});
      }
    }
  }
  await walk(path.join(ctx.root,'src'));
  const classes=ctx.catalog.blocks.filter(b=>b.className);
  for(const c of classes) {
    const {source}=await repositoryFile(ctx.root,c.source),code=maskStrings(stripComments(source)),members=new Map();
    for(const other of classes)for(const m of code.matchAll(new RegExp('\\b'+escape(other.className)+'(?:\\s*[*&]\\s*|\\s+)(\\w+)\\b','g')))members.set(m[1],other.className);
    result.members.set(c.className,members);
    result.bases.set(c.className,classes.filter(other=>new RegExp('\\b'+escape(other.className)+'\\b').test(c.bases||'')).map(other=>other.className));
  }
  return result;
}
async function buildIndex(ctx) {
  const files=ctx.assembly.files.filter(f=>/\.(ino|h|hpp|cpp)$/.test(f.name)).map(f=>({...f,functions:functions(f.content)}));
  const objects=new Map(),declarations=new Map();
  for(const f of files) {
    f.indexed=reconstructExample(f.content,f.name,ctx.catalog);
    for(const n of f.indexed.assembly.nodes) {
      const m=n.reason.match(/^Source declaration: (\w+) (\w+)\(/);
      if(m){objects.set(m[2],m[1]);declarations.set(m[2],{file:f.name,line:f.indexed.locations.nodes[n.id]?.line});}
    }
  }
  return {files,objects,declarations,library:await libraryIndex(ctx)};
}
function cachedIndex(ctx) {
  const key=ctx.root+':'+ctx.catalog.fingerprint,previous=cache.get(ctx.assembly);
  if(previous?.key===key)return previous.pending;
  const pending=buildIndex(ctx);cache.set(ctx.assembly,{key,pending});pending.catch(()=>cache.delete(ctx.assembly));return pending;
}
function target(ctx,request) {
  if(request.nodeId) {
    const node=ctx.assembly.nodes.find(n=>n.id===request.nodeId);if(!node)throw new Error('Unknown graph block.');
    return {kind:'node',id:node.id,nodes:[node],text:node.label+' '+node.reason,routine:ctx.assembly.routines?.find(r=>r.nodeIds.includes(node.id))};
  }
  if(request.edgeIndex!==undefined) {
    const e=ctx.assembly.edges[request.edgeIndex];if(!Number.isInteger(request.edgeIndex)||!e)throw new Error('Unknown graph connection.');
    const nodes=[e.from,e.to].map(id=>ctx.assembly.nodes.find(n=>n.id===id));
    return {kind:'edge',id:String(request.edgeIndex),nodes,text:e.label,edge:e,routine:ctx.assembly.routines?.find(r=>r.nodeIds.includes(e.to))};
  }
  if(request.routineId) {
    const r=ctx.assembly.routines?.find(r=>r.id===request.routineId);if(!r)throw new Error('Unknown routine.');
    return {kind:'routine',id:r.id,nodes:[],text:r.name+' '+r.reason,routine:r};
  }
  throw new Error('Choose a block, connection or routine.');
}
function ref(scope,file,source,start,end,explanation,match='matched') {
  const line=lineAt(source,start),endLine=Math.min(lineAt(source,end),line+59);
  const excerpt=source.split('\n').slice(line-1,endLine).join('\n');
  const symbol=maskStrings(stripComments(excerpt)).match(/\b\w+(?:::|\.|->)\w+\s*(?=\()/)?.[0].trim();
  return {scope,file,line,endLine,explanation,match,excerpt,symbol:match==='context'?'':symbol||''};
}
function appMatches(index,t) {
  const result=[];
  for(const f of index.files) {
    if(!t.routine)for(const n of f.indexed.assembly.nodes) {
      const m=n.reason.match(/^Source declaration: (\w+) (\w+)\(/);if(!m)continue;
      if(!new RegExp('\\b'+escape(m[1].replace(/^Loom_/,''))+'\\b','i').test(t.nodes[0]?.label||t.text))continue;
      const line=f.indexed.locations.nodes[n.id]?.line;
      if(line){const start=f.content.split('\n').slice(0,line-1).join('\n').length+(line>1?1:0),end=f.content.indexOf(';',start);result.push(ref('application',f.name,f.content,start,end<0?start+1:end+1,'Sketch declaration supporting this feature.'));}
    }
    const code=maskStrings(stripComments(f.content));
    const scope=t.routine&&f.functions.find(fn=>!fn.className&&fn.method===t.routine.name);
    const start=scope?.body??0,end=scope?.end??code.length;
    const text=t.text.toLowerCase(),tokens=words(t.text),candidates=[];
    for(const m of code.slice(start,end).matchAll(/[^;{}\n]+[;{}]?/g)) {
      const statement=m[0].trim();if(!statement)continue;
      const calls=[...statement.matchAll(/\b(?:(\w+)\s*(?:\.|->)\s*)?(\w+)\s*\(/g)];
      let score=0;
      for(const c of calls) {
        if(c[1]&&new RegExp('\\b'+escape(c[1])+'\\s*(?:\\.|->)\\s*'+escape(c[2])+'\\b','i').test(t.text))score+=30;
        else if(c[2].length>2&&new RegExp('\\b'+escape(c[2])+'\\b','i').test(t.text)&&!['if','for','while','static_cast'].includes(c[2]))score+=12;
      }
      const identifiers=[...new Set(statement.match(/\b[A-Za-z_]\w*\b/g)||[])];
      for(const id of identifiers)if(id.length>3&&!['const','uint32_t','false','true','void','return','unsigned'].includes(id)&&new RegExp('\\b'+escape(id)+'\\b','i').test(t.text))score+=5;
      const shared=[...words(statement)].filter(word=>tokens.has(word));score+=shared.length;
      if(/^\s*(?:constexpr|const|static\s+const)\b/.test(statement)&&shared.length>=2)score+=6;
      if(score>=6)candidates.push({score,start:start+m.index+m[0].search(/\S/),end:start+m.index+m[0].length});
    }
    candidates.sort((a,b)=>b.score-a.score||a.start-b.start);
    if(candidates.length) {
      const best=candidates[0],ties=candidates.filter(c=>c.score===best.score);
      const inferred=ties.length>1||best.score<25;
      for(const c of ties.slice(0,3))result.push(ref('application',f.name,f.content,c.start,c.end,inferred?'Suggested code location inferred from symbol mentions; confirm against the explanation.':'Sketch call matching this explanation.',inferred?'candidate':'matched'));
    } else if(scope)result.push(ref('application',f.name,f.content,scope.start,scope.end,'Routine context; an exact operation could not be matched.','context'));
  }
  return result;
}
function routineMatches(index,t) {
  const definitions=index.files.flatMap(file=>file.functions.filter(fn=>!fn.className&&fn.method===t.routine.name).map(fn=>({file,fn})));
  return definitions.map(({file,fn})=>({...ref('application',file.name,file.content,file.content.lastIndexOf('\n',fn.start-1)+1,fn.end,
    definitions.length===1?'Function definition of '+t.routine.name+'(). Review its conditions and callback/call sites separately.':'Multiple definitions of '+t.routine.name+'() were found. Review compile-time conditions and overloads.',definitions.length===1?'matched':'candidate'),symbol:t.routine.name}));
}
function implementations(index,text,appRefs) {
  const code=appRefs.filter(r=>r.match!=='context').map(r=>r.excerpt).join('\n'),calls=[];
  for(const m of maskStrings(stripComments(code+'\n'+text)).matchAll(/\b(\w+)\s*(?:\.|->)\s*(\w+)/g)) {
    const className=index.objects.get(m[1]);if(className)calls.push({className,method:m[2]});
  }
  for(const [variable,className] of index.objects) {
    if(new RegExp('\\b'+escape(className)+'\\b|\\b'+escape(className.replace(/^Loom_/,''))+'\\b','i').test(text)) {
      for(const call of [...calls])calls.push({className,method:call.method});
      if(!calls.some(c=>c.className===className))calls.push({className,method:className});
    }
  }
  const refs=[],seen=new Set();
  function add(className,method,depth,via) {
    if(refs.length>=10)return;
    let matches=index.library.functions.filter(f=>f.className===className&&f.method===method);
    if(!matches.length)for(const base of index.library.bases.get(className)||[])matches.push(...index.library.functions.filter(f=>f.className===base&&f.method===method));
    for(const f of matches) {
      const key=f.file+':'+f.line;if(seen.has(key))continue;seen.add(key);
      refs.push(ref('repository',f.file,f.source,f.start,f.end,via?'Called by '+via+'; inspect this implementation for the delegated behavior.':'Implementation of '+className+'::'+method+'().'));
      if(depth>=1)continue;
      const members=index.library.members.get(className)||new Map();
      for(const m of f.code.matchAll(/\b(?:(\w+)\s*(::|\.|->)\s*)?(\w+)\s*\(/g)) {
        const owner=m[1]?(m[2]==='::'?m[1]:members.get(m[1])||index.objects.get(m[1])):index.library.members.has(m[3])?m[3]:className;
        if(owner)add(owner,m[3],depth+1,className+'::'+method+'()');
      }
    }
  }
  for(const c of calls)add(c.className,c.method,0,'');
  return refs;
}
export async function sourceLinks(ctx,request) {
  if(!ctx.assembly)throw new Error('Assemble the application or load a recipe first.');
  const t=target(ctx,request);
  const index=await cachedIndex(ctx),links=[],issues=[];
  for(const mapped of ctx.assembly.sourceLinks||[])if(mapped.targetKind===t.kind&&mapped.targetId===t.id) {
    try {
      const source=mapped.scope==='repository'?(await repositoryFile(ctx.root,mapped.file)).source:ctx.assembly.files.find(f=>f.name===mapped.file)?.content;
      if(source===undefined)throw new Error('Unknown application source file.');
      const offset=source.indexOf(mapped.anchor);
      if(offset<0||source.indexOf(mapped.anchor,offset+1)>=0)throw new Error('Source anchor must match one unique code region.');
      const code=maskStrings(stripComments(source));if(!code.slice(offset,offset+mapped.anchor.length).trim())throw new Error('Source anchor contains no executable code or declaration.');
      links.push(ref(mapped.scope,mapped.file,source,offset,offset+mapped.anchor.length,mapped.explanation,'mapped'));
    } catch(e){issues.push(mapped.file+': '+e.message);}
  }
  if(!links.some(r=>r.scope==='application')) {
    const routines=t.kind==='routine'?routineMatches(index,t):[];
    const matches=routines.length?routines:appMatches(index,t);
    links.push(...matches);
    if(t.kind==='edge')for(const node of [...t.nodes].reverse())links.push(...appMatches(index,{...t,kind:'node',text:node.label+' '+node.reason,routine:ctx.assembly.routines?.find(r=>r.nodeIds.includes(node.id))}));
  }
  links.push(...implementations(index,t.text+' '+t.nodes.map(n=>n.label+' '+n.reason).join(' '),links));
  const hasCall=links.some(r=>r.scope==='application'&&r.match!=='context');
  const unique=links.filter((r,i)=>(r.match!=='context'||!hasCall)&&links.findIndex(o=>o.scope===r.scope&&o.file===r.file&&o.line===r.line&&o.endLine===r.endLine)===i);
  unique.sort((a,b)=>(a.scope==='application'?0:10)-(b.scope==='application'?0:10)||['mapped','matched','candidate','context'].indexOf(a.match)-['mapped','matched','candidate','context'].indexOf(b.match));
  if(ctx.assemblyOrigin==='example'&&ctx.origin?.path)for(const r of unique)if(r.scope==='application'){r.applicationFile=r.file;r.scope='repository';r.file=ctx.origin.path;}
  return {links:unique,issues};
}

export async function flowSources(ctx) {
  if(!ctx.assembly)return {nodes:{},edges:[]};
  const key=ctx.root+':'+ctx.catalog.fingerprint+':'+ctx.assemblyOrigin+':'+ctx.origin?.path,previous=flowCache.get(ctx.assembly);
  if(previous?.key===key)return previous.pending;
  const pending=(async()=>{
    const index=await cachedIndex(ctx),nodes={},edges=[];
    const sketch=r=>r.scope==='application'||r.applicationFile;
    const applicationReferences=links=>links.filter(sketch).map(({scope,file,applicationFile,line,endLine,match})=>({scope,file,applicationFile,line,endLine,match}));
    const best=links=>links.find(r=>sketch(r)&&['mapped','matched'].includes(r.match))||links.find(r=>!sketch(r)&&['mapped','matched'].includes(r.match))||links.find(sketch)||links[0];
    await Promise.all(ctx.assembly.nodes.map(async node=>{
      const result=await sourceLinks(ctx,{nodeId:node.id});
      let code=result.links.filter(r=>sketch(r)&&['mapped','matched'].includes(r.match)).map(r=>r.excerpt).join('\n');
      if(!code&&/^Source (?:call|declaration)/.test(node.reason))code=node.reason;
      code=maskStrings(stripComments(code));
      const owners=[];
      for(const [object,className]of index.objects){
        const call=new RegExp('\\b'+escape(object)+'\\s*(?:\\.|->)\\s*(\\w+)\\s*\\(').exec(code);
        const declaration=new RegExp('\\b'+escape(className)+'\\s+[*&]?\\s*'+escape(object)+'\\s*\\(').test(code);
        if(call||declaration)owners.push({object,className,method:call?.[1]||null,...index.declarations.get(object),catalogId:ctx.catalog.blocks.find(b=>b.className===className)?.id});
      }
      const primary=best(result.links);
      nodes[node.id]={owners,reference:primary?{...primary,excerpt:undefined,linkIndex:result.links.indexOf(primary)}:null,applicationReferences:applicationReferences(result.links)};
    }));
    await Promise.all(ctx.assembly.edges.map(async(_,edgeIndex)=>{
      const result=await sourceLinks(ctx,{edgeIndex}),primary=best(result.links);
      edges[edgeIndex]=primary?{...primary,excerpt:undefined,linkIndex:result.links.indexOf(primary),applicationReferences:applicationReferences(result.links)}:null;
    }));
    return {nodes,edges};
  })();
  flowCache.set(ctx.assembly,{key,pending});pending.catch(()=>flowCache.delete(ctx.assembly));return pending;
}
