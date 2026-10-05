#include "Loom_TraceAuto.h"
#include "Loom_Trace.h"
#include "Loom_Manager.h"
#include "Logger.h"

namespace {
struct AutomaticTrace {
    Loom_Trace recorder;
    Manager *owner = nullptr;
    SDManager *sd = nullptr;
    bool heapHooks = false;
    uint8_t window = Loom_Trace::DEFAULT_HEAP_WINDOW_EVENTS;
    bool attempted = false;

    static void start(Manager &manager, void *context) {
        auto &session = *static_cast<AutomaticTrace *>(context);
        if (session.owner != &manager || session.attempted) return;
        session.attempted = true;
        if (!session.recorder.begin(*session.sd, session.heapHooks, session.window)) {
            Serial.println(F("[TRACE] Could not start automatic SD capture; normal operation continues"));
            return;
        }
        Logger *logger = Logger::getInstance();
        logger->enableTrace(session.recorder);
        logger->setTraceAutoSave(true);
        Serial.print(F("[TRACE] Timeline: "));
        Serial.println(session.recorder.getPerfettoPath());
        Serial.print(F("[TRACE] Heap records: "));
        Serial.println(session.recorder.getRecordPath());
        Serial.println(session.recorder.isHeapCaptureEnabled() ?
            F("[TRACE] Bounded allocator capture ON") : F("[TRACE] Allocator capture OFF"));
        logger->flushTrace(); // Persist the automatic baseline even without a later call.
    }
};
} // namespace

bool loomTraceAttach(Manager &manager, SDManager *sd, bool heapHooks, uint8_t heapWindowEvents) {
    if (sd == nullptr || heapWindowEvents == 0 || heapWindowEvents >= Loom_Trace::EVENT_CAPACITY) {
        Serial.println(F("[TRACE] Automatic capture needs SD support and a heap window of 1..23"));
        return false;
    }
    // This object exists only if the opt-in function is linked. It owns a fixed
    // event buffer; no heap allocation, Manager member buffer, or global constructor.
    static AutomaticTrace session;
    if (session.owner != nullptr) {
        return session.owner == &manager && session.sd == sd && session.heapHooks == heapHooks &&
               session.window == heapWindowEvents && (!session.attempted || session.recorder.isRecording());
    }
    if (Loom_Trace::current() != nullptr) {
        Serial.println(F("[TRACE] A recorder is already active; automatic attachment rejected"));
        return false;
    }
    session.owner = &manager;
    session.sd = sd;
    session.heapHooks = heapHooks;
    session.window = heapWindowEvents;
    Logger::getInstance()->setTraceStartup(AutomaticTrace::start, &session);
    if (manager.isInitialized()) AutomaticTrace::start(manager, &session);
    return !session.attempted || session.recorder.isRecording();
}
