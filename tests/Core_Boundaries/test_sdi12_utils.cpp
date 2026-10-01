// Deferred production-parser tests. No Arduino hardware or compiler has been run for this pass.
#include "../../src/Utilities/Loom_SDI12Utils.h"
#include <assert.h>
#include <initializer_list>

int main() {
    using namespace loomSDI12;
    assert(identifyModel("013METER   TER54 100123") == Model::TER54);
    assert(identifyModel("013METER   TER21 389123") == Model::TER21);
    assert(identifyModel("013DECAGON GS3   402123") == Model::GS3);
    assert(identifyModel("013METER   TER11 114123") == Model::TER11);
    assert(identifyModel("013METER   TER12 114123") == Model::TER12);
    assert(identifyModel("013METER   OTHER 100TER54") == Model::Unknown);
    assert(identifyModel("013METER") == Model::Unknown);
    assert(identifyModel(nullptr) == Model::Unknown);

    uint16_t seconds = 0;
    uint8_t values = 0;
    assert(parseMeasurementReply("00018", '0', seconds, values));
    assert(seconds == 1 && values == 8);
    assert(parseMeasurementReply("a0002", 'a', seconds, values));
    assert(seconds == 0 && values == 2);
    assert(parseMeasurementReply("Z9993", 'Z', seconds, values));
    assert(seconds == 999 && values == 3);
    for (const char *bad : {"", "0", "10018", "0a018", "00010", "00019", "000108", "00018x"}) {
        assert(!parseMeasurementReply(bad, '0', seconds, values));
    }
    assert(!parseMeasurementReply(nullptr, '0', seconds, values));

    float readings[MAX_VALUES] = {};
    size_t count = 0;
    assert(appendDataReply("a-12.4-4.25", 'a', readings, 2, count));
    assert(count == 2 && readings[0] < -12.39f && readings[1] == -4.25f);
    count = 0;
    assert(appendDataReply("0+0.12-2.5+0.25+3.5", '0', readings, 8, count));
    assert(count == 4);
    assert(appendDataReply("0+0.38-4.5+0.45+5.5", '0', readings, 8, count));
    assert(count == 8 && readings[5] == -4.5f && readings[7] == 5.5f);
    assert(!appendDataReply("0+1", '0', readings, 8, count)); // Never overrun the advertised count.
    for (const char *bad : {"", "0", "1+1", "0++1", "0+", "0+1.2.3", "0+1x", "0+1e2", "0+nan",
                            "0+inf", "0+1 +2", "0+1\r"}) {
        count = 0;
        assert(!appendDataReply(bad, '0', readings, 8, count));
    }
    count = 0;
    assert(appendDataReply("0+.5-.25+0-0", '0', readings, 4, count));
    assert(count == 4 && readings[0] == 0.5f && readings[1] == -0.25f);
    count = 0;
    assert(!appendDataReply("0+1", '0', nullptr, 8, count));
    return 0;
}
