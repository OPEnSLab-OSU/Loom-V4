#include <stack>
#include <MemoryFree.h>
#include "Hardware/Loom_Hypnos/SDManager.h"

#define FUNCTION_START Logger::getInstance()->startFunction(__FILE__, __func__, __LINE__)
#define FUNCTION_END(ret) Logger::getInstance()->endFunction(ret) 
#define LOG(msg) Logger::getInstance()->debugLog(msg)

/**
 * Class for handling debug log information
 * 
 * @author Will Richards 
 */ 
class Logger{
    private:
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

        // Log a debug message to the debug log file    
        void debugLog(String message){
            functionInfo* info = callStack.top();
            String banner = "[" + info->fileName + ":" + info->funcName + ":" + String(info->lineNumber) + "] ";
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

