#!/usr/bin/env node
'use strict';
const fs = require('node:fs');
const path = require('node:path');
const trace = require('./trace_converter.js');
const input = process.argv[2];
if (!input) {
    console.error('Usage: node trace_to_perfetto.js <trace_N.ndjson> [output-base]');
    process.exitCode = 1;
} else {
    try {
        const output = process.argv[3] || path.join(path.dirname(input), path.parse(input).name);
        const converted = trace.convert(fs.readFileSync(input, 'utf8'));
        fs.writeFileSync(output + '.perfetto.json', JSON.stringify(converted.perfetto));
        fs.writeFileSync(output + '.memory.json', JSON.stringify(converted.report, null, 2));
        console.log(`Saved ${output}.perfetto.json and ${output}.memory.json`);
        for (const warning of converted.report.warnings) console.log('Capture note: ' + warning);
    } catch (error) {
        console.error(error.message); process.exitCode = 1;
    }
}
