#pragma once

#include <OPEnS_RTC.h>

#if !defined(LOOM_OPENS_RTC_PATCH_LEVEL) || LOOM_OPENS_RTC_PATCH_LEVEL < 1
#error "SDManager requires the hardened OPEnS_RTC dependency from Loom/dependencies."
#endif
#include <SPI.h>
#include <SdFat.h>

#include "../../Loom_Manager.h"
#include "../../Module.h"

/**
 * Class used to manage interaction with the SD card read/writer on the Hypnos board
 *
 * @author Will Richards
 */
class SDManager : public Module {
  protected:
    /* These aren't used with the Hypnos */
    void measure() override {};
    void package() override {};

    void initialize() override {};
    void power_up() override {};
    void power_down() override {};

  public:
    /**
     * SDManager Constructor
     *
     * @param man Reference to the manager
     * @param sd_chip_select Pin to control the SD card on
     */
    SDManager(Manager *man, int sd_chip_select);

    /**
     * Initialize the SD card
     */
    bool begin();

    /**
     * Log the current sensor data to the SD card
     * @param currentTime The current time provided by the RTC this allows us to set accurate
     * modified/created times for files
     */
    bool log(DateTime currentTime);

    /**
     * Read the contents of a given file on the SD card and return them as a string
     *
     * The returned buffer is sized to the file and must be freed by the caller.
     * Files larger than the implementation safety limit return nullptr.
     *
     * @param fileName Name of the file to read from
     */
    char *readFile(const char *fileName);

    /*
     * Returns a reference to the opened file
     */
    File &getFile(const char *fileName) {
        myFile = sd.open(fileName);
        return myFile;
    };

    /**
     * Open an independent read handle.
     *
     * Callers that retain a file while other Loom code may log to SD must use an independent
     * handle. The ordinary logger opens its own debug file and must not replace a retained batch
     * reader through SDManager::myFile.
     */
    File openFile(const char *fileName) { return sd.open(fileName); };

    /**
     * Write a single line to a file
     * @param filename File to write to
     * @param content String to write to the line
     */
    bool writeLineToFile(const char *filename, const char *content);

    /** Enable direct-Serial phase markers around single-line SD writes for beta diagnosis. */
    void setWriteDebug(bool enabled = true) { writeDebug = enabled; };

    /**
     * Get the default SD card file name
     */
    const char *getDefaultFilename() { return this->fileName; };

    /**
     * Get the current batch file name
     */
    const char *getBatchFilename();

    /**
     * Has the SD card been initialized previously
     */
    bool hasSDInitialized() { return sdInitialized; };

    /**
     * Checks if a file exists
     * @param fileName The name of the file to check
     */
    bool fileExists(const char *fileName) { return sd.exists(fileName); };

    /**
     * Sets the batch size and thus enables batch logging
     */
    void setBatchSize(int size) { batch_size = size; };

    /**
     * Get the current batch index
     */
    int getCurrentBatch() { return current_batch; };

    /**
     * Clear the pending batch only after every record has been delivered.
     *
     * Keeping this operation separate from logBatch() makes the batch file transactional: a
     * temporary network failure cannot cause the next sample to truncate unsent records.
     */
    bool clearBatch();

    /**
     * Log to a different name other than one matching the device name
     */
    void setLogName(const char *name) {
        char requestedName[sizeof(overrideFileName)];
        strncpy(requestedName, name ? name : "", sizeof(requestedName) - 1);
        requestedName[sizeof(requestedName) - 1] = '\0';

        // Selecting a genuinely different base name should create a new session
        // file. Repeating the same setting must not rotate the file on wake.
        if (strcmp(overrideFileName, requestedName) != 0) {
            memcpy(overrideFileName, requestedName, sizeof(overrideFileName));
            logFileSelected = false;
        }
    };

    /* Get whatever number we are currently appending to the SD fileNames*/
    int getCurrentFileNumber() { return file_count; };

  private:
    static constexpr size_t LOG_BASENAME_SIZE = Manager::DEVICE_NAME_SIZE;
    static constexpr size_t LOG_FILENAME_SIZE = LOG_BASENAME_SIZE + 20;

    Manager *manInst; // Reference to the manager

    File myFile;       // File object used to handle reading and writing
    File scanningFile; // Used specifically to search through the directory
    File root;         // Open the root directory as a file

    SdFat sd; // SD Card Object

    int chip_select;       // Chip select pin for the SD card
    char device_name[LOG_BASENAME_SIZE]; // Starting point of the SD file name

    // A 63-character base + 10-digit counter + "-Batch.txt" + null needs at most 84 bytes.
    char batchFileName[LOG_FILENAME_SIZE];
    char fileName[LOG_FILENAME_SIZE];
    char overrideFileName[LOG_BASENAME_SIZE];

    int batch_size = -1;   // How many packets to log per batch
    int current_batch = 0; // Current count of the batch
    int file_count = 0;    // What file number are we logging to

    bool sdInitialized = false;   // Whether the card is reachable for the current operation
    bool logFileSelected = false; // Whether this MCU boot session already chose its CSV filename
    bool writeDebug = false;      // Direct-Serial beta trace; never written through Logger

    void logBatch(); // Append one JSON record to the batch file

    bool writeHeaders(); // Create the headers for the CSV file based off what info we are storing
    bool updateCurrentFileName(); // Update the current file name to log to based on files already
                                  // existing on the SD card
};
