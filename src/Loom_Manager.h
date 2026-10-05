#pragma once

#include "Loom_WarningGuards.h"

LOOM_EXTERNAL_INCLUDE_BEGIN
#include <ArduinoJson.h>
LOOM_EXTERNAL_INCLUDE_END
#include <vector>

#include "Module.h"
#include "Utilities/Loom_JsonUtils.h"

#define WAIT_TIME_MS 20000 // Time to wait for the serial interface to start
#define BAUD_RATE 115200   // Serial interface baud rate

/**
 * Runs the registered modules and holds one shared packet of their latest readings.
 * A sketch typically calls measure(), package(), then its storage/network module to save/send.
 *
 * @author Will Richards
 */
class Manager {
  public:
    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Setup: identify this device and register its modules.
    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Repository examples use at most 19 characters. A 63-character ceiling leaves substantial
    // application headroom without reserving 100 bytes in every Manager and SDManager instance.
    static constexpr size_t DEVICE_NAME_SIZE = 64;

    /**
     * Constructs a new Manager
     * @param devName Device name provided for logging purposes
     * @param instanceNum Instance number for logging purposes
     */
    Manager(const char *devName, uint32_t instanceNum);

    /**
     * Add a module to the measurement cycle. Manager borrows it; it does not delete it.
     * The module must stay alive for every Manager call that uses it (usually the whole sketch).
     * @param module Pointer to the module; null pointers are rejected
     */
    void registerModule(Module *module);

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Packet access: these references and views use the shared JSON buffer.
    ////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * Borrow the current packet without copying it. package() replaces its contents.
     * @return reference to Manager's JSON document
     */
    DynamicJsonDocument &getDocument();

    /** Check at the output boundary, including data added by a sketch after package(). */
    bool isPacketValid() const { return loomJsonIsComplete(doc); }

    /**
     * Add a named value to a module's part of the current packet, for example a temperature.
     * @param moduleName Module name to store the data under
     * @param dataName Key name of the data we are inserting
     * @param data The data itself; constant C string pointers must stay valid until save/send
     */
    template <typename T> void addData(const char *moduleName, const char *dataName, T data) {
        JsonObject moduleData = get_data_object(moduleName);
        moduleData[dataName] = data;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Main cycle: initialize once, then measure -> package -> save/send in the sketch.
    ////////////////////////////////////////////////////////////////////////////////////////////////
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
    bool isInitialized() const { return hasInitialized; }

    // Optional diagnostics belong to the sketch, so ordinary Manager needs no flash/SD driver.
    // BeforeInitialize lets it print the last saved checkpoint BEFORE module states change.
    // Other events report a completed call, not proof that every driver succeeded.
    enum class HealthEvent {
        BeforeInitialize,
        Initialized,
        Measured,
        Packaged,
        PoweredUp,
        PoweredDown,
        Idle,
        Resumed
    };
    using HealthObserver = void (*)(Manager &, HealthEvent, void *);
    /** Borrow callback/context until replaced or cleared with nullptr. Main-loop calls only.
     * Keep it bounded: no Manager operations, watchdog changes, blocking modem work or ISR use.
     * A storage observer must rate-limit writes and verify supply separately. */
    void setHealthObserver(HealthObserver observer, void *context = nullptr) {
        healthObserver = observer;
        healthContext = context;
    }

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
    void power_up(int wakeWatchdogMs);

    /**
     *  Calls the power_down function on each module to safely enter sleep
     */
    void power_down();

    /** Enter/leave driver standby with the shared rails still powered. Calls are idempotent.
     * measure() is paused while idle; package()/storage remain available for the last sample.
     * Module defaults do nothing. Deep sleep still uses power_down()/power_up(). */
    void idle();
    void resume();
    bool isIdle() const { return modulesIdle; }
    bool canRemovePower() const;

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

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Device information: names and identifiers used in packets and storage.
    ////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * Gets the current device name set by the user
     * @return current device name
     */
    const char *get_device_name() { return deviceName; };

    /**
     * Set the device name at runtime
     * @param name New name of the device
     */
    void set_device_name(const char *name) {
        strncpy(deviceName, name ? name : "", sizeof(deviceName) - 1);
        deviceName[sizeof(deviceName) - 1] = '\0';
    };

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
    const char *get_serial_num() {
        // Hypnos can initialize SD before Manager::initialize(). Read the ID on first use so
        // its early boot journal never sees an uninitialized buffer. No heap allocation.
        if (serial_num[0] == '\0') {
            read_serial_num();
        }
        return serial_num;
    };

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
     * Find or create this module's data in the current packet. The returned object is a view
     * into Manager's document, not a separate buffer; use it before the next package() call.
     * @param moduleName Name of the module we are trying to store data for
     */
    JsonObject get_data_object(const char *moduleName);

    /**
     * Get the current packet number that will be packaged by the manager
     */
    int get_packet_number() { return packetNumber; };

  private:
    /* Device Information */
    char deviceName[DEVICE_NAME_SIZE]; // Name of the device
    uint32_t instanceNumber;           // Instance number of the device
    uint32_t packetNumber = 1;         // Tracks the current packet number
    char serial_num[33] = {};

    void read_serial_num(); // Read the serial number out of the feather's registers

    /* Module Data */
    DynamicJsonDocument doc;       // One heap pool, allocated in the constructor and reused.
    std::vector<Module *> modules; // Borrowed pointers, in registration order.

    /* Validation */
    bool hasInitialized = false;
    bool modulesIdle = false;
    bool usingHypnos = false;
    bool hypnosEnabled = false; // Sensor power must be on before initialization.
    HealthObserver healthObserver = nullptr;
    void *healthContext = nullptr; // Borrowed; Manager owns no diagnostic storage or allocations.
    void notifyHealth(HealthEvent event);
};
