// Protocol fixture. Never used by the normal app and never calls an LLM.
import { writeFile } from 'node:fs/promises';
if(process.argv.includes('app-server')) { await import('./fake-models.mjs');process.exit(0); }
let input = '';
for await (const chunk of process.stdin) input += chunk;
const request = JSON.parse(input.split('<feature-studio-request>')[1].split('</feature-studio-request>')[0].trim());
const args = process.argv.slice(2);
const option = name => args[args.indexOf(name) + 1];
const model = args.includes('--model') ? option('--model') : '';
console.log(JSON.stringify({ type: 'thread.started', thread_id: 'fixture-session' }));
console.log(JSON.stringify({ type: 'item.started', item: { type: 'command_execution' } }));
if (model === 'test-wait') await new Promise(resolve => setTimeout(resolve, 30000));
if (model === 'test-delay') await new Promise(resolve => setTimeout(resolve, 500));
if (model === 'test-activity') {
  console.log(JSON.stringify({type:'item.completed',item:{type:'reasoning',text:'INTERNAL_REASONING_SHOULD_NOT_APPEAR'}}));
  console.log(JSON.stringify({type:'item.completed',item:{type:'command_execution',aggregated_output:'RAW_COMMAND_OUTPUT_SHOULD_NOT_APPEAR'}}));
  for(let i=0;i<2;i++)console.log(JSON.stringify({type:'item.started',item:{type:'command_execution'}}));
  console.log(JSON.stringify({type:'item.completed',item:{type:'agent_message',text:'Checking the selected example and the sensor setup.'}}));
  await new Promise(resolve=>setTimeout(resolve,800));
}
const a = {
  schemaVersion: 1, designRevision: request.designRevision,
  sourceFingerprint: model === 'test-stale' ? 'obsolete-source' : request.sourceFingerprint,
  summary: 'Fixture assembly',
  nodes: request.design.blocks.map(b => ({ id: b.id, label: b.label, category: 'sensors', added: false, reason: 'Test request', requestIds: [b.id] })),
  edges: [], files: [{ name: 'FeatureApplication.ino', content: 'void setup() {}\nvoid loop() {}\n' }],
  decisions: [], warnings: [],
  validation: JSON.stringify({ cwd: process.cwd(), args, receivedDescription: request.design.description })
};
await writeFile(option('--output-last-message'), JSON.stringify(a));
console.log(JSON.stringify({ type: 'turn.completed', usage: {} }));
