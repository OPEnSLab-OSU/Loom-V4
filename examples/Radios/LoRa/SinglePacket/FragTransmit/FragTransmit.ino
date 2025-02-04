/**
 * This is an example use case for LoRa communication
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include <Loom_Manager.h>

#include <stdio.h>

#include <Radio/Loom_LoRa/Loom_LoRa.h>
#include <DummyModule/DummyModule.h>

#define NAME_SIZE 20

Manager manager("Device", 2);

Loom_LoRa lora(manager, 2, 23, 10, 1000);

std::vector<Loom_Dummy*> modules;

void setup() {
    for (int i = 0; i < 5; i++) {
        char *name = new char[NAME_SIZE];
        snprintf(name, NAME_SIZE, "dummy %i", i);
        Loom_Dummy *dummy = new Loom_Dummy(manager, name);
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
