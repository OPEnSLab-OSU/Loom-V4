#include "Loom_Trace.h"
#include "Hardware/Loom_Hypnos/SDManager.h"
#include "Utilities/Loom_MemoryUtils.h"
#include <cstdio>
#include <cstring>
#include <malloc.h>

// A weak reference does not pull allocator wrappers into an ordinary build. The --wrap
// references pull that object in when heap capture is linked, making this check succeed.
extern "C" bool loomTraceHeapHooksLinked() __attribute__((weak));

Loom_Trace *Loom_Trace::active = nullptr;

Loom_Trace::~Loom_Trace() {
    if (active == this) {
        active = nullptr;
    }
}

namespace {
// Catch short writes even if an underlying Print implementation forgets to set its error flag.
class CheckedOutput : public Print {
  public:
    explicit CheckedOutput(Print &destination) : destination(destination) {}
    size_t write(uint8_t value) override {
        const size_t written = destination.write(value);
        complete = complete && written == 1;
        return written;
    }
    size_t write(const uint8_t *data, size_t length) override {
        const size_t written = destination.write(data, length);
        complete = complete && written == length;
        return written;
    }
    bool succeeded() const { return complete && !destination.getWriteError(); }

  private:
    Print &destination;
    bool complete = true;
};

// Stream escaped JSON directly to the SD handle; no String or JSON document allocation.
bool text(Print &output, const char *value) {
    bool ok = output.write('"') == 1;
    if (value != nullptr) {
        while (*value != '\0') {
            const unsigned char c = static_cast<unsigned char>(*value++);
            if (c == '"' || c == '\\') {
                ok = (output.write('\\') == 1) && ok;
                ok = (output.write(c) == 1) && ok;
            } else if (c < 0x20) {
                char escaped[7];
                snprintf(escaped, sizeof(escaped), "\\u%04x", static_cast<unsigned int>(c));
                ok = (output.print(escaped) == 6) && ok;
            } else {
                ok = (output.write(c) == 1) && ok;
            }
        }
    }
    return (output.write('"') == 1) && ok;
}

bool number(Print &output, uint64_t value) {
    char digits[21];
    size_t offset = sizeof(digits);
    do {
        digits[--offset] = static_cast<char>('0' + value % 10);
        value /= 10;
    } while (value != 0);
    const size_t length = sizeof(digits) - offset;
    return output.write(reinterpret_cast<const uint8_t *>(digits + offset), length) == length;
}

bool address(Print &output, uintptr_t value) {
    char digits[2 * sizeof(uintptr_t) + 3];
    snprintf(digits, sizeof(digits), "0x%lx", static_cast<unsigned long>(value));
    return text(output, digits);
}

int64_t diagnosticValue(const Loom_Trace::Event &event) {
    // Signed high word + unsigned low word supports negative status and UTC past 2038.
    return static_cast<int64_t>(event.gap) * INT64_C(4294967296) + event.size;
}

bool signedNumber(Print &output, int64_t value) {
    if (value < 0) {
        return output.write('-') == 1 && number(output, static_cast<uint64_t>(-(value + 1)) + 1);
    }
    return number(output, static_cast<uint64_t>(value));
}
} // namespace

bool Loom_Trace::acceptContext() const {
#if defined(ARDUINO_ARCH_SAMD)
    if (__get_IPSR() != 0) {
        return false;
    }
#endif
    return recording && !busy;
}

uint64_t Loom_Trace::timestamp() {
    // SAMD millis/micros share the SysTick clock. Repeat across a tick boundary, and subtract
    // modulo 2^32 so micros rollover does not lose the sub-millisecond part. Extend millis
    // rollover (sample at least once per 49 days). Standby time is excluded by this clock.
    uint32_t ms = millis();
    uint32_t us = micros();
    const uint32_t after = millis();
    if (after != ms) {
        ms = after;
        us = micros();
    }
    if (hasClock && ms < previousMs) {
        clockEpochMs += (uint64_t(1) << 32);
    }
    previousMs = ms;
    hasClock = true;
    const uint32_t fraction = us - ms * 1000u;
    return (clockEpochMs + ms) * 1000u + (fraction < 1000u ? fraction : 999u);
}

bool Loom_Trace::begin(SDManager &manager, bool hooks) {
    if (recording || (active != nullptr && active != this) || !manager.canWriteDebugLogs()) {
        return false;
    }
    sd = &manager;
    heapHooks = hooks && loomTraceHeapHooksLinked != nullptr && loomTraceHeapHooksLinked();
    // SDManager chooses one immutable boot/session number, considering old trace files too.
    // A later CSV schema rotation does not rename the running diagnostics or trace.
    const int session = sd->getDebugFileNumber();
    snprintf(path, sizeof(path), "/debug/trace_%i.ndjson", session);
    snprintf(perfettoPath, sizeof(perfettoPath), "/debug/trace_%i.perfetto.json", session);
    if (session < 0 || sd->fileExists(path) || sd->fileExists(perfettoPath)) {
        return false;
    }
    busy = true;
    char header[128];
    snprintf(header, sizeof(header),
        "{\"v\":1,\"kind\":\"session\",\"session_number\":%i,\"heap_hooks\":%s,\"clock\":\"active_us\"}",
        session, heapHooks ? "true" : "false");
    const bool saved = sd->writeLineToFile(path, header) && sd->writeLineToFile(
        perfettoPath,
        "{\"traceEvents\":["
        "{\"ph\":\"M\",\"name\":\"process_name\",\"pid\":1,\"args\":{\"name\":\"Wisp active time (initialization excluded)\"}},"
        "{\"ph\":\"M\",\"name\":\"thread_name\",\"pid\":1,\"tid\":1,\"args\":{\"name\":\"Nested function calls\"}},"
        "{\"ph\":\"M\",\"name\":\"thread_name\",\"pid\":1,\"tid\":2,\"args\":{\"name\":\"Heap allocations and memory\"}},"
        "{\"ph\":\"M\",\"name\":\"thread_name\",\"pid\":1,\"tid\":3,\"args\":{\"name\":\"Objects and checkpoints\"}},"
        "{\"ph\":\"M\",\"name\":\"thread_name\",\"pid\":1,\"tid\":4,\"args\":{\"name\":\"Trace recording overhead\"}},"
        "{\"ph\":\"M\",\"name\":\"thread_name\",\"pid\":1,\"tid\":5,\"args\":{\"name\":\"Diagnostic values and RTC checks\"}}]}");
    busy = false;
    if (!saved) {
        return false;
    }
    active = this;
    recording = true;
    value("SD capture session number (matches output log)", session, "session number");
    marker("trace started: pre-existing allocations are outside capture");
    memory("capture baseline");
    marker(heapHooks ? "Heap hooks enabled: allocations after capture are recorded"
                     : "Heap hooks OFF: use heap mode for individual allocations");
    return true;
}

void Loom_Trace::push(const Event &event) {
    if (count < EVENT_CAPACITY) {
        events[count++] = event;
    } else {
        if (dropped != UINT32_MAX) {
            ++dropped;
        }
        lastDroppedUs = event.timestampUs;
    }
}

bool Loom_Trace::boundary() {
    if (!acceptContext()) {
        return false;
    }
    if (count == EVENT_CAPACITY) {
        if (!storageAvailable) {
            // A power-off boundary cannot drain. Mark missing function/label events too;
            // they must not silently leave the desktop stack looking complete.
            if (dropped != UINT32_MAX) { ++dropped; }
            lastDroppedUs = timestamp();
            return false;
        }
        if (!flush()) { return false; }
    }
    return acceptContext();
}

void Loom_Trace::setStorageAvailable(bool available) {
    if (available == storageAvailable) { return; }
    if (!available) {
        marker("SD powering down: pending records saved before SPI is disabled");
        flush();
        storageAvailable = false;
    } else {
        storageAvailable = true;
        marker("SD restored after wake: trace writes available");
    }
}

bool Loom_Trace::enter(const char *file, const char *function, uint32_t line, const void *object) {
    if (!boundary()) {
        return false;
    }
    Event event;
    event.kind = 'B';
    event.timestampUs = timestamp();
    event.file = file;
    event.name = function;
    event.line = line;
    event.address = reinterpret_cast<uintptr_t>(object);
    event.gap = LoomMemory::freeMemoryBytes();
    const struct mallinfo info = mallinfo();
    event.size = info.uordblks > 0 ? static_cast<uint32_t>(info.uordblks) : 0;
    event.previousAddress = info.fordblks > 0 ? static_cast<uintptr_t>(info.fordblks) : 0;
    push(event);
    return true;
}

void Loom_Trace::leave() {
    if (!boundary()) {
        return;
    }
    Event event;
    event.kind = 'E';
    event.timestampUs = timestamp();
    event.gap = LoomMemory::freeMemoryBytes();
    const struct mallinfo info = mallinfo();
    event.size = info.uordblks > 0 ? static_cast<uint32_t>(info.uordblks) : 0;
    event.previousAddress = info.fordblks > 0 ? static_cast<uintptr_t>(info.fordblks) : 0;
    push(event);
}

void Loom_Trace::label(const char *name, const void *pointer, uint32_t size) {
    if (!boundary()) {
        return;
    }
    Event event;
    event.kind = 'T';
    event.timestampUs = timestamp();
    event.name = name;
    event.address = reinterpret_cast<uintptr_t>(pointer);
    event.size = size;
    push(event);
}

void Loom_Trace::marker(const char *name) {
    if (!boundary()) {
        return;
    }
    Event event;
    event.kind = 'I';
    event.timestampUs = timestamp();
    event.name = name;
    push(event);
}

void Loom_Trace::object(const char *name, const void *pointer, uint32_t size,
                        const void *owner, int port, int i2cAddress, bool ready) {
    if (pointer == nullptr || !boundary()) {
        return;
    }
    Event event;
    event.kind = 'U';
    event.timestampUs = timestamp();
    strncpy(event.objectName, name != nullptr ? name : "Unnamed object", sizeof(event.objectName) - 1);
    event.objectName[sizeof(event.objectName) - 1] = '\0';
    event.address = reinterpret_cast<uintptr_t>(pointer);
    event.previousAddress = reinterpret_cast<uintptr_t>(owner);
    event.size = size;
    event.line = port >= 0 ? static_cast<uint32_t>(port + 1) : 0;
    event.gap = i2cAddress;
    event.file = ready ? "ready" : "unavailable";
    push(event);
}

void Loom_Trace::retireObject(const void *pointer) {
    if (pointer == nullptr || !boundary()) {
        return;
    }
    Event event;
    event.kind = 'D';
    event.timestampUs = timestamp();
    event.address = reinterpret_cast<uintptr_t>(pointer);
    push(event);
}

void Loom_Trace::memory(const char *name) {
    if (!boundary()) {
        return;
    }
    const struct mallinfo info = mallinfo();
    Event event;
    event.kind = 'C';
    event.timestampUs = timestamp();
    event.name = name;
    event.gap = LoomMemory::freeMemoryBytes();
    event.size = info.uordblks > 0 ? static_cast<uint32_t>(info.uordblks) : 0;
    event.line = info.fordblks > 0 ? static_cast<uint32_t>(info.fordblks) : 0;
    event.address = info.ordblks > 0 ? static_cast<uintptr_t>(info.ordblks) : 0;
    event.previousAddress = info.keepcost > 0 ? static_cast<uintptr_t>(info.keepcost) : 0;
    push(event);
}

void Loom_Trace::value(const char *name, int64_t amount, const char *unit, const void *object) {
    if (!boundary()) {
        return;
    }
    Event event;
    event.kind = 'V';
    event.timestampUs = timestamp();
    event.name = name;
    event.file = unit;
    event.address = reinterpret_cast<uintptr_t>(object);
    const uint64_t bits = static_cast<uint64_t>(amount);
    event.size = static_cast<uint32_t>(bits);
    // Avoid implementation-defined conversion of the sign bit to int32_t.
    event.gap = static_cast<int32_t>(amount / INT64_C(4294967296));
    if (amount < 0 && event.size != 0) {
        --event.gap;
    }
    push(event);
}

void Loom_Trace::heapEvent(char kind, void *pointer, size_t size, void *previous, uintptr_t caller) {
    Loom_Trace *trace = active;
    if (trace == nullptr || !trace->heapHooks || !trace->acceptContext()) {
        return;
    }
    Event event;
    event.kind = kind;
    event.timestampUs = trace->timestamp();
    event.address = reinterpret_cast<uintptr_t>(pointer);
    event.previousAddress = reinterpret_cast<uintptr_t>(previous);
    event.size = static_cast<uint32_t>(size);
    event.line = static_cast<uint32_t>(caller);
    trace->push(event); // Never perform filesystem work from an allocator wrapper.
}

bool Loom_Trace::writeEvents(Print &destination, void *context) {
    CheckedOutput output(destination);
    Loom_Trace &trace = *static_cast<Loom_Trace *>(context);
    for (size_t index = 0; index < trace.count; ++index) {
        const Event &event = trace.events[index];
        output.print(F("{\"kind\":\""));
        output.write(event.kind);
        output.print(F("\",\"ts\":"));
        if (!number(output, event.timestampUs)) {
            return false;
        }
        output.print(F(",\"addr\":"));
        if (!address(output, event.address)) {
            return false;
        }
        output.print(F(",\"old\":"));
        if (!address(output, event.previousAddress)) {
            return false;
        }
        output.print(F(",\"size\":"));
        output.print(event.size);
        output.print(F(",\"gap\":"));
        output.print(event.gap);
        output.print(F(",\"line\":"));
        output.print(event.line);
        output.print(F(",\"file\":"));
        if (!text(output, event.file)) {
            return false;
        }
        output.print(F(",\"name\":"));
        if (!text(output, event.kind == 'U' ? event.objectName : event.name)) {
            return false;
        }
        output.println('}');
    }
    if (trace.dropped != 0) {
        output.print(F("{\"kind\":\"lost\",\"ts\":"));
        if (!number(output, trace.lastDroppedUs)) {
            return false;
        }
        output.print(F(",\"count\":"));
        output.print(trace.dropped);
        output.println('}');
    }
    return output.succeeded();
}

bool Loom_Trace::writeChromeEvents(Print &destination, void *context) {
    CheckedOutput output(destination);
    Loom_Trace &trace = *static_cast<Loom_Trace *>(context);
    auto begin = [&output](char phase, const char *name, uint64_t ts, unsigned int track) {
        output.print(F(",\r\n{\"ph\":\""));
        output.write(phase);
        output.print(F("\",\"name\":"));
        text(output, name);
        output.print(F(",\"cat\":\"Loom\",\"pid\":1,\"tid\":"));
        output.print(track);
        output.print(F(",\"ts\":"));
        number(output, ts);
        if (phase == 'I') {
            output.print(F(",\"s\":\"t\""));
        }
    };
    auto totals = [&output](const Event &event) {
        output.print(F("\"Heap bytes in use\":"));
        output.print(event.size);
        output.print(F(",\"Reusable free bytes\":"));
        output.print(event.line);
        output.print(F(",\"Free chunks\":"));
        output.print(static_cast<uint32_t>(event.address));
        output.print(F(",\"Top free chunk bytes\":"));
        output.print(static_cast<uint32_t>(event.previousAddress));
        output.print(F(",\"Stack-to-heap gap estimate bytes\":"));
        output.print(event.gap);
        output.print(F(",\"Available RAM estimate bytes (no stack reserve)\":"));
        number(output, static_cast<uint64_t>(event.line) + (event.gap > 0 ? event.gap : 0));
    };
    for (size_t index = 0; index < trace.count; ++index) {
        const Event &event = trace.events[index];
        char phase = 'I';
        unsigned int track = 3;
        // U holds a copied name in the union; never interpret it as a pointer.
        const char *name = event.kind == 'U' ? event.objectName : event.name;
        switch (event.kind) {
        case 'B': case 'E': phase = event.kind; track = 1; break;
        case 'A': name = "Heap block allocated"; track = 2; break;
        case 'F': name = "Heap block freed"; track = 2; break;
        case 'R': name = "Heap block reallocated"; track = 2; break;
        case 'N': name = "Heap allocation failed"; track = 2; break;
        case 'Z': name = "Zero-size realloc: outcome unknown"; track = 2; break;
        case 'C': phase = 'C'; name = "Allocator totals (includes baseline)"; track = 2; break;
        case 'D': name = "Object retired before deletion"; break;
        case 'V': track = 5; break;
        case 'O': phase = 'X'; name = "Save trace files to SD"; track = 4; break;
        default: break;
        }
        begin(phase, name, event.timestampUs, track);
        if (phase == 'X') {
            output.print(F(",\"dur\":"));
            output.print(event.size);
        }
        output.print(F(",\"args\":{"));
        if (event.kind == 'C') {
            totals(event);
        } else {
            output.print(F("\"Record kind\":\""));
            output.write(event.kind);
            output.write('"');
        }
        if (event.kind != 'C' && event.kind != 'I' && event.kind != 'O') {
            output.print(F(",\"Address\":"));
            address(output, event.address);
            if (event.kind == 'B' || event.kind == 'E') {
                output.print(F(",\"Source file\":"));
                text(output, event.file);
                output.print(F(",\"Source line\":"));
                output.print(event.line);
                output.print(F(",\"Stack-to-heap gap estimate bytes\":"));
                output.print(event.gap);
                output.print(F(",\"Heap bytes in use at call boundary\":"));
                output.print(event.size);
                output.print(F(",\"Reusable free bytes at call boundary\":"));
                output.print(static_cast<uint32_t>(event.previousAddress));
            } else if (event.kind == 'U') {
                output.print(F(",\"Owning mux address\":"));
                address(output, event.previousAddress);
                output.print(F(",\"Mux port (-1 means none)\":"));
                output.print(static_cast<int32_t>(event.line) - 1);
                output.print(F(",\"I2C address (-1 means none)\":"));
                output.print(event.gap);
                output.print(F(",\"State\":"));
                text(output, event.file);
                output.print(F(",\"Known container bytes (0 means unknown)\":"));
                output.print(event.size);
            } else if (event.kind == 'V') {
                output.print(F(",\"Value\":"));
                signedNumber(output, diagnosticValue(event));
                output.print(F(",\"Unit\":"));
                text(output, event.file);
            } else if (event.kind == 'T') {
                output.print(F(",\"Container bytes (not added to heap)\":"));
                output.print(event.size);
            } else if (event.kind != 'D') {
                output.print(F(",\"Previous address\":"));
                address(output, event.previousAddress);
                output.print(F(",\"Requested bytes (free size resolved offline)\":"));
                output.print(event.size);
                output.print(F(",\"Caller instruction address\":"));
                address(output, event.line);
            }
        }
        output.print(F("}}"));
        if (event.kind == 'B' || event.kind == 'E') {
            begin('C', "Heap bytes in use at function boundaries", event.timestampUs, 2);
            output.print(F(",\"args\":{\"Heap bytes in use\":"));
            output.print(event.size);
            output.print(F(",\"Reusable free bytes\":"));
            output.print(static_cast<uint32_t>(event.previousAddress));
            output.print(F(",\"Available RAM estimate bytes (no stack reserve)\":"));
            number(output, static_cast<uint64_t>(event.previousAddress) +
                               static_cast<uint32_t>(event.gap > 0 ? event.gap : 0));
            output.print(F(",\"Stack-to-heap gap estimate bytes\":"));
            output.print(event.gap);
            output.print(F("}}"));
        }
        if (event.kind == 'C') {
            begin('I', event.name, event.timestampUs, 3);
            output.print(F(",\"args\":{ "));
            totals(event);
            output.print(F("}}"));
        }
    }
    if (trace.dropped != 0) {
        begin('I', "LOST TRACE EVENTS: call and allocation history incomplete", trace.lastDroppedUs, 3);
        output.print(F(",\"args\":{\"Dropped events\":"));
        output.print(trace.dropped);
        output.print(F("}}"));
    }
    return output.succeeded();
}

bool Loom_Trace::flush() {
    if (!acceptContext() || sd == nullptr || !storageAvailable) {
        return false;
    }
    if (count == 0 && dropped == 0) {
        return true;
    }
    busy = true; // Exclude SD/logger internals and any allocations made by them.
    const uint64_t startedUs = timestamp();
    const bool saved = sd->writeDebugRecords(path, writeEvents, this) &&
                       sd->appendDebugTrace(perfettoPath, writeChromeEvents, this);
    busy = false;
    if (saved) {
        count = 0;
        dropped = 0;
        // This record is saved by the next drain. A separate viewer track makes recording
        // overhead visible without pretending it was useful application work.
        Event overhead;
        overhead.kind = 'O';
        overhead.timestampUs = startedUs;
        overhead.size = static_cast<uint32_t>(timestamp() - startedUs);
        push(overhead);
    } else {
        // Never keep retrying an uncertain diagnostic append in sensor/allocator paths.
        // Preserve the prefix for inspection and stop capture for this boot.
        recording = false;
        if (active == this) {
            active = nullptr;
        }
    }
    return saved;
}
