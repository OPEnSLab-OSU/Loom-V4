#include "Loom_K30.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_K30::Loom_K30(Manager& man, int addr, bool warmUp, int valMult) : Module("K30"), manInst(&man), valMult(valMult), warmUp(warmUp), addr(addr){
    module_address = addr;
    manInst->registerModule(this);
    type = CommType::I2C;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_K30::Loom_K30(Manager& man, int rx, int tx, bool warmUp, int valMult) : Module("K30"), manInst(&man), valMult(valMult), warmUp(warmUp){
    manInst->registerModule(this);
    type = CommType::SERIAL_MODE;
    K30_Serial = new Uart(&sercom1, rx, tx, SERCOM_RX_PAD_3, UART_TX_PAD_0);

    //Assign pins 10 & 11 SERCOM functionality
    pinPeripheral(tx, PIO_SERCOM);
    pinPeripheral(rx, PIO_SERCOM);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_K30::~Loom_K30(){
    if(K30_Serial != nullptr)
        delete K30_Serial;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_K30::initialize(){
    if(type == I2C){
        Wire.begin();

        // Check if the sensor is actually connected
        Wire.beginTransmission(addr);
        if(Wire.endTransmission() != 0){
            printModuleName(); Serial.println("Failed to initialize sensor!");
            moduleInitialized = false;
            return;
        }
    }
    else{
        K30_Serial->begin(9600);
    }

    // If we want to wait for the sensor to warmup do so here
    if(warmUp){
        printModuleName(); Serial.println("Warm-up was enabled for this sensor. Initialization will now pause for 6 minutes");

        // Pause for 6 minutes
        manInst->pause(60000 * 6);
    }

    printModuleName(); Serial.println("Initialized successfully!");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_K30::measure(){
    getCO2Level();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_K30::package(){
    JsonObject json = manInst->get_data_object(getModuleName());
    json["CO2_Level"] = CO2Levels;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_K30::verifyChecksum(){
    // Verify the data is actually correct
    byte sum = 0;
    sum = buffer[0] + buffer[1] + buffer[2]; 
    return sum == buffer[3];
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
void Loom_K30::getCO2Level() {
    if(type == SERIAL_MODE){
        printModuleName(); Serial.println("Sending Request to K30 Sensor");
        delay(100);
        while (!K30_Serial->available()) //keep sending request until we start to get a response
        {
            printModuleName(); Serial.println("Sensor_Serial Available, Retrieving Response");  // TODO:This doesn't make sense: in line 41 and line 49
            K30_Serial->write(read_CO2, 7);
            delay(50);
        }

        int timeout = 0; //set a timeoute counter
        while (K30_Serial->available() < 7 ) //Wait to get a 7 byte response
        {
            printModuleName(); Serial.println("Sensor_Serial Unavailable, Trying to send request again");
            timeout++;
            if (timeout > 10)   //if it takes to long there was probably an error
            {
                while (K30_Serial->available()) //flush whatever we have
                    K30_Serial->read();
                break;                        //exit and try again
            }
            delay(50);
        }
        for (int i = 0; i < 7; i++)
        {
            sensor_response[i] = K30_Serial->read();
        }
        printModuleName(); Serial.println("Finished Sending Request to K30 sensor");

        int high = sensor_response[3];                        //high byte for value is 4th byte in packet in the packet
        int low = sensor_response[4];                         //low byte for value is 5th byte in the packet
        unsigned long val = high * 256 + low;                 //Combine high byte and low byte with this formula to get value
        CO2Levels = val;
    }
    else{
        // Send the request for data
        Wire.beginTransmission(addr);
        Wire.write(0x22);
        Wire.write(0x00);
        Wire.write(0x08);
        Wire.write(0x2A);
        Wire.endTransmission();

        // Wait to ensure data is properly recorded
        delay(10);

        // Request 4 bytes of data from the sensor
        Wire.requestFrom(addr, 4);

        // Read the data from the wire
        for(int i = 0; i < 4; i++){
            buffer[i] = Wire.read();
        }

        // Make sure the data is correct
        if(verifyChecksum()){
            // Bit shift the result into an integer
            CO2Levels = 0;
            CO2Levels |= buffer[1] & 0xFF;
            CO2Levels = CO2Levels << 8;
            CO2Levels |= buffer[2] & 0xFF;
        }
        else{
            printModuleName(); Serial.println("Failed to validate checksum! Using previously recorded data.");
        }
    }
}