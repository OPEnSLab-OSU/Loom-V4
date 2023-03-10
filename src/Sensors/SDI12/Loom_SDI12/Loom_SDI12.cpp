#include "Loom_SDI12.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_SDI12::Loom_SDI12(Manager& man, const int pinNumber): Module("SDI12"), sdiInterface(pinNumber) { 
    manInst = &man;
    manInst->registerModule(this); 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_SDI12::Loom_SDI12(const int pinNumber) : Module("SDI12"), sdiInterface(pinNumber) {}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::initialize(){
    char response[RESPONSE_SIZE];
    LOG(F("Initializing SDI-12 Sensors..."));

    // On init we set the SDI pin to OUTPUT so we can request data
    pinMode(sdiInterface.getDataPin(), OUTPUT);

    // Start the interface and then wait for 100ms to allow things to settle and startup correctly
    sdiInterface.begin();
    delay(100);

    // Create a list of addresses that have a sensor connected to them
    inUseAddresses = scanAddressSpace();

    // Request the sensor data from all connected devices to pull the sensor name
    for(int i = 0; i < inUseAddresses.size(); i++){
        memset(response, '\0', RESPONSE_SIZE);
        requestSensorInfo(response, inUseAddresses[i]);
        addressToType.insert(std::pair<char, const char*>(inUseAddresses[i], response));
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::measure(){
   
    // On measure we also want to reset the mode to output in case the 4G board has messed with it
    pinMode(sdiInterface.getDataPin(), OUTPUT);
    delay(30);

    // Populate the variables that will be used to package data
    for(int i = 0; i < inUseAddresses.size(); i++){
        getData(inUseAddresses[i]);
    }
    
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::package(){
    char output[OUTPUT_SIZE];
    for(int i = 0; i < inUseAddresses.size(); i++){
        
        if(strstr(getSensorInfo(inUseAddresses[i]), "GS3") != NULL){
            snprintf(output, OUTPUT_SIZE, "GS3_%i", i);
            JsonObject json = manInst->get_data_object(output);
            json["Temperature"] = sensorData[0];
            json["Dielectric_Permittivity"] = sensorData[1];
            json["Conductivity"] = sensorData[2];
        }
        else if(strstr(getSensorInfo(inUseAddresses[i]), "TER") != NULL){
            snprintf(output, OUTPUT_SIZE, "Teros_%i", i);
            JsonObject json = manInst->get_data_object(output);
            json["Temperature"] = sensorData[0];
            json["Volumetric_Water_Content"] = sensorData[1];
            if(strstr(getSensorInfo(inUseAddresses[i]), "TER12") != NULL)
                json["Conductivity"] = sensorData[2];
        }
    }
    
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::power_up(){
   pinMode(sdiInterface.getDataPin(), OUTPUT);
   sdiInterface.begin();
   delay(100);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::power_down(){
    sdiInterface.end();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
std::vector<char> Loom_SDI12::scanAddressSpace(){
    std::vector<char> activeSensors;
    char output[OUTPUT_SIZE];

    // Print the module name followed by the message saying please wait
    LOG(F("Scanning SDI-12 Address Space this make take a little while..."));

    // Scan over the characters that can be used as addresses for refrencing the sensors
	for (char i = '0'; i <= '9'; i++){
		if(checkActive(i)){
			activeSensors.push_back(i);
		}
	}

	for(char i = 'a'; i <= 'z'; i++){
		if(checkActive(i)){
			activeSensors.push_back(i);
		}
	}

	for (char i = 'A'; i <= 'Z'; i++){
		if(checkActive(i)){
			activeSensors.push_back(i);
		}
	}

    // Check if we actually found any connected devices
    if(activeSensors.size() > 0){
        // Print the module name followed by the message saying please wait
        LOG(F("== We found the following active Addresses =="));
        for(int i = 0; i < activeSensors.size(); i++){
            snprintf(output, OUTPUT_SIZE, "    Address: %c", activeSensors[i]);
            LOG(output); 
        }
    }
    else{
        LOG(F("== No SDI-12 Devices Were Discovered == "));
    }

    return activeSensors;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_SDI12::checkActive(char addr){
    // Attempt to contact the sensor 3 times
    char response[RESPONSE_SIZE];
	for (int i =0; i < 3; i++){
        memset(response, '\0', RESPONSE_SIZE);
        sendCommand(response, addr, "!");
		if(strlen(response) > 0) return true;
	}

	sdiInterface.clearBuffer();
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
std::vector<char> Loom_SDI12::getInUseAddresses(){
   return inUseAddresses;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
const char* Loom_SDI12::getSensorInfo(char addr){
    return addressToType[addr];
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::sendCommand(char response[RESPONSE_SIZE], char addr, const char* command){
    // Send a request to the sensor at the given address and then wait 30ms before continuing
    char output[25];
    snprintf(output, 25, "%c%s", addr, command);
    sdiInterface.sendCommand(output);
    delay(30);
    readResponse(response);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::readResponse(char response[RESPONSE_SIZE]){
    int index = 0;
    // While data is available to be read read until an end line character appears.
    while(sdiInterface.available()){
        char c = sdiInterface.read();

        // Command responses terminate with an endline so we should stop when we see this
        if (c == '\n'){
            response[index] = '\0';
            break;
        }

        // If we are under 30 then add the data to the array
        if(index < RESPONSE_SIZE){
            response[index] = c;
        }

        index++;
        delay(20); // SDI-12 is slow so we need to wait after each character
    }

    // Replace the carriage return with a null-byte
    char* pch = strstr(response, "\r");
    if(pch != NULL){
        response[pch-response] == '\0';
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::requestSensorInfo(char response[RESPONSE_SIZE], char addr){
    sendCommand(response, addr, "I!");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::getData(char addr){
    char		buf[20];
	char*		p;
    char        response[RESPONSE_SIZE];

    // Request a measurement from the sensor at the given address
    sendCommand(response, addr, "M!");
    memset(response, '\0', RESPONSE_SIZE);
    sendCommand(response, addr, "D0!");
    
    // If the value returned was 0 we want to re-request data
    if(strlen(response) == 1){
        WARNING(F("Invalid data received! Retrying..."));
        delay(3000);

        // Request a measurement from the sensor at the given address
        sendCommand(response, addr, "M!");
        memset(response, '\0', RESPONSE_SIZE);
        sendCommand(response, addr, "D0!");

        TIMER_RESET;
	    if(strlen(response) == 1){
            WARNING(F("Retrying for a second time..."));
            delay(3000);

            // Request a measurement from the sensor at the given address
            sendCommand(response, addr, "M!");
            memset(response, '\0', RESPONSE_SIZE);
            sendCommand(response, addr, "D0!");
            TIMER_RESET;
            
	    }
    }

    // Check if there is actually data to store in the variables
    if(strlen(response) > 1){
        // If the sensor is a copy the used values an
        if(strstr(getSensorInfo(addr), "GS3") != NULL){
            // Read out the results and parse out each of the data readings and pares them to floats
            p = strtok(response, "+");
            
            sensorData[1] = (atof(strtok(NULL, "+")));
            sensorData[0] = (atof(strtok(NULL, "+")));
            sensorData[2] = (atof(strtok(NULL, "+")));
        }

        // Teros
        else if(strstr(getSensorInfo(addr), "TER") != NULL){
            // Read out the results and parse out each of the data readings and pares them to floats
            p = strtok(response, "+");
            
            sensorData[1] = (atof(strtok(NULL, "+")));
            sensorData[0] = (atof(strtok(NULL, "+")));

            // If we are on the Teros 12
            if(strstr(getSensorInfo(addr), "12") != NULL)
                sensorData[2] = (atof(strtok(NULL, "+")));
        }
    }
    else{
        ERROR(F("Failed to record new data! Using previous valid information!"));
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////