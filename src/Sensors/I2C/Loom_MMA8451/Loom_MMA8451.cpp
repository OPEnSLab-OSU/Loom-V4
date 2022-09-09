#include "Loom_MMA8451.h";

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MMA8451::Loom_MMA8451(
                        Manager& man, 
                        int addr,
                        mma8451_range_t range  
                    ) : Module("MMA8451"), manInst(&man), address(addr), range(range) {
                        module_address = addr;
                        // Register the module with the manager
                        manInst->registerModule(this);
                        
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MMA8451::initialize() {

    // If we have less than 2 bytes of data from the sensor
    if(!mma.begin(address)){
        printModuleName(); Serial.println("Failed to initialize MMA8451! Check connections and try again...");
        moduleInitialized = false;
        return;
    }
    else{
        printModuleName(); Serial.println("Successfully initialized MMA8451!");
        mma.setRange(range);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MMA8451::measure() {
    // Update the sensor
    mma.read();

    // Get Sensor event
    sensors_event_t event;
    mma.getEvent(&event);

    // Get acceleration data
    accel[0] = event.acceleration.x;
	accel[1] = event.acceleration.y;
	accel[2] = event.acceleration.z;

    // Get orientation
    orientation = mma.getOrientation();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MMA8451::package() {
    JsonObject json = manInst->get_data_object(getModuleName());
    json["X Acceleration"] = accel[0];
    json["Y Acceleration"] = accel[1];
    json["Z Acceleration"] = accel[2];

    // Convert the orientation to a string
    String orientationString = "";
    switch (orientation) {
        case MMA8451_PL_PUF: orientationString = "Portrait Up Front";       break;
        case MMA8451_PL_PUB: orientationString = "Portrait Up Back";		break;
        case MMA8451_PL_PDF: orientationString = "Portrait Down Front";	    break;
        case MMA8451_PL_PDB: orientationString = "Portrait Down Back";		break;
        case MMA8451_PL_LRF: orientationString = "Landscape Right Front";	break;
        case MMA8451_PL_LRB: orientationString = "Landscape Right Back";	break;
        case MMA8451_PL_LLF: orientationString = "Landscape Left Front";	break;
        case MMA8451_PL_LLB: orientationString = "Landscape Left Back";	    break;
	}
    
    // Package the orientation string
    json["Orientation"] = orientationString;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////