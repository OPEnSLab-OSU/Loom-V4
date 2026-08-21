#include "SDManager.h"
#include "Logger.h"

namespace {
constexpr size_t MAX_SD_READ_BYTES = 4999;

void copyBounded(char *destination, size_t destinationSize, const char *source,
                 size_t maxSourceCharacters) {
    if (destinationSize == 0)
        return;

    size_t index = 0;
    if (source != nullptr) {
        while (index < destinationSize - 1 && index < maxSourceCharacters &&
               source[index] != '\0') {
            destination[index] = source[index];
            ++index;
        }
    }
    destination[index] = '\0';
}

void appendLiteral(char *destination, size_t destinationSize, const char *suffix) {
    if (destinationSize == 0 || suffix == nullptr)
        return;

    size_t destinationIndex = strlen(destination);
    size_t suffixIndex = 0;
    while (destinationIndex < destinationSize - 1 && suffix[suffixIndex] != '\0')
        destination[destinationIndex++] = suffix[suffixIndex++];
    destination[destinationIndex] = '\0';
}

void buildNumberedName(char *destination, size_t destinationSize, const char *base, int number,
                       const char *suffix) {
    char numberText[12];
    snprintf(numberText, sizeof(numberText), "%i", number);

    const size_t reserved = strlen(numberText) + strlen(suffix) + 1;
    const size_t maxBaseCharacters = destinationSize > reserved ? destinationSize - reserved : 0;
    copyBounded(destination, destinationSize, base, maxBaseCharacters);
    appendLiteral(destination, destinationSize, numberText);
    appendLiteral(destination, destinationSize, suffix);
}

void writeCsvTimestamp(File &file, const char *timestamp) {
    if (timestamp == nullptr)
        return;

    for (size_t index = 0; timestamp[index] != '\0' && timestamp[index] != 'Z'; ++index)
        file.write(index == 10 ? ' ' : timestamp[index]);
}
} // namespace

//////////////////////////////////////////////////////////////////////////////////////////////////////
SDManager::SDManager(Manager *man, int sd_chip_select)
    : Module("SD Manager"), manInst(man), chip_select(sd_chip_select) {
    strncpy(device_name, manInst->get_device_name(), sizeof(device_name) - 1);
    device_name[sizeof(device_name) - 1] = '\0';
    memset(batchFileName, '\0', sizeof(batchFileName));
    memset(fileName, '\0', sizeof(fileName));
    memset(overrideFileName, '\0', sizeof(overrideFileName));
} // Disables Lora so we can use the SD card on hypnos
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::writeLineToFile(const char *filename, const char *content) {
    if (filename == nullptr || filename[0] == '\0' || content == nullptr) {
        printModuleName("Cannot write a null/empty filename or null content!");
        return false;
    }

    // Check if the SD card is actually functional
    if (sdInitialized) {
        // Open the given file for writing
        myFile = sd.open(filename, O_RDWR | O_CREAT | O_APPEND);

        // Check if the file was actually opened, if so write the content to the file
        if (myFile) {
            const size_t contentLength = strlen(content);
            const bool wroteContent = myFile.print(content) == contentLength;
            const bool wroteNewline = myFile.println() > 0;
            myFile.close();
            if (wroteContent && wroteNewline)
                return true;
            printModuleName("Failed while writing file contents!");
            return false;
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
bool SDManager::writeHeaders() {
    // Preserve the legacy byte layout: the serial text contained an LF before println() added its
    // normal line ending, leaving the established blank separator without a 513-byte stack array.
    myFile.print(manInst->get_serial_num());
    myFile.write('\n');
    myFile.println();

    JsonObject document = manInst->getDocument().as<JsonObject>();

    // Write the first header directly. Building both headers in RAM previously consumed 1 KB of
    // stack on a Cortex-M0 just before an SD write.
    myFile.print(F("ID,,"));

    // If there is a key that contains timestamp data when need to include that separately
    if (document.containsKey("timestamp"))
        myFile.print(F("timestamp,,"));

    JsonArray contentsArray = document["contents"].as<JsonArray>();
    for (JsonVariant v : contentsArray) {
        myFile.print(v.as<JsonObject>()["module"].as<const char *>());
        size_t fieldCount = v.as<JsonObject>()["data"].as<JsonObject>().size();
        while (fieldCount-- > 0)
            myFile.print(',');
    }
    myFile.println();

    // The second header is a separate pass over the small in-memory JSON tree.
    myFile.print(F("name,instance,"));
    if (document.containsKey("timestamp"))
        myFile.print(F("time_utc,time_local,"));
    for (JsonVariant v : contentsArray) {
        for (JsonPair keyValue : v.as<JsonObject>()["data"].as<JsonObject>()) {
            myFile.print(keyValue.key().c_str());
            myFile.print(',');
        }
    }
    myFile.println();

    return !myFile.getWriteError();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::log(DateTime currentTime) {
    bool logged = false;

    if (sdInitialized) {

        // Open the file in read/write mode, create the file if we need to and append the content to
        // the end of the file
        myFile = sd.open(fileName, O_RDWR | O_CREAT | O_APPEND);

        if (myFile) {
            myFile.clearWriteError();

            // If this file has never been written to before we need to create and write the proper
            // headers to the file
            if (myFile.available() <= 3) {
                const uint32_t originalSize = myFile.fileSize();
                // Set the date created timestamp of the File
                myFile.timestamp(T_CREATE, currentTime.year(), currentTime.month(),
                                 currentTime.day(), currentTime.hour(), currentTime.minute(),
                                 currentTime.second());

                if (!writeHeaders() || !myFile.sync()) {
                    myFile.truncate(originalSize);
                    myFile.close();
                    printModuleName("Failed while writing CSV headers!");
                    return false;
                }
            }

            const uint32_t recordStart = myFile.fileSize();
            myFile.clearWriteError();

            // Write the Instance data that isn't included in the JSON packet
            myFile.print(manInst->get_device_name());
            myFile.print(',');
            myFile.print(manInst->get_instance_num());
            myFile.print(',');

            JsonObject document = manInst->getDocument().as<JsonObject>();

            // If there is a key that contains timestamp data when need to include that separately
            if (document.containsKey("timestamp")) {
                writeCsvTimestamp(myFile,
                                  document["timestamp"]["time_utc"].as<const char *>());
                myFile.print(',');
                writeCsvTimestamp(myFile,
                                  document["timestamp"]["time_local"].as<const char *>());
                myFile.print(',');
            }

            // Get the contents containing the reset of the sensor data
            JsonArray contentsArray = document["contents"].as<JsonArray>();

            // Loop over each
            for (JsonVariant v : contentsArray) {

                // Get all JSON keys
                for (JsonPair keyValue : v.as<JsonObject>()["data"].as<JsonObject>()) {
                    JsonVariant value = keyValue.value();
                    if (value.is<const char *>())
                        myFile.print(value.as<const char *>());
                    else
                        serializeJson(value, myFile);
                    myFile.print(',');
                }
            }

            myFile.println();

            const bool recordComplete = !myFile.getWriteError() && myFile.sync();

            // Set the last modified date
            if (recordComplete)
                myFile.timestamp(T_WRITE, currentTime.year(), currentTime.month(),
                                 currentTime.day(), currentTime.hour(), currentTime.minute(),
                                 currentTime.second());

            if (!recordComplete)
                myFile.truncate(recordStart);

            // Close the file
            myFile.close();

            if (!recordComplete) {
                printModuleName("Failed while writing CSV data!");
                return false;
            }

            // Inform the user that we have successfully written to the file
            LOGF("Successfully logged data to %s", fileName);
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

    // Card reachability can be lost for one wake while the selected log file
    // must persist for the whole MCU boot session. Keeping these states
    // separate prevents a transient SD initialization failure from advancing
    // to a new numbered CSV on the following wake.
    const bool recoveringExistingLog = logFileSelected && !sdInitialized;

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

    // Choose a numbered log file only once per MCU boot, or after setLogName()
    // explicitly changes the requested base name. A normal wake or recovery
    // from a temporary sd.begin() failure resumes the existing file.
    if (!logFileSelected) {
        // Try to open the root of the file system so we can get the files on the device
        if (!root.open("/", O_RDONLY)) {
            sdInitialized = false;
            printModuleName("ERROR");
            ERROR(F("Failed to open root file system on SD Card!"));
            printModuleName("After ERROR");
            return false;
        }
        if (!updateCurrentFileName()) {
            sdInitialized = false;
            root.close();
            return false;
        }
        logFileSelected = true;
    }

    sdInitialized = true;

    if (recoveringExistingLog) {
        printModuleName("SD card recovered; resuming data log:");
        printModuleName(fileName);
    }

    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::updateCurrentFileName() {
    // uint16_t indexDir = 0; // Reserved for directory-index tracking.
    char f_name[LOG_FILENAME_SIZE];
    char *strLocation;

    // What number we need to append to the file name
    file_count = 0;

    // While there is a next file to open, open it
    while (scanningFile.openNext(&root)) {
        scanningFile.getName(f_name, sizeof(f_name));

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
        buildNumberedName(fileName, sizeof(fileName), overrideFileName, getCurrentFileNumber(),
                          ".csv");
        buildNumberedName(batchFileName, sizeof(batchFileName), overrideFileName,
                          getCurrentFileNumber(), "-Batch.txt");

    } else {
        // Set all the fileNames
        buildNumberedName(fileName, sizeof(fileName), device_name, getCurrentFileNumber(), ".csv");
        buildNumberedName(batchFileName, sizeof(batchFileName), device_name,
                          getCurrentFileNumber(), "-Batch.txt");
    }

    // Close the root file after we have decided what to name the next file
    root.close();

    printModuleName("Data will be logged to:");
    printModuleName(fileName);

    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
const char *SDManager::getBatchFilename() {
    return batchFileName;
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
    while (index < fileSize) {
        const int value = myFile.read();
        if (value < 0)
            break;
        fileContents[index++] = static_cast<char>(value);
    }
    fileContents[index] = '\0';
    myFile.close();

    if (index != fileSize) {
        free(fileContents);
        printModuleName("Failed to read the complete file!");
        return nullptr;
    }
    return fileContents;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void SDManager::logBatch() {
    // Never discard an unsent batch here. The publisher clears it explicitly, and only after
    // every record succeeds. If a network outage lasts for several samples, the file grows on SD
    // instead of consuming SRAM or silently dropping the older records.
    myFile = sd.open(batchFileName, O_WRITE | O_CREAT | O_APPEND);

    // Check if the file has been opened properly and write the JSON packet to one line.
    if (myFile) {
        const uint32_t recordStart = myFile.fileSize();
        const size_t expectedJsonBytes = measureJson(manInst->getDocument());
        myFile.clearWriteError();
        const size_t jsonBytes = serializeJson(manInst->getDocument(), myFile);
        const size_t newlineBytes = myFile.println();

        // A partial record is worse than no record: it cannot be published and would make the
        // following line ambiguous. Roll it back while the file is still open.
        const bool complete = expectedJsonBytes > 0 && jsonBytes == expectedJsonBytes &&
                              newlineBytes == 2 && !myFile.getWriteError() && myFile.sync();
        const bool rolledBack = complete || myFile.truncate(recordStart);
        myFile.close();

        if (complete)
            current_batch++;
        else if (rolledBack)
            printModuleName("Failed while writing batch data!");
        else
            printModuleName("Failed to roll back a partial batch record!");

    } else {
        printModuleName("Failed to open file!");
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::clearBatch() {
    if (!sdInitialized || batch_size <= 0 || batchFileName[0] == '\0') {
        printModuleName("Cannot clear batch because SD batch logging is unavailable!");
        return false;
    }

    myFile = sd.open(batchFileName, O_WRITE | O_CREAT | O_TRUNC);
    if (!myFile) {
        printModuleName("Failed to clear the published batch file!");
        return false;
    }

    myFile.close();
    current_batch = 0;
    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
