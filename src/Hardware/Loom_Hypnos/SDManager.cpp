#include "SDManager.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
SDManager::SDManager(Manager* man, int sd_chip_select) : manInst(man), Module("SD Manager"), chip_select(sd_chip_select) {
    strncpy(device_name, manInst->get_device_name(), 100);
 } // Disables Lora so we can use the SD card on hypnos 
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::writeLineToFile(const char* filename, const char* content){

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
        printModuleName("Failed to Open File!");
        return false;
    }

    printModuleName("SD Card was improperly initialized and as such this functionality was disabled!");
    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void SDManager::writeHeaders(){
    char header1[512];
    char header2[512];

    // Append the serial number to the top of the CSV file, reset the header1 array
    snprintf_P(header1, 512, PSTR("%s\n"), manInst->get_serial_num());
    myFile.println(header1);

    // Clear both arrays
    memset(header1, '\0', 512);
    memset(header2, '\0', 512);

    JsonObject document = manInst->getDocument().as<JsonObject>();
    strncat(header1, "ID,,", 512);
    strncat(header2, "name,instance,", 512);
    
    // If there is a key that contains timestamp data when need to include that separately 
    if(document.containsKey("timestamp")){
        strncat(header1, "timestamp,,", 512);
        strncat(header2, "time_utc,time_local,", 512);
    }
    
    // Get the contents containing the reset of the sensor data
    JsonArray contentsArray = document["contents"].as<JsonArray>();

    // Loop over each 
    for(JsonVariant v : contentsArray) {
        // Get the module name
        strncat(header1, v.as<JsonObject>()["module"].as<const char*>(), 512);

        // Get all JSON keys  
        for(JsonPair keyValue : v.as<JsonObject>()["data"].as<JsonObject>()){
            strncat(header2, keyValue.key().c_str(), 512);
            strncat(header2, ",", 512);
            strncat(header1, ",", 512);
        }
    }

    // Write the headers to the file
    myFile.println(header1);
    myFile.println(header2);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::log(DateTime currentTime){
    char output[2000];
    
    if(sdInitialized){
        
        // Open the file in read/write mode, create the file if we need to and append the content to the end of the file
        myFile = sd.open(fileName, O_RDWR | O_CREAT | O_APPEND);
        
        if(myFile){
            
            // If this file has never been written to before we need to create and write the proper headers to the file
            if(myFile.available() <= 3){
                // Set the date created timestamp of the File
                myFile.timestamp(T_CREATE, currentTime.year(), currentTime.month(), currentTime.day(), currentTime.hour(), currentTime.minute(), currentTime.second());
                
                writeHeaders();
            }    
            
            if(!useOverriden)
            snprintf_P(output, 2000, PSTR("%s,%i,"), manInst->get_device_name(), manInst->get_instance_num());
            // Write the Instance data that isn't included in the JSON packet
            myFile.print(output);
            memset(output, '\0', 2000); // Clear array

            JsonObject document = manInst->getDocument().as<JsonObject>();
            

            // If there is a key that contains timestamp data when need to include that separately 
            if(document.containsKey("timestamp")){
                char utcArr[21];
                char localArr[21];
                memset(utcArr, '\0', 21);
                memset(localArr, '\0', 21);
                strncpy(utcArr, document["timestamp"]["time_utc"].as<const char*>(), 21);
                strncpy(localArr, document["timestamp"]["time_local"].as<const char*>(), 21);

                // Format date with spaces when logging to SD
                char *indexPointer = strchr(utcArr, 'Z');
                utcArr[10] = ' ';
                utcArr[indexPointer-utcArr] = '\0';

                
                // Format date with spaces when logging to SD
                indexPointer = strchr(localArr, 'Z');
                localArr[10] = ' ';
                localArr[indexPointer-localArr] = '\0';

                // Format the time stamp in the CSV file
                strncat(output, utcArr, 2000);
                strncat(output, ",", 2000);
                strncat(output, localArr, 2000);
                strncat(output, ",", 2000);
            }
            

            // Get the contents containing the reset of the sensor data
            JsonArray contentsArray = document["contents"].as<JsonArray>();

            // Loop over each 
            for(JsonVariant v : contentsArray) {

                // Get all JSON keys  
                for(JsonPair keyValue : v.as<JsonObject>()["data"].as<JsonObject>()){
                    strncat(output, keyValue.value().as<String>().c_str(), 2000);
                    strncat(output, ",", 2000);
                }
            }

            // Write the matching data into the CSV file
            myFile.println(output);

            // Set the last modified date
            myFile.timestamp(T_WRITE , currentTime.year(), currentTime.month(), currentTime.day(), currentTime.hour(), currentTime.minute(), currentTime.second());

            // Close the file
            myFile.close();

            // Inform the user that we have successfully written to the file
            snprintf_P(output, 2000, PSTR("Successfully logged data to %s"), fileName);
            printModuleName(output);
            
        }
        else{
            printModuleName("Failed to open log file!");
        }

        // If we want to log batch data do so
        if(batch_size > 0)
            logBatch();
        
    }
    else{
        printModuleName("Failed to log! SD card not Initialized!");
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

        printModuleName("Successfully initialized SD Card!");
    }

    

    // Only should be run on the first initialize not when it wakes up from sleep
    if(!sdInitialized){
        // Try to open the root of the file system so we can get the files on the device
        if(!root.open("/", O_RDONLY)){
            ERROR(F("Failed to open root file system on SD Card!"));
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
    char f_name[260];

    // What number we need to append to the file name
    file_count = 0;

    // While there is a next file to open, open it
    while(scanningFile.openNext(&root)){
        scanningFile.getName(f_name, 25);

        // Check if the substring exists
        char* strLocation = strstr(f_name, device_name);
        if(strLocation != NULL){
            // Increase the file count per loop to track what the next file should be
            file_count++;
        }
        scanningFile.close();
    }

    // Account for the batch files if we are using batch logging
    if(batch_size > 0){
        file_count = file_count / 2;
    }

    if(!useOverriden){
        // Set all the fileNames
        snprintf_P(fileName, 260, PSTR("%s%i.csv"), device_name, getCurrentFileNumber()); 
        snprintf_P(fileNameNoExtension, 260, PSTR("%s%i"), device_name, getCurrentFileNumber()); 
        snprintf_P(batchFileName, 260, PSTR("%s-Batch.txt"), fileNameNoExtension);
    }

    // Close the root file after we have decided what to name the next file
    root.close();

    char output[OUTPUT_SIZE];
    snprintf_P(output, OUTPUT_SIZE, PSTR("Data will be logged to %s"), fileName);
    printModuleName(output);

    return true;

}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
char* SDManager::readFile(const char* fileName){
    // Clear contents 
    char* fileContents =  (char*) malloc(5000);
    memset(fileContents, '\0', 5000);

    long index = 0;
    if(sdInitialized){
        myFile = sd.open(fileName);

        if(myFile){
            // read from the file until there's nothing else in it:
            while (myFile.available()) {
                fileContents[index] = (char)(myFile.read());
                index++;
            }
            fileContents[index] = '\0';
            myFile.close();
        }
        else{
            printModuleName("Failed to open file!");
        }
    }
    else{
        printModuleName("Failed to read! SD card not Initialized!");
    }
    return fileContents;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void SDManager::logBatch(){
    char f_name[260];
    char jsonString[2000];
    snprintf_P(f_name, 260, PSTR("%s-Batch.txt"), fileNameNoExtension);
    // We want to clear the file after the batch size has been exceeded
    if(current_batch >= batch_size){
        current_batch = 0;
        myFile = sd.open(f_name, O_WRITE | O_TRUNC | O_APPEND);
    }
    else{
        myFile = sd.open(f_name, O_WRITE | O_CREAT | O_APPEND);
    }
    // Check if the file has been opened properly and write the JSON packet to one line
    if(myFile){
      
        manInst->getJSONString(jsonString);
        myFile.println(jsonString);
        myFile.close();
        current_batch++;
        
    }else{
        printModuleName("Failed to open file!");
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////