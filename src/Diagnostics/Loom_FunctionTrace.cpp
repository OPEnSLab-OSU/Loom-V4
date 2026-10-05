#include "Logger.h"

// Keep scope capture separate from text/RTC output. An opted-in trace must still
// work when setDebugOutput(false) suppresses DEBUG messages and summaries.
void FunctionInstrumentor::beginTrace(Logger *logger, const char *file, const char *func, int line,
                                      const void *object) {
    if (logger->traceCallbacks != nullptr &&
        logger->traceCallbacks->enter(logger->trace, file, func, static_cast<uint32_t>(line), object)) {
        trace = logger->trace;
        ++logger->traceDepth;
    }
}

void FunctionInstrumentor::endTrace() {
    if (trace == nullptr) return;
    Logger *logger = Logger::getInstance();
    logger->traceCallbacks->leave(trace);
    --logger->traceDepth;
    // Save after the outermost recorded call exits, including early returns.
    // Unrecorded enclosing scopes (e.g. initialization) must not defer a save.
    // The recorder rejects storage-off/ISR contexts and excludes its own SD work.
    if (logger->traceAutoSave && logger->traceDepth == 0) {
        logger->traceCallbacks->flush(trace);
    }
}
