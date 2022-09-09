#include "Loom_AS7263.h";

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_AS7263::Loom_AS7263(
                        Manager& man, 
                        int addr,
                        uint8_t gain,
                        uint8_t mode,
                        uint8_t integration_time 
                    ) : Module("AS7263"), manInst(&man), gain(gain), mode(mode), integration_time(integration_time) {
                        module_address = addr;

                        // Register the module with the manager
                        manInst->registerModule(this);
                        
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_AS7263::initialize() {

    // If we have less than 2 bytes of json from the sensor
    if(!asInst.begin()){
        printModuleName(); Serial.println("Failed to initialize AS7263! Check connections and try again...");
        moduleInitialized = false;
        return;
    }
    else{
        printModuleName(); Serial.println("Successfully initialized AS7263!");
        asInst.setGain(gain);
		asInst.setMeasurementMode(mode);
        asInst.setIntegrationTime(integration_time);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_AS7263::measure() {
    
	// Take a measurement and wait for it to be ready
    asInst.takeMeasurements();
	while(!asInst.dataAvailable()){
		delay(5);
	}

	// NIR
	nir[0] = asInst.getCalibratedR();
	nir[1] = asInst.getCalibratedS();
	nir[2] = asInst.getCalibratedT();
	nir[3] = asInst.getCalibratedU();
	nir[4] = asInst.getCalibratedV();
	nir[5] = asInst.getCalibratedW();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_AS7263::package() {
    JsonObject json = manInst->get_data_object(getModuleName());
	json["NIR 1"] = nir[0];
	json["NIR 2"] = nir[1];
	json["NIR 3"] = nir[2];
	json["NIR 4"] = nir[3];
	json["NIR 5"] = nir[4];
	json["NIR 6"] = nir[5];
}
//////////////////////////////////////////////////////////////////////////////////////////////////////