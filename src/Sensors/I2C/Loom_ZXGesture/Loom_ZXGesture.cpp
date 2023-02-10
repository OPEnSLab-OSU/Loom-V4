#include "Loom_ZXGesture.h"

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
    if(!zx.init()){
        printModuleName("Failed to initialize ZX Gesture Sensor! Check connections and try again...");
    }
    else{

        // Make sure the version number is correct
        uint8_t ver = zx.getModelVersion();
        //printModuleName();
        if (ver != ZX_MODEL_VER) {
            moduleInitialized = false;
            printModuleName("Incorrect Model Version. Expected Version: " + String(ZX_MODEL_VER) + " Actual Version: " + String(ver));
            return;
        } else {
            printModuleName("Model Version: " + String(ver));
        }

        // Read the register map version and ensure the library will work
        ver = zx.getRegMapVersion();
        //printModuleName();
        if (ver != ZX_REG_MAP_VER) {
            moduleInitialized = false;
            printModuleName("Incorrect Register Map Version. Expected Version: " + String(ZX_REG_MAP_VER) + " Actual Version: " + String(ver));
            return;
        } else {
            printModuleName("Register Map Version: " + String(ver));
        }

        printModuleName("Successfully initialized ZX Gesture Sensor!");
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
            printModuleName("No acknowledge received from the device");
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
                    printModuleName("Error occurred while reading position data");
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
                    case RIGHT_SWIPE: gestureString = "Right Swipe"; break;
                    case LEFT_SWIPE: gestureString = "Left Swipe"; break;
                    case UP_SWIPE: gestureString = "Up Swipe"; break;
                    default: gestureString = "No Gesture";
                }
            }

            // Defaults if no gesture was detected
            else{
                gestureString = "No Gesture";
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