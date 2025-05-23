#pragma once

#include <queue>
#include <MemoryFree.h>
#include "Hardware/Loom_Hypnos/Loom_Hypnos.h"

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
class Logger{
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
        struct functionInfo{
            char fileName[260];
            char funcName[260];
            unsigned long lineNumber;
            int netMemoryUsage;
            unsigned long time;
            int indentCount;
        };
        
        // Function call list
        std::queue<struct functionInfo*> callStack;
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

        /**
         * Truncate the __FILE__ output to just show the name instead of the whole path
         * @param fileName String to truncate
         * 
         * @return Pointer to a malloced char*
        */
        void truncateFileName(const char* fileName, char array[260]){
           
            int i;
            // Find the last \\ in the file path to know where the name is
            char *lastOccurance = strrchr(fileName, '\\');
            
            int lastSlash = lastOccurance-fileName+1;

            // Loop from the last slash to the end of the string
            for(i = lastSlash; i < strlen(fileName); i++){
                array[i-lastSlash] = fileName[i];
            }
            array[i-lastSlash] = '\0';
        };

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
            char fileName[260];
            truncateFileName(file, fileName);      
            
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


        /**
         * Marks the start of a new function
         * @param file File name that the function is in
         * @param func Function name that this call is in
         * @param num Current line number in the file of which this call is located
        */
        void startFunction(const char* file, const char* func, unsigned long num, int freeMemory){
            if(enableFunctionSummaries){
                // Log the start time of the function
                char fileName[260];
                memset(fileName, '\0', 260);
                truncateFileName(file, fileName);
                struct functionInfo* newFunction = (struct functionInfo*) malloc(sizeof(struct functionInfo));
                strncpy(newFunction->fileName, fileName, 260);
                strncpy(newFunction->funcName, func, 260);
                newFunction->lineNumber = num;
                newFunction->netMemoryUsage = freeMemory;
                newFunction->time = millis();
                newFunction->indentCount = callStack.size();
                callStack.push(newFunction);
            }
        };

        /**
         * Marks the end of a function, logs summary to SD card
         * @param freeMemory The amount of available memory on the device
        */
        void endFunction(int freeMemory){
            if(enableFunctionSummaries){
                char fileName[100];
                char file[260];
                char func[260];
                char output[300];
                char indents[10];

                // 0 fill the indents array
                memset(indents, '\0', 10);

                struct functionInfo* info = callStack.front();
                int memUsage = info->netMemoryUsage;
                unsigned long time = millis() - info->time;
                int percentage = ((float)memUsage / 32000.0) * 100;

                // Copy to internal variables
                strncpy(file, info->fileName, 260);
                strncpy(func, info->funcName, 260);

                // Cap the number of inents at the size of the array
                if(info->indentCount > 10){
                    info->indentCount = 10;
                }

                // Set the number of tab characters we need to add
                memset(indents, '\t', info->indentCount);
            
                // Pop the last function off the call stack
                free(info);
                callStack.pop();
                
                // Format the fileName and log output, this function uses 976 bytes
                snprintf_P(fileName, 100,PSTR("/debug/funcSummaries_%i.log"), sdInst->getCurrentFileNumber());
                snprintf_P(output, 300, PSTR("%s[%s:%s] Summary\n%s\tInitial Free Memory: %i B (%i %% Free)\n%s\tEnding Free Memory: %i B\n%s\tNet Usage: %i B\n%s\tElapsed Time: %u MS"), indents, file, func, indents, memUsage, percentage, indents, freeMemory, indents, memUsage-freeMemory, indents, time);
                
                // Log as long as we have given it a SD card instance
                if(sdInst != nullptr && enableSDLogging)
                    sdInst->writeLineToFile(fileName, output);
            }
        };

        /* Enable function summaries to view memory usage */
        void enableSummaries(){ enableFunctionSummaries = true; };

        /* Save flash write by not logging everything to SD */
        void enableSD(){ enableSDLogging = true; };
};

