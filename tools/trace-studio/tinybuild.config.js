export default {
    bundler: {
        entryPoints: ['index.js'], outfile: 'dist/index', bundleBrowser: true,
        bundleESM: false, bundleTypes: false, bundleNode: false, minify: true,
        sourcemap: true
    },
    server: {
        protocol: 'http', host: '127.0.0.1', port: 8080, startpage: 'index.html',
        hotreload: 5000, socket_protocol: 'ws', watch: ['../trace'],
        routes: { '/trace_converter.js': '../trace/trace_converter.js' }
    }
};
