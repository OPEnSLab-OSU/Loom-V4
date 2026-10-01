#pragma once

#include <cstddef>
#include <cstdint>

class Print;
class SDManager;

/**
 * Optional main-loop trace recorder. Instantiate only in a trace-enabled debug sketch.
 * Events borrow static strings; recording uses no heap or SD. Function boundaries drain the
 * fixed buffer when necessary. Heap hooks never drain it and can therefore lose burst events.
 * Every loss is persisted explicitly. Interrupts and the recorder's own SD work are excluded.
 */
class Loom_Trace {
  public:
    static constexpr size_t EVENT_CAPACITY = 24;

    struct Event {
        uint64_t timestampUs = 0;
        const char *file = nullptr;
        union {
            const char *name = nullptr;
            char objectName[32]; // U events copy mutable module names before deletion/rename.
        };
        uintptr_t address = 0;
        uintptr_t previousAddress = 0;
        uint32_t size = 0;
        int32_t gap = 0;
        uint32_t line = 0;
        char kind = 'I';
    };

    Loom_Trace() = default;
    ~Loom_Trace();
    Loom_Trace(const Loom_Trace &) = delete;
    Loom_Trace &operator=(const Loom_Trace &) = delete;

    /** Start after SD initialization. Paths must be unique per boot; never append a new boot
     * to an old trace. heapHooks tells the reader whether the optional linker hooks are built. */
    bool begin(SDManager &sd, bool heapHooks = false);
    bool flush();
    bool isRecording() const { return recording && !busy; }
    bool isHeapCaptureEnabled() const { return recording && heapHooks; }
    const char *getRecordPath() const { return path; }
    const char *getPerfettoPath() const { return perfettoPath; }
    void setStorageAvailable(bool available);
    bool isStorageAvailable() const { return storageAvailable; }

    bool enter(const char *file, const char *function, uint32_t line, const void *object = nullptr);
    void leave();
    /** name must have static lifetime. size is informational for a label, not another allocation. */
    void label(const char *name, const void *address, uint32_t size = 0);
    /** Observe a named object, including pre-capture objects. Not an allocation event.
     * Names are copied (31 characters). owner/port/address identify mux children;
     * size is the known container size, or zero when unknown. */
    void object(const char *name, const void *address, uint32_t size = 0,
                const void *owner = nullptr, int port = -1, int i2cAddress = -1,
                bool ready = true);
    void retireObject(const void *address);
    static Loom_Trace *current() { return active; }
    void marker(const char *name);
    /** Named diagnostic value. name/unit must have static lifetime; signed 64-bit values
     * reuse existing event fields, so this adds no recorder RAM or allocation ledger. */
    void value(const char *name, int64_t amount, const char *unit = "count",
               const void *object = nullptr);
    /** Allocator totals, including blocks created before capture. Call only at checkpoints. */
    void memory(const char *name);

    /** Used only by optional linker wrappers. A/realloc R/F/free/failure N. */
    static void heapEvent(char kind, void *address, size_t size, void *previous = nullptr,
                          uintptr_t caller = 0);

  private:
    Event events[EVENT_CAPACITY];
    size_t count = 0;
    SDManager *sd = nullptr;
    char path[48] = {};
    char perfettoPath[48] = {};
    uint32_t dropped = 0;
    uint64_t lastDroppedUs = 0;
    uint32_t previousMs = 0;
    uint64_t clockEpochMs = 0;
    bool hasClock = false;
    bool recording = false;
    bool busy = false;
    bool heapHooks = false;
    bool storageAvailable = true;
    static Loom_Trace *active;

    uint64_t timestamp();
    bool acceptContext() const;
    void push(const Event &event);
    bool boundary();
    static bool writeEvents(Print &output, void *context);
    static bool writeChromeEvents(Print &output, void *context);
};
