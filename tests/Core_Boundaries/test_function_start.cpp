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

class Loom_Trace {};
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

bool Logger::shouldLogSummaries() { return summaryWriter != nullptr; }
void Logger::enableSummaries() { summaryWriter = &FunctionInstrumentor::writeSummary; }
void Logger::enableTrace(Loom_Trace &recorder) { trace = &recorder; }
void FunctionInstrumentor::writeSummary(Logger *, bool starting, const char *, const char *, int) {
    if (starting) {
        ++summaryEntries;
    } else {
        ++summaryExits;
    }
}
void FunctionInstrumentor::beginTrace(Logger *logger, const char *file, const char *func, int line,
                                     const void *object) {
    if (logger->trace != nullptr) {
        trace = logger->trace;
        entries.push_back({file, func, line, object, logger->stackDepth});
    }
}
void FunctionInstrumentor::endTrace() {
    if (trace != nullptr) {
        ++exits;
    }
}
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
}
