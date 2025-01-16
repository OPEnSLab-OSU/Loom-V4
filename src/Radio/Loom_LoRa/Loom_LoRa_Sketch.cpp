#include "Loom_LoRa_Sketch.h"
#include "Logger.h"
#include "Module.h"
#include <cstdio>

Loom_LoRa_Sketch::Loom_LoRa_Sketch(
    Manager& manager,
    const uint8_t address, 
    const uint8_t powerLevel, 
    const uint8_t retryCount, 
    const uint16_t retryTimeout
) : Module("LoRa"),
        manager(&manager), 
        radioDriver{RFM95_CS, RFM95_INT},
        deviceAddress(address),
        powerLevel(powerLevel),
        sendRetryCount(retryCount),
        receiveRetryCount(retryCount),
        retryTimeout(retryTimeout)
{
    this->radioManager = new RHReliableDatagram(radioDriver, this->deviceAddress);
    this->manager->registerModule(this);
}

Loom_LoRa_Sketch::Loom_LoRa_Sketch(
    Manager& manager,
    const uint8_t powerLevel, 
    const uint8_t retryCount, 
    const uint16_t retryTimeout
) : Module("LoRa"),
        manager(&manager), 
        radioDriver{RFM95_CS, RFM95_INT},
        deviceAddress(0),
        powerLevel(powerLevel),
        sendRetryCount(retryCount),
        receiveRetryCount(retryCount),
        retryTimeout(retryTimeout)
{
    this->deviceAddress = this->manager->get_instance_num();
    this->radioManager = new RHReliableDatagram(radioDriver, this->deviceAddress);
    this->manager->registerModule(this);
}

Loom_LoRa_Sketch::~Loom_LoRa_Sketch() {
    delete radioManager;
}

void Loom_LoRa_Sketch::initialize() {
    // Set CS pin as pull up
    pinMode(RFM95_CS, INPUT_PULLUP);
    
    // Reset the radio
    pinMode(RFM95_RST, OUTPUT);
    digitalWrite(RFM95_RST, HIGH);

    // Initialize the radio manager
    if (radioManager->init()) {
        LOG(F("Radio manager successfully initialized!"));

    } else {
        ERROR(F("Radio manager failed to initialize!"));
        moduleInitialized = false;
        return;
    }

    // Set the radio frequency
    if (radioDriver.setFrequency(RF95_FREQ)) {
        snprintf(logOutput, OUTPUT_SIZE, "Radio frequency successfully set to: %f", RF95_FREQ);
        LOG(logOutput);
    } else {
        ERROR(F("Failed to set frequency!"));
        moduleInitialized = false;
        return;
    }

    // Set radio power level
    snprintf(logOutput, OUTPUT_SIZE, "Setting device power level to: %i", powerLevel);
    LOG(logOutput);
    radioDriver.setTxPower(powerLevel, false);

    // Set timeout time
    snprintf(logOutput, OUTPUT_SIZE, "Timeout time set to: %i,", retryTimeout);
    LOG(logOutput);
    radioManager->setTimeout(retryTimeout);

    // Set retry attempts
    snprintf(logOutput, OUTPUT_SIZE, "Transmit retry count set to: %i", sendRetryCount);
    LOG(logOutput);
    radioManager->setRetries(sendRetryCount);

    // Print the set address of the device
    snprintf(logOutput, OUTPUT_SIZE, "Address set to: %i", radioManager->thisAddress());
    LOG(logOutput);
    
    // https://cdn.sparkfun.com/assets/a/e/7/e/b/RFM95_96_97_98W.pdf, Page 22

    // Set bandwidth
    radioDriver.setSignalBandwidth(125000);

    // Higher spreading factors give us more range
    radioDriver.setSpreadingFactor(7); 

    // Coding rate should be 4/5
	radioDriver.setCodingRate4(5);	
	radioDriver.sleep();
}

void Loom_LoRa_Sketch::power_up() {
    if (poweredUp) {
        radioDriver.available();
    }
}

void Loom_LoRa_Sketch::power_down() {
    if (poweredUp) {
        radioDriver.sleep();
    }
}

void Loom_LoRa_Sketch::package() {
    if (!moduleInitialized) {
        return;
    }

    JsonObject json = manager->get_data_object(getModuleName());
    json["RSSI"] = signalStrength;
}

bool Loom_LoRa_Sketch::receiveFromLoRa(uint8_t *buf, uint8_t buf_size, uint timeout, uint8_t *fromAddress) {
    bool status = true;

    memset(buf, 0, buf_size);

    LOG(F("Waiting for message..."));

    if (timeout) {
        status = radioManager->recvfromAckTimeout(buf, &buf_size, timeout, fromAddress);
    } else {
        status = radioManager->recvfromAck(buf, &buf_size, fromAddress);
    }

    if (!status) {
        WARNING(F("No message received"));
    }

    radioDriver.sleep();
    return status;
}

bool Loom_LoRa_Sketch::receiveFrag(uint timeout, bool *isReady) {
    if (!moduleInitialized) {
        ERROR(F("LoRa module not initialized!"));
        return false;
    } 

    uint8_t buf[MAX_MESSAGE_LENGTH] = {}; // TODO(rwheary): don't use alloca
    uint8_t bufLen = sizeof(buf);
    uint8_t fromAddress;

    bool recvStatus = receiveFromLoRa(buf, bufLen, timeout, &fromAddress);
    if (!recvStatus) {
        return false;
    }

    snprintf(logOutput, OUTPUT_SIZE, "Received packet from %i", fromAddress);
    LOG(logOutput);

    StaticJsonDocument<300> tempDoc;

    // cast buf to const to avoid mutation
    auto err = deserializeMsgPack(tempDoc, (const char *) buf, bufLen);
    if (err != DeserializationError::Ok) {
        snprintf(logOutput, OUTPUT_SIZE, "Error occurred parsing MsgPack: %s", err.c_str());
        ERROR(logOutput);
        return false;
    }

    if (tempDoc.containsKey("numPackets")) {
        *isReady = handleFragHeader(tempDoc, fromAddress);

    } else if (frags.find(fromAddress) != frags.end()) {
        *isReady = handleFragBody(tempDoc, fromAddress);
    
    } else if (tempDoc.containsKey("module")) {
        *isReady = handleLostFrag(tempDoc, fromAddress);

    } else {
        *isReady = handleSingleFrag(tempDoc);
    }

    return true;
}

bool Loom_LoRa_Sketch::handleFragHeader(JsonDocument &workingDoc, uint8_t fromAddress) {
    int expectedFragCount = workingDoc["numPackets"].as<int>();
    workingDoc.remove("numPackets");

    int packetSpace = 300 * (expectedFragCount + 1);

    if (frags.find(fromAddress) != frags.end()) {
        snprintf(logOutput, OUTPUT_SIZE, "Dropping corrupted packet received from %i", fromAddress);
        WARNING(logOutput);

        frags.erase(fromAddress);
    }

    // this should never fail
    auto inserted = frags.emplace(std::make_pair(
        fromAddress,
        PartialPacket { expectedFragCount, DynamicJsonDocument(packetSpace) }));

    // TODO(rwheary): does this copy data over to the correctly sized buffer??
    inserted.first->second.working = workingDoc;

    return false;
}

bool Loom_LoRa_Sketch::handleFragBody(JsonDocument &workingDoc, uint8_t fromAddress) {
    PartialPacket *partialPacket = &frags.find(fromAddress)->second;

    JsonArray contents = partialPacket->working["contents"].as<JsonArray>();
    contents.add(workingDoc);

    partialPacket->remainingFragments--;

    snprintf(logOutput, OUTPUT_SIZE, "Received frag body from %i. Remaining fragments: %i", fromAddress, partialPacket->remainingFragments);
    LOG(logOutput);

    if (partialPacket->remainingFragments == 0) { 
        manager->getDocument() = std::move(partialPacket->working);
        frags.erase(fromAddress);

        return true;
    }

    return false;
}

bool Loom_LoRa_Sketch::handleSingleFrag(JsonDocument &workingDoc) {
    // TODO(rwheary): does this copy data over to the correctly sized buffer??
    manager->getDocument() = workingDoc;
    
    return true;
}

bool Loom_LoRa_Sketch::handleLostFrag(JsonDocument &workingDoc, uint8_t fromAddress) {
    snprintf(logOutput, OUTPUT_SIZE, "Dropping fragmented packet body with no header received from %i", fromAddress);
    WARNING(logOutput);

    return false;
}

bool Loom_LoRa_Sketch::receive(uint timeout) {
    // TODO(rwheary): i think this like, sucks? But if we assume interleaved
    // fragments we lose nice properties like knowing how many packets to expect.
    // I think this should work but it needs testing.
    bool documentIsReady = false;

    int retryCount = receiveRetryCount;
    while (retryCount > 0) {
        bool status = receiveFrag(timeout, &documentIsReady);

        if (documentIsReady) {
            return true;
        }

        if (!status) {
            retryCount--;
        }
    }

    return false;
}

