#include "Logger.h"
#if defined(LOOM_ENABLE_TRACE) && LOOM_ENABLE_TRACE
#include "Diagnostics/Loom_Trace.h"
#endif
#include "Hardware/Loom_Hypnos/Loom_Hypnos.h"
#include "Utilities/Loom_MemoryUtils.h"

#if defined(LOOM_ENABLE_TRACE) && LOOM_ENABLE_TRACE
bool Logger::flushTrace() { return trace != nullptr && trace->flush(); }

void FunctionInstrumentor::beginTrace(Logger *logger, const char *file, const char *func, int line,
                                      const void *object) {
    if (logger->debugOutputEnabled && logger->trace != nullptr &&
        logger->trace->enter(file, func, static_cast<uint32_t>(line), object)) {
        trace = logger->trace;
    }
}

void FunctionInstrumentor::endTrace() {
    if (trace != nullptr) {
        trace->leave();
    }
}
#else
bool Logger::flushTrace() { return false; }
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
void Logger::log(char *message, bool silent) {
    char filePath[32];

    // If we want to actually print to serial
    if (!silent) {
        Serial.println(message);
    }

    // Log as long as we have given it a SD card instance
    if (sdInst != nullptr && enableSDLogging && sdInst->canWriteDebugLogs()) {
        snprintf_P(filePath, sizeof(filePath), PSTR("/debug/output_%i.log"),
                   sdInst->getCurrentFileNumber());
        if (!sdInst->writeLineToFile(filePath, message)) {
            Serial.println(F("Could not save message to the SD debug log!"));
        }
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
size_t Logger::writePrefix(char *destination, size_t destinationSize, LogContext log) {
    if (destination == nullptr || destinationSize == 0) {
        return 0;
    }

    const char *fileName = baseFileName(log.file);
    int written = 0;
    Logger *logger = Logger::getInstance();
    DateTime time;
    const bool hasTime = logger->rtcTimestampsEnabled && logger->hypnosInst != nullptr &&
                         logger->hypnosInst->tryGetCurrentTime(time);
    if (hasTime) {
        char timestamp[21];
        logger->hypnosInst->dateTime_toString(time, timestamp);
        written = snprintf_P(destination, destinationSize, PSTR("[%s] [%s] [%s:%s:%lu] "),
                             timestamp, log.level, fileName, log.func, log.lineNum);
    } else {
        // A failed clock read must not make a real fault look as though it happened in 2000.
        written = snprintf_P(destination, destinationSize, PSTR("[%s] [%s:%s:%lu] "), log.level,
                             fileName, log.func, log.lineNum);
    }

    if (written <= 0) {
        return 0;
    }
    if (static_cast<size_t>(written) >= destinationSize) {
        return destinationSize - 1;
    }
    return static_cast<size_t>(written);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Logger::setHypnos(Loom_Hypnos *hypnos) {
    hypnosInst = hypnos;
    sdInst = hypnos->getSDManager();
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Logger::logDocument(const DynamicJsonDocument &document) {
    if (!debugOutputEnabled) {
        return;
    }
    serializeJsonPretty(document, Serial);
    Serial.println();
    if (sdInst != nullptr && enableSDLogging && sdInst->canWriteDebugLogs()) {
        char filePath[32];
        snprintf_P(filePath, sizeof(filePath), PSTR("/debug/output_%i.log"),
                   sdInst->getCurrentFileNumber());
        if (!sdInst->writeJsonToFile(filePath, document)) {
            Serial.println(F("Could not save JSON to the SD debug log!"));
        }
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Logger::shouldLogSummaries() {
    // Summaries use SD only. Skip them before initialization and after an uncertain append,
    // even while Serial output and FUNCTION_START/FUNCTION_END tracking remain active.
    return debugOutputEnabled && enableFunctionSummaries && enableSDLogging && sdInst != nullptr &&
           sdInst->canWriteDebugLogs();
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void FunctionInstrumentor::writeSummary(Logger *logger, bool starting, const char *file,
                                        const char *func, int lineNum) {
    const int freemem = LoomMemory::freeMemoryBytes();
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

    if (!logger->sdInst->writeLineToFile(logfileName, output)) {
        Serial.println(F("Could not write instrumentation to file!"));
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
