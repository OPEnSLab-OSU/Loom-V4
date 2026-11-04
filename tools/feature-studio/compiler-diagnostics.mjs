// GCC/Clang-style messages only. Preserve the compiler's wording and avoid
// guessing that similarly named library or generated files are sketch lines.
const cleanPath=value=>value.replaceAll('\\','/').replace(/^"|"$/g,'').replace(/\/\.\//g,'/');
export function compilerDiagnostics(text,{sketch='',files=[]}={}) {
  const items=[],seen=new Set(),windows=/^[A-Za-z]:[\\/]/.test(sketch);
  const normalize=value=>{const result=cleanPath(value);return windows?result.toLowerCase():result;};
  const directory=normalize(sketch).replace(/\/$/,'');
  for(const raw of text.replace(/\x1b\[[0-?]*[ -/]*[@-~]/g,'').split(/\r?\n/)) {
    const match=raw.match(/^\s*(.+?):(\d+)(?::(\d+))?:\s*(fatal error|error|warning|note):\s*(.+)$/i);
    if(!match)continue;
    const [,reportedFile,lineText,columnText,kind,message]=match,line=Number(lineText),column=columnText?Number(columnText):null;
    if(!Number.isSafeInteger(line)||line<1||column!==null&&(!Number.isSafeInteger(column)||column<1))continue;
    const file=cleanPath(reportedFile.trim()),severity=kind.toLowerCase()==='fatal error'?'error':kind.toLowerCase();
    const key=JSON.stringify([file,line,column,severity,message]);if(seen.has(key))continue;seen.add(key);
    if(items.length>=100)return {items,truncated:true};
    const pathname=normalize(file),output=files.find(f=>/^[\w][\w.-]*$/.test(f.name)&&
      (pathname===normalize(f.name)||pathname==='./'+normalize(f.name)||directory&&pathname===directory+'/'+normalize(f.name)));
    const lineCount=output?.lineCount??(typeof output?.content==='string'?output.content.split('\n').length:0);
    items.push({file,line,column,severity,message,applicationFile:output&&line<=lineCount?output.name:null});
  }
  return {items,truncated:false};
}
