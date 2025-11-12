#include "../Interfaces/RadioDriverInterface.h"

class MockDriver : public RadioDriverInterface {
public:
    bool sleepCalled = false;
    int16_t fakeRssi = -50;

    void sleep() override {
        sleepCalled = true;  // just record that sleep was called
    }

    void available() override {
        // maybe record this or do nothing
    }

    int16_t lastRssi() override {
        return fakeRssi;  // return a controlled RSSI for testing
    }
};