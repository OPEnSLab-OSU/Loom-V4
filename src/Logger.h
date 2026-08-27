#pragma once

#include "Hardware/Loom_Hypnos/Loom_Hypnos.h"
#include <MemoryFree.h>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

// To acquire a function call summary, just add INSTRUMENT() to the top of the
// relevant function.
#define LOOM_LOGGER_JOIN_IMPL(a, b) a##b
#define LOOM_LOGGER_JOIN(a, b) LOOM_LOGGER_JOIN_IMPL(a, b)
#define INSTRUMENT()                                                                               \
    FunctionInstrumentor LOOM_LOGGER_JOIN(_loomInstrumentor_, __LINE__)(__FILE__, __func__,        \
                                                                        __LINE__);

// DEPRECATED - use INSTRUMENT
#define FUNCTION_START INSTRUMENT()
// DEPRECATED - use INSTRUMENT
#define FUNCTION_END

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
        Logger::getInstance()->genericLogFormatted(log, PSTR(msg), ##__VA_ARGS__);                  \
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

    // Whether or not to use the SD card or log function summaries
    bool enableFunctionSummaries = false;
    bool enableSDLogging = false;
    bool rtcTimestampsEnabled = true;

    SDManager *sdInst = nullptr;
    Loom_Hypnos *hypnosInst = nullptr;

    Logger(){};

    /**
     * Generic log function - prints to Serial and logs to SD
     *
     * @param message The message we want to log
     * @param silent Whether to print to the serial monitor
     */
    void log(char *message, bool silent) {
        char filePath[32];

        // If we want to actually print to serial
        if (!silent)
            Serial.println(message);

        // Log as long as we have given it a SD card instance
        if (sdInst != nullptr && enableSDLogging && sdInst->hasSDInitialized()) {
            snprintf_P(filePath, sizeof(filePath), PSTR("/debug/output_%i.log"),
                       sdInst->getCurrentFileNumber());
            sdInst->writeLineToFile(filePath, message);
        }
    }

    static const char *baseFileName(const char *src) {
        if (src == nullptr)
            return "";

        const char *backslash = strrchr(src, '\\');
        const char *slash = strrchr(src, '/');
        const char *separator = backslash;
        if (separator == nullptr || (slash != nullptr && slash > separator))
            separator = slash;
        return separator == nullptr ? src : separator + 1;
    }

    static size_t writePrefix(char *destination, size_t destinationSize, LogContext log) {
        if (destination == nullptr || destinationSize == 0)
            return 0;

        const char *fileName = baseFileName(log.file);
        int written = 0;
        Logger *logger = Logger::getInstance();
        if (logger->rtcTimestampsEnabled && logger->hypnosInst != nullptr &&
            logger->hypnosInst->isRTCInitialized()) {
            DateTime time = logger->hypnosInst->getCurrentTime();
            char timestamp[21];
            logger->hypnosInst->dateTime_toString(time, timestamp);
            written = snprintf_P(destination, destinationSize, PSTR("[%s] [%s] [%s:%s:%lu] "),
                                 timestamp, log.level, fileName, log.func, log.lineNum);
        } else {
            written = snprintf_P(destination, destinationSize, PSTR("[%s] [%s:%s:%lu] "),
                                 log.level, fileName, log.func, log.lineNum);
        }

        if (written <= 0)
            return 0;
        if (static_cast<size_t>(written) >= destinationSize)
            return destinationSize - 1;
        return static_cast<size_t>(written);
    }

  public:
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
    void setHypnos(Loom_Hypnos *hypnos) {
        hypnosInst = hypnos;
        sdInst = hypnos->getSDManager();
    };

    void genericLog(LogContext log, const __FlashStringHelper *msg) {
        // ATSAMD21 flash is memory-mapped, so F() strings can be consumed without a RAM copy.
        genericLog(log, reinterpret_cast<const char *>(msg));
    }

    void genericLog(LogContext log, const char *msg) {
        char logMessage[OUTPUT_SIZE] = {};
        const size_t prefixLength = writePrefix(logMessage, sizeof(logMessage), log);
        strncpy(logMessage + prefixLength, msg ? msg : "",
                sizeof(logMessage) - prefixLength - 1);

        this->log(logMessage, log.silent);
    }

    void genericLogFormatted(LogContext log, const char *format, ...) {
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
    void logLong(char *message, bool silent) { log(message, silent); };

    /* Enable function summaries to view memory usage */
    void enableSummaries() { enableFunctionSummaries = true; };

    /* Save flash write by not logging everything to SD */
    void enableSD() { enableSDLogging = true; };

    /* Avoid an RTC I2C transaction for every Serial log in constrained field sketches. */
    void disableRTCTimestamps() { rtcTimestampsEnabled = false; };

    bool shouldLogSummaries() {
        return enableFunctionSummaries && sdInst != nullptr && enableSDLogging;
    }

    /**
     * Truncate the __FILE__ output to just show the name instead of the whole path
     * Always null-terminates within dstSize.
     */
    static void truncateFileName(char *dst, size_t dstSize, const char *src) {
        if (dst == nullptr || dstSize == 0)
            return;
        if (src == nullptr) {
            dst[0] = '\0';
            return;
        }

        const char *name = baseFileName(src);
        strncpy(dst, name, dstSize - 1);
        dst[dstSize - 1] = '\0';
    }
};

class FunctionInstrumentor {
  private:
    // Keep the large formatting buffers out of the constructor/destructor frames. GCC otherwise
    // reserves them before the early return even when field deployments disable summaries.
    static __attribute__((noinline)) void writeSummary(Logger *logger, bool starting,
                                                       const char *file, const char *func,
                                                       int lineNum) {
        const int freemem = freeMemory();
        char logfileName[48];
        snprintf_P(logfileName, sizeof(logfileName), PSTR("/debug/funcSummaries_%i.log"),
                   logger->sdInst->getCurrentFileNumber());

        char output[OUTPUT_SIZE] = {};
        if (starting) {
            char fileName[LOGGER_FILENAME_SIZE] = {};
            Logger::truncateFileName(fileName, sizeof(fileName), file);
            snprintf_P(output, sizeof(output), PSTR("start,%d,%s,%s,%d,%d,%lu"),
                       static_cast<int>(logger->stackDepth - 1), fileName, func, lineNum, freemem,
                       millis());
        } else {
            snprintf_P(output, sizeof(output), PSTR("end,%d, , , ,%d,%lu"),
                       static_cast<int>(logger->stackDepth), freemem, millis());
        }

        if (!logger->sdInst->writeLineToFile(logfileName, output))
            Serial.println(F("Could not write instrumentation to file!"));
    }

  public:
    // delete all other constructors
    FunctionInstrumentor(const FunctionInstrumentor &) = delete;
    FunctionInstrumentor &operator=(const FunctionInstrumentor &) = delete;

    FunctionInstrumentor(const char *file, const char *func, int lineNum) {
        Logger *logger = Logger::getInstance();
        logger->stackDepth++;

        if (logger->shouldLogSummaries())
            writeSummary(logger, true, file, func, lineNum);
    }

    ~FunctionInstrumentor() {
        Logger *logger = Logger::getInstance();
        logger->stackDepth--;

        if (logger->shouldLogSummaries())
            writeSummary(logger, false, nullptr, nullptr, 0);
    }
};
