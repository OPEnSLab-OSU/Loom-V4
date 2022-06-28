#include "SDManager.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
SDManager::SDManager(Manager* man, int sd_chip_select) : manInst(man), Module("SD Manager"), chip_select(sd_chip_select) {
    device_name = manInst->get_device_name();
 } // Disables Lora so we can use the SD card on hypnos 
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::writeLineToFile(String filename, String content){

    // Check if the SD card is actually functional

    if(sdInitialized){
        // Open the given file for writing
        myFile = sd.open(filename, FILE_WRITE);
    
        // Check if the file was actually opened, if so write the content to the file
        if(myFile){
            myFile.println(content);
            myFile.close();
            printModuleName(); Serial.println("Content successfully written to file: " + filename);
            return true;
        }
        printModuleName(); Serial.println("Failed to Open File!");
        return false;
    }

    printModuleName(); Serial.println("SD Card was improperly initialized and as such this functionality was disabled!");
    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void SDManager::createHeaders(){

    // Constants not pulled from the JSON
    headers[0] += "ID,,";
    headers[1] += "name,instance,";

    // Get all JSON keys  
    for(JsonPair keyValue : manInst->getDocument().as<JsonObject>()){
        
        headers[0] += String(keyValue.key().c_str());

        // Sub sections
        for(JsonPair value : manInst->getDocument()[keyValue.key().c_str()].as<JsonObject>()){
            headers[1] += String(value.key().c_str()) + ",";

            //data += ((manInst->getDocument().as<JsonObject>()[keyValue.key().c_str()][String(value.key().c_str())]).as<String>()) + ",";

            headers[0] += ",";
        }

    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::log(DateTime currentTime){

    // String representing all the sensor data in a csv format
    String data = "";

    if(sdInitialized){

        // Open the file in read/write mode, create the file if we need to and append the content to the end of the file
        myFile = sd.open(fileName, O_RDWR | O_CREAT | O_APPEND);

        if(myFile){

            // If this file has never been written to before we need to create and write the proper headers to the file
            if(myFile.available() <= 3){
                // Set the date created timestamp of the File
                myFile.timestamp(T_CREATE, currentTime.year(), currentTime.month(), currentTime.day(), currentTime.hour(), currentTime.minute(), currentTime.second());
                
                createHeaders();
                myFile.println(headers[0]);
                myFile.println(headers[1]);
            }    

            // Write the Instance data that isn't included in the JSON packet
            myFile.print(device_name + "," + String(manInst->get_instance_num()) + ",");

            // Create the string of Sensor data from the JSON document to be written to the file
            for(JsonPair keyValue : manInst->getDocument().as<JsonObject>()){
                for(JsonPair value : manInst->getDocument()[keyValue.key().c_str()].as<JsonObject>()){
                    data += ((manInst->getDocument().as<JsonObject>()[keyValue.key().c_str()][String(value.key().c_str())]).as<String>()) + ",";
                }
            }

            // Write the matching data into the CSV file
            myFile.println(data);

            // Set the last modified date
            myFile.timestamp(T_WRITE , currentTime.year(), currentTime.month(), currentTime.day(), currentTime.hour(), currentTime.minute(), currentTime.second());

            // Close the file
            myFile.close();

            // Inform the user that we have successfully written to the file
            printModuleName(); Serial.println("Successfully logged data to " + fileName);
        }
        else{
            printModuleName(); Serial.println("Failed to open log file!");
        }
    }
    else{
        printModuleName(); Serial.println("Failed to log! SD card not Initialized!");
    }
    
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::begin(){
    digitalWrite(8, HIGH);  // Disable LoRa

    printModuleName(); Serial.println("Initializing SD Card...");

    // Start the SD card with the fastest SPI speed
    if(!sd.begin(chip_select, SD_SCK_MHZ(50))){
        printModuleName(); Serial.println("Failed to Initialize SD Card! SD Card functionality will be disabled, is there an SD card inserted into the device?");
        return false;
    }
    else{
        printModuleName(); Serial.println("Successfully initialized SD Card!");
    }

    

    // Only should be run on the first initialize not when it wakes up from sleep
    if(!sdInitialized){
        // Try to open the root of the file system so we can get the files on the device
        if(!root.open("/", O_RDONLY)){
            printModuleName(); Serial.println("Failed to open root file system on SD Card!");
            return false;
        }
        updateCurrentFileName();
    }
    
    // Once the SD card has initialized the first round through we don't want to update the file name
    sdInitialized = true;

    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::updateCurrentFileName(){
    uint16_t indexDir = 0;

    // What number we need to append to the file name
    int file_count = -1;

    // While there is a next file to open, open it
    while(scanningFile.openNext(&root)){

        // Increase the file count per loop to track what the next file should be
        file_count++;
        scanningFile.close();
    }

    // Fully piece together the file name where will write the SD data to
    fileName = device_name + String(file_count) + ".csv";

    // Close the root file after we have decided what to name the next file
    root.close();
    printModuleName(); Serial.println("Data will be logged to " + fileName);

}
//////////////////////////////////////////////////////////////////////////////////////////////////////