#include "../Interfaces/DatagramManagerInterface.h"

class MockManager : public DatagramManagerInterface {
public:
    bool initCalled = false;
    bool sendCalled = false;
    bool recvCalled = false;
    bool recvResult = true; // pretend we always receive a packet
    uint8_t lastFromAddress = 42;

    bool init() override {
        initCalled = true;
        return true; // pretend initialization always succeeds
    }

    void setRetries(uint8_t count) override {}
    void setTimeout(uint16_t timeout) override {}

    bool sendtoWait(uint8_t* data, size_t len, uint8_t addr) override {
        sendCalled = true;
        return true; // pretend send always succeeds
    }

    bool recvfromAck(uint8_t* buffer, uint8_t* len, uint8_t* from) override {
        recvCalled = true;
        *from = lastFromAddress;
        return recvResult; // control success/failure
    }

    bool recvfromAckTimeout(uint8_t* buffer, uint8_t* len, uint16_t timeout, uint8_t* from) override {
        return recvfromAck(buffer, len, from);
    }

    void setThisAddress(uint8_t addr) override {}
};