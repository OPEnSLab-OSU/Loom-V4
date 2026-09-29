#pragma once

#include <vector>

#include "Loom_Manager.h"

class Loom_BatchSD;
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
     * @param channelID ThingSpeak channel receiving the fields
     * @param clientID Client ID supplied by ThingSpeak
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
     * Batch replay is unavailable because ThingSpeak fields are callback-generated rather than
     * stored in the Loom batch file. This overload is retained for source compatibility and
     * returns false instead of publishing incorrect values.
     */
    bool publish(Loom_BatchSD &batchSD);

    /**
     * Load the MQTT credentials from a JSON string, used to pull credentials from a file
     * @param json JSON formatted string containing the login credentials, this is freed at
     * the end
     */
    void loadConfigFromJSON(char *json) override;

    /**
     * Add a new function to the list of functions that we are going to pass into ThingSpeak
     *
     * @param fieldNumber The corresponding field number
     * @param readValue Function called during publishing, with signature float someFunction()
     */
    void addFunction(int fieldNumber, FloatReturnFuncDefs readValue);

    /**
     * Add a new function to the list of functions that we are going to pass into ThingSpeak
     *
     * @param fieldNumber The corresponding field number
     * @param readValue Function called during publishing, with signature float someFunction(int)
     * @param parameter The parameter to supply to the function when we call it
     */
    void addFunction(int fieldNumber, FloatReturnFuncDefsWithParam readValue, int parameter);

  private:
    static constexpr size_t MESSAGE_SIZE = 1024;
    static constexpr size_t MAX_FIELDS = 8;

    struct FieldFunction {
        int fieldNumber;
        FloatReturnFuncDefs readValue;
    };
    struct ParameterizedFieldFunction {
        int fieldNumber;
        FloatReturnFuncDefsWithParam readValue;
        int parameter;
    };

    Manager *manager;  // Instance of the manager
    int channelID = 0; // The channelID we are publishing to

    /**
     * Format the packet to be sent to ThingSpeak
     * Example: field1=343&field2=421.4&created_at=2023-02-21T11:46:51Z&status=MQTTPUBLISH
     *
     * @param topic The topic buffer we should format to publish data to our given feed
     * @param message The message buffer we should fill with our formatted packet
     */
    bool formatMessage(char topic[MAX_TOPIC_LENGTH], char message[MESSAGE_SIZE]);

    // Keep the two lists separate: payloads historically emit no-argument fields first.
    std::vector<FieldFunction> fieldsWithoutParameters;
    std::vector<ParameterizedFieldFunction> fieldsWithParameters;
};
