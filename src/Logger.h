#pragma once

#include <stack>
#include <MemoryFree.h>
#include "Hardware/Loom_Hypnos/SDManager.h"

#define FUNCTION_START Logger::getInstance()->startFunction(__FILE__, __func__, __LINE__)
#define FUNCTION_END(ret) Logger::getInstance()->endFunction(ret) 

#define SLOG(msg) Logger::getInstance()->debugLog(String(msg), true, __FILE__, __func__, __LINE__)          // Log a message without printing to the serial
#define LOG(msg) Logger::getInstance()->debugLog(String(msg), false, __FILE__, __func__, __LINE__)          // Log a generic message
#define ERROR(msg) Logger::getInstance()->errorLog(String(msg), false, __FILE__, __func__, __LINE__)        // Log an error message
#define WARNING(msg) Logger::getInstance()->warningLog(String(msg), false, __FILE__, __func__, __LINE__)    // Log a warning message


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
            String fileName;
            String funcName;
            unsigned int lineNumber;
            int netMemoryUsage;
            int totalMemoryUsage;
            unsigned long time;
            functionInfo(String fileName, String funcName, unsigned int lineNumber, int mem, unsigned long time) : fileName(fileName), funcName(funcName), lineNumber(lineNumber), netMemoryUsage(mem), time(time) {};
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
         * @param lineNumber The lineNumber the log took place on
        */
        void log(String message, bool silent, long lineNumber){
            
            // If we want to actually print to serial
            if(!silent)
                Serial.println(message);

            // Log as long as we have given it a SD card instance
            if(sdInst != nullptr)
                sdInst->writeLineToFile("/debug/output_" + String(sdInst->getCurrentFileNumber()) + ".log", String(message));
        }

        /**
         * Truncate the __FILE__ output to just show the name instead of the whole path
         * @param fileName String to truncate
        */
        String truncateFileName(String fileName){
             // Get the last slash in the file name
            int lastSlash = fileName.lastIndexOf('\\')+1;
            return fileName.substring(lastSlash, fileName.length());
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
        void debugLog(String message, bool silent, String file, String func, long lineNumber){
            String banner = "[DEBUG] [" + truncateFileName(file) + ":" + func + ":" + String(lineNumber) + "] ";
            log(banner + message, silent, lineNumber);
        };

        /**
         * Logs an Error Message to the SD card and the serial monitor
         * @param message Message to log
         * @param silent If set to silent it will not appear in the serial monitor
         * @param lineNumber The current line number this log is on
        */
        void errorLog(String message, bool silent, String file, String func, long lineNumber){
            String banner = "[ERROR] [" + truncateFileName(file) + ":" + func + ":" + String(lineNumber) + "] ";
            log(banner + message, silent, lineNumber);
        };

        /**
         * Logs a Warning Message to the SD card and the serial monitor
         * @param message Message to log
         * @param silent If set to silent it will not appear in the serial monitor
         * @param lineNumber The current line number this log is on
        */
        void warningLog(String message, bool silent, String file, String func, long lineNumber){
            String banner = "[WARNING] [" + truncateFileName(file) + ":" + func + ":" + String(lineNumber) + "] ";
            log(banner + message, silent, lineNumber);
        };

        /**
         * Marks the start of a new function
         * @param file File name that the function is in
         * @param func Function name that this call is in
         * @param num Current line number in the file of which this call is located
        */
        void startFunction(String file, String func, unsigned int num){
            // Log the start time of the function
            int netMemoryUsage = freeMemory();
            unsigned long time = millis();
            
            // Get the last slash in the file name
            int lastSlash = file.lastIndexOf('\\')+1;

            callStack.push(new functionInfo(truncateFileName(file), func, num, netMemoryUsage, time));
        };

        /**
         * Marks the end of a function, logs summary to SD card
         * @param ret Openly typed variable to show the return type of the function
        */
        template<class T>
        void endFunction(T ret){
            // Log the start time of the function
            functionInfo* info = callStack.top();
            info->netMemoryUsage = info->netMemoryUsage - freeMemory();
            info->totalMemoryUsage = freeMemory();
            info->time = millis() - info->time;
            float percentage = ((float)info->totalMemoryUsage / 32000.0) * 100;
            
            // Delete the top most function on the call stack
            String banner = "[" + info->fileName + ":" + info->funcName + "]";

            // Log as long as we have given it a SD card instance
            if(sdInst != nullptr)
                sdInst->writeLineToFile("/debug/funcSummaries_" + String(sdInst->getCurrentFileNumber()) + ".log", String(banner + " Summary\n\tFunction Memory Usage: " + String(info->netMemoryUsage) + "\n\tFree Memory: " + String(info->totalMemoryUsage) + " B (" + String(percentage) + "\% Free)\n\tElapsed Time: " + String(info->time) + " MS\n\tReturn Status: " + String(ret)));
            
            delete(info);
            callStack.pop();
        };
};

