#include "Loom_SEN55.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_SEN55::Loom_SEN55(Manager &man, float temperatureOffset, int addr) : I2CDevice("SEN55"), manInst(&man), tempOffset(temperatureOffset) {
    module_address = addr;
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN55::initialize()
{
    FUNCTION_START;

    // Start I2C communication with the SEN55
    Wire.begin();
    SEN55Instance.begin(Wire);
    SEN55Instance.setTemperatureOffsetSimple(tempOffset);

    /* Attempt to reset the device to test connection */
    uint16_t error = SEN55Instance.deviceReset();
    if(error) {
        ERROR(F("SEN55 failed to reset device!"));
        this->moduleInitialized = false;
        FUNCTION_END;
        return;
    }
    shouldMeasurePm = true;

    LOG(F("SEN55 Successfully Initialized!"));
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN55::measure()
{
    FUNCTION_START;
    /* Start a new measurement with a PM values*/
    uint16_t error = SEN55Instance.startMeasurement();
    if(error) {
        ERROR(F("SEN55 failed to start measurements!"));
        FUNCTION_END;
        return;
    }

    // Wait 30 seconds here for the PM values to populate
    LOG(F("Waiting 30 seconds for PM values to populate..."));
    delay(30000)

    // Read the values from the SEN55
    error = SEN55Instance.readMeasuredValues( massConcentration[0], massConcentration[1], massConcentration[2],
                                              massConcentration[3], ambientHumidity, ambientTemperature, vocIndex, noxIndex 
                                            );
    if(error){
        ERROR(F("Error reading measurements from sensor"));
        FUNCTION_END;
        return;
    }

   
    // Switch the SEN55 back to a measure mode with no particulate matter to save power
    error = SEN55Instance.startMeasurementWithoutPm();
    if(error) {
        ERROR(F("Failed to switch measurement mode back to no particulate matter!"));
        FUNCTION_END;
        return;
    }  
   
    FUNCTION_END;
    return;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN55::package()
{
    FUNCTION_START;

    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        json["Mass_Concentration_Pm1p0"] = massConcentration[0];
        json["Mass_Concentration_Pm2p5"] = massConcentration[1];
        json["Mass_Concentration_Pm4p0"] = massConcentration[2];
        json["Mass_Concentration_Pm10p0"] = massConcentration[3];
        json["Ambient_Humidity"] = ambientHumidity;
        json["Ambient_Temperature"] = ambientTemperature;
        json["Voc_Index"] = vocIndex;
        json["Nox_Index"] = noxIndex;
    }

    FUNCTION_END;
    return;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
