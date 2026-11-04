// Feasibility comes from source review, never browser keyword guesses.
export const assessmentLabels = {
  supported: 'Supported', 'needs-info': 'Needs information', partial: 'Partly feasible', unsupported: 'Not feasible'
};
const severity = {supported:0,'needs-info':1,partial:2,unsupported:3};
export function compatibilityForNode(assembly, id) {
  return (assembly?.featureAssessments || []).filter(review => review.requestId === id || review.nodeIds.includes(id))
    .sort((a,b) => severity[b.status] - severity[a.status])[0] || null;
}
export function compatibilityConcerns(assembly) {
  return (assembly?.featureAssessments || []).filter(review => review.status !== 'supported');
}
