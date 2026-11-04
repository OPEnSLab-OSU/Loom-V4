const hints = {
  'build-menu': ['2 · Build the reviewed code', 'Compile for the preselected Loomified Feather M0. Setup can get missing Arduino tools and libraries. Nothing runs automatically.'],
  'flash-menu': ['3 · Flash your board', 'Choose the COM port and upload the successful build. Flash does not build automatically.'],
  'new-design': ['New design', 'Open a blank project tab. Your current project stays saved.'],
  'restore-design': ['Return to the previous design', 'Restore the local snapshot made when you started a new design. The current design is saved in turn.'],
  'empty-sensors': ['Pick a sensor', 'Choose the sensor you need; describe logging and timing in the behavior brief. Codex adds the supporting pieces.'],
  'empty-brief': ['Describe the behavior', 'A sensor plus a prompt such as “log to SD every 5 minutes” is enough to start.'],
  handoff: ['Assemble application', 'Ask Codex to review the selected source and generate the application code and flow graph.', 'Choose a model, then assemble'],
  'examples-button': ['Open a recipe', 'Explore the examples in your checkout as editable feature blocks and source graphs.', 'Choose a starting point'],
  'toggle-tray': ['Feature library', 'Browse sensors, storage, radios and custom behaviors. Add the features your application needs.', 'Click a feature or drag it onto the canvas'],
  'close-tray': ['Close Ingredients', 'Tuck the ingredient list away and return to the category dock.'],
  'toggle-brief': ['Tell it what to do', 'Describe the overall behavior in plain language. Codex fills in the supporting pieces.'],
  'close-brief': ['Close the behavior brief', 'Keep your description and return to the canvas.'],
  'requirements-tab': ['Requested features', 'Choose features and describe their settings. Codex adds the required supporting components.'],
  'flow-tab': ['See how it fits together', 'Explore the ordered data flow and supporting dependencies. Scheduling loops stay separate.'],
  arrange: ['Arrange blocks', 'Space the blocks so their labels and connections are easier to read.'],
  clear: ['Clear requested features', 'Clear the ingredient canvas. Use Ctrl+Z straight afterward to bring it back.'],
  'zoom-fit': ['Fit graph to view', 'Fit all blocks and connections in the canvas.', 'Scroll to zoom · Drag the background to pan'],
  'zoom-in': ['Zoom in', 'Make the blocks and their settings easier to read.'],
  'zoom-out': ['Zoom out', 'Pull back to see more of the workflow.'],
  'code-tab': ['Application code', 'Open the files returned by Codex, or the source of the loaded recipe.'],
  'decisions-tab': ['Assembly notes', 'See dependency choices, custom behavior, compatibility questions and validation.'],
  'download-file': ['Keep this file', 'Download the selected application file.'],
  'copy-code': ['Copy current file', 'Copy the original file contents without display line numbers or search highlighting.'],
  'find-code': ['Find text in code', 'Search the current file. Enter goes to the next match; Shift+Enter goes to the previous one. Ctrl+F opens this search while reviewing code.'],
  'download-svg': ['Keep the workflow', 'Save a standalone diagram with the same connections and hierarchy.'],
  'refresh': ['Rescan source', 'Discover the classes and examples in the selected checkout again.'],
  export: ['Export project', 'Export the complete project: feature blocks, assembled flow, generated source files, notes and source references.'],
  'run-assembly': ['Start assembly', 'Start the Codex run with the selected model and permission mode.'],
  'close-inspector': ['Close settings', 'Close these settings without changing your feature.'],
  'add-custom': ['Describe a feature', 'Describe a custom setting, sensor option or behavior that the library does not expose.']
};
const elastic = new WeakMap();
export function finishStretch(target) { elastic.get(target)?.finish(); }
export function stretchControl(target, pressed = false) {
  if (!target || matchMedia('(prefers-reduced-motion: reduce)').matches) return;
  const running = elastic.get(target);
  if (running && !pressed) return;
  const startShape = running ? getComputedStyle(running.skin).transform : 'scale(1)';
  running?.finish();
  const computed = getComputedStyle(target);
  const properties = ['background', 'borderColor', 'boxShadow', 'position', 'isolation'];
  const previous = Object.fromEntries(properties.map(k => [k, target.style[k]]));
  const skin = document.createElement('span');
  skin.className = 'elastic-surface';
  Object.assign(skin.style, {
    position: 'absolute', inset: '-' + (parseFloat(computed.borderTopWidth) || 0) + 'px',
    borderRadius: computed.borderRadius, background: computed.background,
    border: computed.border, boxShadow: computed.boxShadow, zIndex: '-1',
    pointerEvents: 'none', margin: '0', padding: '0', boxSizing: 'border-box',
    minWidth: '0', minHeight: '0', maxWidth: 'none', maxHeight: 'none'
  });
  if (computed.position === 'static') target.style.position = 'relative';
  target.style.isolation = 'isolate'; target.style.background = 'transparent';
  target.style.borderColor = 'transparent'; target.style.boxShadow = 'none';
  target.append(skin);
  const record = {
    skin,
    finish() {
      if (elastic.get(target) !== record) return;
      animation.cancel(); skin.remove();
      for (const k of properties) target.style[k] = previous[k];
      elastic.delete(target);
    }
  };
  const animation = skin.animate([
    { transform: startShape },
    { transform: pressed ? 'scale(1.11,.86)' : 'scale(1.06,.94)', offset: .24 },
    { transform: pressed ? 'scale(1.11,.86)' : 'scale(1.06,.94)', offset: .34 },
    { transform: pressed ? 'scale(.97,1.055)' : 'scale(.98,1.035)', offset: .63 },
    { transform: 'scale(1.015,.99)', offset: .84 },
    { transform: 'scale(1)' }
  ], { duration: pressed ? 920 : 780, easing: 'cubic-bezier(.35,0,.3,1)' });
  elastic.set(target, record); animation.onfinish = () => record.finish();
  const graphic = target.querySelector('.block-icon,.dock-symbol,.example-icon') || target.querySelector('svg');
  if (graphic) {
    const startIcon = getComputedStyle(graphic).transform;
    const lean = (target.getAttribute('aria-label') || target.id || target.textContent).length % 2 ? 1 : -1;
    graphic.getAnimations().forEach(a => a.cancel());
    graphic.style.transformOrigin = '50% 85%';
    graphic.animate([
      { transform: startIcon },
      { transform: 'translateY(1px) rotate(' + (-5 * lean) + 'deg) scale(1.14,.83)', offset: .18 },
      { transform: 'translateY(-3px) rotate(' + (-6 * lean) + 'deg) scale(.9,1.13)', offset: .34 },
      { transform: 'translateY(' + (pressed ? -13 : -9) + 'px) rotate(' + (7 * lean) + 'deg) scale(.99,1.02)', offset: .53 },
      { transform: 'translateY(' + (pressed ? -11 : -7) + 'px) rotate(' + (-4 * lean) + 'deg) scale(1)', offset: .65 },
      { transform: 'translateY(1px) rotate(' + (3 * lean) + 'deg) scale(1.13,.85)', offset: .8 },
      { transform: 'translateY(-2px) rotate(' + (-2 * lean) + 'deg) scale(.98,1.035)', offset: .92 },
      { transform: 'translateY(0) rotate(0) scale(1)' }
    ], { duration: pressed ? 1120 : 1020, easing: 'cubic-bezier(.35,0,.55,1)' });
  }
}

export function installHoverHints({ icon, isDragging }) {
  const tip = document.getElementById('game-tooltip');
  let current, timer;
  document.querySelectorAll('[title]').forEach(e => {
    e.dataset.hint ||= e.getAttribute('title'); e.removeAttribute('title');
  });
  function hide() {
    clearTimeout(timer);
    tip.classList.remove('show');
    if (current) {
      const ids = (current.getAttribute('aria-describedby') || '').split(/\s+/).filter(id => id && id !== tip.id);
      ids.length ? current.setAttribute('aria-describedby', ids.join(' ')) : current.removeAttribute('aria-describedby');
    }
    current = null;
  }
  function show(target) {
    if (!target.isConnected || isDragging()) return;
    const preset = hints[target.id] || [];
    const label = target.dataset.hintTitle || preset[0] || target.getAttribute('aria-label') || target.textContent.trim().replace(/\s+/g, ' ');
    const body = target.dataset.hint || preset[1] || (target.classList.contains('node') ? 'Select to inspect its settings and connections.' : '');
    if (!label) return;
    const dialog = target.closest('dialog[open]');
    (dialog || document.body).append(tip);
    tip.querySelector('.hint-title').textContent = label.slice(0, 90);
    tip.querySelector('.hint-body').textContent = body.slice(0, 300);
    tip.querySelector('.hint-body').hidden = !body;
    const action = target.dataset.hintAction || preset[2] || '';
    tip.querySelector('.hint-action').textContent = action;
    tip.querySelector('.hint-action').hidden = !action;
    const category = target.dataset.hintCategory || 'intent';
    tip.dataset.category=category;
    tip.querySelector('.hint-orb').replaceChildren(icon(category));
    tip.style.setProperty('--hint-color', getComputedStyle(target).getPropertyValue('--block-color').trim() || '#83a25b');
    const r = target.getBoundingClientRect(), t = tip.getBoundingClientRect();
    const below = r.top < t.height + 16;
    const x = Math.max(10, Math.min(innerWidth - t.width - 10, r.left + r.width / 2 - t.width / 2));
    const y = Math.max(10, Math.min(innerHeight - t.height - 10, below ? r.bottom + 13 : r.top - t.height - 13));
    tip.style.left = x + 'px'; tip.style.top = y + 'px';
    tip.style.setProperty('--arrow-left', Math.max(18, Math.min(t.width - 18, r.left + r.width / 2 - x)) + 'px');
    tip.classList.toggle('below', below); tip.classList.add('show');
    const ids = new Set((target.getAttribute('aria-describedby') || '').split(/\s+/).filter(Boolean));
    ids.add(tip.id); target.setAttribute('aria-describedby', [...ids].join(' '));
  }
  function enter(event, keyboard = false) {
    const target = event.target.closest('[data-hint],button,.node,.utility-menu>summary');
    if (!target || current === target || event.pointerType === 'touch' || isDragging()) return;
    hide(); current = target;
    if ((keyboard || !target.contains(event.relatedTarget)) && target.matches('button,.node,summary')) {
      stretchControl(target);
    }
    timer = setTimeout(() => { if (current === target) show(target); }, keyboard ? 80 : 330);
  }
  document.addEventListener('pointerover', e => enter(e));
  document.addEventListener('pointerout', e => { if (current?.contains(e.target) && !current.contains(e.relatedTarget)) hide(); });
  document.addEventListener('focusin', e => enter(e, true));
  document.addEventListener('focusout', e => { if (current?.contains(e.target)) hide(); });
  document.addEventListener('pointerdown', hide, { capture: true });
  document.addEventListener('scroll', hide, { capture: true, passive: true });
  document.addEventListener('close', hide, { capture: true });
  document.addEventListener('keydown', e => { if (e.key === 'Escape') hide(); });
  window.addEventListener('resize', hide);
}
