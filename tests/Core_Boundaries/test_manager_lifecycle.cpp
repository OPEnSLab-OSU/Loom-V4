// Deferred: compile the production Manager after compiler execution resumes.
// No initialize() call: it reads SAMD hardware serial registers, which these fakes do not model.
#include <cassert>
#define WATCHDOG_ENABLE
#include "Arduino.h"
#include "Adafruit_SleepyDog.h"
#if defined(_MSC_VER) && !defined(__clang__)
// Logger's GCC noinline annotation affects production stack frames, not this dispatch fixture.
#define __attribute__(attributes)
#endif
#include "Logger.h"
#if defined(_MSC_VER) && !defined(__clang__)
#undef __attribute__
#endif

// Keep the real Manager/Module/JSON/watchdog code. Only logging is suppressed in this fixture.
#undef FUNCTION_START
#undef FUNCTION_START_OBJECT
#undef FUNCTION_END
#undef LOG
#undef LOGF
#undef WARNING
#undef WARNINGF
#undef ERROR
#undef ERRORF
#define FUNCTION_START FunctionInstrumentationContext(__FILE__, __func__, __LINE__, nullptr)
#define FUNCTION_START_OBJECT(object)
#define FUNCTION_END
#define LOG(...)
#define LOGF(...)
#define WARNING(...)
#define WARNINGF(...)
#define ERROR(...)
#define ERRORF(...)
#include "Loom_Manager.cpp"

FakeWatchdogRegisters registers;
FakeWatchdogRegisters *WDT = &registers;
FakeWatchdog Watchdog;
FakeSerial Serial;
uint32_t millis() { return 0; }
void Logger::logDocument(const DynamicJsonDocument &) {}

class TestModule : public Module {
  public:
    TestModule(const char *name, bool slowStartup = false)
        : Module(name), slowStartup(slowStartup) {}
    void initialize() override {}
    void measure() override {}
    void package() override { ++packages; }
    void power_up() override {
        guardAtWake = loomWatchdogIsEnabled();
        ++wakes;
        if (slowStartup) {
            LoomWatchdogPause pause;
            assert(!loomWatchdogIsEnabled());
        }
        assert(loomWatchdogIsEnabled() == guardAtWake);
    }
    void power_down() override { ++shutdowns; }
    bool canRemovePower() const override { return safeToRemove; }
    bool slowStartup, guardAtWake = false, safeToRemove = true;
    unsigned int wakes = 0, packages = 0, shutdowns = 0;
};

struct HealthEvents {
    unsigned int calls = 0;
    Manager::HealthEvent last = Manager::HealthEvent::BeforeInitialize;
};
void observeHealth(Manager &manager, Manager::HealthEvent event, void *context) {
    auto &events = *static_cast<HealthEvents *>(context);
    ++events.calls;
    events.last = event;
    if (event == Manager::HealthEvent::Packaged) {
        assert(manager.getDocument()["contents"].size() > 0); // Callback sees completed packaging.
    }
}

int main() {
    TestModule modem("LTE", true), sensor("sensor");
    Manager manager("lifecycle", 1);
    HealthEvents health;
    manager.setHealthObserver(observeHealth, &health);
    manager.registerModule(&modem);
    manager.registerModule(&sensor);
    manager.registerModule(&modem); // One object must still receive exactly one call per cycle.
    manager.registerModule(nullptr);
    assert(std::strcmp(modem.getModuleName(), "LTE") == 0);
    manager.package();
    assert(health.calls == 1 && health.last == Manager::HealthEvent::Packaged);
    assert(modem.packages == 1 && sensor.packages == 1);
    assert(manager.getDocument()["id"]["name"] == "lifecycle");

    manager.getDocument().clear(); // Receiving a packet can replace every old JSON view.
    manager.getDocument()["id"]["name"] = "remote";
    manager.getDocument().createNestedArray("contents");
    manager.addData("receiver", "RSSI", -70);
    assert(manager.getDocument()["id"]["name"] == "remote");
    assert(manager.getDocument()["contents"].size() == 1);
    assert(manager.getDocument()["contents"][0]["module"] == "receiver");
    assert(manager.getDocument()["contents"][0]["data"]["RSSI"] == -70);
    manager.addData("receiver", "samples", 2);
    assert(manager.getDocument()["contents"].size() == 1);
    manager.getDocument().clear();
    manager.addData("fresh", "zero", 0);
    assert(manager.getDocument()["contents"][0]["module"] == "fresh");
    assert(manager.getDocument()["contents"][0]["data"]["zero"] == 0);

    manager.power_up();
    assert(health.calls == 2 && health.last == Manager::HealthEvent::PoweredUp);
    assert(modem.wakes == 1 && sensor.wakes == 1);
    assert(modem.guardAtWake && sensor.guardAtWake); // LTE pause cannot unprotect the next sensor.
    assert(!loomWatchdogIsEnabled()); // Preserve the legacy compile-time timer policy.

    const unsigned int before = Watchdog.enables;
    manager.power_up(2000);
    assert(Watchdog.enables == before + 1); // Driver restores configuration, not enable(default).
    assert(Watchdog.requestedMs == 2000 && loomWatchdogIsEnabled());
    assert(modem.guardAtWake && sensor.guardAtWake);

    modem.setModuleName("field modem");
    manager.power_up(2000);
    assert(modem.wakes == 3 && sensor.wakes == 3);
    assert(modem.guardAtWake && sensor.guardAtWake); // Display labels never select watchdog policy.

    modem.moduleInitialized = false;
    modem.safeToRemove = false; // Failed initialization does not prove hardware is off.
    assert(!manager.canRemovePower());
    manager.power_down();
    assert(modem.shutdowns == 1 && sensor.shutdowns == 1);
    modem.safeToRemove = true;
    assert(manager.canRemovePower());
    manager.power_down();
    assert(modem.shutdowns == 1 &&
           sensor.shutdowns == 2); // Unavailable, safe module stays skipped.
    assert(health.last == Manager::HealthEvent::PoweredDown);
    const auto observed = health.calls;
    manager.setHealthObserver(nullptr);
    manager.package();
    assert(health.calls == observed); // Clearing the borrowed callback stops diagnostics.
}
