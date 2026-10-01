/* Classic worker: keep parsing and lifetime reconstruction away from the controls. */
importScripts('/trace_converter.js');
self.onmessage = event => {
    const { id, content } = event.data;
    try {
        self.postMessage({ id, result: LoomTrace.convert(content) });
    } catch (error) {
        self.postMessage({ id, error: error.message });
    }
};
