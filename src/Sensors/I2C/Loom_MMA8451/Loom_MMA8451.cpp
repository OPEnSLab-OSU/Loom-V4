#include "Loom_MMA8451.h"
#include "Logger.h"

uint8_t Loom_MMA8451::interruptPin;
InterruptCallbackFunction Loom_MMA8451::isr;

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MMA8451::Loom_MMA8451(
                        Manager& man,
                        int addr,
                        bool useMux,
                        mma8451_range_t range,
                        int intPin,
                        uint8_t sensitivity
                    ) : I2CDevice("MMA8451"), manInst(&man), address(addr), range(range), sensitivity(sensitivity){
                        module_address = addr;
                        interruptPin = intPin;
                        
                        // Register the module with the manager
                        if(!useMux)
                            manInst->registerModule(this);
                        
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MMA8451::initialize() {

    // If we have less than 2 bytes of data from the sensor
    if(!mma.begin(address)){
        ERROR(F("Failed to initialize MMA8451! Check connections and try again..."));
        moduleInitialized = false;
        return;
    }
    else{
        LOG(F("Successfully initialized MMA8451!"));
        mma.setRange(range);
    }

    // If we actually set an interrupt pin we want to enable the functionality
    if(interruptPin != -1){
        pinMode(interruptPin, INPUT_PULLUP);

        mma.writeRegister8(MMA8451_REG_CTRL_REG3, CTRL_REG3);
        mma.writeRegister8(MMA8451_REG_CTRL_REG4, CTRL_REG4);
        mma.writeRegister8(MMA8451_REG_CTRL_REG5, CTRL_REG5);
        mma.writeRegister8(MMA8451_REG_TRANSIENT_CFG, REG_TRANS_CFG);
        mma.writeRegister8(MMA8451_REG_TRANSIENT_THS, sensitivity);
        mma.writeRegister8(MMA8451_REG_TRANSIENT_CT, REG_TRANS_CT);
        attachInterrupt(digitalPinToInterrupt(interruptPin), IMU_ISR, FALLING);
        LOG(F("Interrupt Configured!"));
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MMA8451::measure() {
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
        
        
        // Update the sensor
        mma.read();

        // Get Sensor event
        sensors_event_t event;
        mma.getEvent(&event);

        // Get acceleration data
        accel[0] = event.acceleration.x;
        accel[1] = event.acceleration.y;
        accel[2] = event.acceleration.z;

        // Get orientation
        orientation = mma.getOrientation();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MMA8451::package() {
    char orientationString[25];
    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        json["X_Acc"] = accel[2];
        json["Y_Acc"] = accel[0];
        json["Z_Acc"] = accel[1];
        
        switch (orientation) {
            case MMA8451_PL_PUF: strncpy(orientationString, "Portrait_Up_Front\0", 25);       break;
            case MMA8451_PL_PUB: strncpy(orientationString, "Portrait_Up_Back\0" , 25);		break;
            case MMA8451_PL_PDF: strncpy(orientationString, "Portrait_Down_Front\0",25);	    break;
            case MMA8451_PL_PDB: strncpy(orientationString, "Portrait_Down_Back\0",25);		break;
            case MMA8451_PL_LRF: strncpy(orientationString, "Landscape_Right_Front\0",25);	break;
            case MMA8451_PL_LRB: strncpy(orientationString, "Landscape_Right_Back\0",25);	    break;
            case MMA8451_PL_LLF: strncpy(orientationString, "Landscape_Left_Front\0",25);	    break;
            case MMA8451_PL_LLB: strncpy(orientationString, "Landscape_Left_Back\0",25);	    break;
        }
        
        // Package the orientation string
        json["Orien"] = orientationString;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MMA8451::power_up() {
    if(moduleInitialized){
        mma.setRange(range);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MMA8451::IMU_ISR(){
    // Detach and reattach and call the user defined function
    detachInterrupt(digitalPinToInterrupt(interruptPin));
    isr();
    attachInterrupt(digitalPinToInterrupt(interruptPin), IMU_ISR, FALLING);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
