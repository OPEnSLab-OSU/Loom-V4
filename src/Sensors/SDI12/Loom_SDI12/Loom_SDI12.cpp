#include "Loom_SDI12.h"
#include "Logger.h"
#include <stdlib.h>
#include <math.h>
//////////////////////////////////////////////////////////////////////////////////////////////////////
static bool parseSignedSDI12(
    const char *response,
    char address,
    float *values,
    size_t count)
{
    if (!response || !values || count == 0 || count > 8)
    {
        return false;
    }

    if (response[0] != address)
    {
        return false;
    }

    const char *ptr = response + 1;
    float temp[8];

    for (size_t i = 0; i < count; i++)
    {
        if (*ptr != '+' && *ptr != '-')
        {
            return false;
        }

        char *endPtr;
        float value = strtof(ptr, &endPtr);

        if (endPtr == ptr ||
            !isfinite(value) ||
            value == -9999.0f)
        {
            return false;
        }

        if (*endPtr != '+' &&
            *endPtr != '-' &&
            *endPtr != '\0')
        {
            return false;
        }

        temp[i] = value;
        ptr = endPtr;
    }

    if (*ptr != '\0')
    {
        return false;
    }

    for (size_t i = 0; i < count; i++)
    {
        values[i] = temp[i];
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Existing Loom_SDI12 constructor
//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_SDI12::Loom_SDI12(Manager &man, const int pinNumber) : Module("SDI12"), sdiInterface(pinNumber)
{
    manInst = &man;
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_SDI12::Loom_SDI12(const int pinNumber) : Module("SDI12"), sdiInterface(pinNumber) {}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::initialize()
{
    char output[OUTPUT_SIZE];
    LOG(F("Initializing SDI-12 Sensors..."));

    // On init we set the SDI pin to OUTPUT so we can request data
    pinMode(sdiInterface.getDataPin(), OUTPUT);

    // Start the interface and then wait for 100ms to allow things to settle and startup correctly
    sdiInterface.begin();
    delay(100);
    // !*** Ommitting for now, do not want sensor discovery
    // Create a list of addresses that have a sensor connected to them
    // inUseAddresses = scanAddressSpace();

    // Request the sensor data from all connected devices to pull the sensor name
    // for(int i = 0; i < inUseAddresses.size(); i++){
    //     char* response = (char*) malloc(sizeof(char) * RESPONSE_SIZE);
    //     memset(response, '\0', RESPONSE_SIZE);
    //     requestSensorInfo(response, inUseAddresses[i]);
    //     addressToType.insert(std::pair<char, const char*>(inUseAddresses[i], response));

    //     // Allocate string of size 100 for each SDI device to store a name
    //     sensorNames.push_back((char*) malloc(sizeof(char) * 100));
    //     memset(sensorNames[i], '\0', 100);
    // }
    for (int i = 0; i < KNOWN_SENSORS; i++)
    {
        char response[RESPONSE_SIZE];
        // Enable the sensor only after identification succeeds.
        in_use_sensors[i].in_use = 0;

        if (checkActive(in_use_sensors[i].addr))
        {
            requestSensorInfo(response, in_use_sensors[i].addr);
            if (strstr(response, in_use_sensors[i].name))
            {
                snprintf(output, OUTPUT_SIZE, "%s found!", in_use_sensors[i].name);
                LOG(output);
                in_use_sensors[i].in_use = 1;
            }
            else
            {
                LOG("Connected sensor is unknown or using the incorrect address");
                LOG("The following addresses are required for known sensors: ");
                LOG("Teros12 - Address '0'");
                LOG("Teros54 - Address '1'");
                LOG("Teros21 - Address '2'");
                LOG("Teros11 - Address '3'");
                snprintf(output, OUTPUT_SIZE, "Please double check sensor with address %c", in_use_sensors[i].addr);
                LOG(output);
                in_use_sensors[i].in_use = 0;
            }
        }
        else
        {
            snprintf(output, OUTPUT_SIZE, "%s is not connected, or is using the wrong address. Expected address: %c",
                     in_use_sensors[i].name, in_use_sensors[i].addr);
            LOG(output);
            in_use_sensors[i].in_use = 0;
        }
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::measure()
{

    // On measure we also want to reset the mode to output in case the 4G board has messed with it
    pinMode(sdiInterface.getDataPin(), OUTPUT);
    delay(30);

    // Populate the variables that will be used to package data
    for (int i = 0; i < KNOWN_SENSORS; i++)
    {
        if (in_use_sensors[i].in_use)
        {
            getData(in_use_sensors[i].addr);
        }
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::package()
{

    // for(int i = 0; i < inUseAddresses.size(); i++){
    //     if(strstr(getSensorInfo(inUseAddresses[i]), "GS3") != NULL){
    //         if(strlen(sensorNames[i]) <= 0){
    //             snprintf(sensorNames[i], 100, "GS3_%i", i);
    //         }
    //         JsonObject json = manInst->get_data_object(sensorNames[i]);
    //         json["Temperature"] = sensorData[0];
    //         json["Dielectric_Permittivity"] = sensorData[1];
    //         json["Conductivity"] = sensorData[2];
    //     }
    //     else if(strstr(getSensorInfo(inUseAddresses[i]), "TER") != NULL){
    //         if(strlen(sensorNames[i]) <= 0){
    //             snprintf(sensorNames[i], 100, "TER_%i", i);
    //         }
    //         JsonObject json = manInst->get_data_object(sensorNames[i]);
    //         json["Temperature"] = sensorData[0];
    //         json["Volumetric_Water_Content"] = sensorData[1];
    //         if(strstr(getSensorInfo(inUseAddresses[i]), "TER12") != NULL)
    //             json["Conductivity"] = sensorData[2];
    //     }
    // }

    // Teros 12
    if (in_use_sensors[0].in_use)
    {
        JsonObject json = manInst->get_data_object(in_use_sensors[0].name);
        json["Temperature_C"] = in_use_sensors[0].measurements[0];
        json["Calibrated_ADC_Counts_VWC"] = in_use_sensors[0].measurements[1];
        json["Electrical_Conductivity_dS_m"] = in_use_sensors[0].measurements[2];
    }

    // Teros 11
    if (in_use_sensors[3].in_use)
    {
        JsonObject json = manInst->get_data_object(in_use_sensors[3].name);
        json["Temperature_C"] = in_use_sensors[3].measurements[0];
        json["Calibrated_ADC_Counts_VWC"] = in_use_sensors[3].measurements[1];
    }

    // Teros 54
    if (in_use_sensors[1].in_use)
    {
        JsonObject json = manInst->get_data_object(in_use_sensors[1].name);
        json["Calibrated_ADC_Counts_VWC_15cm"] = in_use_sensors[1].measurements[0];
        json["Temperature_C_15cm"] = in_use_sensors[1].measurements[1];
        json["Calibrated_ADC_Counts_VWC_30cm"] = in_use_sensors[1].measurements[2];
        json["Temperature_C_30cm"] = in_use_sensors[1].measurements[3];
        json["Calibrated_ADC_Counts_VWC_45cm"] = in_use_sensors[1].measurements[4];
        json["Temperature_C_45cm"] = in_use_sensors[1].measurements[5];
        json["Calibrated_ADC_Counts_VWC_60cm"] = in_use_sensors[1].measurements[6];
        json["Temperature_C_60cm"] = in_use_sensors[1].measurements[7];
    }

    // Teros 21
    if (in_use_sensors[2].in_use)
    {
        JsonObject json = manInst->get_data_object(in_use_sensors[2].name);
        json["Water_Potential_kPa"] = in_use_sensors[2].measurements[0];
        json["Temperature_C"] = in_use_sensors[2].measurements[1];
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::power_up()
{
    pinMode(sdiInterface.getDataPin(), OUTPUT);
    sdiInterface.begin();
    delay(100);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::power_down()
{
    sdiInterface.end();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
// std::vector<char> Loom_SDI12::scanAddressSpace(){
//     std::vector<char> activeSensors;
//     char output[OUTPUT_SIZE];

//     // Print the module name followed by the message saying please wait
//     LOG(F("Scanning SDI-12 Address Space this make take a little while..."));

//     // Scan over the characters that can be used as addresses for refrencing the sensors
// 	for (char i = '0'; i <= '9'; i++){
// 		if(checkActive(i)){
// 			activeSensors.push_back(i);
// 		}
// 	}

// 	for(char i = 'a'; i <= 'z'; i++){
// 		if(checkActive(i)){
// 			activeSensors.push_back(i);
// 		}
// 	}

// 	for (char i = 'A'; i <= 'Z'; i++){
// 		if(checkActive(i)){
// 			activeSensors.push_back(i);
// 		}
// 	}

//     // Check if we actually found any connected devices
//     if(activeSensors.size() > 0){
//         // Print the module name followed by the message saying please wait
//         LOG(F("== We found the following active Addresses =="));
//         for(int i = 0; i < activeSensors.size(); i++){
//             snprintf(output, OUTPUT_SIZE, "    Address: %c", activeSensors[i]);
//             LOG(output);
//         }
//     }
//     else{
//         LOG(F("== No SDI-12 Devices Were Discovered == "));
//     }

//     return activeSensors;
// }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_SDI12::checkActive(char addr)
{
    // Attempt to contact the sensor 3 times
    char response[RESPONSE_SIZE];
    for (int i = 0; i < 3; i++)
    {
        memset(response, '\0', RESPONSE_SIZE);
        sendCommand(response, addr, "!");
        if (strlen(response) > 0)
            return true;
    }

    sdiInterface.clearBuffer();
    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
// std::vector<char> Loom_SDI12::getInUseAddresses(){
//    return inUseAddresses;
// }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
const char *Loom_SDI12::getSensorInfo(char addr)
{
    return addressToType[addr];
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::sendCommand(char response[RESPONSE_SIZE], char addr, const char *command)
{
    // Send a request to the sensor at the given address and then wait 30ms before continuing
    char output[25];
    memset(output, '\0', 25);
    snprintf(output, 25, "%c%s", addr, command);
    sdiInterface.clearBuffer();
    sdiInterface.sendCommand(output);
    delay(30);
    readResponse(response);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::readResponse(char response[RESPONSE_SIZE])
{
    size_t index = 0;
    unsigned long start = millis();

    memset(response, '\0', RESPONSE_SIZE);

    while (millis() - start < 1000)
    {
        if (sdiInterface.available())
        {
            char c = sdiInterface.read();

            if (c == '\n')
            {
                response[index] = '\0';
                return;
            }

            if (c == '\r')
            {
                continue;
            }

            // Reserve one byte for the null terminator.
            if (index >= RESPONSE_SIZE - 1)
            {
                // response[index++] = c;
                response[0] = '\0';
                WARNING(F("SDI-12 response exceeded buffer capacity."));
                return;
            }

            response[index++] = c;
        }
    }
    // Discard partial data if the response did not finish in time.
    response[0] = '\0';
    WARNING(F("SDI-12 response timed out."));
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::requestSensorInfo(char response[RESPONSE_SIZE], char addr)
{
    sendCommand(response, addr, "I!");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SDI12::getData(char addr)
{
    // char buf[20];
    // char *p;
    if (addr != '0' && addr != '1' &&
        addr != '2' && addr != '3')
    {
        ERROR(F("Unsupported SDI-12 address"));
        return;
    }

    char response[RESPONSE_SIZE];
    char response2[RESPONSE_SIZE];
    float valuesD0[4] = {};
    float valuesD1[4] = {};
    bool dataReady = false;
    char output[OUTPUT_SIZE];

    // Request a measurement from the sensor at the given address
    snprintf(output, OUTPUT_SIZE, "Requesting data from %s, this will take 5 seconds...", in_use_sensors[addr - '0'].name);
    LOG(output);
    for (int attempt = 0; attempt < 3; attempt++)
    {

        sendCommand(response, addr, "M!");
        LOG(response);
        delay(5000);

        // All sensors retrieve D0
        sendCommand(response, addr, "D0!");

        if (addr == '1')
        {
            // TEROS 54 additionally retrieves D1
            sendCommand(response2, addr, "D1!");

            bool validD0 = parseSignedSDI12(
                response, addr, valuesD0, 4);

            bool validD1 = parseSignedSDI12(
                response2, addr, valuesD1, 4);

            dataReady = validD0 && validD1;
        }
        else // TEROS 12, TEROS 21, or TEROS 11
        {
            // TEROS 12 returns three values.
            // TEROS 11 and TEROS 21 return two values.
            size_t expectedCount = 0;

            if (addr == '0') // TEROS 12
            {
                expectedCount = 3;
            }
            else if (addr == '2') // TEROS 21
            {
                expectedCount = 2;
            }
            else if (addr == '3') // TEROS 11
            {
                expectedCount = 2;
            }

            dataReady = parseSignedSDI12(
                response, addr, valuesD0, expectedCount);
        }

        if (dataReady)
            break;

        TIMER_RESET;

        if (attempt < 2)
            WARNING(F("Invalid SDI-12 data received! Retrying..."));
    }
    // sendCommand(response, addr, "M!");
    // LOG(response);
    // delay(5000);
    // if (!using_ter54)
    // {
    //     sendCommand(response, addr, "D0!");
    //     LOG(response);
    // }
    // else
    // {
    //     sendCommand(response, addr, "D1!");
    // }

    // // If the value returned was 0 we want to re-request data
    // if (strlen(response) <= 1)
    // {
    //     WARNING(F("Invalid data received! Retrying..."));

    //     // Request a measurement from the sensor at the given address
    //     sendCommand(response, addr, "M!");
    //     LOG(response);
    //     delay(5000);
    //     if (!using_ter54)
    //     {
    //         sendCommand(response, addr, "D0!");
    //     }
    //     else
    //     {
    //         sendCommand(response, addr, "D1!");
    //     }

    //     TIMER_RESET;
    //     if (strlen(response) <= 1)
    //     {
    //         WARNING(F("Retrying for a second time..."));

    //         // Request a measurement from the sensor at the given address
    //         sendCommand(response, addr, "M!");
    //         LOG(response);
    //         delay(5000);
    //         if (!using_ter54)
    //         {
    //             sendCommand(response, addr, "D0!");
    //         }
    //         else
    //         {
    //             sendCommand(response, addr, "D0!");
    //             sendCommand(response, addr, "D1!");
    //         }
    //         TIMER_RESET;
    //     }
    // }

    // Check if there is actually data to store in the variables
    // Stop if the required data was not successfully retrieved.
    if (!dataReady)
    {
        ERROR(F("Failed to record new data! Using previous valid information!"));
        return;
    }

    // TEROS 11
    if (addr == '3')
    {
        in_use_sensors[3].measurements[1] = valuesD0[0];
        in_use_sensors[3].measurements[0] = valuesD0[1];
    }

    // TEROS 12
    if (addr == '0')
    {
        in_use_sensors[0].measurements[1] = valuesD0[0];
        in_use_sensors[0].measurements[0] = valuesD0[1];
        in_use_sensors[0].measurements[2] = valuesD0[2];
    }

    // TEROS 54
    if (addr == '1')
    {
        for (int i = 0; i < 4; i++)
        {
            in_use_sensors[1].measurements[i] = valuesD0[i];
            in_use_sensors[1].measurements[i + 4] = valuesD1[i];
        }
    }

    // TEROS 21
    if (addr == '2')
    {
        in_use_sensors[2].measurements[0] = valuesD0[0];
        in_use_sensors[2].measurements[1] = valuesD0[1];
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////