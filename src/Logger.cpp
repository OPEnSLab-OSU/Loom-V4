#include "Logger.h"

Logger *Logger::instance = nullptr;

char Logger::logFile[100] = {};
char Logger::activeFile[260] = {};
char Logger::timeBuf[21] = {};
char Logger::flashMessage[OUTPUT_SIZE] = {};
char Logger::logMessage[OUTPUT_SIZE] = {};

char FunctionInstrumentor::activeFile[300] = {};
char FunctionInstrumentor::logFile[100] = {};
char FunctionInstrumentor::output[300] = {};
