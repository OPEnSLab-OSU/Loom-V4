#pragma once

#include <queue>
#include <MemoryFree.h>

#include "Module.h"

/*
 * Keep Logger.h free of Hypnos/SDManager headers. The logging macros are used
 * broadly; including Hypnos here would pull SD/SPI and related vendor headers
 * into nearly every translation unit.
 */
class Loom_Hypnos;
class SDManager;

#define FUNCTION_START Logger::getInstance()->startFunction(__FILE__, __func__, __LINE__, freeMemory())     // Marks the start of a function
#define FUNCTION_END Logger::getInstance()->endFunction(freeMemory())                                       // Marks the end of a function

#define SLOG(msg) Logger::getInstance()->debugLog(msg, true, __FILE__, __func__, __LINE__)                  // Log a message without printing to the serial
#define LOG(msg) Logger::getInstance()->debugLog(msg, false, __FILE__, __func__, __LINE__)                  // Log a generic message
#define LOG_LONG(msg) Logger::getInstance()->logLong(msg, false)                                            // Log a long message
#define ERROR(msg) Logger::getInstance()->errorLog(msg, false, __FILE__, __func__, __LINE__)                // Log an error message
#define WARNING(msg) Logger::getInstance()->warningLog(msg, false, __FILE__, __func__, __LINE__)            // Log a warning message

#define LOGF(msg, ...) { \
    char buf[OUTPUT_SIZE]; \
    snprintf_P(buf, sizeof(buf), PSTR(msg), __VA_ARGS__); \
    LOG(buf); \
}

#define ERRORF(msg, ...) { \
    char buf[OUTPUT_SIZE]; \
    snprintf_P(buf, sizeof(buf), PSTR(msg), __VA_ARGS__); \
    ERROR(buf); \
}

#define WARNINGF(msg, ...) { \
    char buf[OUTPUT_SIZE]; \
    snprintf_P(buf, sizeof(buf), PSTR(msg), __VA_ARGS__); \
    WARNING(buf); \
}

#define ENABLE_SD_LOGGING Logger::getInstance()->enableSD()                                                 // Enable SD logging of debug information
#define ENABLE_FUNC_SUMMARIES Logger::getInstance()->enableSummaries()                                      // Enable logging of function mem usage summaries

/**
 * Arduino Logger class that allows for standardized log outputs as well as function memory usage summaries to find memory leaks that may lead to unexpected crashing
 *
 * @author Will Richards
 */
class Logger {
    private:

        /**
         * Function Info - Contains important information of the run state of the function
         * fileName - Name of the file that we are in
         * funcName - Name of the function that we are in
         * lineNumber - The current line number of the function
         * netMemoryUsage - The amount of memory that was allocated or deallocated in that function alone (bytes)
         * time - The time the function took to return (ms)
         * indentCount - This is allows for better formatting in the funcSummaries output
         */
        struct functionInfo {
            char fileName[260];
            char funcName[260];
            unsigned long lineNumber;
            int netMemoryUsage;
            unsigned long time;
            int indentCount;
        };

        // Function call list
        std::queue<functionInfo*> callStack;
        int indentNum = 0;

        // Whether or not to use the SD card or log function summaries
        bool enableFunctionSummaries = false;
        bool enableSDLogging = false;

        static Logger* instance;
        SDManager* sdInst = nullptr;
        Loom_Hypnos* hypnosInst = nullptr;

        /**
         * Generic Log Function prints to Serial and Logs to SD
         *
         * @param message The message we want to log
         * @param silent Whether or not the message gets print to the serial monitor
         */
        void log(const char* message, bool silent);

        /**
         * Truncate the __FILE__ output to just show the name instead of the whole path
         * @param fileName String to truncate
         *
         * @return Pointer to a malloced char*
         */
        void truncateFileName(const char* fileName, char array[260]);

        Logger() {};

    public:
        // Deleting copy constructor.
        Logger(const Logger &obj) = delete;

        /* Get an instance of the logger object */
        static Logger* getInstance();

        /**
         * Set the instance of the SD Manager
         * @param manager Pointer to the SD manager to allow us to utilize SD logging functionality
         */
        void setSDManager(SDManager* manager);

        /**
         * Set the instance of the Hypnos, this should be used if you want the current timestamp added to the front of the logger output
         * @param hypnos Pointer to the hypnos object this also sets the sdInst
         */
        void setHypnos(Loom_Hypnos* hypnos);

        /**
         * Logs a Debug Message to the SD card and the serial monitor
         * @param message Message to log
         * @param silent If set to silent it will not appear in the serial monitor
         * @param lineNumber The current line number this log is on
         */
        void debugLog(const char* message, bool silent, const char* file, const char* func, unsigned long lineNumber) {
            genericLog("DEBUG", message, silent, file, func, lineNumber);
        };

        void debugLog(const String& message, bool silent, const char* file, const char* func, unsigned long lineNumber) {
            genericLog("DEBUG", message.c_str(), silent, file, func, lineNumber);
        };

        void debugLog(short message, bool silent, const char* file, const char* func, unsigned long lineNumber) {
            char buff[16];
            snprintf_P(buff, sizeof(buff), PSTR("%d"), message);
            genericLog("DEBUG", buff, silent, file, func, lineNumber);
        };

        void debugLog(unsigned short message, bool silent, const char* file, const char* func, unsigned long lineNumber) {
            char buff[16];
            snprintf_P(buff, sizeof(buff), PSTR("%u"), message);
            genericLog("DEBUG", buff, silent, file, func, lineNumber);
        };

        void debugLog(int message, bool silent, const char* file, const char* func, unsigned long lineNumber) {
            char buff[16];
            snprintf_P(buff, sizeof(buff), PSTR("%d"), message);
            genericLog("DEBUG", buff, silent, file, func, lineNumber);
        };

        void debugLog(unsigned int message, bool silent, const char* file, const char* func, unsigned long lineNumber) {
            char buff[16];
            snprintf_P(buff, sizeof(buff), PSTR("%u"), message);
            genericLog("DEBUG", buff, silent, file, func, lineNumber);
        };

        void debugLog(long message, bool silent, const char* file, const char* func, unsigned long lineNumber) {
            char buff[24];
            snprintf_P(buff, sizeof(buff), PSTR("%ld"), message);
            genericLog("DEBUG", buff, silent, file, func, lineNumber);
        };

        void debugLog(unsigned long message, bool silent, const char* file, const char* func, unsigned long lineNumber) {
            char buff[24];
            snprintf_P(buff, sizeof(buff), PSTR("%lu"), message);
            genericLog("DEBUG", buff, silent, file, func, lineNumber);
        };

        void debugLog(float message, bool silent, const char* file, const char* func, unsigned long lineNumber) {
            String buff(message, 6);
            genericLog("DEBUG", buff.c_str(), silent, file, func, lineNumber);
        };

        void debugLog(double message, bool silent, const char* file, const char* func, unsigned long lineNumber) {
            String buff(message, 6);
            genericLog("DEBUG", buff.c_str(), silent, file, func, lineNumber);
        };

        /**
         * Logs an Error Message to the SD card and the serial monitor
         * @param message Message to log
         * @param silent If set to silent it will not appear in the serial monitor
         * @param lineNumber The current line number this log is on
         */
        void errorLog(const char* message, bool silent, const char* file, const char* func, unsigned long lineNumber) {
            genericLog("ERROR", message, silent, file, func, lineNumber);
        };

        /**
         * Logs a Warning Message to the SD card and the serial monitor
         * @param message Message to log
         * @param silent If set to silent it will not appear in the serial monitor
         * @param lineNumber The current line number this log is on
         */
        void warningLog(const char* message, bool silent, const char* file, const char* func, unsigned long lineNumber) {
            genericLog("WARNING", message, silent, file, func, lineNumber);
        };

        /**
         * Logs a Debug Message stored in flash to the SD card and the serial monitor
         */
        void debugLog(const __FlashStringHelper* message, bool silent, const char* file, const char* func, unsigned long lineNumber);

        /**
         * Logs a Warning Message stored in flash to the SD card and the serial monitor
         */
        void warningLog(const __FlashStringHelper* message, bool silent, const char* file, const char* func, unsigned long lineNumber);

        /**
         * Logs an Error Message stored in flash to the SD card and the serial monitor
         */
        void errorLog(const __FlashStringHelper* message, bool silent, const char* file, const char* func, unsigned long lineNumber);

        /**
         * Generic logging function to cut down on redundant code in each log function
         * @param level Strings representing different log levels eg. DEBUG, WARNING, ERROR
         * @param message The actual message we want to log
         * @param silent Whether or not we want to actually print the data to the Serial monitor or just log it to the SD card
         * @param file Name of the file in which the log was called
         * @param func Name of the function in which the log was called
         * @param lineNumber The line number that the log was called on
         */
        void genericLog(const char* level, const char* message, bool silent, const char* file, const char* func, unsigned long lineNumber);

        /*
            Log an entire char* instead of fixing it to an array you must construct your message before passing it into this function
         */
        void logLong(const char* message, bool silent);

        /**
         * Marks the start of a new function
         * @param file File name that the function is in
         * @param func Function name that this call is in
         * @param num Current line number in the file of which this call is located
         */
        void startFunction(const char* file, const char* func, unsigned long num, int freeMemory);

        /**
         * Marks the end of a function, logs summary to SD card
         * @param freeMemory The amount of available memory on the device
         */
        void endFunction(int freeMemory);

        /* Enable function summaries to view memory usage */
        void enableSummaries();

        /* Save flash write by not logging everything to SD */
        void enableSD();
};
