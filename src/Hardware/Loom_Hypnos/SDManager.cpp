#include "SDManager.h"
#include "Logger.h"

namespace {
constexpr size_t MAX_SD_READ_BYTES = 4999;

bool appendText(char *destination, size_t destinationSize, const char *source) {
    if (destination == nullptr || destinationSize == 0 || source == nullptr)
        return false;

    const size_t used = strnlen(destination, destinationSize);
    if (used >= destinationSize)
        return false;

    const size_t available = destinationSize - used - 1;
    const size_t sourceLength = strlen(source);
    const size_t copyLength = sourceLength < available ? sourceLength : available;
    memcpy(destination + used, source, copyLength);
    destination[used + copyLength] = '\0';
    return copyLength == sourceLength;
}
} // namespace

//////////////////////////////////////////////////////////////////////////////////////////////////////
SDManager::SDManager(Manager *man, int sd_chip_select)
    : manInst(man), Module("SD Manager"), chip_select(sd_chip_select) {
    strncpy(device_name, manInst->get_device_name(), sizeof(device_name) - 1);
    device_name[sizeof(device_name) - 1] = '\0';
    memset(overrideFileName, '\0', 260);
} // Disables Lora so we can use the SD card on hypnos
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::writeLineToFile(const char *filename, const char *content) {

    // Check if the SD card is actually functional
    if (sdInitialized) {
        // Open the given file for writing
        myFile = sd.open(filename, O_RDWR | O_CREAT | O_APPEND);

        // Check if the file was actually opened, if so write the content to the file
        if (myFile) {
            myFile.println(content);
            myFile.close();
            return true;
        }
        printModuleName("Failed to Open File!");
        return false;
    }

    /* Wait for a bit so the user has time to read it */
    printModuleName(
        "SD Card was improperly initialized and as such this functionality was disabled!");
    delay(5000);
    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void SDManager::writeHeaders() {
    char header1[513];
    char header2[513];

    // Append the serial number to the top of the CSV file, reset the header1 array
    snprintf_P(header1, 512, PSTR("%s\n"), manInst->get_serial_num());
    myFile.println(header1);

    // Clear both arrays
    memset(header1, '\0', 512);
    memset(header2, '\0', 512);

    JsonObject document = manInst->getDocument().as<JsonObject>();
    appendText(header1, sizeof(header1), "ID,,");
    appendText(header2, sizeof(header2), "name,instance,");

    // If there is a key that contains timestamp data when need to include that separately
    if (document.containsKey("timestamp")) {
        appendText(header1, sizeof(header1), "timestamp,,");
        appendText(header2, sizeof(header2), "time_utc,time_local,");
    }

    // Get the contents containing the reset of the sensor data
    JsonArray contentsArray = document["contents"].as<JsonArray>();

    // Loop over each
    for (JsonVariant v : contentsArray) {
        // Get the module name
        appendText(header1, sizeof(header1), v.as<JsonObject>()["module"].as<const char *>());

        // Get all JSON keys
        for (JsonPair keyValue : v.as<JsonObject>()["data"].as<JsonObject>()) {
            appendText(header2, sizeof(header2), keyValue.key().c_str());
            appendText(header2, sizeof(header2), ",");
            appendText(header1, sizeof(header1), ",");
        }
    }

    // Write the headers to the file
    myFile.println(header1);
    myFile.println(header2);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::log(DateTime currentTime) {
    char output[MAX_JSON_SIZE + 1];
    bool logged = false;

    if (sdInitialized) {

        // Open the file in read/write mode, create the file if we need to and append the content to
        // the end of the file
        myFile = sd.open(fileName, O_RDWR | O_CREAT | O_APPEND);

        if (myFile) {

            // If this file has never been written to before we need to create and write the proper
            // headers to the file
            if (myFile.available() <= 3) {
                // Set the date created timestamp of the File
                myFile.timestamp(T_CREATE, currentTime.year(), currentTime.month(),
                                 currentTime.day(), currentTime.hour(), currentTime.minute(),
                                 currentTime.second());

                writeHeaders();
            }

            snprintf_P(output, MAX_JSON_SIZE, PSTR("%s,%i,"), manInst->get_device_name(),
                       manInst->get_instance_num());

            // Write the Instance data that isn't included in the JSON packet
            myFile.print(output);
            memset(output, '\0', MAX_JSON_SIZE); // Clear array

            JsonObject document = manInst->getDocument().as<JsonObject>();

            // If there is a key that contains timestamp data when need to include that separately
            if (document.containsKey("timestamp")) {
                char utcArr[21];
                char localArr[21];
                memset(utcArr, '\0', 21);
                memset(localArr, '\0', 21);
                strncpy(utcArr, document["timestamp"]["time_utc"].as<const char *>(), 21);
                strncpy(localArr, document["timestamp"]["time_local"].as<const char *>(), 21);

                // Format date with spaces when logging to SD
                char *indexPointer = strchr(utcArr, 'Z');
                if (indexPointer != nullptr) {
                    utcArr[10] = ' ';
                    utcArr[indexPointer - utcArr] = '\0';
                }

                // Format date with spaces when logging to SD
                indexPointer = strchr(localArr, 'Z');
                if (indexPointer != nullptr) {
                    localArr[10] = ' ';
                    localArr[indexPointer - localArr] = '\0';
                }

                // Format the time stamp in the CSV file
                appendText(output, sizeof(output), utcArr);
                appendText(output, sizeof(output), ",");
                appendText(output, sizeof(output), localArr);
                appendText(output, sizeof(output), ",");
            }

            // Get the contents containing the reset of the sensor data
            JsonArray contentsArray = document["contents"].as<JsonArray>();

            // Loop over each
            for (JsonVariant v : contentsArray) {

                // Get all JSON keys
                for (JsonPair keyValue : v.as<JsonObject>()["data"].as<JsonObject>()) {
                    appendText(output, sizeof(output), keyValue.value().as<String>().c_str());
                    appendText(output, sizeof(output), ",");
                }
            }

            // Write the matching data into the CSV file
            myFile.println(output);

            // Set the last modified date
            myFile.timestamp(T_WRITE, currentTime.year(), currentTime.month(), currentTime.day(),
                             currentTime.hour(), currentTime.minute(), currentTime.second());

            // Close the file
            myFile.close();

            // Inform the user that we have successfully written to the file
            snprintf_P(output, MAX_JSON_SIZE, PSTR("Successfully logged data to %s"), fileName);
            LOG(output);
            logged = true;

        } else {
            printModuleName("Failed to open log file!");
        }

        // If we want to log batch data do so
        if (logged && batch_size > 0)
            logBatch();

    } else {
        printModuleName("Failed to log! SD card not Initialized!");
    }
    return logged;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::begin() {

    pinMode(8, OUTPUT);
    digitalWrite(8, HIGH); // Disable LoRa

    printModuleName("Initializing SD Card...");

    /*
     Start the SD card with the fastest SPI speed
     Changing to 4MHz because 24MHz has occasional stability issues on different devices.
     SD_SCK_MHZ is from the SDFat library which takes the an integer parameter that sets the Serial
     clock frequency that SPI uses to communicate between the SD and the MCU(m0) Setting it to 50 is
     essentially asking the m0 to set the SCK freq to the max, up to 50MHz. (for an m0 the max is
     24MHz)

     Other possible settings can be multiples of 48MHz(m0 CPU Freq) such as,
     24MHz
     16MHz
     12MHz
     8MHz
     6MHz
     4MHz
   */
    if (!sd.begin(chip_select, SD_SCK_MHZ(4))) {
        sdInitialized = false;
        printModuleName("Failed to Initialize SD Card! SD Card functionality will be disabled, is "
                        "there an SD card inserted into the device?");
        return false;
    } else {
        // Make a debug folder if it doesn't already exist
        if (!sd.exists("debug"))
            sd.mkdir("debug");

        printModuleName("Successfully initialized SD Card!");
    }

    // Only should be run on the first initialize not when it wakes up from sleep
    if (!sdInitialized) {
        // Try to open the root of the file system so we can get the files on the device
        if (!root.open("/", O_RDONLY)) {
            sdInitialized = false;
            printModuleName("ERROR");
            ERROR(F("Failed to open root file system on SD Card!"));
            printModuleName("After ERROR");
            return false;
        }
        updateCurrentFileName();
    }

    // Once the SD card has initialized the first round through we don't want to update the file
    // name
    sdInitialized = true;

    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::updateCurrentFileName() {
    uint16_t indexDir = 0;
    char f_name[260];
    char *strLocation;

    // What number we need to append to the file name
    file_count = 0;

    // While there is a next file to open, open it
    while (scanningFile.openNext(&root)) {
        scanningFile.getName(f_name, 25);

        if (strlen(overrideFileName) > 0) {
            // Check if the substring exists
            strLocation = strstr(f_name, overrideFileName);
        } else {
            // Check if the substring exists
            strLocation = strstr(f_name, device_name);
        }

        if (strLocation != NULL) {
            // Increase the file count per loop to track what the next file should be
            file_count++;
        }
        scanningFile.close();
    }

    // Account for the batch files if we are using batch logging
    if (batch_size > 0) {
        file_count = file_count / 2;
    }

    if (strlen(overrideFileName) > 0) {
        // Set all the fileNames with the override name
        snprintf_P(fileName, 260, PSTR("%s%i.csv"), overrideFileName, getCurrentFileNumber());
        snprintf_P(fileNameNoExtension, 260, PSTR("%s%i"), overrideFileName,
                   getCurrentFileNumber());
        snprintf_P(batchFileName, 260, PSTR("%s-Batch.txt"), fileNameNoExtension);

    } else {
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
char *SDManager::readFile(const char *fileName) {
    if (!sdInitialized) {
        printModuleName("Failed to read! SD card not Initialized!");
        return nullptr;
    }

    myFile = sd.open(fileName);
    if (!myFile) {
        printModuleName("Failed to open file!");
        return nullptr;
    }

    const size_t fileSize = myFile.size();
    if (fileSize > MAX_SD_READ_BYTES) {
        printModuleName("File is too large to read safely into memory!");
        myFile.close();
        return nullptr;
    }

    char *fileContents = static_cast<char *>(malloc(fileSize + 1));
    if (fileContents == nullptr) {
        printModuleName("Failed to allocate memory for file contents!");
        myFile.close();
        return nullptr;
    }

    size_t index = 0;
    while (myFile.available() && index < fileSize)
        fileContents[index++] = static_cast<char>(myFile.read());
    fileContents[index] = '\0';
    myFile.close();
    return fileContents;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void SDManager::logBatch() {
    char f_name[260];
    char jsonString[MAX_JSON_SIZE];
    snprintf_P(f_name, 260, PSTR("%s-Batch.txt"), fileNameNoExtension);
    // We want to clear the file after the batch size has been exceeded
    if (current_batch >= batch_size) {
        current_batch = 0;
        myFile = sd.open(f_name, O_WRITE | O_TRUNC | O_APPEND);
    } else {
        myFile = sd.open(f_name, O_WRITE | O_CREAT | O_APPEND);
    }
    // Check if the file has been opened properly and write the JSON packet to one line
    if (myFile) {

        manInst->getJSONString(jsonString);
        myFile.println(jsonString);
        myFile.close();
        current_batch++;

    } else {
        printModuleName("Failed to open file!");
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
