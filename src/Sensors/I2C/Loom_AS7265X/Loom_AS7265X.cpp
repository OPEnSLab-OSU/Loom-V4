#include "Loom_AS7265X.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_AS7265X::Loom_AS7265X(
                        Manager& man,
						bool useMux, 
                        int addr,
                        bool use_bulb,
                        uint8_t gain,
                        uint8_t mode,
                        uint8_t integration_time 
                    ) : I2CDevice("AS7265X"), manInst(&man), use_bulb(use_bulb), gain(gain), mode(mode), integration_time(integration_time) {
						module_address = addr;

                        // Register the module with the manager
						if(!useMux)
                        	manInst->registerModule(this);
                        
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_AS7265X::initialize() {

    // If we have less than 2 bytes of json from the sensor
    if(!asInst.begin()){
        printModuleName("Failed to initialize AS7265X! Check connections and try again...");
        moduleInitialized = false;
        return;
    }
    else{
        printModuleName("Successfully initialized AS7265X!");
        asInst.setGain(gain);
		asInst.setMeasurementMode(mode);
        asInst.setIntegrationCycles(integration_time);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_AS7265X::measure() {
	if(moduleInitialized){
		if(needsReinit){
			initialize();
		}
		else if(!checkDeviceConnection()){
			printModuleName("No acknowledge received from the device");
			return;
		}
	
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
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_AS7265X::package() {
	if(moduleInitialized){
		JsonObject json = manInst->get_data_object(getModuleName());
		json["UV_1"] = uv[0];
		json["UV_2"] = uv[1];
		json["UV_3"] = uv[2];
		json["UV_4"] = uv[3];
		json["UV_5"] = uv[4];
		json["UV_6"] = uv[5];

		json["Color_1"] = color[0];
		json["Color_2"] = color[1];
		json["Color_3"] = color[2];
		json["Color_4"] = color[3];
		json["Color_5"] = color[4];
		json["Color_6"] = color[5];

		json["NIR_1"] = nir[0];
		json["NIR_2"] = nir[1];
		json["NIR_3"] = nir[2];
		json["NIR_4"] = nir[3];
		json["NIR_5"] = nir[4];
		json["NIR_6"] = nir[5];
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_AS7265X::power_up() {
	if(moduleInitialized){
		asInst.setGain(gain);
		asInst.setMeasurementMode(mode);
        asInst.setIntegrationCycles(integration_time);
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////