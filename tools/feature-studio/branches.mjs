import path from 'node:path';
import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
const run = promisify(execFile);
async function git(root, args) {
  const { stdout } = await run('git', ['--no-optional-locks', '-C', root, ...args], { windowsHide: true, timeout: 15000, maxBuffer: 500000 });
  return stdout.trim();
}
export async function readBranches(root) {
  try {
    const top = await git(root, ['rev-parse', '--show-toplevel']);
    const [branch, revision, refs] = await Promise.all([
      git(root, ['branch', '--show-current']), git(root, ['rev-parse', '--short', 'HEAD']),
      git(root, ['for-each-ref', '--format=%(refname:short)', 'refs/heads'])
    ]);
    return { available: true, branch: branch || null, revision, branches: refs.split('\n').filter(Boolean), canSwitch: path.resolve(top).toLowerCase() === path.resolve(root).toLowerCase() };
  } catch { return { available: false, branch: null, revision: null, branches: [], canSwitch: false }; }
}
export async function switchBranch(root, branch) {
  const info = await readBranches(root);
  if (!info.canSwitch) throw new Error('Switch branches from the root of the selected Git checkout.');
  if (typeof branch !== 'string' || !info.branches.includes(branch) || branch.startsWith('-')) throw new Error('Choose an existing local branch.');
  try {
    // Normal Git protection applies. No force, reset, stash, cleanup or fetch.
    await git(root, ['switch', '--', branch]);
  } catch (e) {
    throw new Error('Git kept the current checkout. ' + String(e.stderr || e.message).trim().slice(0, 1800));
  }
  return readBranches(root);
}
