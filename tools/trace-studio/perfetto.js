const origin = 'https://ui.perfetto.dev';

// Source and origin are both verified. The timeout removes every listener/timer.
export function waitForPerfetto(target, timeoutMs = 30000) {
    return new Promise((resolve, reject) => {
        const cleanup = () => {
            clearInterval(ping); clearTimeout(timeout); window.removeEventListener('message', ready);
        };
        const ready = event => {
            if (event.source === target && event.origin === origin && event.data === 'PONG') {
                cleanup(); resolve();
            }
        };
        const ping = setInterval(() => target.postMessage('PING', origin), 200);
        const timeout = setTimeout(() => {
            cleanup(); reject(new Error('Perfetto did not respond. Check your connection, or download the JSON and open it in Perfetto.'));
        }, timeoutMs);
        window.addEventListener('message', ready);
        target.postMessage('PING', origin);
    });
}

export async function sendTrace(target, blob, name, range = null, isCurrent = () => true) {
    // The stable UI supports buffers; streamed postMessage input is newer.
    // Bound the extra browser copy and let larger recordings open as local files.
    if (blob.size > 256 * 1024 * 1024) throw new Error('For recordings over 256 MB, open the original file directly in Perfetto.');
    const buffer = await blob.arrayBuffer();
    await waitForPerfetto(target);
    if (!isCurrent()) return;
    target.postMessage({ perfetto: { buffer, title: name,
        fileName: name, localOnly: true, keepApiOpen: true } }, origin, [buffer]);
    if (range) {
        // Perfetto expects seconds in the trace's coordinate system. It retries
        // range requests while the newly posted trace is still loading.
        const padding = Math.max(1000, (range.endUs - range.startUs) * .1);
        target.postMessage({ perfetto: { timeStart: Math.max(0, range.startUs - padding) / 1e6,
            timeEnd: (range.endUs + padding) / 1e6, viewPercentage: 1 } }, origin);
    }
}

export const perfettoUrl = origin + '/#!/?mode=embedded';
