#include "Loom_MB1232.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MB1232::Loom_MB1232(
                        Manager& man,
                        int addr,
                        bool useMux 
                    ) : I2CDevice("MB1232"), manInst(&man), address(addr) {
                        module_address = addr;
                        // Register the module with the manager
                        
                        if(!useMux)
                            manInst->registerModule(this);
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MB1232::initialize() {
    // Start wire interface
    Wire.begin();

    // Start the I2C transmission
    Wire.beginTransmission(address);

    // Range the sensor
    Wire.write(RangeCommand);
    Wire.endTransmission();
    delay(100);

    // Request 2 bytes of data from the sensor
    Wire.requestFrom(address, byte(2));

    // If we have less than 2 bytes of data from the sensor
    if(Wire.available() < 2){
        ERROR(F("Failed to initialize MB1232! Check connections and try again..."));
        moduleInitialized = false;
    }
    else{
        LOG(F("Successfully initialized MB1232!"));

    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MB1232::measure() {
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
            ERROR(F("No acknowledge received from the device"));
            return;
        }
    
    
        Wire.beginTransmission(address);

        Wire.write(RangeCommand);
        Wire.endTransmission();
        delay(100);

        // Send a request containing the number two to get the range from the sensor
        Wire.requestFrom(address, byte(2));

        if (Wire.available() >= 2) {
            // The sensor communicates two bytes, each a range. The
            // high byte is typically zero, in which case the low
            // byte is equal to the range, so only the range is transmitted.
            // The low byte will not be less than 20.
            byte high = Wire.read();
            byte low  = Wire.read();
            byte tmp  = Wire.read();

            range = (high * 256) + low;
        } else {
            ERROR(F("Error reading from MB1232"));
        }
    }
    
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MB1232::package() {
    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        json["Range_cm"] = range;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////