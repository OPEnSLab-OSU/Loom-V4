// Exercise the browser protocol without a network or a browser engine.
const assert = require('node:assert/strict');
const fs = require('node:fs');
(async () => {
    const moduleText = fs.readFileSync(require('node:path').join(__dirname, '../tools/trace-studio/perfetto.js'), 'utf8');
    const { sendTrace, waitForPerfetto, scrollTrace } = await import('data:text/javascript;base64,' + Buffer.from(moduleText).toString('base64'));
    const listeners = new Set(), posts = [];
    global.window = { addEventListener: (type, handler) => listeners.add(handler), removeEventListener: (type, handler) => listeners.delete(handler) };
    const emit = event => [...listeners].forEach(handler => handler(event));
    const target = { postMessage(data, origin, transfer) {
        posts.push({ data, origin, transfer });
        if (data === 'PING') emit({ source: target, origin: 'https://ui.perfetto.dev', data: 'PONG' });
    } };
    await sendTrace(target, new Blob(['{"traceEvents":[]}']), 'test.json', { startUs: 1000000, endUs: 2000000 });
    assert.equal(posts[1].origin, 'https://ui.perfetto.dev');
    assert.equal(posts[1].data.perfetto.keepApiOpen, true);
    assert.equal(posts[1].data.perfetto.localOnly, true);
    assert.equal(posts[1].transfer[0], posts[1].data.perfetto.buffer);
    assert.deepEqual(posts[2].data.perfetto, { timeStart: .9, timeEnd: 2.1, viewPercentage: 1 });
    assert.equal(listeners.size, 0);
    posts.length = 0;
    assert.equal(await scrollTrace(target, { startUs: 1000000, endUs: 2000000 }), true);
    assert.deepEqual(posts[1].data.perfetto, { timeStart: .9, timeEnd: 2.1, viewPercentage: 1 });
    assert.equal(posts.length, 2); // PING + range only: preserve the loaded trace and its workspace.
    posts.length = 0;
    assert.equal(await scrollTrace(target, { startUs: 1000000, endUs: 2000000 }, () => false), false);
    assert.deepEqual(posts.map(post => post.data), ['PING']);
    posts.length = 0;
    await sendTrace(target, new Blob(['x']), 'stale.json', null, () => false);
    assert.deepEqual(posts.map(post => post.data), ['PING']);
    const hostile = { postMessage() {
        emit({ source: target, origin: 'https://ui.perfetto.dev', data: 'PONG' });
        emit({ source: hostile, origin: 'https://untrusted.example', data: 'PONG' });
    } };
    await assert.rejects(waitForPerfetto(hostile, 5), /did not respond/);
    assert.equal(listeners.size, 0);
    await assert.rejects(sendTrace(target, { size: 257 * 1024 * 1024 }, 'large.json'), /over 256 MB/);
    console.log('PASS Perfetto readiness, origin/source validation, range units, stale-load cancellation, transfer and size limit');
})().catch(error => { console.error(error); process.exitCode = 1; });
