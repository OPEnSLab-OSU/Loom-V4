#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <MemoryFree.h>
#include "Hardware/Loom_Hypnos/Loom_Hypnos.h"

// #define FUNCTION_START Logger::getInstance()->startFunction(__FILE__, __func__, __LINE__, freeMemory())     // Marks the start of a function
// #define FUNCTION_END Logger::getInstance()->endFunction(freeMemory())                                       // Marks the end of a function
#define FUNCTION_START FunctionInstrumentor _instrumentor##__LINE__(__FILE__, __func__, __LINE__);
#define FUNCTION_END

#define SLOG(msg) Logger::getInstance()->debugLog(msg, true, __FILE__, __func__, __LINE__)                  // Log a message without printing to the serial
#define LOG(msg) Logger::getInstance()->debugLog(msg, false, __FILE__, __func__, __LINE__)                  // Log a generic message
#define LOG_LONG(msg) Logger::getInstance()->logLong(msg, false)                                            // Log a long message
#define ERROR(msg) Logger::getInstance()->errorLog(msg, false, __FILE__, __func__, __LINE__)                // Log an error message
#define WARNING(msg) Logger::getInstance()->warningLog(msg, false, __FILE__, __func__, __LINE__)            // Log a warning message

#define LOGF(msg, ...) { \
    char buf[OUTPUT_SIZE]; \
    snprintf_P(buf, sizeof(buf), PSTR(msg), ##__VA_ARGS__); \
    LOG(buf); \
}

#define ERRORF(msg, ...) { \
    char buf[OUTPUT_SIZE]; \
    snprintf_P(buf, sizeof(buf), PSTR(msg), ##__VA_ARGS__); \
    ERROR(buf); \
}

#define WARNINGF(msg, ...) { \
    char buf[OUTPUT_SIZE]; \
    snprintf_P(buf, sizeof(buf), PSTR(msg), ##__VA_ARGS__); \
    WARNING(buf); \
}

#define ENABLE_SD_LOGGING Logger::getInstance()->enableSD()                                                 // Enable SD logging of debug information
#define ENABLE_FUNC_SUMMARIES Logger::getInstance()->enableSummaries()                                      // Enable logging of function mem usage summaries


/**
 * Arduino Logger class that allows for standardized log outputs as well as function memory usage summaries to find memory leaks that may lead to unexpected crashing
 * 
 * @author Will Richards 
 */ 
class Logger{
    private:
        friend class FunctionInstrumentor;
        
        unsigned int stackDepth = 0;

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
         * @param silent Wether or not the message gets print to the serial monitor
        */
        void log(char* message, bool silent){
            char filePath[100];
            
            // If we want to actually print to serial
            if(!silent)
                Serial.println(message);

            snprintf_P(filePath, 100, PSTR("/debug/output_%i.log"), sdInst->getCurrentFileNumber());
            // Log as long as we have given it a SD card instance
            if(sdInst != nullptr && enableSDLogging)
                sdInst->writeLineToFile(filePath, message);
        }


        Logger() {};
    
    public:
        // Deleting copy constructor.
        Logger(const Logger &obj) = delete;

        /* Get an instance of the logger object */
        static Logger *getInstance(){
            if(instance == nullptr){
                instance = new Logger();
                return instance;
            }
            else{
                return instance;
            }
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

        /**
         * Logs a Debug Message to the SD card and the serial monitor
         * @param message Message to log
         * @param silent If set to silent it will not appear in the serial monitor
         * @param lineNumber The current line number this log is on
        */
        void debugLog(const char* message, bool silent, const char* file, const char* func, unsigned long lineNumber){
            genericLog("DEBUG", message, silent, file, func, lineNumber);
        };

        /**
         * Logs an Error Message to the SD card and the serial monitor
         * @param message Message to log
         * @param silent If set to silent it will not appear in the serial monitor
         * @param lineNumber The current line number this log is on
        */
        void errorLog(const char* message, bool silent, const char* file, const char* func, unsigned long lineNumber){
            genericLog("ERROR", message, silent, file, func, lineNumber);
        };

         /**
         * Logs a Warning Message to the SD card and the serial monitor
         * @param message Message to log
         * @param silent If set to silent it will not appear in the serial monitor
         * @param lineNumber The current line number this log is on
        */
        void warningLog(const char* message, bool silent, const char* file, const char* func, unsigned long lineNumber){
            genericLog("WARNING", message, silent, file, func, lineNumber);
        };

        /**
         * Logs a Debug Message to the SD card and the serial monitor
         * @param message Message to log
         * @param silent If set to silent it will not appear in the serial monitor
         * @param lineNumber The current line number this log is on
        */
        void debugLog(const __FlashStringHelper* message, bool silent, const char* file, const char* func, unsigned long lineNumber){
            char buff[150];
            memcpy_P(buff, message, 150);
            genericLog("DEBUG", buff, silent, file, func, lineNumber);
        };

         /**
         * Logs a Warning Message to the SD card and the serial monitor
         * @param message Message to log
         * @param silent If set to silent it will not appear in the serial monitor
         * @param lineNumber The current line number this log is on
        */
        void warningLog(const __FlashStringHelper* message, bool silent, const char* file, const char* func, unsigned long lineNumber){
            char buff[150];
		    memcpy_P(buff, message, 150);
            genericLog("WARNING", buff, silent, file, func, lineNumber);
        };

         /**
         * Logs an Error Message to the SD card and the serial monitor
         * @param message Message to log
         * @param silent If set to silent it will not appear in the serial monitor
         * @param lineNumber The current line number this log is on
        */
        void errorLog(const __FlashStringHelper* message, bool silent, const char* file, const char* func, unsigned long lineNumber){
            char buff[150];
		    memcpy_P(buff, message, 150);
            genericLog("ERROR", buff, silent, file, func, lineNumber);
        };

        /**
         * Generic logging function to cut down on redundant code in each log function
         * @param level Strings representing different log levels eg. DEBUG, WARNING, ERROR
         * @param message The actual message we want to log
         * @param silent Whether or not we want to actually print the data to the Serial monitor or just log it to the SD card
         * @param file Name of the file in which the log was called
         * @param func Name of the function in which the log was called
         * @param lineNumber The line number that the log was called on
        */
        void genericLog(const char* level, const char* message, bool silent, const char* file, const char* func, unsigned long lineNumber){
            char logMessage[OUTPUT_SIZE];
            char fileName[260] = {};
            truncateFileName(fileName, file);      
            
            /* If the hypnos is not included or the hypnos is included but the RTC hasn't been initialized yet we want to print without the time */
            if(hypnosInst == nullptr || (hypnosInst != nullptr && !hypnosInst->isRTCInitialized()))
                snprintf_P( logMessage, 
                            OUTPUT_SIZE, 
                            PSTR("[%s] [%s:%s:%u] %s"), 
                            level, 
                            fileName, 
                            func, 
                            lineNumber, 
                            message
                        );
            else
                snprintf_P( logMessage, 
                            OUTPUT_SIZE, 
                            PSTR("[%s] [%s] [%s:%s:%u] %s"), 
                            hypnosInst->getCurrentTime().text(), 
                            level, 
                            fileName, 
                            func, 
                            lineNumber, 
                            message
                        );
            /* Log the message*/          
            log(logMessage, silent);
        }

        /*
            Log an entire char* instead of fixing it to an array you must construct your message before passing it into this function
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
    FunctionInstrumentor(
        const char* file, const char* func, int lineNum
    ) : marker('M')
    {
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

private:
    char marker; // exists only to normalize memory
};


