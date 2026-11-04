import {test} from 'node:test';
import assert from 'node:assert/strict';
import {compilerDiagnostics} from '../compiler-diagnostics.mjs';

test('compiler messages retain drive letters, Unicode, columns, severity and exact sketch destinations',()=>{
  const sketch='C:\\Build space\\温度\\Example',files=[{name:'Example.ino',content:'// example\nvoid setup() {}\nvoid loop() {}\n'},{name:'settings.h',content:'#define SETTING 1\n'}];
  const log='\x1b[31mC:\\Build space\\温度\\Example\\Example.ino:3:7: error: missing thing <value>\x1b[0m\r\n'+
    'c:/build space/温度/example/settings.h:1: warning: a setting\n'+
    'C:/libraries/Example.ino:3: note: another file with the same name\n';
  const result=compilerDiagnostics(log,{sketch,files});
  assert.equal(result.truncated,false);assert.equal(result.items.length,3);
  assert.deepEqual(result.items[0],{file:'C:/Build space/温度/Example/Example.ino',line:3,column:7,severity:'error',message:'missing thing <value>',applicationFile:'Example.ino'});
  assert.equal(result.items[1].applicationFile,'settings.h');assert.equal(result.items[1].severity,'warning');
  assert.equal(result.items[2].applicationFile,null,'a matching basename outside the build is not a sketch reference');
});

test('generated and out-of-range lines stay readable without invented sketch links',()=>{
  const text='/tmp/App/App.ino.cpp:99:4: error: generated file\n/tmp/App-else/App.ino:1: error: sibling directory\n/tmp/App/App.ino:25: error: beyond source\n./settings.h:1:2: fatal error: Missing.h: No such file or directory\n';
  const {items}=compilerDiagnostics(text,{sketch:'/tmp/App',files:[{name:'App.ino',lineCount:3},{name:'settings.h',lineCount:2}]});
  assert.deepEqual(items.map(item=>item.applicationFile),[null,null,null,'settings.h']);
  assert.equal(items[3].severity,'error');assert.match(items[3].message,/Missing.h/);
});

test('repeated messages are deduplicated, unrelated output ignored, and broad logs bounded',()=>{
  const message='App.ino:2:3: error: expected a semicolon';
  assert.equal(compilerDiagnostics(message+'\n'+message+'\nSketch uses 1000 bytes\nApp.ino:0: error: bad line\n').items.length,1);
  const log=Array.from({length:105},(_,index)=>'App.ino:'+(index+1)+': warning: item '+index).join('\n');
  const result=compilerDiagnostics(log);assert.equal(result.items.length,100);assert.equal(result.truncated,true);
  assert.deepEqual(compilerDiagnostics('No compiler messages here'),{items:[],truncated:false});
});
