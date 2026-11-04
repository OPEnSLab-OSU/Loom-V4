// Preserve offsets and string literals while removing C/C++ comments.
export function stripComments(source) {
  let out = '', state = 'code', quote = '';
  for (let i = 0; i < source.length; i++) {
    const c = source[i], next = source[i + 1];
    if (state === 'line') {
      if (c === '\n') { state = 'code'; out += c; } else out += ' ';
    } else if (state === 'block') {
      if (c === '*' && next === '/') { out += '  '; i++; state = 'code'; }
      else out += c === '\n' ? c : ' ';
    } else if (state === 'string') {
      out += c;
      if (c === '\\' && next) { out += next; i++; }
      else if (c === quote) state = 'code';
    } else if (c === '/' && next === '/') { out += '  '; i++; state = 'line'; }
    else if (c === '/' && next === '*') { out += '  '; i++; state = 'block'; }
    else {
      out += c;
      if (c === '"' || c === "'") { state = 'string'; quote = c; }
    }
  }
  return out;
}

// Search symbols without mistaking diagnostic text or string values for code.
// Offsets are retained so arguments and source links still use the original text.
export function maskStrings(source) {
  let quote = '', out = '';
  for (let i = 0; i < source.length; i++) {
    const c = source[i];
    if (quote) {
      out += c === '\n' ? '\n' : ' ';
      if (c === '\\' && i + 1 < source.length) { i++; out += source[i] === '\n' ? '\n' : ' '; }
      else if (c === quote) quote = '';
    } else if (c === '"' || c === "'") { quote = c; out += ' '; }
    else out += c;
  }
  return out;
}
