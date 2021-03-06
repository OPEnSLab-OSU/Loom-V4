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
        void print_measurements() override {};
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
        String readFile(String fileName);

        /**
         * Has the SD card been initialized previously
         */ 
        bool hasSDInitialized() { return sdInitialized; };

        /**
         * Checks if a file exists
         * @param fileName The name of the file to check
         */ 
        bool fileExists(String fileName) { return sd.exists(fileName.c_str()); };

        /**
         * Count the number of packets in the SD file
         * 
         * @param fileName File to count from
         */
        int countPackets(String fileName);
        
    private:

        Manager* manInst;                                       // Reference to the manager

        File myFile;                                            // File object used to handle reading and writing
        File scanningFile;                                      // Used specifically to search through the directory
        File root;                                              // Open the root directory as a file

        SdFat sd;                                               // SD Card Object

        int chip_select;                                        // Chip select pin for the SD card
        String device_name;                                     // Device name of the whole thing used as the starting point of the SD file name
        String fileName;                                        // Current file name that data is being logged to

        bool sdInitialized = false;                             // If the SD card actually initialized
        String headers[2] = {"", ""};                           // Contains the main and sub headers that are added to the top of the CSV files

        bool writeLineToFile(String filename, String content);  // Write the given string to a file
        void createHeaders();                                   // Create the headers for the CSV file based off what info we are storing
        bool updateCurrentFileName();                           // Update the current file name to log to based on files already existing on the SD card
};