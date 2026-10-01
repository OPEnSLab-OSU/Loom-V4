const formatters = new Map();
export function zoneOffset(us, zone) {
    return new Intl.DateTimeFormat('en', { timeZone: zone, timeZoneName: 'longOffset' })
        .formatToParts(new Date(us / 1000)).find(part => part.type === 'timeZoneName').value.replace('GMT', 'UTC');
}
export function dateLabel(us, zone = 'UTC', short = false) {
    const date = new Date(us / 1000);
    if (zone === 'UTC') return short ? date.toISOString().slice(11, 23) : date.toISOString().replace('T', ' ').replace('Z', ' UTC');
    const key = zone + ':' + short;
    if (!formatters.has(key)) formatters.set(key, new Intl.DateTimeFormat('en-CA', { timeZone: zone, ...(short ? {} : { year: 'numeric', month: '2-digit', day: '2-digit' }),
        hour: '2-digit', minute: '2-digit', second: '2-digit', fractionalSecondDigits: 3,
        hourCycle: 'h23', ...(short ? {} : { timeZoneName: 'short' }) }));
    return formatters.get(key).format(date);
}

export function traceDisplayClock(report, wall, mode, zone) {
    const wallMode = mode !== 'active' && !!wall;
    const timeZone = mode === 'local' ? zone : 'UTC';
    const row = index => report.history[Math.max(0, Math.min(report.history.length - 1, index))];
    const at = index => wallMode ? wall.rows[row(index).index].utcUs : row(index).timeUs;
    return { wall: wallMode, mode: wallMode ? mode : 'active', zone: timeZone, at,
        label: wallMode ? (mode === 'local' ? 'Local wall time · ' + zone : 'UTC wall time') + ' (estimated)' : 'Awake execution (standby excluded)',
        format: us => wallMode ? dateLabel(us, timeZone) : (us / 1e6).toLocaleString(undefined, { maximumFractionDigits: 6 }) + ' s awake',
        tick: us => wallMode ? dateLabel(us, timeZone, true) : (us / 1e6).toLocaleString(undefined, { maximumFractionDigits: 6 }) + ' s',
    };
}

export function clockTicks(min, max) {
    const step = [.000001, .000002, .000005, .00001, .00002, .00005, .0001, .0002, .0005, .001, .002, .005, .01, .02, .05,
        .1, .2, .5, 1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 900, 1800, 3600, 7200, 10800, 21600, 43200, 86400].find(value => value >= (max - min) / 6) || 86400;
    const ticks = [];
    for (let i = Math.ceil(min / step); i * step <= max && ticks.length < 20; i++) ticks.push(i * step);
    return ticks;
}
