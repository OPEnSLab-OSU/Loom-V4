#include "../Logger.h"
#include "../Hardware/Loom_Hypnos/SDManager.h"
#include "../Utilities/Loom_MemoryUtils.h"

// Only ENABLE_FUNC_SUMMARIES / enableSummaries() pulls this writer into a sketch.
void Logger::enableSummaries() { summaryWriter = &FunctionInstrumentor::writeSummary; }

void FunctionInstrumentor::writeSummary(Logger *logger, bool starting, const char *file,
                                        const char *func, int lineNum) {
    const int freemem = LoomMemory::freeMemoryBytes();
    char logfileName[SDManager::DEBUG_FILENAME_SIZE];
    if (!logger->sdInst->getDebugFilePath(logfileName, sizeof(logfileName),
                                         loomDebugFiles::Kind::Summaries)) return;

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
