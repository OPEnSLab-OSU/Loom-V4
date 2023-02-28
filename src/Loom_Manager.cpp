#include "Loom_Manager.h"
#include "Logger.h"
Logger* Logger::instance = nullptr;

//////////////////////////////////////////////////////////////////////////////////////////////////////
Manager::Manager(const char* devName, uint32_t instanceNum) : deviceName(devName), instanceNumber(instanceNum), doc(2000) {
    strncpy(ths->deviceName, devName, 100);
    Logger::getInstance();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
 void Manager::registerModule(Module* module){
    char* location;
    // If there are no duplicates proceed as normal
    for(int i = 0; i < modules.size(); i++){
        // Find the pointer to the module name
        location = strstr(modules[i].first, module->getModuleName());
        
        // Check if the module name contains the base string to make sure this works past 2 modules of the same type
        if(location != NULL){
            // Append the address to the name
            char modifiedName[100];

            // Format first module name
            sprintf(modifiedName, "%s_%i", modules[i].second->getModuleName(), modules[i].second->module_address);
            modules[i].second->setModuleName(modifiedName);

            // Format second string using the same array
            sprintf(modifiedName, "%s_%i", module->getModuleName(), module->module_address);
            module->setModuleName(modifiedName);

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
    FUNCTION_START;
    if(hasInitialized){
       for(int i = 0; i < modules.size(); i++){
            if(modules[i].second->moduleInitialized)
                modules[i].second->measure();
            else{
                modules[i].second->printModuleName("Not initialized!");
            }
            TIMER_RESET;
        }
    }
    else{
            ERROR("Unable to collect data as the manager and thus all sensors connected to it have not been initialized! Call manager.initialize() to fix this.");
    }
    FUNCTION_END("void");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::package(){
    FUNCTION_START;
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
        if(modules[i].second->moduleInitialized){
            modules[i].second->package();
        } else{
            modules[i].second->printModuleName("Not initialized!");
        }
        TIMER_RESET;
    }
    packetNumber++;
    FUNCTION_END("void");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
JsonObject Manager::get_data_object(const char* moduleName){

    // Check if the key already exists in the array
    for(JsonVariant value : contentsArray){

        // If the data already exists
        if(strcmp(value.as<JsonObject>()["module"].as<const char*>(), moduleName) == 0){
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
    FUNCTION_START;
    for(int i = 0; i < modules.size(); i++){
        if(modules[i].second->moduleInitialized)
            modules[i].second->power_up();
        else{
            modules[i].second->printModuleName("Not initialized!");
        }
        TIMER_RESET;
    }
    FUNCTION_END("void");
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
        TIMER_RESET;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::display_data(){
    
    FUNCTION_START;
    if(!doc.isNull()){

        // Get the json size with the null terminator
        size_t jsonSize = measureJsonPretty(doc)+1;
        char *jsonStr = malloc(jsonSize);
        char *output = malloc(jsonSize+15);
        serializeJsonPretty(doc, jsonStr);
        
        // Read the jsonStr into the proper output format
        snprintf(output, jsonSize+15,"Data Json: \n%s\n", jsonStr);
        free(jsonStr);
        LOG(output);
        free(output);
    }
    else{
        LOG("JSON Document is Null there is no data to display");
    }
    
    FUNCTION_END("void");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::initialize() {
    FUNCTION_START;
    // If you are using a hypnos board that has not been enabled, this needs to occur before initializing sensors
    if(usingHypnos && !hypnosEnabled){
        LOG("Your sketch is set to use a Hypnos board which has not been enabled before attempting to initialize sensors. \nThis will causing hanging please enable the board before initialization. Continuing but know this may cause issues!"); 
    }

    LOG("** Initializing Modules **");
    read_serial_num();
    for(int i = 0; i < modules.size(); i++){
        modules[i].second->initialize();
    }
    hasInitialized = true;
    LOG("** Setup Complete ** ");


    TIMER_ENABLE;
    FUNCTION_END("void");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
char* Manager::getJSONString(){
    char* jsonString = malloc(measureJson(doc));
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

    // Copy the contents of the calculated char array into the member variable
    strncpy(serial_num, serial_no, 33);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Manager::pause(const uint32_t ms) const {
    TIMER_DISABLE;
    int waitTime = millis() + ms;
    while (millis() < waitTime);
    TIMER_ENABLE;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////