#include <cstdint>

class RadioDriverInterface {
public:
    virtual void sleep() = 0;
    virtual void available() = 0;
    virtual int16_t lastRssi() = 0;
    virtual ~RadioDriverInterface() {}
};