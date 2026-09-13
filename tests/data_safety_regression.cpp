// Host-side fault injection: uses production helpers and the installed ArduinoJson headers.
#include "Utilities/Loom_SDUtils.h"
#include "Utilities/Loom_JsonUtils.h"
#include <ArduinoJson.h>
#include <assert.h>
#include <stdio.h>
#include <string>

struct FaultFile {
    std::string data;
    size_t position = 0;
    size_t failAt = SIZE_MAX;
    size_t reads = 0;
    bool closeOK = true;
    int closes = 0;
    explicit FaultFile(const char *text) : data(text) {}
    bool available() const { return position < data.size(); }
    uint32_t curPosition() const { return static_cast<uint32_t>(position); }
    int read() {
        ++reads;
        // A regression must fail promptly instead of hanging the test runner.
        assert(reads <= data.size() + 1);
        if (position == failAt || !available())
            return -1;
        return static_cast<unsigned char>(data[position++]);
    }
    bool close() { ++closes; return closeOK; }
};

void testRecords() {
    using loomSD::RecordResult;
    uint32_t start = 0;
    size_t length = 0;
    FaultFile records("\r\n{\"a\":1}\r\n{\"b\":2}\n{\"c\":3}");
    for (int i = 0; i < 3; ++i) {
        assert(loomSD::nextRecord(records, start, length, 2000) == RecordResult::Ready);
        assert(length == 7);
        assert(records.data.substr(start, length)[0] == '{');
    }
    assert(loomSD::nextRecord(records, start, length, 2000) == RecordResult::End);
    FaultFile blanks("\r\n\n\r");
    assert(loomSD::nextRecord(blanks, start, length, 2000) == RecordResult::End);
    FaultFile empty("");
    assert(loomSD::nextRecord(empty, start, length, 2000) == RecordResult::End);

    const char *text = "\r\n{\"a\":1}\r\n";
    for (size_t failure = 0; failure < std::string(text).size(); ++failure) {
        FaultFile broken(text);
        broken.failAt = failure;
        RecordResult result;
        do {
            result = loomSD::nextRecord(broken, start, length, 2000);
        } while (result == RecordResult::Ready);
        assert(result == RecordResult::ReadError);
        assert(broken.position == failure);
    }
    FaultFile fits("1234\n");
    assert(loomSD::nextRecord(fits, start, length, 5) == RecordResult::Ready);
    FaultFile tooLong("12345more-without-newline");
    assert(loomSD::nextRecord(tooLong, start, length, 5) == RecordResult::TooLong);
    assert(tooLong.reads == 5); // Bounded failure, not a drain of an arbitrary-sized file.
    puts("PASS batch delimiters, EOF, every read-error offset, and size boundary");
}

void testNames() {
    int next = 0;
    assert(loomSD::advanceLogNumber("Deploy_1.csv", "Deploy_", next));
    assert(loomSD::advanceLogNumber("Deploy_1-Batch.txt", "Deploy_", next));
    assert(next == 2); // Pair 0 deleted: never reopen pair 1.
    assert(loomSD::advanceLogNumber("Deploy_7-Batch.txt", "Deploy_", next));
    assert(next == 8); // Orphaned batch.
    assert(loomSD::advanceLogNumber("dEpLoY_12.CSV", "Deploy_", next));
    assert(next == 13); // FAT is case-insensitive.
    assert(loomSD::advanceLogNumber("Deploy_00015.csv", "Deploy_", next));
    assert(next == 16);
    const char *unrelated[] = {"OtherDeploy_99.csv", "Deploy_99.csv.backup", "Deploy_.csv",
        "Deploy_x99.csv", "Deploy_999999999999999999.json", "Deploy_", "", "De"};
    for (const char *name : unrelated) {
        assert(loomSD::advanceLogNumber(name, "Deploy_", next));
        assert(next == 16);
    }
    assert(!loomSD::advanceLogNumber("Deploy_2147483647.csv", "Deploy_", next));
    assert(!loomSD::advanceLogNumber("Deploy_999999999999999999.csv", "Deploy_", next));
    assert(next == 16);
    puts("PASS sparse names, orphaned pairs, case, exact suffixes, and counter overflow");
}

void testClose() {
    for (int wroteAll = 0; wroteAll < 2; ++wroteAll) {
        for (int closed = 0; closed < 2; ++closed) {
            FaultFile file("");
            file.closeOK = closed != 0;
            assert(loomSD::closeAfterWrite(file, wroteAll != 0) == (wroteAll && closed));
            assert(file.closes == 1);
        }
    }
    puts("PASS close failure cannot report success; failed writes still close");
}

void testJson() {
    DynamicJsonDocument doc(2000);
    assert(!loomJsonIsComplete(doc));
    doc["id"]["name"] = "WISP";
    doc["contents"][0]["module"] = "SEN66_5";
    doc["contents"][0]["data"]["PM2.5"] = 1.25;
    assert(loomJsonIsComplete(doc));

    char legacy[2000];
    const size_t expected = serializeJsonPretty(doc, legacy, sizeof(legacy));
    std::string streamed;
    assert(serializeJsonPretty(doc, streamed) == expected);
    assert(streamed == legacy);
    assert(measureJsonPretty(doc) == expected);

    // Additions after package() must invalidate the same document at the output boundary.
    JsonArray extra = doc.createNestedArray("extra");
    for (int i = 0; i < 1000 && !doc.overflowed(); ++i)
        extra.add(i);
    assert(doc.overflowed());
    assert(!loomJsonIsComplete(doc));
    doc.clear();
    doc["id"]["name"] = "WISP";
    assert(loomJsonIsComplete(doc)); // The next good sample is not permanently blocked.
    puts("PASS JSON overflow rejection, next-packet recovery, and pretty-output equivalence");
}

int main() {
    testRecords();
    testNames();
    testClose();
    testJson();
    puts("All data-safety helper tests passed.");
}
