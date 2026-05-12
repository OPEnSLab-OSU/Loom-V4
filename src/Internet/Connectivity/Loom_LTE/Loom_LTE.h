#pragma once

// GSM Model Number
// #define TINY_GSM_MODEM_UBLOX
#define TINY_GSM_MODEM_SARAR4

#include "../NetworkComponent.h"
#include "Loom_Manager.h"
#include <TinyGsmClient.h>
#include <functional>

#include "../../../Hardware/Loom_BatchSD/Loom_BatchSD.h"

// Specify what serial interface we want to use
#define SerialAT Serial1

enum LTE_VERSION { SPARKFUN, OPENS };

// SARAR5 has GNSS reciever, passing its value "1" to TinyGSM getGPS function uses GPS , while 
// passing SARAR4 (2), will use cellLocate as it does not have GNSS reciever. 
enum GPS_TYPE{
    SARAR5 = 1,
    SARAR4 = 2
    
};

/**
 * Loomified Control for a 4G LTE Board
 *
 * @author Will Richards
 */
class Loom_LTE : public NetworkComponent {
  protected:
    /* These aren't used with the Wifi manager */
    void measure() override {};

    bool isConnected() override { return modem.isGprsConnected(); };

  public:
    /**
     * Construct a new LTE instance
     * @param man Reference to the manager
     * @param apn Name of the LTE network
     * @param user Username to use
     * @param pass Password to use
     * @param powerPin Pin used to power the device
     */
    Loom_LTE(Manager &man, const char *apn, const char *user, const char *pass,
             const int powerPin = A5, LTE_VERSION version = SPARKFUN);

    /**
     * Construct a new LTE instance assuming credentials will be pulled from an SD card
     * @param man Reference to the manager
     */
    Loom_LTE(Manager &man);

    // Initialize the device and connect to the network
    void initialize() override;

    // Reconnect to the network
    void power_up() override;

    // Disconnect from the network
    void power_down() override;

    // Signal Strength
    void package() override;

    // Get the current time from the network
    bool getNetworkTime(int *year, int *month, int *day, int *hour, int *minute, int *second,
                        float *tz) override;

    /**
     * Load the config to connect to the LTE network from a JSON string
     * @param json Json file read, this is freed before returning
     */
    void loadConfigFromJSON(char *json);

    /**
     * @brief uses TinyGSM AT commands to retrieve GPS coordinates. 
     *  Stores Latitude and Longitude data in class variables lon and lat
     * Executes during initalization, and is included in the package function
     * @param modem sensor type, meaning GNSS reciever or cellLocate. Cell locate (SARAR4) uses less power and is 
     * less accurate. Technically compatible with SARAR4 but needs to be ironed out. 
     *  Can switch to GNSS reciever (SARAR5) for higher precision, but higher power usage. Only compatible with SARA-R510M8S and
     * the newer SARA-R520M10. 
     */
    void getCoordinates (GPS_TYPE modem); 

    /**
     * Turn on batch upload for the lte which means it will only initialize the module when we need
     * to upload
     * @param batch BatchSD module
     */
    void setBatchSD(Loom_BatchSD &batch) { batch_sd = &batch; };

    /**
     * Connect to the cellular network
     */
    bool connect();

    /**
     * Disconnect from the cellular network
     */
    void disconnect();

    /**
     * Attempt to connect to something remote to see if we actually have an internet connection
     */
    bool verifyConnection();

    /**
     * Get the client to supply to publish platforms that need to communicate using this internet
     * framework
     */
    Client *getClient() override;

    /* Restart the modem */
    void restartModem() {
        // TIMER_RESET;
        modem.poweroff();
        delay(3000);
        modem.restart();
        delay(1000);
        // TIMER_RESET;
    };

    /**
     * Convert an IP address to a string
     */
    void ipToString(IPAddress ip, char array[16]) {
        snprintf(array, 16, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    };

  private:
    void powerBoardOn();
    void powerBoardOff();

    LTE_VERSION lteBoardVersion = SPARKFUN;

    Manager *manInst; // Instance of the manager

    char APN[100];      // LTE Network Name
    char gprsUser[100]; // GPRS Username
    char gprsPass[100]; // GPRS Password

    int powerPin = A5; // Analog pin to power the LTE board

    TinyGsm modem;        // LTE Modem
    TinyGsmClient client; // LTE Client

    bool powerUp = true;
    bool firstInit = true;            // First time it was initialized
    Loom_BatchSD *batch_sd = nullptr; // If we are using batch publish

    bool powered = false; // Device power status

    char locationMethod[20]; // either GPS or CellLocate, indicates expected accuracy. GPS accuracy > CellLocate accuracy

    float lat = 0;                          // GPS latitude

    float lon = 0;                          // GPS longitude



};