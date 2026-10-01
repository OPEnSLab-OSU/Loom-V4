#pragma once

#include "Loom_WarningGuards.h"

#include "Module.h"
LOOM_EXTERNAL_INCLUDE_BEGIN
#include <ArduinoJson.h>
LOOM_EXTERNAL_INCLUDE_END
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

class SDManager;
class Loom_Hypnos;
class Loom_Trace;

// Installed only by an opted-in recorder. These function pointers keep Logger's layout
// identical in sketch/library translation units and let ordinary builds omit the recorder.
struct LoomTraceCallbacks {
    bool (*enter)(Loom_Trace *, const char *, const char *, uint32_t, const void *);
    void (*leave)(Loom_Trace *);
    bool (*flush)(Loom_Trace *);
    void (*storage)(Loom_Trace *, bool);
    void (*object)(Loom_Trace *, const char *, const void *, uint32_t, const void *, int, int, bool);
    void (*retire)(Loom_Trace *, const void *);
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Function summaries: show entry/exit timing and available memory during debugging.
////////////////////////////////////////////////////////////////////////////////////////////////////
// FUNCTION_START begins a scoped debug summary. Its destructor records the exit on every
// return path. FUNCTION_END marks the normal exit in source; it does not emit a second summary.
// FUNCTION_START(this) also identifies the instance in an optional trace. The original
// FUNCTION_START; syntax and INSTRUMENT() remain available without an object address.
#define LOOM_LOGGER_JOIN_IMPL(a, b) a##b
#define LOOM_LOGGER_JOIN(a, b) LOOM_LOGGER_JOIN_IMPL(a, b)
#define LOOM_TRACE_FUNCTION_NAME __PRETTY_FUNCTION__
#define INSTRUMENT()                                                                               \
    FunctionInstrumentor LOOM_LOGGER_JOIN(_loomInstrumentor_, __LINE__)(                           \
        __FILE__, __func__, __LINE__, LOOM_TRACE_FUNCTION_NAME);

#define FUNCTION_START                                                                             \
    const FunctionInstrumentor &LOOM_LOGGER_JOIN(_loomInstrumentor_, __LINE__) =                    \
        FunctionInstrumentationContext(__FILE__, __func__, __LINE__, LOOM_TRACE_FUNCTION_NAME)
#define FUNCTION_END

// Compatibility spelling for code that used the first object-aware trace macro.
#define FUNCTION_START_OBJECT(object) FUNCTION_START(object)

// Compatibility aliases; FUNCTION_START is the shared entry point for summaries and tracing.
#define TRACE_FUNCTION_START(object) FUNCTION_START(object)
#define TRACE_FUNCTION_SCOPE(name, object) FUNCTION_START(object, name)
#define TRACE_FUNCTION_START_OBJECT(object) TRACE_FUNCTION_START(object)


////////////////////////////////////////////////////////////////////////////////////////////////////
// Message helpers: LOG for progress, WARNING for a problem, ERROR for a failed operation.
////////////////////////////////////////////////////////////////////////////////////////////////////
struct LogContext {
    const char *file;
    const char *func;
    unsigned long lineNum;
    bool silent;
    const char *level; // must have static lifetime
};

#define GENERIC_LOG(silent, level, msg)                                                            \
    do {                                                                                           \
        LogContext log{__FILE__, __func__, __LINE__, silent, level};                               \
        Logger::getInstance()->genericLog(log, msg);                                               \
    } while (false)

#define LOG(msg) GENERIC_LOG(false, "DEBUG", msg)
#define SLOG(msg) GENERIC_LOG(true, "DEBUG", msg)
#define WARNING(msg) GENERIC_LOG(false, "WARNING", msg)
#define ERROR(msg) GENERIC_LOG(false, "ERROR", msg)

#define LOG_LONG(msg) Logger::getInstance()->logLong(msg, false)

#define GENERIC_LOGF(silent, level, msg, ...)                                                      \
    do {                                                                                           \
        LogContext log{__FILE__, __func__, __LINE__, silent, level};                               \
        Logger::getInstance()->genericLogFormatted(log, PSTR(msg), ##__VA_ARGS__);                 \
    } while (false)

#define LOGF(msg, ...) GENERIC_LOGF(false, "DEBUG", msg, ##__VA_ARGS__)
#define SLOGF(msg, ...) GENERIC_LOGF(true, "DEBUG", msg, ##__VA_ARGS__)
#define WARNINGF(msg, ...) GENERIC_LOGF(false, "WARNING", msg, ##__VA_ARGS__)
#define ERRORF(msg, ...) GENERIC_LOGF(false, "ERROR", msg, ##__VA_ARGS__)

#define ENABLE_SD_LOGGING Logger::getInstance()->enableSD()
#define ENABLE_FUNC_SUMMARIES Logger::getInstance()->enableSummaries()
#define DISABLE_RTC_LOG_TIMESTAMPS Logger::getInstance()->disableRTCTimestamps()

constexpr size_t LOGGER_FILENAME_SIZE = 64;

/**
 * Arduino Logger class that allows for standardized log outputs as well as
 * function memory usage summaries to find memory leaks that may lead to
 * unexpected crashing
 *
 * @author Will Richards
 */
class Logger {
  private:
    friend class FunctionInstrumentor;

    unsigned int stackDepth = 0;
    Loom_Trace *trace = nullptr; // Borrowed; no event buffer exists unless the sketch opts in.
    const LoomTraceCallbacks *traceCallbacks = nullptr;

    // Whether or not to use the SD card or log function summaries
    bool debugOutputEnabled = true;
    using SummaryWriter = void (*)(Logger *, bool, const char *, const char *, int);
    SummaryWriter summaryWriter = nullptr;
    bool enableSDLogging = false;
    bool rtcTimestampsEnabled = true;

    SDManager *sdInst = nullptr;
    Loom_Hypnos *hypnosInst = nullptr;

    Logger() {};

    /**
     * Generic log function - prints to Serial and logs to SD
     *
     * @param message The message we want to log
     * @param silent Whether to print to the serial monitor
     */
    void log(char *message, bool silent);

    static const char *baseFileName(const char *src) {
        if (src == nullptr) {
            return "";
        }

        const char *backslash = strrchr(src, '\\');
        const char *slash = strrchr(src, '/');
        const char *separator = backslash;
        if (separator == nullptr || (slash != nullptr && slash > separator)) {
            separator = slash;
        }
        return separator == nullptr ? src : separator + 1;
    }

    static size_t writePrefix(char *destination, size_t destinationSize, LogContext log);

  public:
    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Output options: one shared logger, with optional SD storage and RTC timestamps.
    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Deleting copy constructor.
    Logger(const Logger &obj) = delete;

    /* Get an instance of the logger object */
    static Logger *getInstance() {
        static Logger instance;
        return &instance;
    };

    /**
     * Set the instance of the SD Manager
     * @param manager Pointer to the SD manager to allow us to utilize SD logging functionality
     */
    void setSDManager(SDManager *manager) { sdInst = manager; };

    /**
     * Set the instance of the Hypnos, this should be used if you want the current timestamp added
     * to the front of the logger output
     * @param hypnos Pointer to the hypnos object this also sets the sdInst
     */
    void setHypnos(Loom_Hypnos *hypnos);

    void genericLog(LogContext log, const __FlashStringHelper *msg) {
        // ATSAMD21 flash is memory-mapped, so F() strings can be consumed without a RAM copy.
        genericLog(log, reinterpret_cast<const char *>(msg));
    }

    void genericLog(LogContext log, const char *msg) {
        if (!debugOutputEnabled && strcmp(log.level, "DEBUG") == 0) {
            return;
        }
        char logMessage[OUTPUT_SIZE] = {};
        const size_t prefixLength = writePrefix(logMessage, sizeof(logMessage), log);
        strncpy(logMessage + prefixLength, msg ? msg : "", sizeof(logMessage) - prefixLength - 1);

        this->log(logMessage, log.silent);
    }

    void genericLogFormatted(LogContext log, const char *format, ...) {
        if (!debugOutputEnabled && strcmp(log.level, "DEBUG") == 0) {
            return;
        }
        char logMessage[OUTPUT_SIZE] = {};
        const size_t prefixLength = writePrefix(logMessage, sizeof(logMessage), log);

        va_list arguments;
        va_start(arguments, format);
        vsnprintf(logMessage + prefixLength, sizeof(logMessage) - prefixLength,
                  format ? format : "", arguments);
        va_end(arguments);

        this->log(logMessage, log.silent);
    }

    /*
     * Directly log a message
     */
    void logLong(char *message, bool silent) {
        if (debugOutputEnabled) {
            log(message, silent);
        }
    }

    // Preserve LOG_LONG's unprefixed pretty-JSON payload on both destinations without
    // allocating MAX_JSON_SIZE bytes on the Feather M0 stack.
    void logDocument(const DynamicJsonDocument &document);

    /** Suppress DEBUG messages, JSON display, and summaries; preserve warnings and errors. */
    void setDebugOutput(bool enabled) { debugOutputEnabled = enabled; }

    /* Enable function summaries to view memory usage */
    // The opt-in definition lives with the writer; unused builds never reference it.
    void enableSummaries();

    /* Save flash write by not logging everything to SD */
    void enableSD() { enableSDLogging = true; };

    /* Avoid an RTC I2C transaction for every Serial log in constrained field sketches. */
    void disableRTCTimestamps() { rtcTimestampsEnabled = false; };

    bool shouldLogSummaries();

    /** Attach only a started recorder with static/sketch lifetime. Extra toggle, independent
     * of the legacy summaries. Setting debug output false also suppresses new trace scopes. */
    void enableTrace(Loom_Trace &recorder);
    bool flushTrace();
    bool hasTrace() const { return traceCallbacks != nullptr; }
    void setTraceStorageAvailable(bool available);
    void traceObject(const char *name, const void *address, uint32_t bytes, const void *owner,
                     int port, int i2cAddress, bool ready);
    void retireTraceObject(const void *address);

    /**
     * Truncate the __FILE__ output to just show the name instead of the whole path
     * Always null-terminates within dstSize.
     */
    static void truncateFileName(char *dst, size_t dstSize, const char *src) {
        if (dst == nullptr || dstSize == 0) {
            return;
        }
        if (src == nullptr) {
            dst[0] = '\0';
            return;
        }

        const char *name = baseFileName(src);
        strncpy(dst, name, dstSize - 1);
        dst[dstSize - 1] = '\0';
    }
};

// Capture the call site before an optional (this) argument. This is only construction
// metadata: the FunctionInstrumentor below owns the one entry/exit pair, including early
// returns. No allocation or extra state is added to the retained scope guard.
struct FunctionInstrumentationContext {
    const char *file;
    const char *func;
    int lineNum;
    const char *qualifiedFunc;
    const void *object;

    FunctionInstrumentationContext(const char *file, const char *func, int lineNum,
                                   const char *qualifiedFunc, const void *object = nullptr)
        : file(file), func(func), lineNum(lineNum), qualifiedFunc(qualifiedFunc), object(object) {}

    FunctionInstrumentationContext operator()(const void *instance = nullptr,
                                               const char *scopeName = nullptr) const {
        return FunctionInstrumentationContext(file, func, lineNum,
                                              scopeName != nullptr ? scopeName : qualifiedFunc,
                                              instance);
    }
};

class FunctionInstrumentor {
  private:
    friend class Logger;
    Loom_Trace *trace = nullptr;
    // Keep formatting buffers out of the constructor/destructor frames when summaries are off.
    static __attribute__((noinline)) void
    writeSummary(Logger *logger, bool starting, const char *file, const char *func, int lineNum);

  public:
    // delete all other constructors
    FunctionInstrumentor(const FunctionInstrumentor &) = delete;
    FunctionInstrumentor &operator=(const FunctionInstrumentor &) = delete;

    // Fold call-site metadata into the existing constructor instead of adding a helper call.
    __attribute__((always_inline)) FunctionInstrumentor(const FunctionInstrumentationContext &context)
        : FunctionInstrumentor(context.file, context.func, context.lineNum, context.qualifiedFunc,
                               context.object) {}

    FunctionInstrumentor(const char *file, const char *func, int lineNum,
                         const char *qualifiedFunc = nullptr, const void *object = nullptr) {
        Logger *logger = Logger::getInstance();
        logger->stackDepth++;
        beginTrace(logger, file, qualifiedFunc != nullptr ? qualifiedFunc : func, lineNum, object);

        if (logger->shouldLogSummaries()) {
            logger->summaryWriter(logger, true, file, func, lineNum);
        }
    }

    ~FunctionInstrumentor() {
        Logger *logger = Logger::getInstance();
        endTrace();
        logger->stackDepth--;

        if (logger->shouldLogSummaries()) {
            logger->summaryWriter(logger, false, nullptr, nullptr, 0);
        }
    }

  private:
    void beginTrace(Logger *logger, const char *file, const char *func, int line,
                    const void *object);
    void endTrace();
};
