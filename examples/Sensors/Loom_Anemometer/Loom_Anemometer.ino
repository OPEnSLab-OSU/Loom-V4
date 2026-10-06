#include "Loom_Manager.h"
#include "../../../src/Hardware/Loom_Anemometer.h"

Manager manager("Device", 1);
Loom_Anemometer anemometer(&manager, 6,7,8);



setup(){
    manager.initialize();
}

loop(){
    manager.measure();
    manager.package();
    manager.display_data();
    manager.pause(5000);
}