import {routineConnections} from './execution-wiring.js';

// A navigation index, not a simulator. Only follow recorded edges; never infer
// a next step from the array order when the graph contains explicit branches.
export function stepConnections(graph,nodeId,sources={}) {
  if(!graph?.nodes.some(node=>node.id===nodeId))return [];
  const nodes=new Map(graph.nodes.map(node=>[node.id,node]));
  const connected=routineConnections(graph).filter(edge=>(edge.from===nodeId||edge.to===nodeId)&&nodes.has(edge.from)&&nodes.has(edge.to));
  const byPair=(edge,kind)=>connected.filter(other=>other.kind===kind&&other.from===edge.from&&other.to===edge.to);
  const paired=edge=>byPair(edge,'flow').length===1&&byPair(edge,'data').length===1;
  const link=edge=>{
    const index=graph.edges.indexOf(edge),navigationId=edge.navigationNodeId||edge.to;
    return {kind:edge.kind,label:edge.label,source:index<0?{nodeId:navigationId}:{edgeIndex:index},
      reference:index<0?sources?.nodes?.[navigationId]?.reference||null:sources?.edges?.[index]||edge.sourceReference||null,
      derived:!!edge.derived};
  };
  const groups=[['flow','Step order'],['data','Data'],['callback','Calls & callbacks'],['cycle','Repetition'],['dependency','Supporting connections']].map(([kind,title])=>({kind,title,rows:[]}));
  for(const edge of connected) {
    if(edge.kind==='data'&&paired(edge))continue;
    const group=groups.find(group=>group.kind===edge.kind);if(!group)continue;
    const outgoing=edge.from===nodeId,otherId=outgoing?edge.to:edge.from;
    const routine=graph.routines?.find(routine=>routine.nodeIds.includes(otherId));
    const links=[link(edge)];if(edge.kind==='flow'&&paired(edge))links.push(link(byPair(edge,'data')[0]));
    group.rows.push({otherId,node:nodes.get(otherId),direction:edge.from===edge.to?'self':outgoing?'outgoing':'incoming',links,
      routine:routine?{name:routine.name,kind:routine.kind,step:routine.nodeIds.indexOf(otherId)+1}:null});
  }
  for(const group of groups)group.rows.sort((a,b)=>(a.direction==='incoming'?0:1)-(b.direction==='incoming'?0:1));
  return groups.filter(group=>group.rows.length);
}
