#include "Logger.h"
#include "Hardware/Loom_Hypnos/Loom_Hypnos.h"


bool Logger::flushTrace() {
    return traceCallbacks != nullptr && traceCallbacks->flush(trace);
}

void Logger::setTraceStorageAvailable(bool available) {
    if (traceCallbacks != nullptr) {
        traceCallbacks->storage(trace, available);
    }
}

void Logger::traceObject(const char *name, const void *address, uint32_t bytes, const void *owner,
                         int port, int i2cAddress, bool ready) {
    if (traceCallbacks != nullptr) {
        traceCallbacks->object(trace, name, address, bytes, owner, port, i2cAddress, ready);
    }
}

void Logger::retireTraceObject(const void *address) {
    if (traceCallbacks != nullptr) {
        traceCallbacks->retire(trace, address);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void Logger::log(char *message, bool silent) {
    // If we want to actually print to serial
    if (!silent) {
        Serial.println(message);
    }

    // Log as long as we have given it a SD card instance
    if (sdInst != nullptr && enableSDLogging && sdInst->canWriteDebugLogs()) {
        char filePath[SDManager::DEBUG_FILENAME_SIZE];
        if (!sdInst->getDebugFilePath(filePath, sizeof(filePath), loomDebugFiles::Kind::Text) ||
            !sdInst->writeLineToFile(filePath, message)) {
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
        char filePath[SDManager::DEBUG_FILENAME_SIZE];
        if (!sdInst->getDebugFilePath(filePath, sizeof(filePath), loomDebugFiles::Kind::Text) ||
            !sdInst->writeJsonToFile(filePath, document)) {
            Serial.println(F("Could not save JSON to the SD debug log!"));
        }
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Logger::shouldLogSummaries() {
    // Summaries use SD only. Skip them before initialization and after an uncertain append,
    // even while Serial output and FUNCTION_START/FUNCTION_END tracking remain active.
    return debugOutputEnabled && summaryWriter != nullptr && enableSDLogging && sdInst != nullptr &&
           sdInst->canWriteDebugLogs();
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
