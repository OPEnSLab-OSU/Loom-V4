// Exercise the production automatic adapter and Logger startup hook. Only the
// recorder's SD operations and Manager hardware readiness are replaced here.
#include <cassert>
#include <cstring>
#if defined(_MSC_VER) && !defined(__clang__)
#define __attribute__(attributes)
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif
#include "Logger.h"
#if defined(_MSC_VER) && !defined(__clang__)
#undef __attribute__
#endif
#include "Loom_Manager.h"
#include "Diagnostics/Loom_Trace.h"

FakeSerial Serial;
class SDManager {
  public:
    bool ready = true;
    bool writesSucceed = true;
};
static unsigned starts = 0;
static unsigned saves = 0;
static unsigned attachments = 0;
static bool requestedHeap = false;
static uint8_t requestedWindow = 0;
Loom_Trace *Loom_Trace::active = nullptr;
Loom_Trace::~Loom_Trace() { if (active == this) active = nullptr; }
bool Loom_Trace::begin(SDManager &storage, bool hooks, uint8_t window) {
    ++starts;
    requestedHeap = hooks;
    requestedWindow = window;
    if (!storage.ready) return false;
    sd = &storage;
    heapHooks = hooks;
    recording = true;
    active = this;
    return true;
}
bool Loom_Trace::flush() {
    ++saves;
    if (!sd->writesSucceed) {
        recording = false;
        active = nullptr;
        return false;
    }
    return true;
}
void Logger::enableTrace(Loom_Trace &recorder) {
    ++attachments;
    trace = &recorder;
    traceAutoSave = false;
}
bool Logger::flushTrace() { return trace->flush(); }
#include "Diagnostics/Loom_TraceAuto.cpp"

int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "deferred";
    Manager manager, other;
    SDManager sd, otherSD;
    Logger *logger = Logger::getInstance();
    logger->setDebugOutput(false);
    assert(!loomTraceAttach(manager, nullptr, true, 16));
    assert(!loomTraceAttach(manager, &sd, true, 0));
    assert(!loomTraceAttach(manager, &sd, true, 24));
    if (std::strcmp(mode, "manual") == 0) {
        Loom_Trace manual;
        assert(manual.begin(sd, false));
        assert(!loomTraceAttach(manager, &sd, true, 16));
        assert(starts == 1 && saves == 0 && attachments == 0);
        return 0;
    }
    const bool immediate = std::strcmp(mode, "immediate") == 0;
    const bool failed = std::strcmp(mode, "failed") == 0;
    const bool appendFailed = std::strcmp(mode, "append-failed") == 0;
    manager.initialized = immediate;
    sd.ready = !failed;
    sd.writesSucceed = !appendFailed;
    assert(loomTraceAttach(manager, &sd, true, 7));
    assert(starts == (immediate ? 1u : 0u));
    assert(saves == (immediate ? 1u : 0u)); // No SD writes during deferred configuration.
    assert(loomTraceAttach(manager, &sd, true, 7)); // Pending/started duplicate is harmless.
    assert(!loomTraceAttach(other, &otherSD, true, 7));
    assert(!loomTraceAttach(manager, &sd, false, 7)); // Conflicting settings are rejected.
    logger->beginConfiguredTrace(other); // Other managers cannot trigger this session.
    assert(starts == (immediate ? 1u : 0u));
    manager.initialized = true;
    logger->beginConfiguredTrace(manager);
    assert(starts == 1 && requestedHeap && requestedWindow == 7);
    assert(attachments == (failed ? 0u : 1u));
    assert(saves == (failed ? 0u : 1u)); // Startup baseline saved without a later call.
    logger->beginConfiguredTrace(manager);
    assert(starts == 1); // No duplicate sessions or hidden retries after failure.
    assert(loomTraceAttach(manager, &sd, true, 7) == (!failed && !appendFailed));
}
