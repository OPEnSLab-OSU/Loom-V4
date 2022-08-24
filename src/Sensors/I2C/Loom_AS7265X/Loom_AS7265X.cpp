#include "Loom_AS7265X.h";

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_AS7265X::Loom_AS7265X(
                        Manager& man, 
                        int addr,
                        bool use_bulb,
                        uint8_t gain,
                        uint8_t mode,
                        uint8_t integration_time 
                    ) : Module("AS7265X"), manInst(&man), use_bulb(use_bulb), gain(gain), mode(mode), integration_time(integration_time) {
                        // Register the module with the manager
                        manInst->registerModule(this);
                        
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_AS7265X::initialize() {

    // If we have less than 2 bytes of json from the sensor
    if(!asInst.begin()){
        printModuleName(); Serial.println("Failed to initialize AS7265X! Check connections and try again...");
        moduleInitialized = false;
        return;
    }
    else{
        printModuleName(); Serial.println("Successfully initialized AS7265X!");
        asInst.setGain(gain);
		asInst.setMeasurementMode(mode);
        asInst.setIntegrationCycles(integration_time);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_AS7265X::measure() {
    // Whether or not to take the measurements with the bulb or not
    if(use_bulb){
        asInst.takeMeasurementsWithBulb();
    }
    else{
        asInst.takeMeasurements();
    }

    // UV
	uv[0] = asInst.getCalibratedA();
	uv[1] = asInst.getCalibratedB();
	uv[2] = asInst.getCalibratedC();
	uv[3] = asInst.getCalibratedD();
	uv[4] = asInst.getCalibratedE();
	uv[5] = asInst.getCalibratedF();

	// Color
	color[0] = asInst.getCalibratedG();
	color[1] = asInst.getCalibratedH();
	color[2] = asInst.getCalibratedI();
	color[3] = asInst.getCalibratedJ();
	color[4] = asInst.getCalibratedK();
	color[5] = asInst.getCalibratedL();

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
void Loom_AS7265X::package() {
    JsonObject json = manInst->get_data_object(getModuleName());
    json["UV 1"] = uv[0];
	json["UV 2"] = uv[1];
	json["UV 3"] = uv[2];
	json["UV 4"] = uv[3];
	json["UV 5"] = uv[4];
	json["UV 6"] = uv[5];

	json["Color 1"] = color[0];
	json["Color 2"] = color[1];
	json["Color 3"] = color[2];
	json["Color 4"] = color[3];
	json["Color 5"] = color[4];
	json["Color 6"] = color[5];

	json["NIR 1"] = nir[0];
	json["NIR 2"] = nir[1];
	json["NIR 3"] = nir[2];
	json["NIR 4"] = nir[3];
	json["NIR 5"] = nir[4];
	json["NIR 6"] = nir[5];
}
//////////////////////////////////////////////////////////////////////////////////////////////////////