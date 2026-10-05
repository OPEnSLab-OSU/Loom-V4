#include "Loom_Trace.h"
#include "Logger.h"

// Nothing in Logger references this implementation until the sketch calls enableTrace().
// The linker can omit this adapter and the recorder when the sketch flag is off.
void Logger::enableTrace(Loom_Trace &recorder) {
    static const LoomTraceCallbacks callbacks = {
        [](Loom_Trace *r, const char *file, const char *name, uint32_t line, const void *object) {
            return r->enter(file, name, line, object);
        },
        [](Loom_Trace *r) { r->leave(); },
        [](Loom_Trace *r) {
            if (!r->isRecording() || !r->isStorageAvailable()) return false;
            if (r->flush()) return true;
            Serial.println(F("[TRACE] capture stopped: SD trace append failed"));
            return false;
        },
        [](Loom_Trace *r, bool available) { r->setStorageAvailable(available); },
        [](Loom_Trace *r, const char *name, const void *address, uint32_t bytes,
           const void *owner, int port, int i2cAddress, bool ready) {
            r->object(name, address, bytes, owner, port, i2cAddress, ready);
        },
        [](Loom_Trace *r, const void *address) { r->retireObject(address); }
    };
    trace = &recorder;
    traceCallbacks = &callbacks;
    traceAutoSave = false; // Existing manual sketches retain explicit save boundaries.
}
