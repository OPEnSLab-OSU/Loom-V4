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
    LOG(F("Initializing SDI-12 Sensors..."));

    // On init we set the SDI pin to OUTPUT so we can request data
    pinMode(sdiInterface.getDataPin(), OUTPUT);

    // Start the interface and then wait for 100ms to allow things to settle and startup correctly
    sdiInterface.begin();
    delay(100);

    // Create a list of addresses that have a sensor connected to them
    scanAddressSpace();

    // Request the sensor data from all connected devices to pull the sensor name
    for(int i = 0; i < num_sensors; i++){
        char response[IDENTIFICATION_RESPONSE_SIZE];
        requestSensorInfo(registered_sensors[i].sensor_name, registered_sensors[i].address);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::measure(){
   
    // On measure we also want to reset the mode to output in case the 4G board has messed with it
    pinMode(sdiInterface.getDataPin(), OUTPUT);
    delay(30);

    // Populate the variables that will be used to package data
    for(int i = 0; i < num_sensors; i++){
        getData(&registered_sensors[i]);
    }
    
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::package(){
    for(int i = 0; i < num_sensors; i++){
        char package_name[PACKAGE_NAME];
        SensorInfo curr_sensor = registered_sensors[i];
        
        if(strstr(curr_sensor.sensor_name, "GS3") != NULL){
            snprintf(package_name, PACKAGE_NAME, "%s_%i", curr_sensor.sensor_name, i);
            JsonObject json = manInst->get_data_object(package_name);
            json["Temperature_C"] = curr_sensor.measurements[0];
            json["Dielectric_Permittivity"] = curr_sensor.measurements[1];
            json["Conductivity"] = curr_sensor.measurements[2];
        }

        else if (strstr(curr_sensor.sensor_name, "TER11") != NULL){
            snprintf(package_name, PACKAGE_NAME, "%s_%i", curr_sensor.sensor_name, i);
            JsonObject json = manInst->get_data_object(curr_sensor.sensor_name);
            json["Temperature"] = curr_sensor.measurements[0];
            json["Volumetric_Water_Content"] = curr_sensor.measurements[1];
        }
        else if(strstr(curr_sensor.sensor_name, "TER12")){
            snprintf(package_name, PACKAGE_NAME, "%s_%i", curr_sensor.sensor_name, i);
            JsonObject json = manInst->get_data_object(package_name);
            json["Temperature"] = curr_sensor.measurements[0];
            json["Volumetric_Water_Content"] = curr_sensor.measurements[1];
            json["Conductivity"] = curr_sensor.measurements[2];
        }

        else if(strstr(curr_sensor.sensor_name, "TER54")){
            snprintf(package_name, PACKAGE_NAME, "%s_%i", curr_sensor.sensor_name, i);
            JsonObject json = manInst->get_data_object(package_name);
            json["Temperature_15cm"] = curr_sensor.measurements[1];
            json["VWC_15cm"] = curr_sensor.measurements[0];
            json["Temperature_30cm"] = curr_sensor.measurements[3];
            json["VWC_30cm"] = curr_sensor.measurements[2];
            json["Temperature_45cm"] = curr_sensor.measurements[4];
            json["VWC_45cm"] = curr_sensor.measurements[5];
            json["Temperature_60cm"] = curr_sensor.measurements[7];
            json["VWC_60cm"] = curr_sensor.measurements[6];
        }

        else if(strstr(curr_sensor.sensor_name, "TER21")){
            snprintf(package_name, PACKAGE_NAME, "%s_%i", curr_sensor.sensor_name, i);
            JsonObject json = manInst->get_data_object(package_name);
        }
        
        
        // if(strstr(curr_sensor_name, "GS3") != NULL){
        //     if(strlen(sensorNames[i]) <= 0){
        //         snprintf(sensorNames[i], 100, "GS3_%i", i);
        //     }
        //     JsonObject json = manInst->get_data_object(sensorNames[i]);
        //     json["Temperature"] = sensorData[0];
        //     json["Dielectric_Permittivity"] = sensorData[1];
        //     json["Conductivity"] = sensorData[2];
        // }
        // else if(strstr(getSensorInfo(inUseAddresses[i]), "TER") != NULL){
        //     if(strlen(sensorNames[i]) <= 0){
        //         snprintf(sensorNames[i], 100, "TER_%i", i);
        //     }
        //     JsonObject json = manInst->get_data_object(sensorNames[i]);
        //     json["Temperature"] = sensorData[0];
        //     json["Volumetric_Water_Content"] = sensorData[1];
        //     if(strstr(getSensorInfo(inUseAddresses[i]), "TER12") != NULL)
        //         json["Conductivity"] = sensorData[2];
        // }

    }
    

//     }
    
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
void Loom_SDI12::scanAddressSpace(){
    char output[OUTPUT_SIZE];

    // Print the module name followed by the message saying please wait
    LOG(F("Scanning SDI-12 Address Space this may take a little while..."));

    // Scan over the characters that can be used as addresses for refrencing the sensors
	for (char i = '0'; i <= '9'; i++){
		if(checkActive(i)){
			registered_sensors[num_sensors].address = i;
            num_sensors++;
		}
	}

	for(char i = 'a'; i <= 'z'; i++){
		if(checkActive(i)){
			registered_sensors[num_sensors].address = i;
            num_sensors++;
		}
	}

	for (char i = 'A'; i <= 'Z'; i++){
		if(checkActive(i)){
			registered_sensors[num_sensors].address = i;
            num_sensors++;
		}
	}

    // Check if we actually found any connected devices
    if(num_sensors > 0){
        // Print the module name followed by the message saying please wait
        LOG(F("== We found the following active Addresses =="));
        for(int i = 0; i < num_sensors; i++){
            snprintf(output, OUTPUT_SIZE, "    Address: %c", registered_sensors[i].address);
            LOG(output); 
        }
    }
    else{
        LOG(F("== No SDI-12 Devices Were Discovered == "));
    }
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
void Loom_SDI12::sendCommand(char* response, char addr, const char* command){
    // Send a request to the sensor at the given address and then wait 30ms before continuing
    char output[RESPONSE_SIZE];
    snprintf(output, RESPONSE_SIZE, "%c%s", addr, command);
    // wait the specified wait time between requesting measurement and requesting data
    if(!strcmp(command, "D0!") || !strcmp(command, "D1!")){
        LOG("STUCK before delay(wait_time)");
        delay(wait_time*1000);
    }
    LOG("STUCK AFTER DELAY WAIT TIME");
    sdiInterface.sendCommand(output);
    delay(30);
    readResponse(response, command);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::readResponse(char* response, const char* command){
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
        delay(20); // SDI-12 is 1200 baud, expect a new character after roughly 8 milliseconds, we round up to 10
    }

    // Replace the carriage return with a null-byte
    char* pch = strstr(response, "\r");
    if(pch != NULL){
        response[pch-response] = '\0';
    }
    // extracts the name of the sensor. 
    if(!strcmp(command, "I!")){
        char temp[MAX_NAME_SIZE];
        LOG(response);
        strncpy(temp, response+11, 6);
        temp[MAX_NAME_SIZE-1] = '\0';
        LOG(temp);
        strcpy(response, temp);
        response[MAX_NAME_SIZE-1] = '\0';
        // cleanse the name of space padding
        for(int i = strlen(response) - 1; i > 0; i--){
            char curr_char = response[i];
            if(curr_char == ' '){
                response[i] = '\0';
            }
        }
    }
    if(!strcmp(command, "M!")){
        // for debug, remove after testing
        LOG(response);

    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::requestSensorInfo(char* response, char addr){
    sendCommand(response, addr, "I!");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::getData(SensorInfo *sensor){
    char		buf[20];
	char*		p;
    char        response[RESPONSE_SIZE] = {'\0'};
    char        addr = sensor->address;
    bool        ter54 = strstr(sensor->sensor_name, "TER54");

    // Request a measurement from the sensor at the given address
    sendCommand(response, addr, "M!");
    //parse_wait_time(response);

    if(!ter54){
        sendCommand(response, addr, "D0!");
    }
    else{
        // D0 with the TER54 returns the raw values, instead of the calculated. 
        // we only care about the calulated 
        
        sendCommand(response, addr, "D1!");
    }
    //delay(specified time from sensor)
    
    // If the value returned was 0 we want to re-request data
    LOG(response);
    if(strlen(response) == 1){
        WARNING(F("Invalid data received! Retrying..."));
        delay(3000);
        LOG("Stuck1");
        sendCommand(response, addr, "M!");
        LOG("Stuck2");
        // parse_wait_time(response);
        if(!ter54){
            sendCommand(response, addr, "D0!");
        }
        else{
            // D0 with the TER54 returns the raw values, instead of the calculated. 
            // we only care about the calulated 
            LOG("Stuck3");
            sendCommand(response, addr, "D1!");
        }
    

        TIMER_RESET;
	    if(strlen(response) == 1){
            WARNING(F("Retrying for a second time..."));
            delay(3000);

            // Request a measurement from the sensor at the given address
            sendCommand(response, addr, "M!");
            // parse_wait_time(response);
            if(!ter54){
                sendCommand(response, addr, "D0!");
            }
            else{
                // D0 with the TER54 returns the raw values, instead of the calculated. 
                // we only care about the calulated 
                sendCommand(response, addr, "D1!");
            }
    
            TIMER_RESET;
            
	    }
    }

    // Check if there is actually data to store in the variables
    if(strlen(response) > 1){
        LOG(response);
        // If the sensor is a copy the used values an
        if(strstr(sensor->sensor_name, "GS3") != NULL){
            // Read out the results and parse out each of the data readings and pares them to floats
            p = strtok(response, "+");
            
            sensor->measurements[1] = (atof(strtok(NULL, "+")));
            sensor->measurements[0] = (atof(strtok(NULL, "+")));
            sensor->measurements[2] = (atof(strtok(NULL, "+")));
        }

        // Teros
        else if(strstr(sensor->sensor_name, "TER11") != NULL){
            // Read out the results and parse out each of the data readings and pares them to floats
            p = strtok(response, "+");
            
            sensor->measurements[1] = (atof(strtok(NULL, "+")));
            sensor->measurements[0] = (atof(strtok(NULL, "+")));
        }
            // If we are on the Teros 12
        else if(strstr(sensor->sensor_name, "TER12") != NULL){
            p = strtok(response, "+");
            sensor->measurements[1] = (atof(strtok(NULL, "+")));
            sensor->measurements[0] = (atof(strtok(NULL, "+")));
            sensor->measurements[2] = (atof(strtok(NULL, "+")));
        
        }
        else if(ter54){
            p = strtok(sensor->sensor_name, "+");
            sensor->measurements[0] = (atof(strtok(NULL, "+")));
            sensor->measurements[1] = (atof(strtok(NULL, "+")));
            sensor->measurements[2] = (atof(strtok(NULL, "+")));
            sensor->measurements[3] = (atof(strtok(NULL, "+")));
            sensor->measurements[4] = (atof(strtok(NULL, "+")));
            sensor->measurements[5] = (atof(strtok(NULL, "+")));
            sensor->measurements[6] = (atof(strtok(NULL, "+")));
            sensor->measurements[7] = (atof(strtok(NULL, "+")));
        }
    }   
    else{
        ERROR(F("Failed to record new data! Using previous valid information!"));
    }
    
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::parse_wait_time(char* response){
    // time needed between M! and D0! (or D1!) is specified in the response to M!
    // here, the signiture is a 5 char response (excluding two bytes for CR and LF). 
    // The first char representing address,
    // the next three indicating wait time, and the last indicating number of measurements
    // ex a0058 indicates address a, 5 seconds, and 8 measurements

    // response[1] is hundreds digit, response[2] is tens, response[3] is ones
    // Subtracting '0' (ASCII 48) converts an ASCII character character directly to its integer value
    if(strlen(response) == 5){
        wait_time = ((response[1] - '0') * 100) + 
                    ((response[2] - '0') * 10) + 
                    (response[3] - '0');
    }
    else{
        LOG("SOMETHING WENT WRONG WITH M! COMMAND");
        wait_time = 1;
    }
    
}
//////////////////////////////////////////////////////////////////////////////////////////////////////