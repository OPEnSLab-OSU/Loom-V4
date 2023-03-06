#include "Loom_ZXGesture.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_ZXGesture::Loom_ZXGesture(
                            Manager& man, 
                            int address, 
                            bool useMux,
                            Mode mode
                    ) : I2CDevice("ZX Gesture"), manInst(&man), zx( ZX_Sensor(address)), mode(mode) {

                        // Register the module with the manager
                        if(!useMux)
                            manInst->registerModule(this);
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ZXGesture::initialize() {
    char output[OUTPUT_SIZE];
    if(!zx.init()){
        ERROR(F("Failed to initialize ZX Gesture Sensor! Check connections and try again..."));
    }
    else{

        // Make sure the version number is correct
        uint8_t ver = zx.getModelVersion();
        if (ver != ZX_MODEL_VER) {
            moduleInitialized = false;
            snprintf(output, OUTPUT_SIZE, "Incorrect Model Version. Expected Version: %u Actual Version: %u", ZX_MODEL_VER, ver);
            ERROR(output);
            return;
        } else {
            snprintf(output, OUTPUT_SIZE, "Model Version: %u", ver);
            LOG(output);
        }

        // Read the register map version and ensure the library will work
        ver = zx.getRegMapVersion();
        //printModuleName();
        if (ver != ZX_REG_MAP_VER) {
            moduleInitialized = false;
            snprintf(output, OUTPUT_SIZE, "Incorrect Register Map Version. Expected Version: %u Actual Version: %u", ZX_REG_MAP_VER, ver);
            ERROR(output);
            return;
        } else {
            snprintf(output, OUTPUT_SIZE, "Register Map Version: %u", ver);
            LOG(output);
        }

        LOG(F("Successfully initialized ZX Gesture Sensor!"));
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ZXGesture::measure() {
    
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

    
        // Check if we are in position detection mode or in gesture detection mode
        if(mode == Mode::POS){
            uint8_t x, z;
            if(zx.positionAvailable()){

                // Read X and Y values into struct
                x = zx.readX();
                z = zx.readZ();

                if ((x != ZX_ERROR) && (z != ZX_ERROR)) {
                    pos.x = x;
                    pos.z = z;
                }
                else {
                    ERROR(F("Error occurred while reading position data"));
                }
            }
            
            // No position available
            else{
                pos.x = 255;
                pos.z = 255;
            }
        }

        // If we are trying to detect a gesture
        else{
            if(zx.gestureAvailable()){
                gesture = zx.readGesture();
                gestureSpeed = zx.readGestureSpeed();

                switch(gesture){
                    case RIGHT_SWIPE:   strncpy(gestureString, "Right Swipe\0", 10); break;
                    case LEFT_SWIPE:    strncpy(gestureString, "Left Swipe\0", 10); break;
                    case UP_SWIPE:      strncpy(gestureString, "Up Swipe\0", 10); break;
                    default:            strncpy(gestureString, "No Gesture\0", 10);
                }
            }

            // Defaults if no gesture was detected
            else{
                strncpy(gestureString, "No Gesture\0", 10);
                gestureSpeed = 0;
            }
        }
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ZXGesture::package() {
    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        switch(mode){
            case POS:
                json["Position_X"] = pos.x;
                json["Position_Z"] = pos.z;
                break;
            case GEST:
                json["Gesture"] = gestureString;
                json["Speed"] = gestureSpeed;
                break;
        }
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
//re-initialization 