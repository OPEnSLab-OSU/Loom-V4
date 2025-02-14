/**
 * This is a testing file for batched transmission
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include <Loom_Manager.h>

#include <stdio.h>

#include <Radio/Loom_LoRa/Loom_LoRa.h>
#include <Module.h>

#define NAME_SIZE 20

class Dummy_Module : public Module {
protected:
    void power_down() override {};
    void initialize() override {};  
    void measure() override {};

public:
    void package() override {
        LOGF("packaging module `%s`...", this->getModuleName());

        JsonObject json = manInst->get_data_object(getModuleName());
        json["randomData"] = "very long string that forces packet fragmentation during lora transmission";
    }

    Loom_Dummy(Manager& man, const char *name) : Module(name), manInst(&man) {
        // Register the module with the manager
        manInst->registerModule(this);
    };

private:
    Manager* manInst;                           // Instance of the manager
};

Manager manager("Device", 2);

Loom_LoRa lora(manager, 2, 23, 10, 1000);

std::vector<Dummy_Module*> modules;

void setup() {
    for (int i = 0; i < 5; i++) {
        char *name = new char[NAME_SIZE];
        snprintf(name, NAME_SIZE, "dummy %i", i);
        Dummy_Module *dummy = new Dummy_Module(manager, name);
        modules.push_back(dummy);
    }

    manager.beginSerial();
    manager.initialize();
}

void loop() {
    manager.package();
    manager.display_data();

    // Send the current JSON document to address 0
    lora.send(0);

    // Wait 5 seconds between transmits
    manager.pause(5000);
}
