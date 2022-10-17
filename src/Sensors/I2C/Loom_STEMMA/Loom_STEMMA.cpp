#include "Loom_STEMMA.h";

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_STEMMA::Loom_STEMMA(
                        Manager& man, 
                        int addr,
                        bool useMux  
                    ) : Module("STEMMA"), manInst(&man), address(addr) {
                        module_address = addr;

                        // Register the module with the manager
                        if(!useMux)
                            manInst->registerModule(this);
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_STEMMA::initialize() {
    if(!stemma.begin(address)){
        printModuleName(); Serial.println("Failed to initialize STEMMA! Check connections and try again...");
        moduleInitialized = false;
    }
    else{
        printModuleName(); Serial.println("Successfully initialized STEMMA Version: " + String(stemma.getVersion()));
        
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_STEMMA::measure() {
    if(moduleInitialized){
        if(needsReinit){
            initialize();
        }
        else if(!checkDeviceConnection()){
            printModuleName(); Serial.println("No acknowledge received from the device");
            return;
        }
        

        // Pull the data from the sensor
        temperature = stemma.getTemp();
        cap = stemma.touchRead(0);
    }
    
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_STEMMA::package() {
    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        json["Temperature"] = temperature;
        json["Capacitive"] = cap;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_STEMMA::print_measurements() {

    printModuleName();
	Serial.println("Measurements:");
	Serial.println("\tTemperature: " + String(temperature));
	Serial.println("\tCapacitive: " + String(cap));
}
//////////////////////////////////////////////////////////////////////////////////////////////////////