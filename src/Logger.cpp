#include "Logger.h"

/*
 * Logger's SD/Hypnos-aware implementation lives in this translation unit so
 * Logger.h can stay lightweight. This preserves logging behavior while avoiding
 * a library-wide transitive include of SDManager/SPI from the public logger API.
 */
#include "Hardware/Loom_Hypnos/Loom_Hypnos.h"
#include "Hardware/Loom_Hypnos/SDManager.h"

Logger* Logger::instance = nullptr;

namespace {

    void appendString(char* destination, size_t destinationSize, const char* source) {
        if(destinationSize == 0) {
            return;
        }

        if(source == nullptr) {
            source = "";
        }

        const size_t currentLength = strlen(destination);
        if(currentLength >= destinationSize - 1) {
            return;
        }

        strncpy(destination + currentLength, source, destinationSize - currentLength - 1);
        destination[destinationSize - 1] = '\0';
    }

    void appendUnsignedLong(char* destination, size_t destinationSize, unsigned long value) {
        char valueBuffer[24];
        snprintf_P(valueBuffer, sizeof(valueBuffer), PSTR("%lu"), value);
        appendString(destination, destinationSize, valueBuffer);
    }

    void appendInt(char* destination, size_t destinationSize, int value) {
        char valueBuffer[16];
        snprintf_P(valueBuffer, sizeof(valueBuffer), PSTR("%d"), value);
        appendString(destination, destinationSize, valueBuffer);
    }

    void copyFlashString(const __FlashStringHelper* message, char* buffer, size_t bufferSize) {
        if(bufferSize == 0) {
            return;
        }

        if(message == nullptr) {
            buffer[0] = '\0';
            return;
        }

        strncpy_P(buffer, reinterpret_cast<const char*>(message), bufferSize - 1);
        buffer[bufferSize - 1] = '\0';
    }
}

void Logger::log(const char* message, bool silent) {
    char filePath[100];

    if(message == nullptr) {
        message = "";
    }

    if(!silent) {
        Serial.println(message);
    }

    if(sdInst != nullptr && enableSDLogging) {
        snprintf_P(filePath, sizeof(filePath), PSTR("/debug/output_%d.log"), sdInst->getCurrentFileNumber());
        sdInst->writeLineToFile(filePath, message);
    }
}

void Logger::truncateFileName(const char* fileName, char array[260]) {
    if(fileName == nullptr) {
        array[0] = '\0';
        return;
    }

    const char* lastOccurrence = strrchr(fileName, '\\');
    if(lastOccurrence == nullptr) {
        lastOccurrence = strrchr(fileName, '/');
    }

    const char* leafName = (lastOccurrence != nullptr) ? lastOccurrence + 1 : fileName;
    strncpy(array, leafName, 259);
    array[259] = '\0';
}

Logger* Logger::getInstance() {
    if(instance == nullptr) {
        instance = new Logger();
    }

    return instance;
}

void Logger::setSDManager(SDManager* manager) {
    sdInst = manager;
}

void Logger::setHypnos(Loom_Hypnos* hypnos) {
    hypnosInst = hypnos;
    sdInst = (hypnos != nullptr) ? hypnos->getSDManager() : nullptr;
}

void Logger::debugLog(const __FlashStringHelper* message, bool silent, const char* file, const char* func, unsigned long lineNumber) {
    char buff[150];
    copyFlashString(message, buff, sizeof(buff));
    genericLog("DEBUG", buff, silent, file, func, lineNumber);
}

void Logger::warningLog(const __FlashStringHelper* message, bool silent, const char* file, const char* func, unsigned long lineNumber) {
    char buff[150];
    copyFlashString(message, buff, sizeof(buff));
    genericLog("WARNING", buff, silent, file, func, lineNumber);
}

void Logger::errorLog(const __FlashStringHelper* message, bool silent, const char* file, const char* func, unsigned long lineNumber) {
    char buff[150];
    copyFlashString(message, buff, sizeof(buff));
    genericLog("ERROR", buff, silent, file, func, lineNumber);
}

void Logger::genericLog(const char* level, const char* message, bool silent, const char* file, const char* func, unsigned long lineNumber) {
    char logMessage[OUTPUT_SIZE];
    char fileName[260];

    logMessage[0] = '\0';
    truncateFileName(file, fileName);

    if(hypnosInst != nullptr && hypnosInst->isRTCInitialized()) {
        appendString(logMessage, sizeof(logMessage), "[");
        appendString(logMessage, sizeof(logMessage), hypnosInst->getCurrentTime().text());
        appendString(logMessage, sizeof(logMessage), "] ");
    }

    appendString(logMessage, sizeof(logMessage), "[");
    appendString(logMessage, sizeof(logMessage), level);
    appendString(logMessage, sizeof(logMessage), "] [");
    appendString(logMessage, sizeof(logMessage), fileName);
    appendString(logMessage, sizeof(logMessage), ":");
    appendString(logMessage, sizeof(logMessage), func);
    appendString(logMessage, sizeof(logMessage), ":");
    appendUnsignedLong(logMessage, sizeof(logMessage), lineNumber);
    appendString(logMessage, sizeof(logMessage), "] ");
    appendString(logMessage, sizeof(logMessage), message);

    log(logMessage, silent);
}

void Logger::logLong(const char* message, bool silent) {
    log(message, silent);
}

void Logger::startFunction(const char* file, const char* func, unsigned long num, int freeMemory) {
    if(!enableFunctionSummaries) {
        return;
    }

    char fileName[260];
    truncateFileName(file, fileName);

    functionInfo* newFunction = static_cast<functionInfo*>(malloc(sizeof(functionInfo)));
    if(newFunction == nullptr) {
        return;
    }

    strncpy(newFunction->fileName, fileName, sizeof(newFunction->fileName) - 1);
    newFunction->fileName[sizeof(newFunction->fileName) - 1] = '\0';
    strncpy(newFunction->funcName, (func != nullptr) ? func : "", sizeof(newFunction->funcName) - 1);
    newFunction->funcName[sizeof(newFunction->funcName) - 1] = '\0';
    newFunction->lineNumber = num;
    newFunction->netMemoryUsage = freeMemory;
    newFunction->time = millis();
    newFunction->indentCount = static_cast<int>(callStack.size());
    callStack.push(newFunction);
}

void Logger::endFunction(int freeMemory) {
    if(!enableFunctionSummaries || callStack.empty()) {
        return;
    }

    functionInfo* info = callStack.front();
    callStack.pop();

    if(info == nullptr) {
        return;
    }

    if(sdInst != nullptr && enableSDLogging) {
        const unsigned long elapsedTime = millis() - info->time;
        const int percentage = static_cast<int>((static_cast<float>(info->netMemoryUsage) / 32000.0f) * 100.0f);
        const int netUsage = info->netMemoryUsage - freeMemory;

        char fileName[100];
        char output[300];
        char indents[11];

        memset(indents, '\0', sizeof(indents));
        if(info->indentCount > 10) {
            info->indentCount = 10;
        }
        memset(indents, '\t', static_cast<size_t>(info->indentCount));

        output[0] = '\0';
        appendString(output, sizeof(output), indents);
        appendString(output, sizeof(output), "[");
        appendString(output, sizeof(output), info->fileName);
        appendString(output, sizeof(output), ":");
        appendString(output, sizeof(output), info->funcName);
        appendString(output, sizeof(output), "] Summary\n");
        appendString(output, sizeof(output), indents);
        appendString(output, sizeof(output), "\tInitial Free Memory: ");
        appendInt(output, sizeof(output), info->netMemoryUsage);
        appendString(output, sizeof(output), " B (");
        appendInt(output, sizeof(output), percentage);
        appendString(output, sizeof(output), " % Free)\n");
        appendString(output, sizeof(output), indents);
        appendString(output, sizeof(output), "\tEnding Free Memory: ");
        appendInt(output, sizeof(output), freeMemory);
        appendString(output, sizeof(output), " B\n");
        appendString(output, sizeof(output), indents);
        appendString(output, sizeof(output), "\tNet Usage: ");
        appendInt(output, sizeof(output), netUsage);
        appendString(output, sizeof(output), " B\n");
        appendString(output, sizeof(output), indents);
        appendString(output, sizeof(output), "\tElapsed Time: ");
        appendUnsignedLong(output, sizeof(output), elapsedTime);
        appendString(output, sizeof(output), " MS");

        snprintf_P(fileName, sizeof(fileName), PSTR("/debug/funcSummaries_%d.log"), sdInst->getCurrentFileNumber());
        sdInst->writeLineToFile(fileName, output);
    }

    free(info);
}

void Logger::enableSummaries() {
    enableFunctionSummaries = true;
}

void Logger::enableSD() {
    enableSDLogging = true;
}
