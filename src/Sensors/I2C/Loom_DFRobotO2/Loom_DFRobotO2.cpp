#include "Loom_DFRobotO2.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_DFRobotO2::Loom_DFRobotO2(Manager &man, bool useMux, int address, int collectNum)
    : I2CDevice("DFRobotO2"), manInst(&man), collectNumber(collectNum) {
    module_address = address;
    if (!useMux)
        manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_DFRobotO2::initialize() {
    FUNCTION_START;
    if (!oxygen.begin(module_address)) {
        ERROR(F("Failed to initialize DFRobotO2! Check connections and try again..."));
        moduleInitialized = false;
    } else {
        LOG(F("Successfully initialized DFRobotO2!"));
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_DFRobotO2::measure() {
    FUNCTION_START;

    if (moduleInitialized) {
        // Get the current connection status
        bool connectionStatus = checkDeviceConnection();

        // If we are connected and we need to reinit
        if (connectionStatus && needsReinit) {
            initialize();
            needsReinit = false;
        }

        // If we are not connected
        else if (!connectionStatus) {
            ERROR(F("No acknowledge received from the device"));
            FUNCTION_END;
            return;
        }

        // Pull the data from the sensor
        oxygenConcentration = oxygen.getOxygenData(collectNumber);
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_DFRobotO2::package() {
    JsonObject json = manInst->get_data_object(getModuleName());

    json["Oxygen Concentration_%Vol"] = oxygenConcentration;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Choose method 1 or method 2 or method 3 to calibrate the oxygen sensor.
 * 1. Directly calibrate the oxygen sensor by adding two parameters to the sensor.
 * 2. Waiting for stable oxygen sensors for about 10 minutes,
 *    OXYGEN_CONECTRATION is the current concentration of oxygen in the air (20.9% by volume except
 * in special cases), Note using the first calibration method, the OXYGEN MV must be 0.
 * 3. Click the button on the back of the Gravity breakout board when in air after the value
 * stabilizes, and it will calibrate the current concentration reading to be 20.9% by volume
 */
////////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_DFRobotO2::calibrate(float calOxygenConcentration, float calOxygenMV = 0) {

    LOG(F("Calibration complete!"));
    oxygen.calibrate(calOxygenConcentration, calOxygenMV);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
