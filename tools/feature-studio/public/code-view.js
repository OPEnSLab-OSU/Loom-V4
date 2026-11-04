// A small, dependency-free lexer. All output is text nodes, including strings
// containing HTML. Coloring never rewrites or formats the application itself.
const keywords = new Set('alignas auto bool break case catch char class const constexpr continue default delete do double else enum explicit extern false float for friend if inline int long namespace new nullptr operator private protected public register return short signed sizeof static struct switch template this throw true try typedef typename union unsigned using virtual void volatile while'.split(' '));
const tokenPattern = /\/\/[^\n]*|\/\*[\s\S]*?(?:\*\/|(?![\s\S]))|R"([^\s()\\]{0,16})\([\s\S]*?\)\1"|"(?:\\[\s\S]|[^"\\])*"|'(?:\\[\s\S]|[^'\\])*'|^\s*#\s*\w+|\b(?:0x[\da-fA-F]+|\d+(?:\.\d+)?(?:[eE][+-]?\d+)?[uUlLfF]*)\b|\b[A-Za-z_]\w*\b|[{}()[\];,.]|(?:->|::|==|!=|<=|>=|&&|\|\||[+*/%=&!<>?:~-])/gm;

export function codeTokens(source, filename = '') {
  if (!/\.(ino|h|hpp|cpp|c|json|example)$/.test(filename)) return [{ text: source, kind: '' }];
  const tokens = []; let previous = 0;
  for (const m of source.matchAll(tokenPattern)) {
    if (m.index > previous) tokens.push({ text: source.slice(previous, m.index), kind: '' });
    const text = m[0]; let kind = '';
    if (text.startsWith('//') || text.startsWith('/*')) kind = 'comment';
    else if (/^(R"|["'])/.test(text)) kind = 'string';
    else if (/^\s*#/.test(text)) kind = 'directive';
    else if (/^\d/.test(text)) kind = 'number';
    else if (keywords.has(text)) kind = 'keyword';
    else if (/^(Loom_\w+|Manager|Logger|String|SDManager|uint\w+|size_t)$/.test(text)) kind = 'type';
    else if (/^[A-Z_][A-Z_\d]+$/.test(text)) kind = 'constant';
    else if (/^[A-Za-z_]\w*$/.test(text) && /^\s*\(/.test(source.slice(m.index + text.length))) kind = 'function';
    else if (/^[^\w\s]+$/.test(text)) kind = 'punctuation';
    tokens.push({ text, kind }); previous = m.index + text.length;
  }
  if (previous < source.length) tokens.push({ text: source.slice(previous), kind: '' });
  return tokens;
}

const rendered = new WeakMap();
export function flowMatchesAtLine(assembly, sources, filename, line) {
  if(!assembly||!sources||!Number.isInteger(line)||line<1)return [];
  const rank={mapped:0,matched:1,candidate:2,context:3},matches=[];
  function add(kind,id,entry) {
    if(!entry)return;
    const references=entry.applicationReferences||[entry.reference||entry];
    const reference=references.filter(r=>r&&(r.applicationFile||r.scope==='application'&&r.file)===filename&&
      Number.isInteger(r.line)&&r.line<=line&&line<=(r.endLine||r.line)).sort((a,b)=>(rank[a.match]??4)-(rank[b.match]??4)||(a.endLine-a.line)-(b.endLine-b.line))[0];
    if(reference)matches.push({kind,id,reference});
  }
  for(const node of assembly.nodes)add('node',node.id,sources.nodes?.[node.id]);
  assembly.edges.forEach((edge,index)=>add('edge',index,sources.edges?.[index]));
  return matches.sort((a,b)=>(rank[a.reference.match]??4)-(rank[b.reference.match]??4)||(a.kind==='node'?0:1)-(b.kind==='node'?0:1));
}
export function findCodeMatches(source, query, limit = 1000) {
  if(!query)return {matches:[],truncated:false};
  const pattern=new RegExp(query.replace(/[.*+?^${}()|[\]\\]/g,'\\$&'),'gi'),matches=[];
  let line=1,lineStart=0,nextLine=source.indexOf('\n');
  for(const match of source.matchAll(pattern)) {
    if(matches.length>=limit)return {matches,truncated:true};
    while(nextLine>=0&&nextLine<match.index){line++;lineStart=nextLine+1;nextLine=source.indexOf('\n',lineStart);}
    matches.push({start:match.index,end:match.index+match[0].length,line,column:match.index-lineStart+1});
  }
  return {matches,truncated:false};
}
export function renderCode(container, source, filename, matches = []) {
  const previous = rendered.get(container);
  const sameFile=previous?.source===source&&previous.filename===filename,searchKey=matches.map(m=>m.start+':'+m.end).join(',');
  if (sameFile && previous.searchKey===searchKey) return;
  const tokens=sameFile?previous.tokens:codeTokens(source,filename);
  const fragment = document.createDocumentFragment();
  let lineNumber=1,offset=0,matchIndex=0;
  let line = document.createElement('span'); line.className = 'code-line'; line.dataset.line=String(lineNumber); fragment.append(line);
  function appendText(text,kind) {
    let cursor=0;
    while(cursor<text.length) {
      while(matches[matchIndex]?.end<=offset)matchIndex++;
      const match=matches[matchIndex],inside=match&&match.start<=offset&&offset<match.end;
      const size=Math.min(text.length-cursor,match?(inside?match.end-offset:match.start-offset):text.length-cursor);
      const value=text.slice(cursor,cursor+size);
      let content=document.createTextNode(value);
      if(inside){const mark=document.createElement('mark');mark.dataset.matchIndex=String(matchIndex);mark.append(content);content=mark;}
      if(kind){const span=document.createElement('span');span.className='syntax-'+kind;span.append(content);line.append(span);}
      else line.append(content);
      cursor+=size;offset+=size;
    }
  }
  for (const token of tokens) {
    const parts = token.text.split('\n');
    parts.forEach((text, i) => {
      if (i) {
        line.append(document.createTextNode('\n'));
        offset++;
        line = document.createElement('span'); line.className = 'code-line'; line.dataset.line=String(++lineNumber); fragment.append(line);
      }
      appendText(text,token.kind);
    });
  }
  container.replaceChildren(fragment);
  rendered.set(container, { source, filename, tokens, searchKey });
}
