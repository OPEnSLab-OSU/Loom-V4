#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
using std::max;
#include "bench_sweep_fakes/Hardware/Loom_Hypnos/SDManager.h"
#include "Radio/Loom_LoRa/Benchmark/BenchRetry.h"

bool SDManager::available = true, SDManager::failWrites = false;
std::map<std::string, std::string> SDManager::files;
MemoryOutput Serial;
uint32_t clockMs = 0;
uint32_t millis() { return clockMs; }

#define BENCH_ENABLE_DENDROMETER 1
namespace bench {
enum class Role { Hub, Node };
Role role = Role::Hub;
bool ready = true;
constexpr uint8_t SensorSample = 0, SyntheticSample = 1, MsgPack = 0x32;
constexpr uint8_t BeaconBytes = 13, BoardKeyBytes = 8;
uint32_t airtimeMs(uint8_t) { return 52; }
bool parseNumber(const char *text, uint32_t low, uint32_t high, uint32_t &value) {
    char *end;
    value = static_cast<uint32_t>(strtoul(text, &end, 10));
    return *text && !*end && value >= low && value <= high;
}
struct LinkSettings { uint8_t mode; };
struct Schedule { uint8_t nodes, chunk, fragments; uint16_t leadMs; LinkSettings link; };
uint32_t slotMs(const Schedule &) { return 496; }
struct Radio { void setModeIdle() {} } radio;
namespace node {
struct Sensors { void setPower(bool) {} } dendrometer;
}
namespace hub {
constexpr Schedule DefaultPlan{2,245,1,120,{0}};
struct Settings { Schedule plan; uint16_t padBytes, gapMs; uint8_t format, source; bool upload; uint8_t retries;
    uint16_t sampleWindowMs; bool individual; uint32_t samplePeriodMs; };
struct NodeStats { uint32_t good, empty, skippedDeadlines, firstMissing, firstPartial, recovered; };
struct DataVolume { uint32_t populatedFields; } volume{};
Manager manager;
Settings config{DefaultPlan,0,0,MsgPack,SyntheticSample,false,MaxRetries,1000,false,0}, active;
NodeStats stats[MaxNodes];
uint32_t uploadPeriods[MaxNodes] = {};
uint32_t rounds = 0, beaconFailures = 0, retrySlots = 0, retryBeaconFailures = 0;
uint32_t runElapsed = 0, radioMs = 0, maxRoundMs = 0, maxPollFinishMs = 0;
bool running = false, compareJsonNext = false, enrollment = false, fitAllowed = true;
void printSettings(const Settings &settings, Print &output = Serial) {
    output.print("nodes="); output.println(settings.plan.nodes);
}
void printStats(Print &output = Serial) { output.print("polls="); output.println(rounds + beaconFailures); }
void deactivate() { running = false; }
void armRun(uint32_t) { active = config; running = true; }
bool fitSlots(uint32_t) {
    assert(!running);
    config.plan.chunk = 128;
    config.plan.fragments = 2;
    return fitAllowed;
}
void beginRun(const Settings &settings, uint32_t limit, bool periods) {
    assert(limit == 1000 && periods);
    active = settings;
    memset(stats, 0, sizeof(stats));
    volume = {};
    rounds = beaconFailures = retrySlots = retryBeaconFailures = 0;
    runElapsed = radioMs = maxRoundMs = maxPollFinishMs = 0;
    running = true;
}
}
}

#include "Radio/Loom_LoRa/Benchmark/BenchSweep.h"

using namespace bench;
void finishTest(bool good = true) {
    hub::running = false;
    hub::rounds = 1000;
    hub::runElapsed = 2000000;
    hub::maxPollFinishMs = hub::maxRoundMs = 1200;
    for (uint8_t i = 0; i < hub::active.plan.nodes; ++i) hub::stats[i].good = 1000;
    hub::volume.populatedFields = 1000 * hub::active.plan.nodes *
        (sweep::source == SensorSample ? 7 : 2);
    if (!good) --hub::volume.populatedFields;
    sweep::service();
}
void finishSizing() {
    hub::running = false;
    sweep::service();
    assert(sweep::stage == sweep::Stage::Measuring);
}
size_t lines(const std::string &text) {
    size_t count = 0;
    for (char c : text) count += c == '\n';
    return count;
}
int main() {
    SDManager::files["SW0001.CSV"] = "older-session";
    hub::uploadPeriods[0] = 300000;
    assert(sweep::start(72));
    assert(strcmp(sweep::csvPath, "SW0002.CSV") == 0);
    const std::string csv = sweep::csvPath, report = sweep::reportPath;
    for (unsigned test = 0; test < 2 * sweep::ProfileCount; ++test) {
        finishSizing();
        finishTest();
        assert(sweep::repetition == 2 && hub::running);
        finishTest();
    }
    assert(!sweep::isActive() && !hub::running);
    assert(lines(SDManager::files[csv]) == 1 + 4 * sweep::ProfileCount);
    assert(SDManager::files[report].find("suite_complete") != std::string::npos);
    assert(sweep::fastest[SensorSample].present && sweep::fewestRetries[SyntheticSample].present);
    assert(sweep::fastest[SensorSample].confirmedAtTest > 0);
    assert(hub::uploadPeriods[0] == 300000 && hub::config.plan.chunk == 245);

    hub::config.plan.nodes = 7;
    assert(sweep::start(8));
    finishSizing();
    finishTest();
    finishTest(false); // Complete radio record with a missing field cannot confirm a rate.
    assert(!sweep::fastest[SyntheticSample].present);
    sweep::stop("test_stop");

    assert(sweep::start(8));
    finishSizing();
    const std::string before = SDManager::files[sweep::csvPath];
    SDManager::failWrites = true;
    finishTest();
    assert(!sweep::isActive() && !hub::running);
    assert(SDManager::files[sweep::csvPath] == before);
    SDManager::failWrites = false;

    assert(sweep::start(1));
    finishSizing();
    clockMs += 3600000;
    sweep::service();
    assert(!sweep::isActive() && !hub::running);
    assert(SDManager::files[sweep::reportPath].find("status=time_limit") != std::string::npos);
    assert(hub::config.plan.nodes == 7 && hub::uploadPeriods[0] == 300000);

    SDManager::available = false;
    assert(!sweep::start(8) && !sweep::isActive());
    assert(sweep::start(8, false));
    finishSizing();
    finishTest();
    assert(Serial.content.find("SWEEP status=clean") != std::string::npos);
    sweep::stop("usb_test_stop");
    assert(!sweep::isActive());

    class ShortOutput : public Print {
      public:
        size_t write(uint8_t) override { return 0; }
    } shortOutput;
    assert(!sweep::writeReport(shortOutput, nullptr));
}
