#pragma once

#include <ArduinoJson.h>
#include <unordered_map>
#include <vector>

#include "Module.h"

#define WAIT_TIME_MS 20000 // Time to wait for the serial interface to start
#define BAUD_RATE 115200   // Serial interface baud rate

/**
 * Unifies all the various sensors to allow for collection in unison
 * This class manages the JSON document store of all sensor information
 *
 * @author Will Richards
 */
class Manager {
  public:
    /**
     * Constructs a new Manager
     * @param devName Device name to provided for logging purposes
     * @param instanceNum Instance number for logging purposes
     */
    Manager(const char *devName, uint32_t instanceNum);

    /**
     * Registers a new sub-module to be controlled by the manager (Used on sensors so measure and
     * package calls can all be called at once)
     * @param module Pointer to a class the inherits from Module that we want to add
     */
    void registerModule(Module *module);

    /**
     * Get a reference to the JSON document that sensor data is stored in
     * @return reference to the main JSON document
     */
    DynamicJsonDocument &getDocument(); // Returns a reference to the main JSON document storing

    /**
     * Add a random piece of data to the overall JSON package in the given module name with a name
     * for the data
     * @param moduleName Module name to store the data under
     * @param dataName Key name of the data we are inserting
     * @param data The data itself
     */
    template <typename T> void addData(const char *moduleName, const char *dataName, T data) {
        JsonObject json = get_data_object(moduleName);
        json[dataName] = data;
    };

    /**
     * Start the Serial interface with some parameters, should we wait up to 20 seconds for the
     * serial interface to open before continuing
     * @param waitForSerial Whether or not we should wait 20 seconds for the user to open the serial
     * monitor before continuing
     */
    void beginSerial(bool waitForSerial = true);

    /**
     * Calls the initialization function on all added modules
     */
    void initialize();

    /**
     *  Calls the measure function to pull data from the sensors on all added modules
     */
    void measure();

    /**
     *  Calls the package function to store all data from those sensors into a nice JSON package
     */
    void package();

    /**
     *  Calls the power_up function on each module to re-init after sleep
     */
    void power_up();

    /**
     *  Calls the power_down function on each module to safely enter sleep
     */
    void power_down();

    /**
     * Prints out the current JSON Document to the Serial bus
     */
    void display_data();

    /**
     * Pause execution for a specified length of time
     * @param ms Time to wait for in milliseconds
     */
    void pause(const uint32_t ms) const;

    /**
     * Get a serialized version of the JSON packet as a string
     * @param array Array to store the string in
     */
    void getJSONString(char array[MAX_JSON_SIZE]);

    /**
     * Gets the current device name set by the user
     * @return current device name
     */
    const char *get_device_name() { return deviceName; };

    /**
     * Set the device name at runtime
     * @param name New name of the device
     */
    void set_device_name(const char *name) { strcpy(deviceName, name); };

    /**
     * Gets the current device instance number
     * @return current device instance number
     */
    int get_instance_num() { return instanceNumber; };

    /**
     * Set the instance number at runtime
     * @param num New instance number of the device
     */
    void set_instance_num(int num) { instanceNumber = num; };

    /**
     * Get the unique serial number of the Feather m0
     * @return Unique serial number
     */
    const char *get_serial_num() { return serial_num; };

    /**
     * Called by the Hypnos on construction to tell the manager it is in use
     */
    void useHypnos() { usingHypnos = true; };

    /**
     * Set the current state of the hypnos enable
     * @param state New state of the hypnos board
     */
    void setEnableState(bool state) { hypnosEnabled = state; };

    /**
     * Get the JSON object to store the module data in
     * @param moduleName Name of the module we are trying to store data for
     */
    JsonObject get_data_object(const char *moduleName);

    /**
     * Get the current packet number that will be packaged by the manager
     */
    int get_packet_number() { return packetNumber; };

  private:
    /* Device Information */
    char deviceName[100];      // Name of the device
    uint32_t instanceNumber;   // Instance number of the device
    uint32_t packetNumber = 1; // Tracks the current packet number
    char serial_num[33];

    void read_serial_num(); // Read the serial number out of the feather's registers

    /* Module Data */
    DynamicJsonDocument doc; // JSON document that will store all sensor information
    JsonArray contentsArray; // Stores the contents of the modules
    std::vector<std::pair<const char *, Module *>>
        modules; // List of modules that have been added to the stack

    /* Validation */
    bool hasInitialized = false; // Whether or not the initialize function has been called, if not
                                 // it could be the source of hanging so we want to know
    bool usingHypnos = false;    // If the setup is using a hypnos
    bool hypnosEnabled = false; // If the power rails on the hypnos are enabled this means we should
                                // be able to initialize
};