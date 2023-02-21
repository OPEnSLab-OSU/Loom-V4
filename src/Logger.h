#include <stack>
#include <MemoryFree.h>
#include "Hardware/Loom_Hypnos/SDManager.h"

#define FUNCTION_START Logger::getInstance()->startFunction(__FILE__, __func__, __LINE__)
#define FUNCTION_END(ret) Logger::getInstance()->endFunction(ret) 
#define LOG(msg) Logger::getInstance()->debugLog(msg, false, __LINE__)         // Log a generic message
#define SLOG(msg) Logger::getInstance()->debugLog(msg, true,  __LINE__)        // Log a message without printing to the serial

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

        /* Set an instance of the SD Manager */
        void setSDManager(SDManager* manager) { sdInst = manager; };

        /* Summarization of the current function*/
        void functionSummary(String ret){
            functionInfo* info = callStack.top();

            String banner = "[" + info->fileName + ":" + info->funcName + "]";

            // Log as long as we have given it a SD card instance
            if(sdInst != nullptr)
                sdInst->writeLineToFile("funcSummaries_" + String(sdInst->getCurrentFileNumber()) + ".log", String(banner + " Summary\n\tFunction Memory Usage: " + String(info->netMemoryUsage) + "\n\tTotal Memory Usage: " + String(info->totalMemoryUsage) +"\n\tElapsed Time: " + String(info->time) + "\n\tReturn Status: " + String(ret)));
        };

        /**
         * Logs a Debug Message to the SD card and the serial monitor
         * @param message Message to log
         * @param silent If set to silent it will not appear in the serial monitor
        */
        void debugLog(String message, bool silent, long lineNumber){
            functionInfo* info = callStack.top();
            String banner = "[" + lineNumber + ":" + info->funcName + ":" + String(info->lineNumber) + "] ";

            if(!silent)
                Serial.println(banner + message);

            // Log as long as we have given it a SD card instance
            if(sdInst != nullptr)
                sdInst->writeLineToFile("debug_" + String(sdInst->getCurrentFileNumber()) + ".log", String(banner + message));
        };

        /* Marks the start of a function */
        void startFunction(String file, String func, unsigned int num){
            // Log the start time of the function
            int netMemoryUsage = freeMemory();
            unsigned long time = millis();
            
            // Get the last slash in the file name
            int lastSlash = file.lastIndexOf('\\')+1;

            callStack.push(new functionInfo(file.substring(lastSlash, file.length()), func, num, netMemoryUsage, time));
        };

        /* Marks the end of a function*/
        template<class T>
        void endFunction(T ret){
            // Log the start time of the function
            functionInfo* info = callStack.top();
            info->netMemoryUsage = freeMemory() - info->netMemoryUsage;
            info->totalMemoryUsage = freeMemory();
            info->time = millis() - info->time;
            
            // Delete the top most function on the call stack
            functionSummary(String(ret));
            delete(info);
            callStack.pop();
            
        };
};

