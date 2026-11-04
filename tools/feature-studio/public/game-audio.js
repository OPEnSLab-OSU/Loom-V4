// Quiet procedural droplets with a soft attack and a diffuse, filtered tail.
// No assets, network requests or background audio are needed.
export function controlSound(target) {
  if (target.matches('.node-remove,.danger,.close-dialog,.tray-close,[aria-label^="Close"]') || target.matches('#toggle-brief[aria-expanded="true"],#toggle-tray[aria-expanded="true"],#toggle-output[aria-expanded="true"]')) return 'remove';
  if (target.matches('.catalog-card')) return 'add';
  if (target.matches('.example-card')) return 'recipe';
  if (target.matches('.quick-ingredient,#category-list button,#example-filters button')) return 'category';
  if (target.matches('#handoff,#run-assembly')) return 'assemble';
  if (target.matches('#zoom-in,#zoom-out,#zoom-fit')) return 'zoom';
  if (target.matches('.source-button,.wire-label,#open-code,#download-file,#download-svg,#find-code,#copy-code')) return 'source';
  if (target.matches('.file-tabs button,#requirements-tab,#flow-tab,#flow-detail')) return 'tab';
  if (target.matches('#toggle-brief,#toggle-tray,#toggle-output,#code-tab,#decisions-tab,summary')) return 'panel';
  if (target.matches('.node')) return 'node';
  return 'click';
}

export function dropSamples(kind = 'click', sampleRate = 44100, variant = 0) {
  const settings = { click: [255, .3], category: [360, .22], node: [230, .28], panel: [285, .32], tab: [325, .2], zoom: [410, .16], source: [390, .26], recipe: [305, .42], assemble: [350, .45], add: [315, .36], remove: [205, .32], success: [285, .53], preview: [295, .65] };
  const [basePitch, duration] = settings[kind] || settings.click;
  const pitch = basePitch * [1, .965, 1.035, 1.015][variant % 4];
  const samples = new Float32Array(Math.ceil(sampleRate * duration));
  if (kind === 'remove') {
    let phase = 0, fast = 0, slow = 0, seed = 71231;
    for (let i = 0; i < samples.length; i++) {
      const t = i / sampleRate;
      seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0;
      const noise = seed / 2147483648 - 1;
      fast += (noise - fast) * (1 - Math.exp(-2 * Math.PI * 2200 / sampleRate));
      slow += (noise - slow) * (1 - Math.exp(-2 * Math.PI * 320 / sampleRate));
      phase += 2 * Math.PI * (160 + 300 * Math.exp(-t * 22)) / sampleRate;
      const attack = Math.sin(Math.min(1,t/.014)*Math.PI/2)**2;
      const envelope = attack * Math.exp(-t * 15);
      samples[i] = ((fast - slow) * .12 + Math.sin(phase) * .075) * envelope;
    }
    return soften(samples,sampleRate);
  }
  const notes = kind === 'preview' ? [[0, 1], [.18, 1.34], [.35, .88]] : ['success', 'assemble', 'recipe'].includes(kind) ? [[0, 1], [.13, kind === 'recipe' ? 1.19 : 1.5]] : [[0, 1]];
  for (const [start, ratio] of notes) {
    let phase = 0, air = 0, seed = 92033;
    for (let i = Math.ceil(start * sampleRate); i < samples.length; i++) {
      const t = i / sampleRate - start;
      const frequency = pitch * ratio * (.76 + 1.1 * (1 - Math.exp(-t * 35)) + .035 * Math.sin(t * 90) * Math.exp(-t * 14));
      phase += 2 * Math.PI * frequency / sampleRate;
      const envelope = Math.sin(Math.min(1,t/.016)*Math.PI/2)**2 * Math.exp(-t * 12);
      const bubble = .35 * Math.sin(phase) + .06 * Math.sin(phase * .5 + .3) * Math.exp(-t * 20);
      const glass = .004 * Math.sin(2 * Math.PI * pitch * ratio * 4.1 * t) * Math.exp(-t * 30);
      seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0;
      air += (seed / 2147483648 - 1 - air) * (1 - Math.exp(-2 * Math.PI * 2600 / sampleRate));
      samples[i] += (bubble + glass + air * .045) * envelope;
    }
  }
  const dry = samples.slice();
  for (const [seconds, amount] of [[.032, .08], [.067, .065], [.108, .04]]) {
    const delay = Math.floor(seconds * sampleRate);
    for (let i = delay; i < samples.length; i++) samples[i] += dry[i - delay] * amount;
  }
  return soften(samples,sampleRate);
}

function soften(samples,sampleRate) {
  let filtered=0;
  const amount=1-Math.exp(-2*Math.PI*1500/sampleRate),fade=sampleRate*.045;
  for(let i=0;i<samples.length;i++) {
    filtered+=(samples[i]-filtered)*amount;
    samples[i]=filtered*Math.min(1,(samples.length-1-i)/fade);
  }
  return samples;
}
export const SOUND_GAIN=.22;

export class WaterSounds {
  constructor() { this.buffers = new Map(); this.lastPlay = 0; this.variations = new Map(); }
  async play(kind = 'click') {
    const Audio = window.AudioContext || window.webkitAudioContext;
    if (!Audio) throw new Error('This browser does not provide audio playback.');
    this.context ||= new Audio();
    if (this.context.state !== 'running') await this.context.resume();
    if (this.context.state !== 'running') throw new Error('Browser audio is blocked. Enable sound for this page and try again.');
    if (kind !== 'preview' && this.context.currentTime - this.lastPlay < .065) return;
    const variant = this.variations.get(kind) || 0;
    this.variations.set(kind, (variant + 1) % 4);
    const key = kind + ':' + variant;
    let buffer = this.buffers.get(key);
    if (!buffer) {
      const samples = dropSamples(kind, this.context.sampleRate, variant);
      buffer = this.context.createBuffer(1, samples.length, this.context.sampleRate);
      buffer.copyToChannel(samples, 0); this.buffers.set(key, buffer);
    }
    const voice = this.context.createBufferSource(), gain = this.context.createGain();
    voice.buffer = buffer; gain.gain.value = SOUND_GAIN;
    voice.connect(gain); gain.connect(this.context.destination);
    voice.onended = () => { voice.disconnect(); gain.disconnect(); };
    voice.start(); this.lastPlay = this.context.currentTime;
    return { state: this.context.state, duration: buffer.duration };
  }
}
