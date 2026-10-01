#pragma once

class Manager;

class Loom_BatchSD;
#include "../MQTTComponent/MQTTComponent.h"

/* Define function signatures for functions of type "float name()" and "float name(int parameter)"*/
using FloatReturnFuncDefs = float (*)();
using FloatReturnFuncDefsWithParam = float (*)(int);

/**
 * Send up to eight callback-generated measurements to a ThingSpeak channel.
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
     * @param fieldNumber ThingSpeak field number, from 1 through 8
     * @param readValue Function called during publishing, with signature float someFunction()
     */
    void addFunction(int fieldNumber, FloatReturnFuncDefs readValue);

    /**
     * Add a new function to the list of functions that we are going to pass into ThingSpeak
     *
     * @param fieldNumber ThingSpeak field number, from 1 through 8
     * @param readValue Function called during publishing, with signature float someFunction(int)
     * @param parameter The parameter to supply to the function when we call it
     */
    void addFunction(int fieldNumber, FloatReturnFuncDefsWithParam readValue, int parameter);

  private:
    // "channels/" + a signed 32-bit channel ID + "/publish" + the final NUL fits in 29 bytes.
    // Leave a little room without using the much larger general-purpose MQTT topic buffer.
    static constexpr size_t TOPIC_SIZE = 32;
    static constexpr size_t MAX_FIELDS = 8;

    struct FieldFunction {
        int fieldNumber;
        FloatReturnFuncDefs readValue;
        FloatReturnFuncDefsWithParam readParameterizedValue;
        int parameter;
    };

    Manager *manager;  // Instance of the manager
    int channelID = 0; // The channelID we are publishing to

    // The service accepts eight fields, so registration needs no growing heap allocation.
    // Publish in two passes to retain the historical plain-before-parameterized grouping.
    FieldFunction fields[MAX_FIELDS] = {};
    size_t fieldCount = 0;
};
