#include "Loom_ZXGesture.h";

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_ZXGesture::Loom_ZXGesture(
                            Manager& man, 
                            int address, 
                            bool useMux,
                            Mode mode
                    ) : Module("ZX Gesture"), manInst(&man), zx( ZX_Sensor(address)), mode(mode) {

                        // Register the module with the manager
                        if(!useMux)
                            manInst->registerModule(this);
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ZXGesture::initialize() {
    if(!zx.init()){
        printModuleName(); Serial.println("Failed to initialize ZX Gesture Sensor! Check connections and try again...");
    }
    else{

        // Make sure the version number is correct
        uint8_t ver = zx.getModelVersion();
        printModuleName();
        if (ver != ZX_MODEL_VER) {
            moduleInitialized = false;
            Serial.println("Incorrect Model Version. Expected Version: " + String(ZX_MODEL_VER) + " Actual Version: " + String(ver));
            return;
        } else {
            Serial.println("Model Version: " + String(ver));
        }

        // Read the register map version and ensure the library will work
        ver = zx.getRegMapVersion();
        printModuleName();
        if (ver != ZX_REG_MAP_VER) {
            moduleInitialized = false;
            Serial.println("Incorrect Register Map Version. Expected Version: " + String(ZX_REG_MAP_VER) + " Actual Version: " + String(ver));
            return;
        } else {
            Serial.println("Register Map Version: " + String(ver));
        }

        printModuleName(); Serial.println("Successfully initialized ZX Gesture Sensor!");
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ZXGesture::measure() {

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
                printModuleName(); Serial.println("Error occurred while reading position data");
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
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ZXGesture::package() {
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
//////////////////////////////////////////////////////////////////////////////////////////////////////