/* Shared by the offline viewer and Node. No dependencies or network requests. */
(function (root) {
    'use strict';
    const zero = '0x0';
    const kinds = new Set(['B', 'E', 'A', 'F', 'R', 'N', 'Z', 'T', 'I', 'C', 'O', 'U', 'D', 'V', 'lost']);
    function address(value) {
        if (typeof value !== 'string' || !/^0x[0-9a-f]+$/i.test(value)) {
            throw new Error('Invalid memory address');
        }
        return '0x' + BigInt(value).toString(16);
    }
    function integer(value, name, minimum = 0) {
        if (!Number.isSafeInteger(value) || value < minimum) {
            throw new Error('Invalid ' + name);
        }
        return value;
    }
    function shortName(signature) {
        const match = signature.match(/([\w:~]+)\s*\(/);
        return match ? match[1] : signature;
    }
    function parse(content) {
        const lines = content.replace(/^\uFEFF/, '').split(/\r?\n/);
        const nonempty = lines.map((line, i) => ({ line, i })).filter(x => x.line.trim());
        const warnings = [];
        const records = [];
        for (const item of nonempty) {
            let row;
            try {
                row = JSON.parse(item.line);
            } catch (error) {
                if (item === nonempty[nonempty.length - 1] && !content.endsWith('\n')) {
                    warnings.push('Unfinished final SD record was ignored. The tail is incomplete.');
                    break;
                }
                throw new Error(`Invalid JSON on line ${item.i + 1}`);
            }
            if (!row || typeof row !== 'object' || Array.isArray(row)) {
                throw new Error(`Invalid record on line ${item.i + 1}`);
            }
            records.push(row);
        }
        const session = records.shift();
        if (!session || session.kind !== 'session' || session.v !== 1 ||
            session.clock !== 'active_us' || typeof session.heap_hooks !== 'boolean') {
            throw new Error('Expected one Loom trace v1 session header');
        }
        if (session.import_warnings !== undefined) {
            if (!Array.isArray(session.import_warnings) ||
                !session.import_warnings.every(message => typeof message === 'string')) {
                throw new Error('Invalid saved capture warnings');
            }
            warnings.push(...session.import_warnings);
            delete session.import_warnings;
        }
        let previous = 0;
        for (const row of records) {
            if (!kinds.has(row.kind)) {
                throw new Error('Unknown event kind or a second boot session in this file');
            }
            integer(row.ts, 'timestamp');
            if (row.ts < previous) {
                throw new Error('Timestamps moved backwards; do not combine different boots');
            }
            previous = row.ts;
            if (row.kind === 'lost') {
                integer(row.count, 'dropped-event count', 1);
                continue;
            }
            row.addr = address(row.addr);
            row.old = address(row.old);
            integer(row.size, 'size');
            integer(row.line, 'line / caller address');
            integer(row.gap, 'stack-to-heap gap', -2147483648);
            if (row.kind === 'V') integer(row.gap * 4294967296 + row.size, 'diagnostic value', -Number.MAX_SAFE_INTEGER);
            if (typeof row.name !== 'string' || typeof row.file !== 'string') {
                throw new Error('Invalid event label');
            }
        }
        return { session, records, warnings };
    }

    // Read the MCU's direct Chrome file back into the same lifetime engine. Generic Chrome
    // exports can be opened in Perfetto but lack Loom allocator/object records for this ledger.
    function fromChrome(content) {
        const document = JSON.parse(content);
        if (!document || !Array.isArray(document.traceEvents)) throw new Error('Expected Chrome traceEvents');
        const saved = document.metadata?.loomTrace;
        if (saved) {
            if (!saved.session || !Array.isArray(saved.records)) throw new Error('Invalid saved Loom records');
            // Enriched desktop exports keep the raw ledger for repeat inspection. Perfetto
            // ignores this metadata; the ordinary parser still validates every restored record.
            return [{ ...saved.session, import_warnings: saved.warnings || [] }, ...saved.records]
                .map(row => JSON.stringify(row)).join('\n') + '\n';
        }
        const native = document.traceEvents;
        if (!native.some(event => event.args && event.args['Record kind'])) {
            throw new Error('This is a general Chrome trace. Open it in Perfetto; detailed heap inspection requires a Loom SD trace.');
        }
        const heap = native.some(event => event.name?.startsWith('Heap hooks enabled:'));
        const rows = [{ kind: 'session', v: 1, clock: 'active_us', heap_hooks: heap }];
        const hex = value => '0x' + integer(value, 'allocator bytes').toString(16);
        for (const event of native) {
            if (event.ph === 'M' || event.ph === 'C') continue;
            const args = event.args || {};
            let kind = args['Record kind'];
            if (args['Dropped events'] !== undefined) {
                rows.push({ kind: 'lost', ts: event.ts, count: args['Dropped events'] }); continue;
            }
            if (!kind && args['Heap bytes in use'] !== undefined) kind = 'C';
            if (!kind) continue;
            const row = { kind, ts: event.ts, addr: args.Address || zero,
                old: args['Previous address'] || zero, size: 0, line: 0, gap: 0,
                name: ['B', 'I', 'C', 'U', 'T', 'V'].includes(kind) ? event.name || '' : '', file: '' };
            if (kind === 'B' || kind === 'E') {
                row.file = args['Source file'] || ''; row.line = args['Source line'] || 0;
                row.size = args['Heap bytes in use at call boundary'] || 0;
                row.old = hex(args['Reusable free bytes at call boundary'] || 0);
                row.gap = args['Stack-to-heap gap estimate bytes'] || 0;
            } else if (kind === 'U') {
                row.old = args['Owning mux address'] || zero;
                row.line = (args['Mux port (-1 means none)'] ?? -1) + 1;
                row.gap = args['I2C address (-1 means none)'] ?? -1;
                row.file = args.State || ''; row.size = args['Known container bytes (0 means unknown)'] || 0;
            } else if (kind === 'V') {
                const value = integer(args.Value, 'diagnostic value', -Number.MAX_SAFE_INTEGER);
                row.gap = Math.floor(value / 4294967296); row.size = value - row.gap * 4294967296;
                row.file = args.Unit || '';
            } else if (kind === 'C') {
                row.size = args['Heap bytes in use']; row.line = args['Reusable free bytes'];
                row.addr = hex(args['Free chunks']); row.old = hex(args['Top free chunk bytes']);
                row.gap = args['Stack-to-heap gap estimate bytes'];
            } else if (kind === 'O') row.size = event.dur;
            else if (kind === 'T') row.size = args['Container bytes (not added to heap)'] || 0;
            else if (kind !== 'I' && kind !== 'D') {
                row.size = args['Requested bytes (free size resolved offline)'] || 0;
                row.line = Number(BigInt(args['Caller instruction address'] || zero));
            }
            rows.push(row);
        }
        return rows.map(row => JSON.stringify(row)).join('\n') + '\n';
    }

    function convert(content) {
        if (/"traceEvents"\s*:/.test(content.slice(0, 1024))) content = fromChrome(content);
        const { session, records, warnings } = parse(content);
        const events = [];
        const calls = [];
        const stack = [];
        const allocations = [];
        const live = new Map();
        const labels = new Map();
        const checkpoints = [];
        const objects = [];
        const activeObjects = new Map();
        const history = [];
        const losses = [];
        let bytes = 0, peak = 0, failures = 0, segment = 0, uncertain = false;
        let overheadUs = 0;
        const firstTs = records.length ? records[0].ts : 0;
        const lastTs = records.length ? records[records.length - 1].ts : firstTs;
        const ts = row => row.ts - firstTs;
        let currentRecordIndex = 0;
        function event(ph, name, time, args = {}, tid = 1, extra = {}) {
            name = name.replace(/Stress /g, 'Sleep ');
            // Chrome treats every numeric counter argument as another plotted series.
            events.push({ ph, name, ts: time, pid: 1, tid, cat: 'Loom', loomEventIndex: currentRecordIndex,
                args: ph === 'C' ? args : { ...args, 'Loom event index': currentRecordIndex }, ...extra });
        }
        function instant(name, time, args = {}, tid = 3) {
            event('I', name, time, args, tid, { s: 't' });
        }
        function counters(time) {
            if (!session.heap_hooks) return;
            event('C', 'Captured heap allocations (requested bytes)', time,
                { 'Live bytes': bytes, 'Live blocks': live.size }, 2);
        }
        function finishCall(call, time, endIndex, status, row = null) {
            call.endUs = time;
            call.endIndex = endIndex;
            call.status = status;
            call.liveBytesAtExit = bytes;
            call.liveBlocksAtExit = live.size;
            call.heapUsedAtExit = row ? row.size : null;
            call.heapFreeAtExit = row ? Number(BigInt(row.old)) : null;
            call.gapAtExit = row ? row.gap : null;
            call.heapChange = row ? row.size - call.heapUsedAtEntry : null;
            call.allocatedDuringCallIds = allocations.filter(block => block.startIndex >= call.startIndex && block.startIndex <= endIndex).map(block => block.id);
            call.stillLiveAtExitIds = [...live.values()].filter(block => block.startIndex >= call.startIndex).map(block => block.id);
            const args = {
                'Source file': call.file, 'Source line': call.line,
                'Full C++ signature': call.signature, 'Object address': call.object,
                'Object label': call.objectLabel,
                'Live captured bytes on entry': call.liveBytesAtEntry,
                'Live captured bytes on exit': bytes,
                'Heap bytes in use on entry': call.heapUsedAtEntry,
                'Heap bytes in use on exit': call.heapUsedAtExit,
                'Heap change (bytes)': call.heapChange,
                'Free heap bytes on entry': call.heapFreeAtEntry,
                'Free heap bytes on exit': call.heapFreeAtExit,
                'Stack-to-heap gap bytes on entry': call.gapAtEntry,
                'Stack-to-heap gap bytes on exit': call.gapAtExit,
                'Call boundary': status,
                'Allocation coverage': uncertain ? 'Incomplete after missing/ambiguous events' :
                    'Only allocations created after capture began'
            };
            args.Function = call.name; args['Call ID'] = call.id;
            args['Call entry event index'] = call.startIndex;
            args['Call exit event index'] = call.endIndex;
            event('X', (status === 'returned' ? '' : '[incomplete] ') + call.displayName,
                call.startUs, args, 1, { dur: Math.max(0, time - call.startUs) });
        }
        function retireObject(pointer, time, index, reason) {
            const object = activeObjects.get(pointer);
            if (!object) return;
            object.endIndex = index; object.endUs = time; object.endReason = reason;
            activeObjects.delete(pointer);
        }
        function endBlock(block, time, index, reason) {
            block.endUs = time;
            block.endIndex = index;
            block.endReason = reason;
            block.endStack = stack.map(frame => ({ id: frame.id, name: frame.name,
                signature: frame.signature, file: frame.file, line: frame.line,
                object: frame.object, objectLabel: frame.objectLabel }));
            bytes -= block.size;
            live.delete(block.address);
            labels.delete(block.address);
        }
        function createBlock(row, time, index) {
            if (row.addr === zero) return;
            if (live.has(row.addr)) {
                uncertain = true;
                warnings.push(`Address ${row.addr} was reused without a recorded free.`);
                endBlock(live.get(row.addr), time, index, 'Missing free / address reused');
                retireObject(row.addr, time, index, 'Address reused without captured deletion');
            }
            const objectFrame = [...stack].reverse().find(frame => frame.object !== zero);
            const block = {
                id: allocations.length + 1, address: row.addr, size: row.size,
                startUs: time, startIndex: index, endUs: null, endIndex: null,
                endReason: null, segment, label: labels.get(row.addr) || 'Unlabelled heap block',
                owner: objectFrame ? objectFrame.objectLabel : 'No labelled object in traced stack',
                ownerAddress: objectFrame ? objectFrame.object : zero,
                stack: stack.map(frame => frame.name),
                stackFrames: stack.map(frame => ({ id: frame.id, name: frame.name,
                    signature: frame.signature, file: frame.file, line: frame.line,
                    object: frame.object, objectLabel: frame.objectLabel })),
                labelHistory: [{ index, name: labels.get(row.addr) || 'Unlabelled heap block' }],
                callerPc: '0x' + row.line.toString(16),
                createdBy: row.kind === 'R' ? 'Reallocation' : 'Allocation'
            };
            allocations.push(block);
            live.set(block.address, block);
            bytes += block.size;
            peak = Math.max(peak, bytes);
            instant('Allocation created', time, {
                'Address': block.address, 'Requested bytes': block.size,
                'Nearest traced call': block.stack[block.stack.length - 1] || 'Outside traced calls',
                'Object context': block.owner, 'Caller instruction address': block.callerPc
            }, 2);
        }
        for (let index = 0; index < records.length; ++index) {
            currentRecordIndex = index;
            const row = records[index], time = ts(row);
            const frame = stack.at(-1), label = labels.get(row.addr) || row.addr;
            let eventName = (row.name || '').replace(/Stress /g, 'Sleep '), category = 'Checkpoints', callId = frame?.id || null;
            if (row.kind === 'E') { eventName = 'Return: ' + (frame?.displayName || 'uncaptured function'); category = 'Calls'; }
            if (['A', 'F', 'R', 'N', 'Z'].includes(row.kind)) {
                category = 'Heap';
                const action = { A: 'Allocate', F: 'Free', R: 'Resize', N: 'Allocation failed', Z: 'Zero-size resize' }[row.kind];
                eventName = action + ': ' + (row.kind === 'F' ? (live.get(row.addr)?.size ?? 'unknown') : row.size) + ' B · ' + label;
            }
            if (row.kind === 'D') { eventName = 'Retire object: ' + label; category = 'Objects'; }
            if (row.kind === 'O') { eventName = 'Save trace to SD · ' + (row.size / 1000).toFixed(3) + ' ms'; category = 'Recorder'; }
            if (row.kind === 'lost') { eventName = row.count + ' events lost · history interrupted'; category = 'Capture quality'; }
            switch (row.kind) {
            case 'B': {
                const name = shortName(row.name);
                const call = {
                    id: calls.length + 1, name, signature: row.name, file: row.file,
                    line: row.line, object: row.addr,
                    objectLabel: labels.get(row.addr) || (row.addr === zero ? 'Not supplied' : row.addr),
                    depth: stack.length, startUs: time, startIndex: index,
                    liveBytesAtEntry: bytes, liveBlocksAtEntry: live.size,
                    heapUsedAtEntry: row.size, heapFreeAtEntry: Number(BigInt(row.old)), gapAtEntry: row.gap
                };
                // Readable aliases for older bench traces; recorded signatures stay intact.
                const displayName = name.replace('waitForStressWake', 'waitForScheduledWake').replace('prepareStressSettings', 'prepareSleepSettings').replace('writeStressSettings', 'writeSleepSettings').replace('printStressRtcTime', 'printSleepRtcTime');
                call.displayName = displayName + (labels.has(row.addr) ? ' · ' + labels.get(row.addr) : '');
                eventName = 'Enter: ' + call.displayName; category = 'Calls'; callId = call.id;
                calls.push(call); stack.push(call);
                event('C', 'Stack-to-heap gap (estimate)', time, { 'Bytes': row.gap }, 2);
                break;
            }
            case 'E':
                if (stack.length) finishCall(stack.pop(), time, index, 'returned', row);
                else instant('Return without captured entry', time);
                event('C', 'Stack-to-heap gap (estimate)', time, { 'Bytes': row.gap }, 2);
                break;
            case 'A': createBlock(row, time, index); break;
            case 'F':
                if (row.addr === zero) break;
                if (live.has(row.addr)) {
                    const block = live.get(row.addr);
                    instant('Allocation freed', time, { 'Address': row.addr,
                        'Requested bytes': block.size, 'Lifetime (us)': time - block.startUs }, 2);
                    endBlock(block, time, index, 'Freed');
                    retireObject(row.addr, time, index, 'Allocation freed');
                } else instant('Free of an uncaptured block', time, { 'Address': row.addr }, 2);
                retireObject(row.addr, time, index, 'Allocation freed (baseline block)');
                labels.delete(row.addr);
                break;
            case 'R':
                if (live.has(row.old)) endBlock(live.get(row.old), time, index, 'Reallocated');
                else if (row.old !== zero) instant('Reallocated a baseline/uncaptured block', time,
                    { 'Previous address': row.old }, 2);
                createBlock(row, time, index);
                break;
            case 'N':
                ++failures;
                instant('Allocation failed', time, { 'Requested bytes': row.size,
                    'Previous block remains live': row.old, 'Caller instruction address':
                        '0x' + row.line.toString(16) }, 2);
                break;
            case 'Z':
                uncertain = true;
                if (live.has(row.old)) endBlock(live.get(row.old), time, index,
                    'Zero-size realloc: outcome unknown');
                if (row.addr !== zero) createBlock(row, time, index);
                warnings.push('Zero-size realloc has allocator-dependent semantics; coverage is uncertain.');
                instant('Zero-size realloc: outcome uncertain', time,
                    { 'Previous address': row.old, 'Returned address': row.addr }, 2);
                break;
            case 'U': {
                retireObject(row.addr, time, index, 'Updated observation');
                const port = row.line - 1, i2cAddress = row.gap;
                const name = row.name + (port >= 0 ? ' · mux port ' + port : '') +
                    (i2cAddress >= 0 ? ' · I2C 0x' + i2cAddress.toString(16) : '');
                const block = live.get(row.addr);
                const object = { id: objects.length + 1, address: row.addr, name,
                    moduleName: row.name, ownerAddress: row.old, port, i2cAddress,
                    containerSize: row.size, state: row.file, startIndex: index,
                    startUs: time, endIndex: null, endUs: null, endReason: null,
                    allocationId: block ? block.id : null,
                    allocationCoverage: block ? 'Allocation observed during capture' :
                        'Object observed; allocation time and storage size may predate capture' };
                objects.push(object); activeObjects.set(row.addr, object); labels.set(row.addr, name);
                eventName = 'Object present: ' + name + ' · ' + row.file; category = 'Objects';
                if (block) { block.label = name; block.labelHistory.push({ index, name }); }
                instant('Object present: ' + name, time, { 'Address': row.addr,
                    'Owning mux address': row.old, 'State': row.file,
                    'Container bytes (not added to heap)': row.size,
                    'Allocation coverage': object.allocationCoverage }, 3);
                break;
            }
            case 'D':
                retireObject(row.addr, time, index, 'Retired before deletion');
                labels.delete(row.addr);
                instant('Object retired before deletion', time, { 'Address': row.addr }, 3);
                break;
            case 'T':
                eventName = 'Label object: ' + row.name; category = 'Objects';
                labels.set(row.addr, row.name);
                if (live.has(row.addr)) {
                    const block = live.get(row.addr); block.label = row.name;
                    block.labelHistory.push({ index, name: row.name });
                }
                instant('Object label: ' + row.name, time,
                    { 'Address': row.addr, 'Container size (not extra heap bytes)': row.size });
                break;
            case 'I': instant(row.name, time); break;
            case 'V': {
                const value = row.gap * 4294967296 + row.size;
                eventName = row.name + ': ' + value.toLocaleString('en-US') + (row.file ? ' ' + row.file : '');
                category = /RTC|sleep|wake|stress/i.test(row.name) ? 'Sleep & RTC' : 'Diagnostics';
                instant(eventName, time, { Value: value, Unit: row.file, 'Object address': row.addr }, category === 'Sleep & RTC' ? 5 : 3);
                break;
            }
            case 'C': {
                const checkpoint = {
                    name: row.name.replace(/Stress /g, 'Sleep '), timeUs: time, index, usedBytes: row.size,
                    freeBytes: row.line, gapBytes: row.gap,
                    freeChunks: Number(BigInt(row.addr)), topFreeBytes: Number(BigInt(row.old)),
                    trackedBytes: bytes, trackedBlocks: live.size,
                    liveAllocationIds: [...live.values()].map(block => block.id),
                    activeObjectIds: [...activeObjects.values()].map(object => object.id),
                    activeCallIds: stack.map(call => call.id),
                    captureIncomplete: uncertain
                };
                checkpoints.push(checkpoint);
                event('C', 'Allocator totals (includes baseline and overhead)', time,
                    { 'Heap bytes in use': row.size, 'Reusable free bytes': row.line,
                        'Free chunks': checkpoint.freeChunks }, 2);
                event('C', 'Stack-to-heap gap (estimate)', time, { 'Bytes': row.gap }, 2);
                event('C', 'Available RAM estimate (no future stack reserve)', time, { Bytes: row.line + Math.max(0, row.gap) }, 2);
                instant('Memory checkpoint: ' + row.name, time, {
                    'Heap bytes in use': row.size, 'Reusable free bytes': row.line,
                    'Live captured requested bytes': bytes,
                    'Coverage': 'Baseline totals are known; baseline block identities are not captured'
                });
                break;
            }
            case 'O':
                overheadUs += row.size;
                event('X', 'Save trace buffer to SD', time, {}, 4, { dur: row.size });
                break;
            case 'lost':
                uncertain = true;
                losses.push({ timeUs: time, index, count: row.count });
                warnings.push(`${row.count} events lost at ${(time / 1000).toFixed(3)} ms; live-block identity is incomplete after this point.`);
                while (stack.length) finishCall(stack.pop(), time, index, 'Events lost; boundary unknown');
                for (const block of [...live.values()]) endBlock(block, time, index,
                    'Events lost: last known live, later lifetime unknown');
                for (const pointer of [...activeObjects.keys()]) retireObject(pointer, time, index,
                    'Events lost: object state unknown until next observation');
                labels.clear();
                ++segment;
                instant('Trace events lost', time, { 'Dropped events': row.count,
                    'Allocation ledger': 'Reset; later events begin a new observed segment' });
                break;
            }
            if (row.kind === 'B' || row.kind === 'E') {
                const checkpoint = { name: row.kind === 'B' ? 'Call entry: ' + shortName(row.name) : 'Function returned',
                    timeUs: time, index, usedBytes: row.size, freeBytes: Number(BigInt(row.old)),
                    gapBytes: row.gap, freeChunks: null, topFreeBytes: null,
                    trackedBytes: bytes, trackedBlocks: live.size,
                    liveAllocationIds: [...live.values()].map(block => block.id),
                    activeObjectIds: [...activeObjects.values()].map(object => object.id),
                    activeCallIds: stack.map(call => call.id), captureIncomplete: uncertain };
                checkpoints.push(checkpoint);
                event('C', 'Allocator totals (includes baseline and overhead)', time,
                    { 'Heap bytes in use': row.size, 'Reusable free bytes': checkpoint.freeBytes }, 2);
                event('C', 'Available RAM estimate (no future stack reserve)', time, { Bytes: checkpoint.freeBytes + Math.max(0, row.gap) }, 2);
            }
            counters(time);
            history.push({ timeUs: time, index, kind: row.kind, name: row.name, eventName, category, callId,
                context: frame?.displayName || '', address: row.addr || '',
                value: row.kind === 'V' ? row.gap * 4294967296 + row.size : null,
                unit: row.kind === 'V' ? row.file : '', bytes, blocks: live.size, incomplete: uncertain });
        }
        while (stack.length) finishCall(stack.pop(), lastTs - firstTs, records.length,
            'Trace ended before return was captured');
        if (!session.heap_hooks) warnings.push('Heap hooks were not linked: individual allocations are unavailable. Allocator checkpoints still show totals.');
        warnings.push('Capture excludes initialization allocations, interrupts, custom allocators, and the recorder’s SD work. Requested sizes exclude allocator metadata.');
        for (const [tid, name] of [[1, 'Nested function calls'], [2, 'Heap and memory'],
            [3, 'Checkpoints and capture quality'], [4, 'Trace recording overhead'], [5, 'Sleep and RTC checks']]) {
            events.push({ ph: 'M', name: 'thread_name', pid: 1, tid, args: { name } });
        }
        events.push({ ph: 'M', name: 'process_name', pid: 1,
            args: { name: 'Loom debug recording (active time)' } });
        events.sort((a, b) => (a.ts || 0) - (b.ts || 0));
        const report = { session, warnings: [...new Set(warnings)], calls, allocations,
            objects, checkpoints, history, losses, durationUs: lastTs - firstTs, peakTrackedBytes: peak,
            allocationFailures: failures, overheadUs };
        return { report, perfetto: { traceEvents: events, displayTimeUnit: 'ms',
            metadata: { source: 'Loom trace v1', clock: 'Active time; excludes standby',
                warnings: report.warnings, loomTrace: { session, records, warnings: report.warnings } } } };
    }

    function snapshot(report, index) {
        const state = report.history.filter(row => row.index <= index).pop();
        const live = report.allocations.filter(block => block.startIndex <= index &&
            (block.endIndex === null || index < block.endIndex)).map(block => ({ ...block,
                label: block.labelHistory?.filter(label => label.index <= index).pop()?.name || block.label }));
        const checkpoint = report.checkpoints.filter(row => row.index <= index).pop() || null;
        const activeObjects = (report.objects || []).filter(object => object.startIndex <= index &&
            (object.endIndex === null || index < object.endIndex));
        const callStack = report.calls.filter(call => call.startIndex <= index && index < call.endIndex)
            .sort((a, b) => a.depth - b.depth);
        return { index, timeUs: state ? state.timeUs : 0, live, activeObjects, callStack,
            liveBytes: live.reduce((total, block) => total + block.size, 0),
            incomplete: state ? state.incomplete : false, checkpoint };
    }
    const api = { parse, convert, snapshot, fromChrome };
    if (typeof module !== 'undefined' && module.exports) module.exports = api;
    else root.LoomTrace = api;
})(typeof globalThis !== 'undefined' ? globalThis : this);
