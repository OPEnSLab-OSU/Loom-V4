// Apply before CSS paints. A local preference never changes project contents.
(() => {
  const key = 'feature-studio-theme';
  const root = document.documentElement;
  let theme = 'dark';
  try { const saved = localStorage.getItem(key); if (saved === 'light' || saved === 'dark') theme = saved; } catch {}
  function apply() {
    root.dataset.theme = theme;
    const button = document.getElementById('theme-toggle');
    if (!button) return;
    button.setAttribute('aria-pressed', String(theme === 'dark'));
    button.dataset.hintTitle = theme === 'dark' ? 'Deep blue dark mode' : 'Light mode';
    button.dataset.hint = 'Switch to ' + (theme === 'dark' ? 'light' : 'dark') + ' mode. Your choice stays on this computer.';
    document.querySelector('meta[name="theme-color"]')?.setAttribute('content', theme === 'dark' ? '#101e32' : '#edf3df');
  }
  apply();
  document.addEventListener('DOMContentLoaded', () => {
    apply();
    document.getElementById('theme-toggle')?.addEventListener('click', () => {
      theme = theme === 'dark' ? 'light' : 'dark';
      try { localStorage.setItem(key, theme); } catch {}
      apply();
    });
  });
  window.addEventListener('storage', event => {
    if (event.key === key && (event.newValue === 'light' || event.newValue === 'dark')) { theme = event.newValue; apply(); }
  });
})();
