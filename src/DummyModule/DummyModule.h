#pragma once

#include "Module.h"
#include "Loom_Manager.h"

/**
 * Dummy module for testing
 */ 
class Loom_Dummy : public Module{
    protected:
        void power_up() override {};
        void power_down() override {};
        void initialize() override {};  
        void measure() override {};
        

    public:
        void package() override;

        /**
         * Templated constructor that only reads the battery voltage
         * @param man Reference to the manager
         */ 
        Loom_Dummy(Manager& man, const char *name) : Module(name), manInst(&man) {
           // Register the module with the manager
           manInst->registerModule(this);
        };

    private:
        Manager* manInst;                           // Instance of the manager
};
