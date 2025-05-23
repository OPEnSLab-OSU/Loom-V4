//TODO: make this an actual test (asserts and stuff?). potentially add automation.

/**
 * This is a testing file for batched transmission
 *
 * How to use:
 * Run this code on at least 2 devices, modifying `42: int id` so that it is
 * unique. Then run the single packet example receive, and ensure that it is
 * receiving from both devices.
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
    void power_up() override {};
    void power_down() override {};
    void initialize() override {};  
    void measure() override {};

public:
    void package() override {
        LOGF("packaging module `%s`...", this->getModuleName());

        JsonObject json = manInst->get_data_object(getModuleName());
        json["randomData"] = "very long string that forces packet fragmentation during lora transmission";
    }

    Dummy_Module(Manager& man, const char *name) : Module(name), manInst(&man) {
        // Register the module with the manager
        manInst->registerModule(this);
    };

private:
    Manager* manInst;                           // Instance of the manager
};

int id = 1;
Manager manager("Device", id);

Loom_LoRa lora(manager, id, 23, 10, 10, 1000);

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
