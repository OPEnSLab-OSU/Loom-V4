#pragma once

// Define these before including this header in any Loom sketch. Existing Wisp
// build flags remain accepted; the generic spelling takes precedence.
#ifndef LOOM_TRACE
#ifdef LOOM_WISP_TRACE
#define LOOM_TRACE LOOM_WISP_TRACE
#else
#define LOOM_TRACE 0
#endif
#endif
#ifndef LOOM_TRACE_HEAP
#ifdef LOOM_WISP_TRACE_HEAP
#define LOOM_TRACE_HEAP LOOM_WISP_TRACE_HEAP
#else
#define LOOM_TRACE_HEAP 0
#endif
#endif

#if LOOM_TRACE
#include "Loom_Trace.h"
#include <Logger.h>
#include <Arduino.h>

inline bool loomTraceBegin(Loom_Trace &recorder, SDManager &sd) {
    if (!recorder.begin(sd, LOOM_TRACE_HEAP != 0)) return false;
    Logger::getInstance()->enableTrace(recorder);
    return true;
}

// Declare before FUNCTION_START so the function's return is recorded before
// the buffer is saved. Use in setup/loop or other deliberate save boundaries,
// rather than every small function. Storage-off/standby leaves events in RAM.
class LoomTraceSaveOnReturn {
  public:
    LoomTraceSaveOnReturn() : trace(Loom_Trace::current()) {}
    ~LoomTraceSaveOnReturn() {
        if (trace && trace->isRecording() && trace->isStorageAvailable()) {
            trace->flush();
        }
    }
    LoomTraceSaveOnReturn(const LoomTraceSaveOnReturn &) = delete;
    LoomTraceSaveOnReturn &operator=(const LoomTraceSaveOnReturn &) = delete;

  private:
    Loom_Trace *trace;
};

#define LOOM_TRACE_RECORDER(name) Loom_Trace name
#define LOOM_TRACE_BEGIN(recorder, sd) loomTraceBegin(recorder, sd)
#define LOOM_TRACE_SKETCH_JOIN_IMPL(a, b) a##b
#define LOOM_TRACE_SKETCH_JOIN(a, b) LOOM_TRACE_SKETCH_JOIN_IMPL(a, b)
#define LOOM_TRACE_SAVE_ON_RETURN() \
    LoomTraceSaveOnReturn LOOM_TRACE_SKETCH_JOIN(_loomTraceSave_, __LINE__)
#define LOOM_TRACE_CHECKPOINT(name) \
    do { if (auto *trace = Loom_Trace::current()) trace->memory(name); } while (false)
#define LOOM_TRACE_VALUE(name, amount, unit) \
    do { if (auto *trace = Loom_Trace::current()) trace->value(name, amount, unit); } while (false)
#define LOOM_TRACE_FLUSH() \
    do { \
        auto *trace = Loom_Trace::current(); \
        if (trace && trace->isRecording() && trace->isStorageAvailable() && !trace->flush()) \
            Serial.println(F("[TRACE] capture stopped: SD trace append failed")); \
    } while (false)
#else
#define LOOM_TRACE_RECORDER(name)
#define LOOM_TRACE_BEGIN(recorder, sd) false
#define LOOM_TRACE_SAVE_ON_RETURN() do {} while (false)
#define LOOM_TRACE_CHECKPOINT(name) do {} while (false)
#define LOOM_TRACE_VALUE(name, amount, unit) do {} while (false)
#define LOOM_TRACE_FLUSH() do {} while (false)
#endif
