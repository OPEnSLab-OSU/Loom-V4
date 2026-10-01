// Keep Perfetto navigation tied to the selected boundary rather than an earlier comparison call.
export function selectionRange(row, call) {
    if (!row) return null;
    const withinCall = call && row.index >= call.startIndex && row.index <= call.endIndex;
    return withinCall ? { startUs: call.startUs, endUs: call.endUs } :
        { startUs: row.timeUs, endUs: row.timeUs };
}
