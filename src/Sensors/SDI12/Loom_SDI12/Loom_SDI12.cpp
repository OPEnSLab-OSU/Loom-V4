#include "Loom_SDI12.h"


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

    printModuleName(); Serial.println("Initializing SDI-12 Sensors...");

    // On init we set the SDI pin to OUTPUT so we can request data
    pinMode(sdiInterface.getDataPin(), OUTPUT);

    // Start the interface and then wait for 100ms to allow things to settle and startup correctly
    sdiInterface.begin();
    delay(100);

    // Create a list of addresses that have a sensor connected to them
    inUseAddresses = scanAddressSpace();

    // Request the sensor data from all connected devices to pull the sensor name
    for(int i = 0; i < inUseAddresses.size(); i++){
        String sensorInfo = requestSensorInfo(inUseAddresses[i]);
        addressToType.insert(std::pair<char, String>(inUseAddresses[i], sensorInfo));
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::measure(){
   

    // Populate the variables that will be used to package data
    for(int i = 0; i < inUseAddresses.size(); i++){
        getData(inUseAddresses[i]);
    }
    
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::print_measurements(){
   
    printModuleName();
	Serial.println("Measurements:");
	Serial.println("\tTemperature: " + String(sensorData[0]));
	Serial.println("\tDielectric Permeability: " + String(sensorData[1]));
	Serial.println("\tConductivity: " + String(sensorData[2]));
   
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::package(){
    for(int i = 0; i < inUseAddresses.size(); i++){
        
        if(getSensorInfo(inUseAddresses[i]).indexOf("GS3") != -1){
            JsonObject json = manInst->get_data_object("GS3_" + String(i));
            json["Temperature"] = sensorData[0];
            json["Dielectric Permittivity"] = sensorData[1];
            json["Conductivity"] = sensorData[2];
        }
        else{
            JsonObject json = manInst->get_data_object("Terros_" + String(i));
            json["Temperature"] = sensorData[0];
            json["Volumetric Water Content"] = sensorData[1];
            if(getSensorInfo(inUseAddresses[i]).indexOf("TER12") != -1)
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

    // Print the module name followed by the message saying please wait
    printModuleName(); Serial.println("Scanning SDI-12 Address Space this make take a little while...");

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
        printModuleName(); Serial.println("== We found the following active Addresses ==");
        for(int i = 0; i < activeSensors.size(); i++){
            printModuleName(); 
            Serial.print("    Address: ");
            Serial.println(activeSensors[i]);
        }
    }
    else{
        printModuleName(); Serial.println("== No SDI-12 Devices Were Discovered == ");
    }

    return activeSensors;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_SDI12::checkActive(char addr){
    // Attempt to contact the sensor 3 times
	for (int i =0; i < 3; i++){
		if(sendCommand(addr, "!").length() > 0) return true;
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
String Loom_SDI12::getSensorInfo(char addr){
    return addressToType[addr];
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
String Loom_SDI12::sendCommand(char addr, String command){

    // Send a request to the sensor at the given address and then wait 30ms before continuing
    sdiInterface.sendCommand(String(addr) + command);
    delay(30);

    return readResponse();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
String Loom_SDI12::readResponse(){
    String response = "";

    // While data is available to be read read until an end line character appears.
    while(sdiInterface.available()){
        char c = sdiInterface.read();

        // Command responses terminate with an endline so we should stop when we see this
        if (c == '\n')
            break;
        
        response += c;
        delay(20); // SDI-12 is slow so we need to wait after each character
    }

    if(response[response.length()-1] == '\r'){
		response[response.length()-1] = '\0'; // Replace carriage return with null terminator byte
	}

    return response;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
String Loom_SDI12::requestSensorInfo(char addr){
    String info = String(sendCommand(addr, "I!"));
    info.trim();
    return info;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::getData(char addr){
    char		buf[20];
	char*		p;

    // Request a measurement from the sensor at the given address
    sendCommand(addr, "M!");
    sendCommand(addr, "D0!").toCharArray(buf, sizeof(buf));

    // If the value returned was 0 we want to re-request data, TEST THIS
    if(sizeof(buf)/ sizeof(buf[0]) < 3){
        // Request a measurement from the sensor at the given address
        sendCommand(addr, "M!");
        sendCommand(addr, "D0!").toCharArray(buf, sizeof(buf));
    }

    p = buf;

    // If the sensor is a copy the used values an
    if(getSensorInfo(addr).indexOf("GS3") != -1){
        // Read out the results and parse out each of the data readings and pares them to floats
        strtok(p, "+");
        
        sensorData[1] = (atof(strtok(NULL, "+")));
        sensorData[0] = (atof(strtok(NULL, "+")));
        sensorData[2] = (atof(strtok(NULL, "+")));
    }

    // Teros
    else if(getSensorInfo(addr).indexOf("TER") != -1){
        // Read out the results and parse out each of the data readings and pares them to floats
        strtok(p, "+");
        
        sensorData[1] = (atof(strtok(NULL, "+")));
        sensorData[0] = (atof(strtok(NULL, "+")));

        // If we are on the Teros 12
        if(getSensorInfo(addr).indexOf("12") != -1)
            sensorData[2] = (atof(strtok(NULL, "+")));
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////