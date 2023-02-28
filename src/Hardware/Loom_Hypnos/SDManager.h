#pragma once

#include <SPI.h>
#include <SdFat.h>
#include <OPEnS_RTC.h>

#include "../../Module.h"
#include "../../Loom_Manager.h"

/**
 * Class used to manage interaction with the SD card read/writer on the Hypnos board
 * 
 * @author Will Richards
 */ 
class SDManager : public Module{
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
        SDManager(Manager* man, int sd_chip_select);

        /**
         * Initialize the SD card
         */ 
        bool begin();

        /**
         * Log the current sensor data to the SD card
         * @param currentTime The current time provided by the RTC this allows us to set accurate modified/created times for files
         */ 
        bool log(DateTime currentTime);

        /**
         * Read the contents of a given file on the SD card and return them as a string
         * @param fileName Name of the file to read from
         */ 
        char* readFile(const char* fileName);


        /**
         * Write a single line to a file
         * @param filename File to write to
         * @param content String to write to the line
        */
        bool writeLineToFile(const char* filename, const char* content); 

        /**
         * Get the default SD card file name
         */ 
        const char* getDefaultFilename(){ return this->fileName; };

        /**
         * Get the current batch file name
         */ 
        const char* getBatchFilename(){
            char* fileName = malloc(260);
            sprintf(fileName, "%s-Batch.txt", fileNameNoExtension);
            return batchFileName;
        };

        /**
         * Has the SD card been initialized previously
         */ 
        bool hasSDInitialized() { return sdInitialized; };

        /**
         * Checks if a file exists
         * @param fileName The name of the file to check
         */ 
        bool fileExists(const char* fileName) { return sd.exists(fileName); };

        /**
         * Sets the batch size and thus enables batch loggin
         */ 
        void setBatchSize(int size) { batch_size = size; };

        /**
         * Get the current batch we are ons
         */ 
        int getCurrentBatch() { return current_batch; };

        /**
         * Log to a different name other than one matching the device name
         */ 
        void setLogName(const char* name) { 
            sprintf(fileName, "%s_%i.csv", name, getCurrentFileNumber()); 
            sprintf(fileNameNoExtension, "%s_%i", name, getCurrentFileNumber()); 
            sprintf(batchFileName, "%s-Batch.txt", fileNameNoExtension);
        };

        /* Get whatever number we are currently appending to the SD fileNames*/
        int getCurrentFileNumber() {return file_count;};

        
    private:

        Manager* manInst;                                       // Reference to the manager

        File myFile;                                            // File object used to handle reading and writing
        File scanningFile;                                      // Used specifically to search through the directory
        File root;                                              // Open the root directory as a file

        SdFat sd;                                               // SD Card Object

        int chip_select;                                        // Chip select pin for the SD card
        char device_name[100];                                  // Device name of the whole thing used as the starting point of the SD file name

        char batchFileName[260];                                // File name to log batches to
        char fileName[260];                                     // Current file name that data is being logged to
        char fileNameNoExtension[260];                          // Current file name that data is being logged to without the file extension

        int batch_size = -1;                                    // How many packets to log per batch
        int current_batch = 0;                                  // Current count of the batch
        int file_count = 0;                                     // What file number are we logging to

        bool sdInitialized = false;                             // If the SD card actually initialized
        char* headers[2];                                       // Contains the main and sub headers that are added to the top of the CSV files


        void logBatch();                                        // Log data in batch format
        
        void createHeaders();                                   // Create the headers for the CSV file based off what info we are storing
        bool updateCurrentFileName();                           // Update the current file name to log to based on files already existing on the SD card
};