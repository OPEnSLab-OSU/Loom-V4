// RTC diagnostics are emitted AFTER sleep. Anchor their captured values to the
// measured wake/restoration boundary, never to the later diagnostic write time.
export function reconstructWallClock(report) {
    const history = report.history;
    const cycles = [];
    let previousDiagnosticIndex = -1;
    for (const before of history.filter(row => row.kind === 'V' && row.name === 'RTC UTC captured before scheduling (reported after wake)')) {
        const following = history.slice(before.index, before.index + 24);
        const value = name => following.find(row => row.kind === 'V' && row.name === name)?.value;
        const after = value('RTC UTC captured after module restoration');
        const preparation = value('Sleep active preparation before wake');
        const restoration = value('Sleep active restoration after wake');
        const sleep = report.calls.filter(call => call.name === 'Loom_Hypnos::sleepImpl' &&
            call.status === 'returned' && call.endIndex < before.index && call.startIndex > previousDiagnosticIndex).at(-1);
        const wake = sleep && report.calls.find(call => call.name === 'Loom_Hypnos::post_sleep' &&
            call.startIndex > sleep.startIndex && call.endIndex <= sleep.endIndex);
        previousDiagnosticIndex = before.index;
        if (!wake || value('RTC UTC after restoration valid') !== 1 || value('Sleep RTC wake callback observed') !== 1 ||
            value('Sleep standby evidence confirmed') !== 1 || !Number.isSafeInteger(before.value) ||
            !Number.isSafeInteger(after) || after < before.value || !Number.isFinite(preparation) ||
            !Number.isFinite(restoration) || preparation < 0 || restoration < 0 ||
            history.slice(sleep.startIndex, before.index + 1).some(row => row.kind === 'lost')) continue;
        const beforeOffsetUs = before.value * 1e6 - (wake.startUs - preparation * 1000);
        const afterOffsetUs = after * 1e6 - (wake.startUs + restoration * 1000);
        if (afterOffsetUs - beforeOffsetUs < 0) continue;
        cycles.push({ index: wake.startIndex, beforeOffsetUs, afterOffsetUs });
    }
    if (!cycles.length) return null;
    const segments = [{ index: 0, offsetUs: cycles[0].beforeOffsetUs }];
    for (const cycle of cycles) {
        if (cycle.afterOffsetUs < segments.at(-1).offsetUs) return null; // A backwards RTC correction cannot give a monotonic chart.
        segments.push({ index: cycle.index, offsetUs: cycle.afterOffsetUs });
    }
    const knownWakes = new Set(cycles.map(cycle => cycle.index));
    const unknownWakes = report.calls.filter(call => call.name === 'Loom_Hypnos::post_sleep' && !knownWakes.has(call.startIndex));
    // A missing sleep report/lost wake must not masquerade as continuous awake time.
    if (unknownWakes.length || history.some(row => row.kind === 'lost')) return null;
    let segment = 0;
    const rows = history.map(row => {
        while (segment + 1 < segments.length && segments[segment + 1].index <= row.index) segment++;
        return { utcUs: row.timeUs + segments[segment].offsetUs, segment };
    });
    if (rows.some((row, index) => index && row.utcUs < rows[index - 1].utcUs)) return null;
    return { rows, sleepCount: cycles.length, startUtcUs: rows[0].utcUs, endUtcUs: rows.at(-1).utcUs };
}

export const utcLabel = utcUs => new Date(utcUs / 1000).toISOString().replace('T', ' ').replace('Z', ' UTC');
