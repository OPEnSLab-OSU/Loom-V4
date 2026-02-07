#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <MemoryFree.h>
#include "Hardware/Loom_Hypnos/Loom_Hypnos.h"

// To acquire a function call summary, just add INSTRUMENT() to the top of the
// relevant function.
#define INSTRUMENT() FunctionInstrumentor _instrumentor##__LINE__( \
    __FILE__, __func__, __LINE__);

// DEPRECATED - use INSTRUMENT
#define FUNCTION_START FunctionInstrumentor _instrumentor##__LINE__( \
    __FILE__, __func__, __LINE__);
// DEPRECATED - use INSTRUMENT
#define FUNCTION_END

struct LogContext {
    const char *file;
    const char *func;
    unsigned long lineNum;
    bool silent;
    const char *level; // must have static lifetime
};

#define GENERIC_LOG(silent, level, msg) do {                                \
    LogContext log{__FILE__, __func__, __LINE__, silent, level};            \
    Logger::getInstance()->genericLog(log, msg);                            \
} while (false)

#define LOG(msg)        GENERIC_LOG(false,   "DEBUG", msg)
#define SLOG(msg)       GENERIC_LOG( true,   "DEBUG", msg)
#define WARNING(msg)    GENERIC_LOG(false, "WARNING", msg)
#define ERROR(msg)      GENERIC_LOG(false,   "ERROR", msg)
 
#define LOG_LONG(msg) Logger::getInstance()->logLong(msg, false)

#define GENERIC_LOGF(silent, level, msg, ...) do {                          \
    LogContext log{__FILE__, __func__, __LINE__, silent, level};            \
    char buf[OUTPUT_SIZE];                                                  \
    snprintf_P(buf, sizeof(buf), PSTR(msg),##__VA_ARGS__);                  \
    Logger::getInstance()->genericLog(log, buf);                            \
} while (false)

#define LOGF(msg, ...)      GENERIC_LOGF(false,   "DEBUG", msg,##__VA_ARGS__)
#define SLOGF(msg, ...)     GENERIC_LOGF(false,   "DEBUG", msg,##__VA_ARGS__)
#define WARNINGF(msg, ...)  GENERIC_LOGF(false, "WARNING", msg,##__VA_ARGS__)
#define ERRORF(msg, ...)    GENERIC_LOGF(false,   "ERROR", msg,##__VA_ARGS__)

#define ENABLE_SD_LOGGING Logger::getInstance()->enableSD()
#define ENABLE_FUNC_SUMMARIES Logger::getInstance()->enableSummaries()

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

    static Logger* instance;
    SDManager* sdInst = nullptr;
    Loom_Hypnos* hypnosInst = nullptr;

    Logger() {};

    /**
     * Generic log function - prints to Serial and logs to SD
     * 
     * @param message The message we want to log
     * @param silent Whether to print to the serial monitor
     */
    void log(char* message, bool silent) {
        char filePath[100];
        
        // If we want to actually print to serial
        if (!silent)
            Serial.println(message);

        snprintf_P(filePath, 100, PSTR("/debug/output_%i.log"), 
                   sdInst->getCurrentFileNumber());

        // Log as long as we have given it a SD card instance
        if (sdInst != nullptr && enableSDLogging)
            sdInst->writeLineToFile(filePath, message);
    }

public:
    // Deleting copy constructor.
    Logger(const Logger &obj) = delete;

    /* Get an instance of the logger object */
    static Logger *getInstance() {
        if (instance == nullptr)
            instance = new Logger();

        return instance;
    };

    /**
     * Set the instance of the SD Manager
     * @param manager Pointer to the SD manager to allow us to utilize SD logging functionality
     */
    void setSDManager(SDManager* manager) { sdInst = manager; };

    /**
     * Set the instance of the Hypnos, this should be used if you want the current timestamp added to the front of the logger output
     * @param hypnos Pointer to the hypnos object this also sets the sdInst
     */
    void setHypnos(Loom_Hypnos* hypnos) { 
        hypnosInst = hypnos; 
        sdInst = hypnos->getSDManager();
    };

    void genericLog(LogContext log, const __FlashStringHelper* msg) {
        char buf[OUTPUT_SIZE];
        memcpy_P(buf, msg, OUTPUT_SIZE);
        genericLog(log, buf);
    }

    void genericLog(LogContext log, const char *msg) {
        char logMessage[OUTPUT_SIZE];
        char fileName[260] = {};
        truncateFileName(fileName, log.file);

        if (hypnosInst != nullptr && hypnosInst->isRTCInitialized()) {
            DateTime t = hypnosInst->getCurrentTime();
            char tbuf[21];
            hypnosInst->dateTime_toString(t, tbuf);
            snprintf_P(
                logMessage, OUTPUT_SIZE, 
                PSTR("[%s] [%s] [%s:%s:%u] %s"), 
                tbuf, 
                log.level, fileName, log.func, log.lineNum, msg
            );
        } else {
            snprintf_P(
                logMessage, OUTPUT_SIZE, 
                PSTR("[%s] [%s:%s:%u] %s"), 
                log.level, fileName, log.func, log.lineNum, msg
            );
        }

        this->log(logMessage, log.silent);
    }

    /*
     * Directly log a message
     */
    void logLong(char* message, bool silent){
        log(message, silent);
    };

    /* Enable function summaries to view memory usage */
    void enableSummaries(){ enableFunctionSummaries = true; };

    /* Save flash write by not logging everything to SD */
    void enableSD(){ enableSDLogging = true; };

    bool shouldLogSummaries() {
        return enableFunctionSummaries && sdInst != nullptr && enableSDLogging;
    }

    /**
     * Truncate the __FILE__ output to just show the name instead of the whole path
     * Expects dst to be at least as large as src
     */
    static void truncateFileName(char *dst, const char* src) {
        const char *name = strrchr(src, '\\');

        if (name == nullptr) name = strrchr(src, '/');
        else name += 1; // shift off '\'

        if (name == nullptr) name = src;
        else name += 1; // shift off '/'

        memcpy(dst, name, strlen(name));
    }
};

// Offset to correct for the difference in the size of the FunctionInstrumentor
// constructor and destructor stack frames.
const int CONSTRUCTOR_SIZE_DIFFERENCE = 320;

class FunctionInstrumentor {
public:
    // delete all other constructors
    FunctionInstrumentor(const FunctionInstrumentor&) = delete;
    FunctionInstrumentor& operator=(const FunctionInstrumentor&) = delete;

    FunctionInstrumentor(
        const char* file, const char* func, int lineNum
    ) {
        int freemem = freeMemory() + CONSTRUCTOR_SIZE_DIFFERENCE;

        Logger *logger = Logger::getInstance();

        logger->stackDepth++;

        if (!logger->shouldLogSummaries()) return;

        char fileName[300] = {};
        Logger::truncateFileName(fileName, file);

        char logfileName[100];
        snprintf_P(
            logfileName, 
            sizeof(logfileName), 
            PSTR("/debug/funcSummaries_%i.log"), 
            logger->sdInst->getCurrentFileNumber()
        );

        char output[300] = {};
        snprintf_P(output, sizeof(output), PSTR("start,%d,%s,%s,%d,%d,%lu"), 
                   logger->stackDepth - 1, fileName, func, lineNum, freemem, millis());
        bool worked = logger->sdInst->writeLineToFile(logfileName, output);
        if (!worked) WARNINGF("Could not write instrumentation to file!");
    }

    ~FunctionInstrumentor() {
        int freemem = freeMemory();

        Logger *logger = Logger::getInstance();

        logger->stackDepth--;

        if (!logger->shouldLogSummaries()) return;

        char logfileName[100];
        snprintf_P(
            logfileName, 
            sizeof(logfileName), 
            PSTR("/debug/funcSummaries_%i.log"), 
            logger->sdInst->getCurrentFileNumber()
        );

        char output[300];
        snprintf_P(output, sizeof(output), PSTR("end,%d, , , ,%d,%lu"), 
                   logger->stackDepth, freemem, millis());
        bool worked = logger->sdInst->writeLineToFile(logfileName, output);
        if (!worked) WARNINGF("Could not write instrumentation to file!");
    }
};


