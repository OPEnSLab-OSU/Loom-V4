// Exercise the real Logger macros and scope guard; only the output sinks are faked.
#ifndef LOOM_ENABLE_TRACE
#define LOOM_ENABLE_TRACE 1
#endif
#include <cassert>
#include <cstring>
#include <vector>
#if defined(_MSC_VER) && !defined(__clang__)
#define __attribute__(attributes)
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif
#include "Logger.h"
#if defined(_MSC_VER) && !defined(__clang__)
#undef __attribute__
#endif

class Loom_Trace {
  public:
    bool recording = true;
    bool available = true;
};
struct Entry {
    const char *file;
    const char *name;
    int line;
    const void *object;
    unsigned depth;
};
static std::vector<Entry> entries;
static unsigned exits = 0;
static unsigned summaryEntries = 0;
static unsigned summaryExits = 0;
static unsigned saves = 0;

bool Logger::shouldLogSummaries() { return debugOutputEnabled && summaryWriter != nullptr; }
void Logger::enableSummaries() { summaryWriter = &FunctionInstrumentor::writeSummary; }
void Logger::enableTrace(Loom_Trace &recorder) {
    // Only the recorder/output sinks are fake; scope capture below uses production code.
    static const LoomTraceCallbacks callbacks = {
        [](Loom_Trace *r, const char *file, const char *name, uint32_t line, const void *object) {
            if (!r->recording || !r->available) return false;
            entries.push_back({file, name, static_cast<int>(line), object,
                               Logger::getInstance()->stackDepth});
            return true;
        },
        [](Loom_Trace *) { ++exits; },
        [](Loom_Trace *r) {
            if (!r->recording || !r->available) return false;
            ++saves;
            return true;
        },
        [](Loom_Trace *r, bool available) { r->available = available; },
        [](Loom_Trace *, const char *, const void *, uint32_t, const void *, int, int, bool) {},
        [](Loom_Trace *, const void *) {}
    };
    trace = &recorder;
    traceCallbacks = &callbacks;
    traceAutoSave = false;
}
void FunctionInstrumentor::writeSummary(Logger *, bool starting, const char *, const char *, int) {
    if (starting) {
        ++summaryEntries;
    } else {
        ++summaryExits;
    }
}
#include "Diagnostics/Loom_FunctionTrace.cpp"
static_assert(sizeof(FunctionInstrumentor) == sizeof(void *), "Scope guard must not grow");

void legacySyntax() { FUNCTION_START; }
void emptySyntax() { FUNCTION_START(); }
void nullSyntax() { FUNCTION_START(nullptr); }
void instrumentSyntax() { INSTRUMENT(); }
struct Device {
    void run(bool earlyReturn) {
        FUNCTION_START(this);
        {
            FUNCTION_START(this, "Read channel");
            if (earlyReturn) {
                return;
            }
        }
        FUNCTION_END;
    }
    void compatibilitySyntax() {
        FUNCTION_START_OBJECT(this);
        {
            TRACE_FUNCTION_START_OBJECT(this);
        }
    }
};

int main() {
    Device device;
    // Neither output enabled: no events, even though the guard still balances depth.
    legacySyntax();
    device.run(true);
    assert(entries.empty() && exits == 0 && summaryEntries == 0 && summaryExits == 0);

    Loom_Trace recorder;
    Logger::getInstance()->enableTrace(recorder);
    legacySyntax();
    emptySyntax();
    nullSyntax();
    instrumentSyntax();
    device.run(true);
    device.run(false);
    device.compatibilitySyntax();
    assert(summaryEntries == 0 && summaryExits == 0); // Trace never enables summaries.
    assert(entries.size() == 10 && exits == 10);
    for (unsigned i = 0; i < 4; ++i) {
        assert(entries[i].object == nullptr && entries[i].depth == 1);
    }
    for (unsigned i = 4; i < 10; ++i) {
        assert(entries[i].object == &device);
        assert(entries[i].depth == (i % 2 == 0 ? 1u : 2u));
        assert(strstr(entries[i].file, "test_function_start.cpp") != nullptr && entries[i].line > 0);
    }
    assert(strcmp(entries[5].name, "Read channel") == 0);
    assert(strcmp(entries[7].name, "Read channel") == 0);

    Logger::getInstance()->enableSummaries();
    legacySyntax();
    device.run(true);
    assert(summaryEntries == 3 && summaryExits == 3);
    assert(entries.size() == 13 && exits == 13); // Both sinks share one balanced scope guard.

    Logger *logger = Logger::getInstance();
    logger->setDebugOutput(false);
    logger->setTraceAutoSave(true);
    device.run(true);
    device.run(false);
    assert(entries.size() == 17 && exits == 17); // Quiet text still captures calls.
    assert(summaryEntries == 3 && summaryExits == 3); // Quiet text suppresses summaries.
    assert(saves == 2); // One save per outer call, not per nested scope; includes early return.

    recorder.available = false;
    device.run(true);
    assert(saves == 2 && entries.size() == 17); // No writes or recorded scopes while SD is off.
    recorder.available = true;
    legacySyntax();
    assert(saves == 3 && entries.back().depth == 1); // Wake resumes balanced capture.

    recorder.recording = false;
    {
        FUNCTION_START; // An enclosing scope that began before capture was active.
        recorder.recording = true;
        legacySyntax();
        assert(saves == 4 && entries.back().depth == 2); // Recorded depth controls saves.
    }
    legacySyntax();
    assert(saves == 5 && entries.back().depth == 1);

    logger->enableTrace(recorder); // Existing manual API keeps explicit save boundaries.
    device.run(true);
    assert(saves == 5 && entries.size() == exits);
}
