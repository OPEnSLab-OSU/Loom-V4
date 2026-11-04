import {filterProjectItems} from './graph-layout.js';

export function filterExamples(examples,query) {
  const items=examples.map(example=>({example,label:example.title,context:[example.group,example.path].join(' '),
    searchText:[...(example.classNames||[]),...(example.classes||[]).map(item=>item.label),...(example.methods||[]).map(name=>name+'()'),
      ...(example.capabilities||[]),...(example.helperFunctions||[]),...(example.nonLoomIncludes||[]),...(example.operations||[]),
      ...(example.routines||[]).flatMap(r=>[r.name+'()',r.kind]),example.customLogic?'custom logic':''].join(' ')}));
  return filterProjectItems(items,query).map(item=>item.example);
}
