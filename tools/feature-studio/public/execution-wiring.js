// Supply structural wiring for older graphs that only list routine membership.
// Explicit agent-authored flow edges take precedence over this source-order view.
export function routineConnections(graph) {
  const edges = [...graph.edges], ids = new Set(graph.nodes.map(n => n.id));
  const add = (from, to, kind, label, navigationNodeId = to) => {
    if (!ids.has(from) || !ids.has(to) || from === to || edges.some(e => e.from === from && e.to === to && e.kind === kind)) return;
    edges.push({ from, to, kind, label, navigationNodeId, derived: true });
  };
  const routines = graph.routines || [];
  if (!edges.some(e => e.kind === 'flow')) {
    for (const r of routines) for (let i = 1; i < r.nodeIds.length; i++) add(r.nodeIds[i - 1], r.nodeIds[i], 'flow', 'Source order · conditions in code');
    const setup = routines.find(r => r.kind === 'setup'), loop = routines.find(r => r.kind === 'loop');
    if (setup && loop) add(setup.nodeIds.at(-1), loop.nodeIds[0], 'flow', 'Enter loop');
    if (loop?.nodeIds.length > 1) add(loop.nodeIds.at(-1), loop.nodeIds[0], 'cycle', 'Next loop · conditions in code');
  }
  for (const r of routines) for (const b of r.bindings) add(b.nodeId, r.nodeIds[0], 'callback',
    (b.kind === 'registration' ? 'Registers ' : 'Calls ') + r.name + '()', b.nodeId);
  return edges;
}
