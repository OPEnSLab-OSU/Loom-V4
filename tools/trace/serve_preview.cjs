// Optional localhost preview for browsers that restrict opening local HTML files.
'use strict';
const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');
const files = new Map([
    ['/', ['trace_viewer.html', 'text/html; charset=utf-8']],
    ['/trace_viewer.html', ['trace_viewer.html', 'text/html; charset=utf-8']],
    ['/trace_converter.js', ['trace_converter.js', 'text/javascript; charset=utf-8']]
]);
const server = http.createServer((request, response) => {
    const entry = files.get(request.url);
    if (!entry || request.method !== 'GET') { response.writeHead(404); response.end(); return; }
    response.writeHead(200, { 'Content-Type': entry[1], 'Cache-Control': 'no-store' });
    fs.createReadStream(path.join(__dirname, entry[0])).pipe(response);
});
server.listen(0, '127.0.0.1', () => console.log('Local trace viewer: http://127.0.0.1:' + server.address().port));
