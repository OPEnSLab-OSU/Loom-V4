import readline from 'node:readline';
for await (const line of readline.createInterface({ input: process.stdin })) {
  const message = JSON.parse(line);
  if (message.method === 'initialize') console.log(JSON.stringify({ id: message.id, result: {} }));
  if (message.method === 'model/list') console.log(JSON.stringify({ id: message.id, result: {
    data: [
      { id: 'model-1', model: 'gpt-6.1-sol', displayName: 'GPT-6.1 Sol', supportedReasoningEfforts: [{ reasoningEffort: 'medium' }, { reasoningEffort: 'high' }], defaultReasoningEffort: 'medium' },
      { id: 'model-2', model: 'fixture-fast', displayName: 'Fixture Fast', supportedReasoningEfforts: [{ reasoningEffort: 'low' }], defaultReasoningEffort: 'low' }
    ], nextCursor: null
  } }));
}
