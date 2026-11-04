import { randomUUID } from 'node:crypto';

export function savedFeature(block, existing = []) {
  const requirement = String(block.settings?.requirement || '').trim();
  const notes = String(block.notes || '').trim();
  if (!requirement && !notes) throw new Error('Describe the feature behavior before saving it.');
  if (requirement.length > 20000 || notes.length > 20000) throw new Error('Keep saved feature descriptions under 20,000 characters.');
  const label = block.label.trim();
  const title = (label && label !== 'Describe a feature' ? label : (requirement || notes).split('\n')[0]).slice(0, 100);
  const previous = existing.find(f => f.title === title);
  if (!previous && existing.length >= 100) throw new Error('The Custom category can hold up to 100 saved features.');
  return {
    id: previous?.id || 'custom:' + randomUUID(), title, category: 'custom', kind: 'intent', saved: true,
    description: (requirement || notes).slice(0, 400), defaults: { requirement },
    savedNotes: notes, savedCount: block.count, source: null, constructors: [], methods: [], examples: [], candidates: []
  };
}

export function withSavedFeatures(catalog, features = []) {
  return { ...catalog, categories: { ...catalog.categories, custom: 'Custom' }, blocks: [...catalog.blocks.filter(b => !b.saved), ...features] };
}
