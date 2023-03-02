#pragma once

#include <queue>
#include <MemoryFree.h>
#include "Hardware/Loom_Hypnos/SDManager.h"

#define FUNCTION_START Logger::getInstance()->startFunction(__FILE__, __func__, __LINE__, freeMemory())
#define FUNCTION_END Logger::getInstance()->endFunction(freeMemory()) 

#define SLOG(msg) Logger::getInstance()->debugLog(msg, true, __FILE__, __func__, __LINE__)          // Log a message without printing to the serial
#define LOG(msg) Logger::getInstance()->debugLog(msg, false, __FILE__, __func__, __LINE__)          // Log a generic message
#define LOG_LONG(msg) Logger::getInstance()->logLong(msg, false)                                   // Log a long message
#define ERROR(msg) Logger::getInstance()->errorLog(msg, false, __FILE__, __func__, __LINE__)        // Log an error message
#define WARNING(msg) Logger::getInstance()->warningLog(msg, false, __FILE__, __func__, __LINE__)    // Log a warning message


/**
 * Class for handling debug log information
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
         * totalMemoryUsage - The total amount of RAM being used at the end of the current function (bytes)
         * time - The time the function took to return (ms)
        */
        struct functionInfo{
            char fileName[260];
            char funcName[260];
            unsigned long lineNumber;
            int netMemoryUsage;
            unsigned long time;
            functionInfo(const char* file, const char* func, unsigned int lineNumber, int mem, unsigned long time) : lineNumber(lineNumber), netMemoryUsage(mem), time(time) {
                strncpy(fileName, file, 260);
                strncpy(funcName, func, 260);
            };
        };
        
        // Function call list
        std::queue<Logger::functionInfo*> callStack;
        int indentNum = 0;

        static Logger* instance;
        SDManager* sdInst = nullptr;

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
            if(sdInst != nullptr)
                sdInst->writeLineToFile(filePath, message);
        }

        /**
         * Truncate the __FILE__ output to just show the name instead of the whole path
         * @param fileName String to truncate
         * 
         * @return Pointer to a malloced char*
        */
        char* truncateFileName(const char* fileName){
            // Get the last slash in the file name
            char* file =  (char*) malloc(260);
            int i;
            // Find the last \\ in the file path to know where the name is
            char *lastOccurance = strrchr(fileName, '\\');
            int lastSlash = lastOccurance-fileName+1;

            // Loop from the last slash to the end of the string
            for(i = lastSlash; i < strlen(fileName); i++){
                file[i-lastSlash] = fileName[i];
            }
            file[i-lastSlash] = '\0';

            return file;
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
         * Logs a Debug Message to the SD card and the serial monitor
         * @param message Message to log
         * @param silent If set to silent it will not appear in the serial monitor
         * @param lineNumber The current line number this log is on
        */
        void debugLog(const char* message, bool silent, const char* file, const char* func, unsigned long lineNumber){
            char logMessage[OUTPUT_SIZE];
            char* shortFileName =  truncateFileName(file);
            snprintf_P(logMessage, OUTPUT_SIZE, PSTR("[DEBUG] [%s:%s:%u] %s"), shortFileName, func, lineNumber, message);
            free(shortFileName);
            log(logMessage, silent);
            
        };

        /**
         * Logs an Error Message to the SD card and the serial monitor
         * @param message Message to log
         * @param silent If set to silent it will not appear in the serial monitor
         * @param lineNumber The current line number this log is on
        */
        void errorLog(const char* message, bool silent, const char* file, const char* func, unsigned long lineNumber){
            char logMessage[OUTPUT_SIZE];
            char* shortFileName =  truncateFileName(file);
            snprintf_P(logMessage, OUTPUT_SIZE, PSTR("[ERROR] [%s:%s:%u] %s"), shortFileName, func, lineNumber, message);
            free(shortFileName);
            log(logMessage, silent);
        };

         /**
         * Logs a Warning Message to the SD card and the serial monitor
         * @param message Message to log
         * @param silent If set to silent it will not appear in the serial monitor
         * @param lineNumber The current line number this log is on
        */
        void warningLog(const char* message, bool silent, const char* file, const char* func, unsigned long lineNumber){
            char logMessage[OUTPUT_SIZE];
            char* shortFileName =  truncateFileName(file);
            snprintf_P(logMessage, OUTPUT_SIZE, PSTR("[WARNING] [%s:%s:%u] %s\0"), shortFileName, func, lineNumber, message);
            free(shortFileName);
            log(logMessage, silent);
        };

        /**
         * Logs a Debug Message to the SD card and the serial monitor
         * @param message Message to log
         * @param silent If set to silent it will not appear in the serial monitor
         * @param lineNumber The current line number this log is on
        */
        void debugLog(const __FlashStringHelper* message, bool silent, const char* file, const char* func, unsigned long lineNumber){
            char logMessage[OUTPUT_SIZE];
            char buff[50];
		    memcpy_P(buff, message, 50);
            char* shortFileName =  truncateFileName(file);
            snprintf_P(logMessage, OUTPUT_SIZE, PSTR("[DEBUG] [%s:%s:%u] %s"), shortFileName, func, lineNumber, buff);
            free(shortFileName);
            log(logMessage, silent);
        };

         /**
         * Logs a Warning Message to the SD card and the serial monitor
         * @param message Message to log
         * @param silent If set to silent it will not appear in the serial monitor
         * @param lineNumber The current line number this log is on
        */
        void warningLog(const __FlashStringHelper* message, bool silent, const char* file, const char* func, unsigned long lineNumber){
            char logMessage[OUTPUT_SIZE];
            char buff[50];
		    memcpy_P(buff, message, 50);
            char* shortFileName =  truncateFileName(file);
            snprintf_P(logMessage, OUTPUT_SIZE, PSTR("[WARNING] [%s:%s:%u] %s\0"), shortFileName, func, lineNumber, buff);
            free(shortFileName);
            log(logMessage, silent);
        };

         /**
         * Logs an Error Message to the SD card and the serial monitor
         * @param message Message to log
         * @param silent If set to silent it will not appear in the serial monitor
         * @param lineNumber The current line number this log is on
        */
        void errorLog(const __FlashStringHelper* message, bool silent, const char* file, const char* func, unsigned long lineNumber){
            char logMessage[OUTPUT_SIZE];
            char buff[50];
		    memcpy_P(buff, message, 50);
            char* shortFileName =  truncateFileName(file);
            snprintf_P(logMessage, OUTPUT_SIZE, PSTR("[ERROR] [%s:%s:%u] %s"), shortFileName, func, lineNumber, buff);
            free(shortFileName);
            log(logMessage, silent);
        };

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
            // Log the start time of the function
            unsigned long time = millis();
            char* shortFileName =  truncateFileName(file);
            callStack.push(new functionInfo(shortFileName, func, num, freeMemory, time));
            free(shortFileName);

        };

        /**
         * Marks the end of a function, logs summary to SD card
         * @param ret Openly typed variable to show the return type of the function
        */
        void endFunction(int freeMemory){
            char fileName[100];
            char file[260];
            char func[260];
            char output[300];
            char indents[10];

            functionInfo* info = callStack.front();
            int memUsage = info->netMemoryUsage;
            unsigned long time = millis() - info->time;
            int percentage = ((float)memUsage / 32000.0) * 100;

            // Copy to internal variables
            strncpy(file, info->fileName, 260);
            strncpy(func, info->funcName, 260);
        
            // Pop the last function off the call stack
            delete(info);
            callStack.pop();

        

            // Format the fileName and log output, this function uses 976 bytes
            snprintf_P(fileName, 100,PSTR("/debug/funcSummaries_%i.log"), sdInst->getCurrentFileNumber());
            snprintf_P(output, 300, PSTR("[%s:%s] Summary\n\tInitial Free Memory: %i B (%i %% Free)\n\tEnding Free Memory: %i B\n\tNet Usage: %i B\n\tElapsed Time: %u MS"), file, func, memUsage, percentage, freeMemory, memUsage-freeMemory, time);
            
            // Log as long as we have given it a SD card instance
            if(sdInst != nullptr)
                sdInst->writeLineToFile(fileName, output);
        };
};

