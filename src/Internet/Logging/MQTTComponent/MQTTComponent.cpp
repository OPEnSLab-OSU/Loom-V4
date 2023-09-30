#include "MQTTComponent.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
void MQTTComponent::initialize(){
    if(strlen(address) <= 0 || port == 0){
        moduleInitialized = false;
        ERROR("Broker address not specified, module will be uninitialized.");
    }else{
        power_up(); 
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool MQTTComponent::connectToBroker() {
    FUNCTION_START;
    char output[OUTPUT_SIZE];
    if(moduleInitialized){

        // Check if we forgot to supply an address or a port number
        if(strlen(address) <= 0  || port == 0){
            ERROR("Broker address or port not set!");
            FUNCTION_END;
            return false;
        }

        // If we are logging in using credentials then supply them
        if(strlen(username) > 0)
            mqttClient.setUsernamePassword(username, password);
        
        int retryAttempts = 0;

        // Try to connect multiple times as some may be dropped
        while(!mqttClient.connected())
        {
            // If our retry limit has been reached we dont want to try to send data cause it wont work
            if(retryAttempts >= maxRetries){
                ERROR(F("MQTT Retry limit exceeded!"));
                TIMER_ENABLE;
                FUNCTION_END;
                return false;
            }

            snprintf_P(output, OUTPUT_SIZE, PSTR("Attempting to connect to broker: %s:%i"), address, port);
            LOG(output);

            // Attempt to Connect to the MQTT client 
            if(!mqttClient.connect(address, port)){
                snprintf_P(output, OUTPUT_SIZE, PSTR("Failed to connect to broker: %s"), getMQTTError());
                ERROR(output);
                delay(5000);
            }

            retryAttempts++;
        }

        LOG(F("Successfully connected to broker!"));

        // Tell the broker we are still here
        mqttClient.poll();
    }
    else{
        ERROR("Module not initialzed!");
        FUNCTION_END;
        return false;
    }

    FUNCTION_END;
    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool MQTTComponent::publishMessage(const char* topic, const char* message, bool retain, int qos){
    FUNCTION_START;

    // Make sure the module is initialized
    if(moduleInitialized){
        if(mqttClient.connected()){
            // Start a message write the data and close the message, publish all messages with retain
            if(mqttClient.beginMessage(topic, retain, qos) != 1){
                ERROR(F("Failed to begin message!"));
            }

            // Print the message to the topic
            mqttClient.println(message);

            // Check to see if we are actually closing messages properly
            if(mqttClient.endMessage() != 1){
                ERROR(F("Failed to close message!"));
                FUNCTION_END;
                return false;
            }
            else{
                LOG(F("Data has been successfully sent!"));
                FUNCTION_END;
                return true;
            }   
        }
        else{
            ERROR("MQTT Client not connected to broker ");
        }
    }
    else{
        ERROR("Module not initialized!");
    }
    FUNCTION_END;
    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool MQTTComponent::getCurrentRetained(const char* topic, char message[MAX_JSON_SIZE]){
    FUNCTION_START;
    char output[OUTPUT_SIZE];

    if(mqttClient.connected()){
        /* Clear the incoming buffer */
        memset(message, '\0', MAX_JSON_SIZE);

        // Subscribe to the given topic we want to read from
        if(!mqttClient.subscribe(topic)){
            snprintf(output, OUTPUT_SIZE, "Failed to subscribe to topic: %s.", topic);
            ERROR(output);
        }

        /* Attempt to parse a message on the current topic */
        int messageSize = mqttClient.parseMessage();
        if(messageSize){
            /* Copy the received message into the message string */
            strncpy(message, mqttClient.messageTopic().c_str(), MAX_JSON_SIZE);

            // Unsubscribe from the topic 
            mqttClient.unsubscribe(topic);

            FUNCTION_END;
            return true;
        }
        else{
            WARNING("Message of size 0 received.");
            FUNCTION_END;
            return false;
            
        }
    }else{
        ERROR("Not connected to MQTT broker.");
        FUNCTION_END;
        return false;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool MQTTComponent::deleteRetained(const char* topic) { publishMessage(topic, "", true); }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
const char* MQTTComponent::getMQTTError(){
    // Convert error codes to actual descriptions
    FUNCTION_START;
    switch(mqttClient.connectError()){
        case -2:
            FUNCTION_END;
            return "CONNECTION_REFUSED";
        case -1:
            FUNCTION_END;
            return "CONNECTION_TIMEOUT";
        case 1:
            FUNCTION_END;
            return "UNACCEPTABLE_PROTOCOL_VERSION";
        case 2:
            FUNCTION_END;
            return "IDENTIFIER_REJECTED";
        case 3:
            FUNCTION_END;
            return "SERVER_UNAVAILABLE";
        case 4:
            FUNCTION_END;
            return "BAD_USER_NAME_OR_PASSWORD";
        case 5:
            FUNCTION_END;
            return "NOT_AUTHORIZED";
        default:
            FUNCTION_END;
            return "UNKNOWN_ERROR";
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
