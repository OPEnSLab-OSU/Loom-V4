#include "Loom_STEMMA.h";

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_STEMMA::Loom_STEMMA(
                        Manager& man, 
                        int addr  
                    ) : Module("STEMMA"), manInst(&man), address(addr) {
                        // Register the module with the manager
                        manInst->registerModule(this);
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_STEMMA::initialize() {
    if(!stemma.begin(address)){
        printModuleName(); Serial.println("Failed to initialize STEMMA! Check connections and try again...");
    }
    else{
        printModuleName(); Serial.println("Successfully initialized STEMMA Version: " + String(stemma.getVersion()));
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_STEMMA::measure() {
    // Pull the data from the sensor
    temperature = stemma.getTemp();
    cap = stemma.touchRead(0);
    
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_STEMMA::package() {
    JsonObject json = manInst->get_data_object(getModuleName());
    json["Temperature"] = temperature;
    json["Capacitive"] = cap;
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