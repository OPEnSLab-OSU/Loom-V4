const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
(async () => {
    const source = fs.readFileSync(path.join(__dirname, '../tools/trace-studio/wall-clock.js'), 'utf8');
    const { reconstructWallClock } = await import('data:text/javascript;base64,' + Buffer.from(source).toString('base64'));
    const v = (name, value) => ({ kind: 'V', name, value, timeUs: 9000000 });
    const history = [ {kind:'B',timeUs:0}, {kind:'B',timeUs:4000000}, {kind:'B',timeUs:5000000}, {kind:'E',timeUs:7000000}, {kind:'E',timeUs:8000000},
        v('RTC UTC captured before scheduling (reported after wake)',100000),
        v('RTC UTC captured after module restoration',100060),
        v('Sleep active preparation before wake',1000), v('Sleep active restoration after wake',2000),
        v('RTC UTC after restoration valid',1), v('Sleep RTC wake callback observed',1), v('Sleep standby evidence confirmed',1)
    ].map((row,index)=>({...row,index}));
    const report = {history,calls:[{name:'Loom_Hypnos::sleepImpl',status:'returned',startIndex:1,endIndex:4},
        {name:'Loom_Hypnos::post_sleep',startIndex:2,endIndex:3,startUs:5000000}]};
    const clock = reconstructWallClock(report);
    assert.equal(clock.rows[0].utcUs,99996000000);
    assert.equal(clock.rows[2].utcUs,100058000000); // Actual wake, not the later diagnostic-write timestamp.
    assert.equal(clock.rows[3].utcUs,100060000000); // RTC read after measured restoration.
    assert.equal(clock.rows[6].utcUs,100062000000); // Reporting did not move the anchor two seconds later.
    assert.equal(clock.rows[2].utcUs-clock.rows[1].utcUs,58000000); // 57 seconds standby + one second awake.
    assert.equal(clock.sleepCount,1);
    const bad = structuredClone(report); bad.history[9].value=0;
    assert.equal(reconstructWallClock(bad),null);
    const lost=structuredClone(report); lost.history[3].kind='lost';
    assert.equal(reconstructWallClock(lost),null);
    const heapLost = structuredClone(report); heapLost.history[3].kind='lost'; heapLost.history[3].lossFlags=1;
    assert.ok(reconstructWallClock(heapLost),'Allocation-only gaps do not remove RTC anchors');
    const heapPaused = structuredClone(report); heapPaused.history[3].kind='L'; heapPaused.history[3].lossFlags=1;
    assert.ok(reconstructWallClock(heapPaused),'Intentionally capped allocation windows retain RTC anchors');
    const clockLost=structuredClone(heapLost); clockLost.history[3].lossFlags=8;
    assert.equal(reconstructWallClock(clockLost),null);
    const missing=structuredClone(report); missing.calls.push({name:'Loom_Hypnos::post_sleep',startIndex:12});
    assert.equal(reconstructWallClock(missing),null);
    assert.equal(reconstructWallClock({history:[],calls:[]}),null);
    if(process.argv[2]) {
        const report=require('../tools/trace/trace_converter.js').convert(fs.readFileSync(process.argv[2],'utf8')).report;
        const actual=reconstructWallClock(report);
        assert.ok(actual,'Overnight recording must have reconstructable clocks');
        console.log(JSON.stringify({sleeps:actual.sleepCount,start:new Date(actual.startUtcUs/1000).toISOString(),end:new Date(actual.endUtcUs/1000).toISOString(),
            activeSeconds:report.durationUs/1e6,wallSeconds:(actual.endUtcUs-actual.startUtcUs)/1e6}));
    }
    console.log('PASS RTC wall-clock anchors, measured restoration, sleep gaps and unavailable/lost-clock fallback');
})().catch(error=>{console.error(error);process.exitCode=1;});
