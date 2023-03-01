#pragma once

#include <stack>
#include <MemoryFree.h>
#include "Hardware/Loom_Hypnos/SDManager.h"

#define FUNCTION_START Logger::getInstance()->startFunction(__FILE__, __func__, __LINE__)
#define FUNCTION_END Logger::getInstance()->endFunction() 

#define SLOG(msg) Logger::getInstance()->debugLog(msg, true, __FILE__, __func__, __LINE__)          // Log a message without printing to the serial
#define LOG(msg) Logger::getInstance()->debugLog(msg, false, __FILE__, __func__, __LINE__)          // Log a generic message
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
            int totalMemoryUsage;
            unsigned long time;
            functionInfo(const char* file, const char* func, unsigned int lineNumber, int mem, unsigned long time) : lineNumber(lineNumber), netMemoryUsage(mem), time(time) {
                strncpy(fileName, file, 260);
                strncpy(funcName, func, 260);
            };
        };
        
        // Function call list
        std::stack<Logger::functionInfo*> callStack;

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

            snprintf(filePath, 100, "/debug/output_%i.log", sdInst->getCurrentFileNumber());
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

            // Terminate the string with \0
            i++;
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
            char logMessage[100];
            char* shortFileName =  truncateFileName(file);
            snprintf(logMessage, 100, "[DEBUG] [%s:%s:%u] %s", shortFileName, func, lineNumber, message);
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
            char logMessage[100];
            char* shortFileName =  truncateFileName(file);
            snprintf(logMessage, 100, "[ERROR] [%s:%s:%u] %s", shortFileName, func, lineNumber, message);
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
            char logMessage[100];
            char* shortFileName =  truncateFileName(file);
            snprintf(logMessage, 100, "[WARNING] [%s:%s:%u] %s\0", shortFileName, func, lineNumber, message);
            free(shortFileName);
            log(logMessage, silent);
        };

        /**
         * Marks the start of a new function
         * @param file File name that the function is in
         * @param func Function name that this call is in
         * @param num Current line number in the file of which this call is located
        */
        void startFunction(const char* file, const char* func, unsigned long num){
            // Log the start time of the function
            int netMemoryUsage = freeMemory();
            unsigned long time = millis();

            callStack.push(new functionInfo(file, func, num, netMemoryUsage, time));
        };

        /**
         * Marks the end of a function, logs summary to SD card
         * @param ret Openly typed variable to show the return type of the function
        */
        void endFunction(){
            // Log the start time of the function
            char fileName[100];
            char output[300];

            functionInfo* info = callStack.top();
            info->netMemoryUsage = info->netMemoryUsage - freeMemory();
            info->totalMemoryUsage = freeMemory();
            info->time = millis() - info->time;
            float percentage = ((float)info->totalMemoryUsage / 32000.0) * 100;

            // Format the fileName and log output
            snprintf(fileName, 100,"/debug/funcSummaries_%i.log", sdInst->getCurrentFileNumber());
            snprintf(output, 300, "[%s:%s] Summary\n\tFunction Memory Usage: %i B\n\tFree Memory: %i B (%f\% Free)\n\tElapsed Time: %u MS", info->fileName, info->funcName, info->netMemoryUsage, info->totalMemoryUsage, percentage, info->time);

            // Log as long as we have given it a SD card instance
            if(sdInst != nullptr)
                sdInst->writeLineToFile(fileName, output);
            
            delete(info);
            callStack.pop();
        };
};

