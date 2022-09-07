#include "Loom_MMA8451.h";

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MMA8451::Loom_MMA8451(
                        Manager& man, 
                        int addr,
                        mma8451_range_t range  
                    ) : Module("MMA8451"), manInst(&man), address(addr), range(range) {
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
    json["X_Acceleration"] = accel[0];
    json["Y_Acceleration"] = accel[1];
    json["Z_Acceleration"] = accel[2];

    // Convert the orientation to a string
    String orientationString = "";
    switch (orientation) {
        case MMA8451_PL_PUF: orientationString = "Portrait_Up_Front";       break;
        case MMA8451_PL_PUB: orientationString = "Portrait_Up_Back";		break;
        case MMA8451_PL_PDF: orientationString = "Portrait_Down_Front";	    break;
        case MMA8451_PL_PDB: orientationString = "Portrait_Down_Back";		break;
        case MMA8451_PL_LRF: orientationString = "Landscape_Right_Front";	break;
        case MMA8451_PL_LRB: orientationString = "Landscape_Right_Back";	break;
        case MMA8451_PL_LLF: orientationString = "Landscape_Left_Front";	break;
        case MMA8451_PL_LLB: orientationString = "Landscape_Left_Back";	    break;
	}
    
    // Package the orientation string
    json["Orientation"] = orientationString;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////