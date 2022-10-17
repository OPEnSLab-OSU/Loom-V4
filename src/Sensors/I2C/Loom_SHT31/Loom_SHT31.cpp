#include "Loom_SHT31.h";

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_SHT31::Loom_SHT31(
                        Manager& man,
                        int address, 
                        bool useMux
                    ) : Module("SHT31"), manInst(&man), i2c_address(address){
                        module_address = address;
                        
                        // Register the module with the manager
                        if(!useMux)
                            manInst->registerModule(this);
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SHT31::initialize() {
    if(!sht.begin(i2c_address)){
        printModuleName(); Serial.println("Failed to initialize SHT31! Check connections and try again...");
        moduleInitialized = false;
    }
    else{
        printModuleName(); Serial.println("Successfully initialized SHT31!");
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SHT31::measure() {
    if(moduleInitialized){
        // Get the current connection status
        bool connectionStatus = checkDeviceConnection();

        // If we are connected and we need to reinit
        if(connectionStatus && needsReinit){
            initialize();
            needsReinit = false;
        }

        // If we are not connected
        else if(!connectionStatus){
            printModuleName(); Serial.println("No acknowledge received from the device");
            return;
        }
        // Pull the data from the sensor
        float temp = sht.readTemperature();
        float humid = sht.readHumidity();

        // If both the temp and humidity values are valid send the data
        if(!isnan(temp) && !isnan(humid)){
            sensorData[0] = temp;
            sensorData[1] = humid;
        }
        else{
            printModuleName(); Serial.println("Collected information was invalid, the previous collected data will be published again.");
        }
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SHT31::package() {
    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        json["Temperature"] = sensorData[0];
        json["Humidity"] = sensorData[1];
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SHT31::print_measurements() {

    printModuleName();
	Serial.println("Measurements:");
	Serial.println("\tTemperature: " + String(sensorData[0]));
	Serial.println("\tHumidity: " + String(sensorData[1]));
}
//////////////////////////////////////////////////////////////////////////////////////////////////////