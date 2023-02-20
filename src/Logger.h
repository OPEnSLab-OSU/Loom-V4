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
            String logBanner;
            int netMemoryUsage;
            int time;
            functionInfo(String banner, int mem, int time) : logBanner(banner), netMemoryUsage(mem), time(time) {}
            
        };
        
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

        void setSDManager(SDManager* manager) { sdInst = manager; };

        // Log a debug message to the debug log file
        void debugLog(String message){
            functionInfo* info = callStack.top();
            Serial.println(info->logBanner + message);

            // Log as long as we have given it a SD card instance
            if(sdInst != nullptr)
                sdInst->writeLineToFile("debug_" + String(sdInst->getCurrentFileNumber()) + ".log", String(info->logBanner + message));
        };

        /* Marks the start of a function */
        void startFunction(String file, String func, unsigned int num){
            // Log the start time of the function
            int netMemoryUsage = freeMemory();
            int time = millis();
            
            // Get the last slash in the file name
            #if _WIN32
                int lastSlash = file.lastIndexOf('\\');
            #else
                int lastSlash = file.lastIndexOf('/');
            #endif

            // Debug log banner
            String logBanner = "[" + file.substring(lastSlash, file.length()) + ":" + func + ":" + String(num) + "]";

            callStack.push(new functionInfo(logBanner, time, netMemoryUsage));
        };

        /* Marks the end of a function*/
        template<class T>
        void endFunction(T ret){
            // Log the start time of the function
            functionInfo* info = callStack.top();
            info->netMemoryUsage = freeMemory() - info->netMemoryUsage;
            info->time = millis() - info->time;
            
            debugLog(" Summary\n\tNet Memory Usage: " + String(info->netMemoryUsage) 
                           + "\n\tElapsed Time: " + String(info->time) 
                           + "\n\tReturn Status: " + String(ret));

            // Delete the top most function on the call stack
            delete(info);
            callStack.pop();
        };
};

