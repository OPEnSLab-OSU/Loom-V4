#include <cstdint>
#include <cstddef>

class DatagramManagerInterface {
public:
    virtual bool init() = 0;
    virtual void setRetries(uint8_t count) = 0;
    virtual void setTimeout(uint16_t timeout) = 0;
    virtual bool sendtoWait(uint8_t* data, size_t len, uint8_t address) = 0;
    virtual bool recvfromAckTimeout(uint8_t* buffer, uint8_t* len, uint16_t timeout, uint8_t* from) = 0;
    virtual bool recvfromAck(uint8_t* buffer, uint8_t* len, uint8_t* from) = 0;
    virtual void setThisAddress(uint8_t addr) = 0;
    virtual ~DatagramManagerInterface() {}
};