#include "../../Test_Components/Mocks/RadioDriverMock.cpp"
#include "../../Test_Components/Mocks/DatagramManagerMock.cpp"

#include "../../../src/Loom_Manager.h"
#include "../../../src/Radio/Loom_Freewave/Loom_Freewave.h"

#include <cassert>

MockDriver mockDriver;
MockManager mockManager;

Manager manager("Device", 1);

Loom_Freewave fw(
    manager,
    &mockDriver,
    &mockManager
);

void testCreation() 
{
    assert(fw); // a freewave object was created
}

void testDestruction()
{
    Loom_Freewave* tempFw = new Loom_Freewave(
        manager,
        &mockDriver,
        &mockManager
    );

    assert(tempFw); // object created

    delete tempFw; // ensure no memory leaks
}

// Test initialization success
void testInitialize()
{
    fw.initialize();
    assert(mockManager.initCalled == true); 
    assert(mockManager.timeoutSet == 1000);    // retry timeout set correctly
    assert(mockManager.retriesSet == 3);       // retry count set correctly

    assert(mockDriver.initCalled == true); 
}

// Test packaging of data
void testPackage()
{
    fw.initialize();
    fw.package();


    // FROM HERE DOWN IS AI NOT ME. NEED TO VALIDATE
    assert(mockManager.getDataObjectCalled == true); // ensure Freewave fetched JSON
}

void testSetAddress()
{
    std::cout << "Running testSetAddress...\n";
    fw.setAddress(42);

    assert(mockManager.thisAddressSet == 42); // Freewave updated manager address
    assert(mockDriver.sleepCalled == true);   // driver went to sleep
}

void testPowerUpDown()
{
    std::cout << "Running testPowerUpDown...\n";
    fw.power_up();
    assert(mockDriver.availableCalled == true);

    fw.power_down();
    assert(mockDriver.sleepCalled == true);
}

void testSend()
{
    std::cout << "Running testSend...\n";
    // Setup: mock JSON conversion always succeeds
    mockManager.jsonToBufferSuccess = true;
    mockManager.sendSuccess = true;

    bool result = fw.send(10);
    assert(result == true);
    assert(mockManager.sendCalled == true);
    assert(mockDriver.sleepCalled == true);
}

void testReceive()
{
    std::cout << "Running testReceive...\n";
    // Setup: mock receive returns success
    mockManager.recvSuccess = true;
    mockManager.recvData = "mock";

    bool result = fw.receive(0);
    assert(result == true);
    assert(mockDriver.sleepCalled == true);
}

int main()
{
    testCreation();
    testDestruction();
    testInitialize();
    testPackage();
    testSetAddress();
    testPowerUpDown();
    testSend();
    testReceive();

    std::cout << "All Loom_Freewave tests passed!\n";
    return 0;
}