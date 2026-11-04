import { test } from 'node:test';
import assert from 'node:assert/strict';
import { reconstructExample } from '../examples.mjs';
import { extractHeader } from '../adapters/loom.mjs';
import { codeTokens, findCodeMatches, flowMatchesAtLine } from '../public/code-view.js';

test('source connections retain the actual constructor and operation lines, ignoring comments', () => {
  const blocks = [
    ...extractHeader('class Manager { public: Manager(const char *name, int id); };', 'Loom_Manager.h'),
    ...extractHeader('class Loom_Test { public: Loom_Test(Manager &man); };', 'Internet/Test.h')
  ];
  const catalog = { adapter: { id: 'loom', target: 'Feather M0' }, fingerprint: 'test', blocks };
  const source = '// Manager ignored("x", 9);\n\nManager manager("Device", 1);\nLoom_Test logger(manager);\nvoid setup() {}\nvoid loop() {\n  manager.measure();\n  manager.package();\n  logger.publish();\n  manager.pause(5000);\n}\n';
  const result = reconstructExample(source, 'examples/Test/Test.ino', catalog);
  const { assembly, locations } = result;
  const dependency = assembly.edges.findIndex(e => e.label === 'constructor dependency');
  assert.equal(locations.edges[dependency].line, 4);
  const call = assembly.edges.findIndex(e => e.label === 'called on manager' && assembly.nodes.find(n => n.id === e.to).label === 'Read sensors');
  assert.equal(locations.edges[call].line, 7);
  const cycle = assembly.edges.findIndex(e => e.kind === 'cycle');
  assert.equal(locations.edges[cycle].line, 10);
  const publish = assembly.nodes.find(n => n.label === 'Publish packet');
  assert.equal(locations.nodes[publish.id].line, 9);
  assert.equal(locations.edges.length, assembly.edges.length);
});

test('coloring preserves every source character, multiline comments and literal HTML', () => {
  const source = '#include <Loom_Manager.h>\n/* two\n lines */\nvoid setup() {\n const char *label = "<script>alert(1)</script>";\n auto raw = R"note(first\nsecond)note";\n manager.measure(); // read\n delay(5000);\n}\n';
  const tokens = codeTokens(source, 'FeatureApplication.ino');
  assert.equal(tokens.map(t => t.text).join(''), source);
  assert.ok(tokens.some(t => t.kind === 'comment' && t.text.includes('\n')));
  assert.ok(tokens.some(t => t.kind === 'string' && t.text.includes('<script>')));
  assert.ok(tokens.some(t => t.kind === 'string' && t.text.includes('first\nsecond')));
  assert.ok(tokens.some(t => t.kind === 'keyword' && t.text === 'void'));
  assert.ok(tokens.some(t => t.kind === 'function' && t.text === 'measure'));
  assert.ok(tokens.some(t => t.kind === 'number' && t.text === '5000'));
});

test('file search treats API punctuation literally and retains original Windows and Unicode offsets',()=>{
  const source='// İ prefix\r\nManager manager;\r\n manager.measure(); manager.measure();\r\nconst char *label = "<div>[.*]</div>";';
  const {matches,truncated}=findCodeMatches(source,'MANAGER.MEASURE()');
  assert.equal(truncated,false);assert.equal(matches.length,2);
  assert.deepEqual(matches.map(match=>[match.line,match.column]),[[3,2],[3,21]]);
  for(const match of matches)assert.equal(source.slice(match.start,match.end),'manager.measure()');
  const special=findCodeMatches(source,'[.*]').matches;
  assert.equal(special.length,1);assert.equal(source.slice(special[0].start,special[0].end),'[.*]');
});
test('file search handles empty and absent queries, with a bound on broad searches',()=>{
  assert.deepEqual(findCodeMatches('read read read',''),{matches:[],truncated:false});
  assert.deepEqual(findCodeMatches('read read read','missing'),{matches:[],truncated:false});
  const limited=findCodeMatches('read read read','read',2);
  assert.equal(limited.matches.length,2);assert.equal(limited.truncated,true);
  assert.equal(findCodeMatches('read read','read',2).truncated,false);
});

test('code-to-flow lookup uses the selected file and all linked regions, with exact matches first',()=>{
  const assembly={nodes:[{id:'save'},{id:'context'},{id:'library'},{id:'other-file'}],edges:[{}]};
  const app=(line,endLine=line,match='mapped',file='App.ino')=>({scope:'application',file,line,endLine,match});
  const sources={nodes:{
    save:{reference:{scope:'repository',file:'src/Storage.cpp',line:8,endLine:9,match:'matched'},applicationReferences:[app(6),app(12,14)]},
    context:{applicationReferences:[app(4,20,'context')]},
    library:{reference:{scope:'repository',file:'App.ino',line:12,endLine:14,match:'mapped'}},
    'other-file':{applicationReferences:[app(12,14,'mapped','Helper.cpp')]}
  },edges:[{applicationReferences:[app(12,13,'candidate'),app(12,12)]}]};
  const matches=flowMatchesAtLine(assembly,sources,'App.ino',12);
  assert.deepEqual(matches.map(m=>[m.kind,m.id,m.reference.match]),[['node','save','mapped'],['edge',0,'mapped'],['node','context','context']]);
  assert.deepEqual(flowMatchesAtLine(assembly,sources,'Helper.cpp',12).map(m=>m.id),['other-file']);
  assert.deepEqual(flowMatchesAtLine(assembly,sources,'App.ino',25),[]);
  assert.deepEqual(flowMatchesAtLine(assembly,null,'App.ino',12),[]);
  assert.deepEqual(flowMatchesAtLine(assembly,sources,'App.ino',NaN),[]);
});

test('example aliases point back to the displayed sketch, and approximate references retain their status',()=>{
  const assembly={nodes:[{id:'example'}],edges:[]};
  const reference={scope:'repository',file:'examples/Test/Test.ino',applicationFile:'FeatureApplication.ino',line:6,endLine:8,match:'candidate'};
  const sources={nodes:{example:{reference}},edges:[]};
  assert.equal(flowMatchesAtLine(assembly,sources,'FeatureApplication.ino',7)[0].reference.match,'candidate');
  assert.deepEqual(flowMatchesAtLine(assembly,sources,'Test.ino',7),[]);
});
