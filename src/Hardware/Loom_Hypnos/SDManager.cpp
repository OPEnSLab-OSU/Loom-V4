#include "SDManager.h"
#include "Logger.h"

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
        myFile = sd.open(filename, O_RDWR | O_CREAT | O_APPEND);
    
        // Check if the file was actually opened, if so write the content to the file
        if(myFile){
            myFile.println(content);
            myFile.close();
            return true;
        }
        LOG("Failed to Open File!");
        return false;
    }

    LOG("SD Card was improperly initialized and as such this functionality was disabled!");
    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void SDManager::createHeaders(){

    // Append the serial number to the top of the CSV file
    headers[0] = manInst->get_serial_num() + "\n";
    JsonObject document = manInst->getDocument().as<JsonObject>();

    // Constants not pulled from the JSON
    headers[0] += "ID,,";
    headers[1] += "name,instance,";
    
    // If there is a key that contains timestamp data when need to include that separately 
    if(document.containsKey("timestamp")){
        headers[0] += "timestamp,,";
        headers[1] += "time_utc,time_local,";
    }
    
    // Get the contents containing the reset of the sensor data
    JsonArray contentsArray = document["contents"].as<JsonArray>();

    // Loop over each 
    for(JsonVariant v : contentsArray) {
        // Get the module name
        headers[0] += v.as<JsonObject>()["module"].as<String>();

        // Get all JSON keys  
        for(JsonPair keyValue : v.as<JsonObject>()["data"].as<JsonObject>()){
            headers[1] += String(keyValue.key().c_str()) + ",";
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
            myFile.print(manInst->get_device_name() + "," + String(manInst->get_instance_num()) + ",");
            JsonObject document = manInst->getDocument().as<JsonObject>();

            // If there is a key that contains timestamp data when need to include that separately 
            if(document.containsKey("timestamp")){
                String utc = document["timestamp"]["time_utc"].as<String>();
                String local = document["timestamp"]["time_local"].as<String>();

                // Format date with spaces when logging to SD
                utc.replace("T", " ");
                utc.replace("Z", "");

                // Format date with spaces when logging to SD
                local.replace("T", " ");
                local.replace("Z", "");

                data += utc + ",";
                data += local + ",";
            }

            // Get the contents containing the reset of the sensor data
            JsonArray contentsArray = document["contents"].as<JsonArray>();

            // Loop over each 
            for(JsonVariant v : contentsArray) {

                // Get all JSON keys  
                for(JsonPair keyValue : v.as<JsonObject>()["data"].as<JsonObject>()){
                    data += String(keyValue.value().as<String>()) + ",";
                }
            }

            // Write the matching data into the CSV file
            myFile.println(data);

            // Set the last modified date
            myFile.timestamp(T_WRITE , currentTime.year(), currentTime.month(), currentTime.day(), currentTime.hour(), currentTime.minute(), currentTime.second());

            // Close the file
            myFile.close();

            // Inform the user that we have successfully written to the file
            LOG("Successfully logged data to " + fileName);
        }
        else{
            LOG("Failed to open log file!");
        }

        // If we want to log batch data do so
        if(batch_size > 0)
            logBatch();
    }
    else{
        LOG("Failed to log! SD card not Initialized!");
    }
    
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::begin(){
    digitalWrite(8, HIGH);  // Disable LoRa

    printModuleName("Initializing SD Card...");

    // Start the SD card with the fastest SPI speed
    if(!sd.begin(chip_select, SD_SCK_MHZ(50))){
        printModuleName("Failed to Initialize SD Card! SD Card functionality will be disabled, is there an SD card inserted into the device?");
        return false;
    }
    else{
        // Make a debug folder if it doesn't already exist
        if(!sd.exists("debug"))
            sd.mkdir("debug");

        LOG("Successfully initialized SD Card!");
    }

    

    // Only should be run on the first initialize not when it wakes up from sleep
    if(!sdInitialized){
        // Try to open the root of the file system so we can get the files on the device
        if(!root.open("/", O_RDONLY)){
            ERROR("Failed to open root file system on SD Card!");
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
    char f_name[25];

    // What number we need to append to the file name
    file_count = 0;

    // While there is a next file to open, open it
    while(scanningFile.openNext(&root)){
        scanningFile.getName(f_name, 25);
        if(String(f_name).startsWith(device_name)){
            // Increase the file count per loop to track what the next file should be
            file_count++;
        }
        scanningFile.close();
    }

    // Check if we set an override name or not
    if(overrideName.length() <= 0)
        fileNameNoExtension = device_name + String(file_count);
    else
        fileNameNoExtension = overrideName + String(file_count);

    fileName = fileNameNoExtension + ".csv";

    // Close the root file after we have decided what to name the next file
    root.close();
    LOG("Data will be logged to " + fileName);

    return true;

}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
String SDManager::readFile(String fileName){
    String output = "";
    if(sdInitialized){
        myFile = sd.open(fileName);
        if(myFile){
            // read from the file until there's nothing else in it:
            while (myFile.available()) {
                output += (char)(myFile.read());
            }
            // close the file:
            myFile.close();
            return output;
        }
        else{
            ERROR("Failed to open file!");
        }
    }
    else{
        ERROR("Failed to read! SD card not Initialized!");
    }
    return "";
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void SDManager::logBatch(){

    // We want to clear the file after the batch size has been exceeded
    if(current_batch >= batch_size){
        current_batch = 0;
        myFile = sd.open(fileNameNoExtension + "-Batch.txt", O_WRITE | O_TRUNC | O_APPEND);
    }
    else{
        myFile = sd.open(fileNameNoExtension + "-Batch.txt", O_WRITE | O_CREAT | O_APPEND);
    }


    // Check if the file has been opened properly and write the JSON packet to one line
    if(myFile){
        myFile.println(manInst->getJSONString());
        myFile.close();
        current_batch++;
    }else{
        ERROR("Failed to open file!");
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////