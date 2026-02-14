#pragma once

#include <ArduinoMqttClient.h>
#include <tuple>

#include "Loom_Manager.h"

#include "../../../Hardware/Loom_BatchSD/Loom_BatchSD.h"
#include "../MQTTComponent/MQTTComponent.h"

/* Define function signatures for functions of type "float name()" and "float name(int parameter)"*/
using FloatReturnFuncDefs = float (*)();
using FloatReturnFuncDefsWithParam = float (*)(int);

/**
 * Platform for logging data to MQTT for logging to a remote database
 *
 * @author Will Richards
 */
class Loom_ThingSpeak : public MQTTComponent {
  protected:
    /* These aren't used with the MQTT */
    void measure() override {};

    void initialize() override {};
    void power_up() override {};
    void power_down() override {};
    void package() override {};

  public:
    /**
     * Construct a new MQTT interface
     * @param man Reference to the manager
     * @param internet_client Reference to whatever connectivity platform is being used
     * @param broker_address Domain where the broker is being hosted
     * @param broker_port Port that the broker is listening on
     * @param database_name Name of the database that will be used by MongoDB
     *
     * Not Required:
     * @param broker_user User name to log into the broker
     * @param broker_pass Password to log into the broker
     */
    Loom_ThingSpeak(Manager &man, NetworkComponent &internet_client, int channelID,
                    const char *clientID, const char *broker_user, const char *broker_pass);

    /**
     * Construct a new MQTT interface, expects credentials to be loaded from JSON
     * @param man Reference to the manager
     */
    Loom_ThingSpeak(Manager &man, NetworkComponent &internet_client);

    /**
     * Publish the current JSON data over MQTT
     */
    bool publish() override;

    /**
     * Publish the current JSON data as a batch
     */
    bool publish(Loom_BatchSD &batchSD);

    /**
     * Load the MQTT credentials from a JSON string, used to pull credentials from a file
     * @param jsonString JSON formatted string containing the login credentials, this is freed at
     * the end
     */
    void loadConfigFromJSON(char *json) override;

    /**
     * Add a new function to the list of functions that we are going to pass into ThingSpeak
     *
     * @param fieldName The corresponding field number
     * @param function The function signature as follows: float someFunction()
     */
    void addFunction(int fieldNumber, FloatReturnFuncDefs function);

    /**
     * Add a new function to the list of functions that we are going to pass into ThingSpeak
     *
     * @param fieldName The corresponding field number
     * @param function The function signature as follows: float someFunction(int)
     * @param parameter The parameter to supply to the function when we call it
     */
    void addFunction(int fieldNumber, FloatReturnFuncDefsWithParam function, int parameter);

  private:
    Manager *manInst; // Instance of the manager
    int channelID;    // The channelID we are publishing to

    /**
     * Format the packet to be sent to ThingSpeak
     * Example: field1=343&field2=421.4&created_at=2023-02-21T11:46:51Z&status=MQTTPUBLISH
     *
     * @param topic The topic buffer we should format to publish data to our given feed
     * @param message The message buffer we should fill with our formatted packet
     */
    void formatMessage(char topic[MAX_TOPIC_LENGTH], char message[MAX_JSON_SIZE]);

    /* List of mappings from field names to functions */
    std::vector<std::pair<int, FloatReturnFuncDefs>>
        functionsNoParam; // List of added functions that take no parameters
    std::vector<std::tuple<int, FloatReturnFuncDefsWithParam, int>>
        functionsParam; // List of added functions that take a parameter
};