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
    if(moduleInitialized && internetClient.moduleInitialized){

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
        ERROR("Module or NetworkComponent not initialzed!");
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
    if(moduleInitialized && internetClient.moduleInitialized){
        
        // Ensure we are connected before starting
        if(!mqttClient.connected()){
             if(!connectToBroker()){
                 ERROR("MQTT Client unable to connect to broker.");
                 FUNCTION_END;
                 return false;
             }
        }

        // Tell the broker we are still here
        mqttClient.poll();

        // --------------------------------------------------------
        // ATTEMPT 1: Normal Publish
        // --------------------------------------------------------
        
        // Start the message
        if(mqttClient.beginMessage(topic, retain, qos) != 1){
            ERROR(F("Failed to begin message!"));
            // If we can't begin, we likely can't end, but we let flow continue or fail here.
        }

        mqttClient.println(message);

        // Check if the message closed properly
        if(mqttClient.endMessage() == 1){
            LOG(F("Data has been successfully sent!"));
            FUNCTION_END;
            return true;
        } 
        
        // --------------------------------------------------------
        // RECOVERY LOGIC: Handle Failed Close Message
        // --------------------------------------------------------
        else {
            ERROR(F("Failed to close message! Attempting recovery..."));
            
            int retryCount = 0;
            const int maxRecoveries = 3;

            while(retryCount < maxRecoveries){
                retryCount++;
                
                // Small delay to let network settle before retry
                delay(1000); 

                // Log the attempt
                char retryLog[50];
                snprintf(retryLog, 50, "Recovery Attempt %d/%d...", retryCount, maxRecoveries);
                LOG(retryLog);

                // 1. Try to reconnect
                // Note: connectToBroker() has its own internal loop, but we call it once here
                // to ensure the socket is refreshed.
                if(connectToBroker()){
                    
                    // 2. Re-attempt the publish sequence
                    mqttClient.poll();
                    
                    if(mqttClient.beginMessage(topic, retain, qos) == 1){
                        mqttClient.println(message);
                        
                        // 3. Check if it worked this time
                        if(mqttClient.endMessage() == 1){
                            LOG(F("Recovery successful! Data sent."));
                            FUNCTION_END;
                            return true;
                        } else {
                            WARNING(F("Recovery send failed. Retrying..."));
                        }
                    } else {
                        WARNING(F("Recovery beginMessage failed."));
                    }
                } else {
                     WARNING(F("Recovery reconnection failed."));
                }
            }

            // If we exit the loop, all 3 retries failed
            ERROR(F("All recovery attempts failed. Ending transmission."));
            FUNCTION_END;
            return false;
        }
    }
    else{
        ERROR("Module or NetworkComponent not initialized!");
    }
    FUNCTION_END;
    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool MQTTComponent::getCurrentRetained(const char* topic, char message[MAX_JSON_SIZE]){
    FUNCTION_START;
    char output[OUTPUT_SIZE];
    LOG(topic);
    if(mqttClient.connected()){
        /* Clear the incoming buffer */
        memset(message, '\0', MAX_JSON_SIZE);

        // Subscribe to the given topic we want to read from
        if(!mqttClient.subscribe(topic, 2)){
            snprintf(output, OUTPUT_SIZE, "Failed to subscribe to topic: %s.", topic);
            ERROR(output);
        }
        else{
            LOG("Successfully subscribed to topic!");
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
            mqttClient.unsubscribe(topic);
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
