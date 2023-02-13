#include "Loom_Manager.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Manager::Manager(String devName, uint32_t instanceNum) : deviceName(devName), instanceNumber(instanceNum), doc(2000) {};
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
 void Manager::registerModule(Module* module){
    // If there are no duplicates proceed as normal
    for(int i = 0; i < modules.size(); i++){
        
        // Check if the module name contains the base string to make sure this works past 2 modules of the same type
        if(modules[i].first.startsWith(module->getModuleName())){
            
            // Append the address to the name
            modules[i].second->setModuleName(modules[i].second->getModuleName() + String("_") + String(modules[i].second->module_address));
            module->setModuleName(module->getModuleName() + String("_") + String(module->module_address));

            // Once we find a module of this type we want to break out to avoid redundant name changes
            break;
        }
    }

    
    modules.push_back(std::make_pair(module->getModuleName(), module));
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
DynamicJsonDocument& Manager::getDocument() {return doc;}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::beginSerial(bool waitForSerial){
    long startMillis = millis();

    Serial.begin(BAUD_RATE);
    // Pause if the Serial is not open and we want to wait
    while(!Serial && waitForSerial){

        // If it has been 20 seconds break out of the loop
        if(millis() >= (startMillis+WAIT_TIME_MS)){
            break;
        }
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::measure() {
    if(hasInitialized){
       for(int i = 0; i < modules.size(); i++){
            if(modules[i].second->moduleInitialized)
                modules[i].second->measure();
            else{
                modules[i].second->printModuleName("Not initialized!");
            }
            Watchdog.reset();
        }
    }
    else{
            Serial.println("[Manager] Unable to collect data as the manager and thus all sensors connected to it have not been initialized! Call manager.initialize() to fix this.");
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::package(){

    // Clear the document so that we don't get null characters after too many updates
    doc.clear();
    doc["type"] = "data";
    doc["id"]["name"] = get_device_name();
    doc["id"]["instance"] = get_instance_num();

    // Get the contents of the JSON document
    contentsArray = doc["contents"];
    if(contentsArray.isNull())
        contentsArray = doc.createNestedArray("contents");

    // Add the packet number to the JSON document
    JsonObject json = get_data_object("Packet");
    json["Number"] = packetNumber;

    for(int i = 0; i < modules.size(); i++){
        if(modules[i].second->moduleInitialized)
            modules[i].second->package();
        else{
            modules[i].second->printModuleName("Not initialized!");
        }
        Watchdog.reset();
    }
    packetNumber++;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
JsonObject Manager::get_data_object(String moduleName){

    // Check if the key already exists in the array
    for(JsonVariant value : contentsArray){

        // If the data already exists
        if(value.as<JsonObject>()["module"].as<String>() == moduleName){
            return value.as<JsonObject>()["data"];
        }
    }

    // If it doesn't already exist create a new object
    JsonObject json = contentsArray.createNestedObject();
    json["module"] = moduleName;
    return json.createNestedObject("data");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::power_up(){
    for(int i = 0; i < modules.size(); i++){
        if(modules[i].second->moduleInitialized)
            modules[i].second->power_up();
        else{
            modules[i].second->printModuleName("Not initialized!");
        }
        Watchdog.reset();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::power_down(){
   for(int i = 0; i < modules.size(); i++){
        if(modules[i].second->moduleInitialized)
            modules[i].second->power_down();
        else{
            modules[i].second->printModuleName("Not initialized!");
        }
        Watchdog.reset();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::display_data(){
    if(!doc.isNull()){
        Serial.println("\n[Manager] Data Json: ");
        serializeJsonPretty(doc, Serial);
        Serial.println("\n");
    }
    else{
        Serial.println("[Manager] JSON Document is Null there is no data to display");
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::initialize() {

    // If you are using a hypnos board that has not been enabled, this needs to occur before initializing sensors
    if(usingHypnos && !hypnosEnabled){
        Serial.println("[Manager] Your sketch is set to use a Hypnos board which has not been enabled before attempting to initialize sensors. \nThis will causing hanging please enable the board before initialization. Continuing but know this may cause issues!"); 
    }

    Serial.println("[Manager] ** Initializing Modules **");
    read_serial_num();
    for(int i = 0; i < modules.size(); i++){
        modules[i].second->initialize();
    }
    hasInitialized = true;
    Serial.println("[Manager] ** Setup Complete ** ");


    Watchdog.enable(WATCHDOG_TIMEOUT);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
String Manager::getJSONString(){
    String jsonString ="";
    serializeJson(doc, jsonString);
    return jsonString;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::read_serial_num(){
    char serial_no[33];
    serial_num = "";
    // Serial numbers are made up of four words located at these specific registers (see datasheet)
	uint32_t sn_words[4];
	sn_words[0] = *(volatile uint32_t *)(0x0080A00C);
	sn_words[1] = *(volatile uint32_t *)(0x0080A040);
	sn_words[2] = *(volatile uint32_t *)(0x0080A044);
	sn_words[3] = *(volatile uint32_t *)(0x0080A048);

    // Take these raw values and convert them into a string of hex characters
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			sprintf(serial_no + (i * 8) + (j * 2), "%02X", (uint8_t)(sn_words[i] >> ((3 - j) * 8)));
		}
	}

    serial_num = String(serial_no);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::pause(const uint32_t ms) const {
    Watchdog.disable();
    int waitTime = millis() + ms;
    while (millis() < waitTime);
    Watchdog.enable(WATCHDOG_TIMEOUT);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::setLogCallback(SDLogDebug func){
    for(int i = 0; i < modules.size(); i++){
        modules[i].second->setLogCallback(func);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////