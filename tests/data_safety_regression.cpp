// Host-side fault injection: uses production helpers and the installed ArduinoJson headers.
#include "Utilities/Loom_SDUtils.h"
#include "Utilities/Loom_JsonUtils.h"
#include <ArduinoJson.h>
#include <assert.h>
#include <stdio.h>
#include <string>
#include <vector>

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
    assert(loomSD::isHistoricalBatch("Deploy_1-Batch.txt", "Deploy_", 2));
    assert(loomSD::isHistoricalBatch("DEPLOY_0001-BATCH.TXT", "Deploy_", 2));
    assert(!loomSD::isHistoricalBatch("Deploy_2-Batch.txt", "Deploy_", 2));
    assert(!loomSD::isHistoricalBatch("Deploy_9-Batch.txt", "Deploy_", 2));
    assert(!loomSD::isHistoricalBatch("Deploy_1.csv", "Deploy_", 2));
    assert(!loomSD::isHistoricalBatch("OtherDeploy_1-Batch.txt", "Deploy_", 2));
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
    std::string quotes(1050, '"');
    doc["text"] = quotes.c_str();
    assert(loomJsonIsComplete(doc));
    assert(doc.memoryUsage() < 2000);
    assert(measureJson(doc) >= 2000);
    assert(!loomJsonFitsWire(doc, 2000));
    doc.clear();
    doc["text"] = "ok";
    assert(loomJsonFitsWire(doc, measureJson(doc) + 1));
    assert(!loomJsonFitsWire(doc, measureJson(doc)));
    puts("PASS JSON overflow rejection, next-packet recovery, and pretty-output equivalence");
}

struct FaultOutput {
    std::string bytes = "old\r\n";
    size_t budget = SIZE_MAX;
    bool writeError = false, truncateOK = true, closeOK = true;
    bool failFirstSync = false, failAllSync = false;
    int syncs = 0, closes = 0;
    uint32_t fileSize() const { return static_cast<uint32_t>(bytes.size()); }
    void clearWriteError() { writeError = false; }
    bool getWriteError() const { return writeError; }
    size_t write(uint8_t value) { return write(&value, 1); }
    size_t write(const uint8_t *data, size_t length) {
        const size_t count = length < budget ? length : budget;
        bytes.append(reinterpret_cast<const char *>(data), count);
        budget -= count;
        writeError = writeError || count != length;
        return count;
    }
    size_t println() { return write(reinterpret_cast<const uint8_t *>("\r\n"), 2); }
    bool sync() { ++syncs; return !failAllSync && !(failFirstSync && syncs == 1); }
    bool truncate(uint32_t length) {
        if (truncateOK)
            bytes.resize(length);
        return truncateOK;
    }
    bool close() { ++closes; return closeOK; }
};

void testBatchCommit() {
    DynamicJsonDocument doc(256);
    doc["value"] = 42;
    const size_t recordSize = measureJson(doc) + 2;
    for (size_t budget = 0; budget <= recordSize; ++budget) {
        FaultOutput file;
        file.budget = budget;
        const auto result = loomSD::appendRecord(file, doc);
        assert(file.closes == 1);
        if (budget < recordSize) {
            assert(result.status == SDWriteStatus::Failed && !result.committed);
            assert(file.bytes == "old\r\n");
        } else {
            assert(result.status == SDWriteStatus::Saved && result.committed);
            assert(file.bytes == "old\r\n{\"value\":42}\r\n");
        }
    }
    FaultOutput flushFailure;
    flushFailure.failFirstSync = true;
    assert(loomSD::appendRecord(flushFailure, doc).status == SDWriteStatus::Failed);
    assert(flushFailure.bytes == "old\r\n");
    FaultOutput rollbackFailure;
    rollbackFailure.budget = 3;
    rollbackFailure.truncateOK = false;
    assert(loomSD::appendRecord(rollbackFailure, doc).status == SDWriteStatus::Uncertain);
    FaultOutput syncFailure;
    syncFailure.failAllSync = true;
    assert(loomSD::appendRecord(syncFailure, doc).status == SDWriteStatus::Uncertain);
    FaultOutput closeFailure;
    closeFailure.closeOK = false;
    const auto uncertainCommit = loomSD::appendRecord(closeFailure, doc);
    assert(uncertainCommit.status == SDWriteStatus::Uncertain && uncertainCommit.committed);

    SDLogResult result;
    result.csv = SDWriteStatus::Saved;
    result.batch = SDWriteStatus::Failed;
    assert(loomSD::canRetryBatch(result, 4, 4));
    assert(!loomSD::canRetryBatch(result, 4, 5));
    for (auto status : {SDWriteStatus::Saved, SDWriteStatus::Rejected, SDWriteStatus::Uncertain,
                       SDWriteStatus::NotAttempted}) {
        result.batch = status;
        assert(!loomSD::canRetryBatch(result, 4, 4));
    }
    result.csv = SDWriteStatus::Failed;
    result.batch = SDWriteStatus::Failed;
    assert(!loomSD::canRetryBatch(result, 4, 4));
    puts("PASS batch commit/rollback/close faults and same-sample batch-only retry eligibility");
}

void testRecoveryCount() {
    int count = 0;
    FaultFile oldBatch("{\"value\":1}\r\n{\"value\":2}\r\n");
    assert(loomSD::countRecords(oldBatch, 2000, count) && count == 2);
    assert(oldBatch.data == "{\"value\":1}\r\n{\"value\":2}\r\n");
    FaultFile partial("{\"value\":1}\r\n{\"val");
    assert(!loomSD::countRecords(partial, 2000, count));
    FaultFile noNewline("{\"value\":1}");
    assert(!loomSD::countRecords(noNewline, 2000, count));
    FaultFile tooLong("12345\r\n");
    assert(!loomSD::countRecords(tooLong, 5, count));
    FaultFile failedRead("{\"value\":1}\r\n");
    failedRead.failAt = 5;
    assert(!loomSD::countRecords(failedRead, 2000, count));
    FaultFile empty("");
    assert(loomSD::countRecords(empty, 2000, count) && count == 0);
    puts("PASS recovery recount, empty batch, read errors, and non-destructive torn-tail rejection");
}

struct StoredBatch {
    std::string name, data;
    bool directory = false;
    size_t failAt = SIZE_MAX;
    StoredBatch(const char *fileName, const char *contents) : name(fileName), data(contents) {}
};
struct FakeDirectory {
    std::vector<StoredBatch> entries;
    uint32_t position = 0;
    bool error = false;
    uint32_t curPosition() const { return position; }
    bool getError() const { return error; }
};
struct DirectoryFile {
    StoredBatch *entry = nullptr;
    size_t position = 0;
    bool error = false, opened = false;
    bool openNext(FakeDirectory *directory) {
        assert(!opened);
        if (directory->position >= directory->entries.size())
            return false;
        entry = &directory->entries[directory->position++];
        position = 0;
        error = false;
        opened = true;
        return true;
    }
    bool getName(char *name, size_t capacity) {
        if (entry->name.size() >= capacity)
            return false;
        memcpy(name, entry->name.c_str(), entry->name.size() + 1);
        return true;
    }
    bool isDirectory() const { return entry->directory; }
    size_t fileSize() const { return entry->data.size(); }
    bool available() const { return position < entry->data.size(); }
    uint32_t curPosition() const { return static_cast<uint32_t>(position); }
    bool getError() const { return error; }
    int read() {
        if (position == entry->failAt || !available()) {
            error = true;
            return -1;
        }
        return static_cast<unsigned char>(entry->data[position++]);
    }
    bool close() { assert(opened); opened = false; return true; }
};

void testRecoveryDirectory() {
    FakeDirectory directory;
    directory.entries = {
        {"Wisp_3-Batch.txt", "{\"torn\":"},
        {"Wisp_0-Batch.txt", ""},
        {"Wisp_5-Batch.txt", "{\"current\":1}\r\n"},
        {"Other_1-Batch.txt", "{\"other\":1}\r\n"},
        {"Wisp_1-Batch.txt", "{\"old\":1}\r\n"},
        {"Wisp_4-Batch.txt", "{\"old\":2}\r\n{\"old\":3}\r\n"}
    };
    const auto original = directory.entries;
    DirectoryFile file;
    uint32_t cursor = 0;
    char name[84] = {};
    int count = 0, rejected = 0;
    const auto reject = [&](const char *) { ++rejected; };
    assert(loomSD::scanRecoveryFiles(directory, file, "Wisp_", 5, cursor, name, count,
                                     2000, nullptr, reject) == loomSD::RecoveryResult::Selected);
    assert(strcmp(name, "Wisp_1-Batch.txt") == 0 && count == 1 && cursor == 5);
    assert(rejected == 1 && !file.opened);
    // Simulate a successful clear: resume after this file; do not mix in the active session.
    name[0] = '\0';
    directory.position = cursor;
    directory.entries[5].failAt = 4;
    assert(loomSD::scanRecoveryFiles(directory, file, "Wisp_", 5, cursor, name, count,
                                     2000, nullptr, reject) == loomSD::RecoveryResult::ReadError);
    assert(cursor == 5 && name[0] == '\0' && !file.opened);
    // Next wake retries the failed entry, not the following directory entry.
    directory.position = cursor;
    directory.entries[5].failAt = SIZE_MAX;
    assert(loomSD::scanRecoveryFiles(directory, file, "Wisp_", 5, cursor, name, count,
                                     2000, nullptr, reject) == loomSD::RecoveryResult::Selected);
    assert(strcmp(name, "Wisp_4-Batch.txt") == 0 && count == 2);
    directory.position = cursor;
    assert(loomSD::scanRecoveryFiles(directory, file, "Wisp_", 5, cursor, name, count,
                                     2000, nullptr, reject) == loomSD::RecoveryResult::Done);
    for (size_t i = 0; i < original.size(); ++i)
        assert(original[i].data == directory.entries[i].data);
    puts("PASS multi-file recovery, active-session exclusion, read-error resume, preserved data");
}

int main() {
    testRecords();
    testNames();
    testClose();
    testJson();
    testBatchCommit();
    testRecoveryCount();
    testRecoveryDirectory();
    puts("All data-safety helper tests passed.");
}
