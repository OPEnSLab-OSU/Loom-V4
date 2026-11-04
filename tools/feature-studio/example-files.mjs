import path from 'node:path';
import {readFile,realpath,lstat} from 'node:fs/promises';

// Copy adjacent implementation files without rewriting includes or flattening
// subfolders. Configuration and credential files need a separate review.
export async function exampleFiles(root,file,indexed,catalog) {
  const folder=path.posix.dirname(file),base=await realpath(root),assembly=indexed.assembly;
  const details=[{source:file,name:assembly.files[0].name,kind:'sketch',included:true,reason:'Main example sketch'}];
  const used=new Set(assembly.files.map(file=>file.name.toLowerCase()));
  for(const source of (catalog.exampleSupportPaths||[]).filter(source=>path.posix.dirname(source)===folder)) {
    const name=path.posix.basename(source),code=/\.(?:h|hpp|cpp|c)$/.test(name);
    const item={source,name,kind:code?'code':/\.json$/.test(name)?'configuration':'reference',included:false,reason:code?'Review separately':'Reference file'};
    details.push(item);
    if(!code){if(item.kind==='configuration')item.reason='Configuration · review separately';continue;}
    if(/secret|credential|creds|password|token|api.?key|config|settings/i.test(name)) {item.reason='Configuration header · review separately';continue;}
    if(!/^[A-Za-z0-9][A-Za-z0-9_.-]{0,100}$/.test(name)||/^(con|nul|aux|prn|com\d|lpt\d)(\.|$)/i.test(name)||used.has(name.toLowerCase())) {item.reason='Output filename needs review';continue;}
    if(assembly.files.length>=40){item.reason='Example exceeds the project file limit';continue;}
    const candidate=path.join(root,source),stat=await lstat(candidate),resolved=await realpath(candidate),relative=path.relative(base,resolved);
    if(stat.isSymbolicLink()||!stat.isFile()||relative.startsWith('..')||path.isAbsolute(relative))throw new Error('Example helper is outside the selected checkout.');
    if(stat.size>500000){item.reason='Helper is too large for the project file limit';continue;}
    const content=await readFile(resolved,'utf8');
    // A generically named helper can still contain local credential settings.
    if(/(?:^\s*#\s*define\s+|\b)(?:\w*(?:password|ssid|secret|token|api_key|credential)\w*)\s*(?:=|\s)\s*"[^"\n]+"/im.test(content)) {item.reason='Contains connection settings · review separately';continue;}
    assembly.files.push({name,content});used.add(name.toLowerCase());item.included=true;item.reason='Adjacent helper source';
  }
  const bundled=details.filter(item=>item.included&&item.kind==='code');
  if(bundled.length)assembly.decisions.push('Included adjacent helper code without changes: '+bundled.map(item=>item.name).join(', ')+'.');
  const separate=details.filter(item=>!item.included&&item.kind!=='reference');
  if(separate.length)assembly.warnings.push('Review these additional example files separately: '+separate.map(item=>item.source+' ('+item.reason+')').join('; ')+'. They are not included in the project export.');
  return details;
}
