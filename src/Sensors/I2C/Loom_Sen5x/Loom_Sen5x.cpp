#include "Loom_Sen5x.h"
#include "Logger.h"

Loom_Sen5x::Loom_Sen5x(Manager &man, float temperatureOffset, int addr) :
I2CDevice("Sen5x"), manInst(&man), tempOffset(temperatureOffset) 
{
    module_address = addr;
    manInst->registerModule(this);
};

void Loom_Sen5x::power_up(){};
void Loom_Sen5x::power_down(){};

void Loom_Sen5x::initialize()
{
    FUNCTION_START;

    Wire.begin();
    sen5xInstance.begin(Wire);

    uint16_t error = sen5xInstance.deviceReset();
    if(error) {
        ERROR(F("Sen5 failed to execute device reset!"));
        this->moduleInitialized = false;
        FUNCTION_END;
        return;
    }
    error = sen5xInstance.startMeasurement();
    if(error) {
        ERROR(F("Sen5 failed to start measurements ( startMeasurement() )!"));
        moduleInitialized = false;
        FUNCTION_END;
        return;
    }

    LOG(F("Sen5 successfully initialized..."));
    FUNCTION_END;
    return;
}

void Loom_Sen5x::measure()
{
    FUNCTION_START;

    uint16_t error = sen5xInstance.readMeasuredValues( massConcentrationPm1p0, massConcentrationPm2p5, massConcentrationPm4p0,
                                                        massConcentrationPm10p0, ambientHumidity, ambientTemperature, vocIndex, noxIndex);
    if(error){
        ERROR(F("Error reading measurements, sen5x readMeasureValues() did not execute successfully..."));
        moduleInitialized = false;
        FUNCTION_END;
        return;
    }

    error = sen5xInstance.readMeasuredPmValues( t_massConcentrationPm1p0, 
                                                t_massConcentrationPm2p5, 
                                                t_massConcentrationPm4p0, 
                                                t_massConcentrationPm10p0,
                                                t_numberConcentrationPm0p5, 
                                                t_numberConcentrationPm1p0,
                                                t_numberConcentrationPm2p5,
                                                t_numberConcentrationPm4p0,
                                                t_numberConcentrationPm10p0,
                                                t_typicalParticleSize );
    if(error){
        ERROR(F("Error reading the massConcentration measurements, sen5x readMeasuredPmValues() did not execute successfully..."));
        moduleInitialized = false;
        FUNCTION_END;
        return;
    }

    LOG(F("--Measurements successfully read--"));
    FUNCTION_END;
    return;
}

void Loom_Sen5x::package()
{
    FUNCTION_START;

    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        json["Mass_Concentration_Pm1p0"] = massConcentrationPm1p0;
        json["Mass_Concentration_Pm2p5"] = massConcentrationPm2p5;
        json["Mass_Concentration_Pm4p0"] = massConcentrationPm4p0;
        json["Mass_Concentration_Pm10p0"] = massConcentrationPm10p0;
        json["Ambient_Humidity"] = ambientHumidity;
        json["Ambient_Temperature"] = ambientTemperature;
        json["Voc_Index"] = vocIndex;
        json["Nox_Index"] = noxIndex;
        json["test t_massConcentrationPm1p0"] = t_massConcentrationPm1p0;
        json["test t_massConcentrationPm2p5"] = t_massConcentrationPm2p5;
        json["test t_massConcentrationPm4p0"] = t_massConcentrationPm4p0;
        json["test t_massConcentrationPm10p0"] = t_massConcentrationPm10p0;
        json["test t_numberConcentrationPm0p5"] = t_numberConcentrationPm0p5;
        json["test t_numberConcentrationPm1p0"] = t_numberConcentrationPm1p0;
        json["test t_numberConcentrationPm2p5"] = t_numberConcentrationPm2p5;
        json["test t_numberConcentrationPm4p0"] = t_numberConcentrationPm4p0;
        json["test t_numberConcentrationPm10p0"] = t_numberConcentrationPm10p0;
        json["test t_typicalParticleSize"] = t_typicalParticleSize;
    }

    FUNCTION_END;
    return;
}