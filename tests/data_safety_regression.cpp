// Host-side fault injection: uses production helpers and the installed ArduinoJson headers.
#include "Utilities/Loom_SDUtils.h"
#include "Utilities/Loom_JsonUtils.h"
#include "Utilities/Loom_LoRaJournalFormat.h"
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
        if (position == failAt || !available()) {
            return -1;
        }
        return static_cast<unsigned char>(data[position++]);
    }
    bool close() {
        ++closes;
        return closeOK;
    }
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

    FaultFile spaces(" \t");
    assert(loomSD::finishJsonRecord(spaces, 2));
    FaultFile garbage(" x");
    assert(!loomSD::finishJsonRecord(garbage, 2));
    FaultFile failedTail(" \t");
    failedTail.failAt = 1;
    assert(!loomSD::finishJsonRecord(failedTail, 2));
    FaultFile crossed("abc");
    crossed.position = 3;
    assert(!loomSD::finishJsonRecord(crossed, 2));
    assert(crossed.reads == 0);
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
    const char *unrelated[] = {"OtherDeploy_99.csv",
                               "Deploy_99.csv.backup",
                               "Deploy_.csv",
                               "Deploy_x99.csv",
                               "Deploy_999999999999999999.json",
                               "Deploy_",
                               "",
                               "De"};
    for (const char *name : unrelated) {
        assert(loomSD::advanceLogNumber(name, "Deploy_", next));
        assert(next == 16);
    }
    assert(!loomSD::advanceLogNumber("Deploy_2147483647.csv", "Deploy_", next));
    assert(!loomSD::advanceLogNumber("Deploy_999999999999999999.csv", "Deploy_", next));
    assert(next == 16);
    puts("PASS sparse names, orphaned pairs, case, exact suffixes, and counter overflow");
    int shared = 0;
    assert(loomSD::advanceLogNumber("Deploy_Test_0.csv", "Deploy_Test_", shared));
    assert(loomSD::advanceDebugNumber("output_0.log", shared));
    assert(shared == 1); // Migration past old independent numbering preserves both captures.
    assert(loomSD::advanceDebugNumber("FUNCSUMMARIES_12.LOG", shared));
    assert(shared == 13);
    assert(loomSD::advanceDebugNumber("trace_14.ndjson", shared));
    assert(shared == 15);
    assert(loomSD::advanceDebugNumber("output_.log", shared));
    assert(shared == 15);
    assert(!loomSD::advanceDebugNumber("output_999999999999999999.log", shared));
    assert(shared == 15);
}

void testDebugNames() {
    using loomDebugFiles::Kind;
    char path[loomDebugFiles::PATH_SIZE];
    assert(loomDebugFiles::buildPath(path, sizeof(path), "Deploy_Test_", Kind::Text, 3));
    assert(strcmp(path, "/debug/Deploy_Test_debug_3.log") == 0);
    assert(loomDebugFiles::buildPath(path, sizeof(path), "WispBench", Kind::Summaries, 3));
    assert(strcmp(path, "/debug/WispBench_funcSummaries_3.log") == 0);
    assert(loomDebugFiles::buildPath(path, sizeof(path), "../Bench / A_", Kind::Text, 3));
    assert(strcmp(path, "/debug/___Bench___A_debug_3.log") == 0);
    assert(loomDebugFiles::buildPath(path, sizeof(path), "", Kind::Text, 3));
    assert(strcmp(path, "/debug/device_debug_3.log") == 0);

    char maximum[loomDebugFiles::NAME_SIZE];
    memset(maximum, 'a', sizeof(maximum) - 1);
    maximum[sizeof(maximum) - 1] = '\0';
    assert(loomDebugFiles::buildPath(path, sizeof(path), maximum, Kind::Summaries, INT_MAX));
    assert(strlen(path) < sizeof(path));
    char tooLong[loomDebugFiles::NAME_SIZE + 1];
    memset(tooLong, 'a', sizeof(tooLong) - 1);
    tooLong[sizeof(tooLong) - 1] = '\0';
    assert(!loomDebugFiles::buildPath(path, sizeof(path), tooLong, Kind::Text, 3));
    assert(path[0] == '\0');
    char shortPath[8];
    assert(!loomDebugFiles::buildPath(shortPath, sizeof(shortPath), "Wisp", Kind::Text, 3));
    assert(shortPath[0] == '\0');
    assert(!loomDebugFiles::buildPath(path, sizeof(path), "Wisp", Kind::Text, -1));
    assert(path[0] == '\0');

    int next = 0;
    assert(loomSD::advanceDebugNumber("output_12.log", next, "Deploy_Test_"));
    assert(loomSD::advanceDebugNumber("Deploy_Test_debug_20.log", next, "Deploy_Test_"));
    assert(loomSD::advanceDebugNumber("DEPLOY_TEST_FUNCSUMMARIES_22.LOG", next, "Deploy_Test_"));
    assert(next == 23); // Orphan diagnostics must reserve a new shared session.
    assert(loomSD::advanceDebugNumber("Deploy_Test_trace_24.perfetto.json", next, "Deploy_Test_"));
    assert(next == 25);
    assert(loomSD::advanceDebugNumber("Deploy_Test_debug_.log", next, "Deploy_Test_"));
    assert(next == 25);
    assert(!loomSD::advanceDebugNumber("Deploy_Test_debug_2147483647.log", next, "Deploy_Test_"));
    puts("PASS Manager/custom diagnostic names, safe bounds, legacy migration and orphan sessions");
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
    for (int i = 0; i < 1000 && !doc.overflowed(); ++i) {
        extra.add(i);
    }
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
    bool sync() {
        ++syncs;
        return !failAllSync && !(failFirstSync && syncs == 1);
    }
    bool truncate(uint32_t length) {
        if (truncateOK) {
            bytes.resize(length);
        }
        return truncateOK;
    }
    bool close() {
        ++closes;
        return closeOK;
    }
};

struct JournalFile : FaultOutput {
    size_t position = 0;
    bool readFailed = false;
    explicit operator bool() const { return true; }
    bool seekSet(uint32_t offset) {
        position = offset;
        return offset <= bytes.size();
    }
    size_t read(uint8_t *destination, size_t length) {
        if (readFailed || length > bytes.size() - position) {
            return 0;
        }
        memcpy(destination, bytes.data() + position, length);
        position += length;
        return length;
    }
    bool getError() const { return readFailed; }
};

void testJournalRecovery() {
    using namespace loomLoRa::journal;
    uint8_t record[8], tail[8];
    write32(record, 123);
    write32(record + 4, checksum(record, 4));
    const std::string valid(reinterpret_cast<const char *>(record), sizeof(record));
    uint32_t end = 0;
    for (size_t torn = 0; torn < sizeof(record); ++torn) {
        JournalFile file;
        file.bytes = valid + valid.substr(0, torn);
        assert(loomSD::recoverFixedRecords(file, tail, sizeof(record), end, validEntry));
        assert(file.bytes == valid && end == sizeof(record) && file.closes == 1);
        assert(read32(tail) == 123);
    }
    JournalFile corrupt;
    corrupt.bytes = valid + valid;
    corrupt.bytes.back() ^= 1;
    const auto preserved = corrupt.bytes;
    assert(!loomSD::recoverFixedRecords(corrupt, tail, sizeof(record), end, validEntry));
    assert(corrupt.bytes == preserved && corrupt.closes == 1);
    puts("PASS LoRa journal partial-tail repair and complete-record preservation");
}

void testJournalCursorRecovery() {
    using namespace loomLoRa::journal;
    constexpr uint8_t queueRecordSize = 16;
    const auto checkpoint = [](uint32_t offset) {
        uint8_t record[CursorSize];
        write32(record, offset);
        write32(record + 4, checksum(record, 4));
        return std::string(reinterpret_cast<const char *>(record), sizeof(record));
    };
    const auto first = checkpoint(queueRecordSize);
    const auto second = checkpoint(2 * queueRecordSize);
    auto bad = second;
    bad.back() ^= 1;
    uint32_t cursor = 0;

    JournalFile complete;
    complete.bytes = first + second;
    assert(recoverCursor(complete, 32, queueRecordSize, cursor) && cursor == 32);
    assert(complete.bytes == first + second && complete.syncs == 0 && complete.closes == 1);

    for (const auto &final : {second.substr(0, 4), bad}) {
        JournalFile torn;
        torn.bytes = first + final;
        assert(recoverCursor(torn, 32, queueRecordSize, cursor) && cursor == 16);
        assert(torn.bytes == first && torn.syncs == 1 && torn.closes == 1);
    }

    // No usable recent checkpoint, an unaligned offset, or a cursor beyond a restored queue.
    for (const auto &entries : {first + bad + bad, first + checkpoint(17), first + second}) {
        JournalFile stale;
        stale.bytes = entries;
        assert(recoverCursor(stale, 16, queueRecordSize, cursor) && cursor == 0);
        assert(stale.bytes.empty() && stale.syncs == 1 && stale.closes == 1);
    }

    JournalFile unreadable;
    unreadable.bytes = first + bad;
    unreadable.readFailed = true;
    cursor = 99;
    assert(!recoverCursor(unreadable, 32, queueRecordSize, cursor));
    assert(cursor == 99 && unreadable.bytes == first + bad && unreadable.closes == 1);

    for (int failure = 0; failure < 3; ++failure) {
        JournalFile interrupted;
        interrupted.bytes = first + bad;
        interrupted.truncateOK = failure != 0;
        interrupted.failAllSync = failure == 1;
        interrupted.closeOK = failure != 2;
        assert(!recoverCursor(interrupted, 32, queueRecordSize, cursor));
        assert(cursor == 99 && interrupted.closes == 1);
    }
    puts("PASS LoRa cursor rollback, safe replay, and SD failure propagation");
}

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

// The ordinary text/pretty-JSON debug writers share this production finalizer with batches.
// Inject every partial-write position plus flush, rollback and close failures.
void testDebugAppend() {
    const char *record = "debug text\r\n";
    const size_t length = strlen(record);
    for (size_t budget = 0; budget <= length; ++budget) {
        FaultOutput file;
        const uint32_t start = file.fileSize();
        file.budget = budget;
        const bool wroteAll =
            file.write(reinterpret_cast<const uint8_t *>(record), length) == length;
        const auto result = loomSD::finishAppend(file, start, wroteAll);
        assert(file.closes == 1);
        assert(result.status == (wroteAll ? SDWriteStatus::Saved : SDWriteStatus::Failed));
        assert(file.bytes == (wroteAll ? "old\r\ndebug text\r\n" : "old\r\n"));
    }
    FaultOutput flush;
    const uint32_t start = flush.fileSize();
    flush.write(reinterpret_cast<const uint8_t *>(record), length);
    flush.failFirstSync = true;
    assert(loomSD::finishAppend(flush, start, true).status == SDWriteStatus::Failed);
    assert(flush.bytes == "old\r\n" && flush.closes == 1);
    for (int failure = 0; failure < 3; ++failure) {
        FaultOutput broken;
        const uint32_t savedSize = broken.fileSize();
        broken.write(reinterpret_cast<const uint8_t *>(record), 3);
        broken.truncateOK = failure != 0;
        broken.failAllSync = failure == 1;
        broken.closeOK = failure != 2;
        assert(loomSD::finishAppend(broken, savedSize, false).status == SDWriteStatus::Uncertain);
        assert(broken.closes == 1);
    }
    puts("PASS debug append rollback, flush and uncertain-close handling");
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
    puts(
        "PASS recovery recount, empty batch, read errors, and non-destructive torn-tail rejection");
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
        if (directory->position >= directory->entries.size()) {
            return false;
        }
        entry = &directory->entries[directory->position++];
        position = 0;
        error = false;
        opened = true;
        return true;
    }
    bool getName(char *name, size_t capacity) {
        if (entry->name.size() >= capacity) {
            return false;
        }
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
    bool close() {
        assert(opened);
        opened = false;
        return true;
    }
};

void testRecoveryDirectory() {
    FakeDirectory directory;
    directory.entries = {{"Wisp_3-Batch.txt", "{\"torn\":"},
                         {"Wisp_0-Batch.txt", ""},
                         {"Wisp_5-Batch.txt", "{\"current\":1}\r\n"},
                         {"Other_1-Batch.txt", "{\"other\":1}\r\n"},
                         {"Wisp_1-Batch.txt", "{\"old\":1}\r\n"},
                         {"Wisp_4-Batch.txt", "{\"old\":2}\r\n{\"old\":3}\r\n"}};
    const auto original = directory.entries;
    DirectoryFile file;
    uint32_t cursor = 0;
    char name[84] = {};
    int count = 0, rejected = 0;
    const auto reject = [&](const char *) { ++rejected; };
    assert(loomSD::scanRecoveryFiles(directory, file, "Wisp_", 5, cursor, name, count, 2000,
                                     nullptr, reject) == loomSD::RecoveryResult::Selected);
    assert(strcmp(name, "Wisp_1-Batch.txt") == 0 && count == 1 && cursor == 5);
    assert(rejected == 1 && !file.opened);
    // Simulate a successful clear: resume after this file; do not mix in the active session.
    name[0] = '\0';
    directory.position = cursor;
    directory.entries[5].failAt = 4;
    assert(loomSD::scanRecoveryFiles(directory, file, "Wisp_", 5, cursor, name, count, 2000,
                                     nullptr, reject) == loomSD::RecoveryResult::ReadError);
    assert(cursor == 5 && name[0] == '\0' && !file.opened);
    // Next wake retries the failed entry, not the following directory entry.
    directory.position = cursor;
    directory.entries[5].failAt = SIZE_MAX;
    assert(loomSD::scanRecoveryFiles(directory, file, "Wisp_", 5, cursor, name, count, 2000,
                                     nullptr, reject) == loomSD::RecoveryResult::Selected);
    assert(strcmp(name, "Wisp_4-Batch.txt") == 0 && count == 2);
    directory.position = cursor;
    assert(loomSD::scanRecoveryFiles(directory, file, "Wisp_", 5, cursor, name, count, 2000,
                                     nullptr, reject) == loomSD::RecoveryResult::Done);
    for (size_t i = 0; i < original.size(); ++i) {
        assert(original[i].data == directory.entries[i].data);
    }
    puts("PASS multi-file recovery, active-session exclusion, read-error resume, preserved data");
}

int main() {
    testRecords();
    testNames();
    testDebugNames();
    testClose();
    testJson();
    testBatchCommit();
    testJournalRecovery();
    testJournalCursorRecovery();
    testDebugAppend();
    testRecoveryCount();
    testRecoveryDirectory();
    puts("All data-safety helper tests passed.");
}
