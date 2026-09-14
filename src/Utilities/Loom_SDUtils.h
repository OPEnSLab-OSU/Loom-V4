#pragma once

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum class SDWriteStatus : uint8_t { NotAttempted, Saved, Failed, Rejected, Uncertain };
struct SDLogResult {
    SDWriteStatus csv = SDWriteStatus::NotAttempted;
    SDWriteStatus batch = SDWriteStatus::NotAttempted;
};

// Small, allocation-free helpers shared by the SD paths and fault-injection tests.
namespace loomSD {
inline bool canRetryBatch(const SDLogResult &result, uint32_t savedPacket, uint32_t packet) {
    return result.csv == SDWriteStatus::Saved && result.batch == SDWriteStatus::Failed &&
           savedPacket == packet;
}

struct AppendResult {
    SDWriteStatus status;
    bool committed;
};

// A retry is safe only after the failed append has been durably rolled back and closed.
template <typename FileType, typename Document>
AppendResult appendRecord(FileType &file, const Document &document) {
    const uint32_t start = file.fileSize();
    const size_t expected = measureJson(document);
    file.clearWriteError();
    const size_t written = serializeJson(document, file);
    const size_t newline = file.println();
    const bool complete = expected > 0 && written == expected && newline == 2 &&
                          !file.getWriteError() && file.sync();
    const bool rolledBack = complete || (file.truncate(start) && file.sync());
    const bool closed = file.close();
    if (complete)
        return {closed ? SDWriteStatus::Saved : SDWriteStatus::Uncertain, true};
    return {rolledBack && closed ? SDWriteStatus::Failed : SDWriteStatus::Uncertain, false};
}

enum class RecordResult { End, Ready, TooLong, ReadError };

// Every iteration must consume a byte or return. available() alone does not detect SD errors.
template <typename FileType>
RecordResult nextRecord(FileType &file, uint32_t &start, size_t &length, size_t limit,
                        bool *terminated = nullptr) {
    length = 0;
    if (terminated)
        *terminated = false;
    while (file.available()) {
        start = file.curPosition();
        const int value = file.read();
        if (value < 0)
            return RecordResult::ReadError;
        if (value == '\r' || value == '\n')
            continue;
        length = 1;
        break;
    }
    if (length == 0)
        return RecordResult::End;
    if (length >= limit)
        return RecordResult::TooLong;

    while (file.available()) {
        const int value = file.read();
        if (value < 0)
            return RecordResult::ReadError;
        if (value == '\r' || value == '\n') {
            if (terminated)
                *terminated = true;
            break;
        }
        if (++length >= limit)
            return RecordResult::TooLong;
    }
    return RecordResult::Ready;
}

// Recovery never edits old records. Refuse an overlong or unterminated (possibly torn) line.
template <typename FileType>
bool countRecords(FileType &file, size_t limit, int &count, void (*progress)() = nullptr) {
    count = 0;
    while (true) {
        if (progress)
            progress();
        uint32_t start = 0;
        size_t length = 0;
        bool terminated = false;
        const RecordResult result = nextRecord(file, start, length, limit, &terminated);
        if (result == RecordResult::End)
            return true;
        if (result != RecordResult::Ready || !terminated || count == INT_MAX)
            return false;
        ++count;
    }
}

template <typename FileType> bool closeAfterWrite(FileType &file, bool wroteAll) {
    // Do not short-circuit close(): even a failed/partial write must release its handle.
    const bool closed = file.close();
    return wroteAll && closed;
}

inline char foldAscii(char value) {
    return value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value;
}

inline bool sameName(const char *left, const char *right) {
    while (*left && foldAscii(*left) == foldAscii(*right)) {
        ++left;
        ++right;
    }
    return foldAscii(*left) == foldAscii(*right);
}

// FAT names are case-insensitive. Only exact base + digits + CSV/batch suffixes count;
// gaps, unrelated substring matches, and incomplete file pairs must not reuse an old session.
// False means a matching number cannot be incremented safely.
inline bool advanceLogNumber(const char *name, const char *base, int &next) {
    while (*base) {
        if (foldAscii(*name) != foldAscii(*base))
            return true;
        ++name;
        ++base;
    }
    const char *digits = name;
    while (*name >= '0' && *name <= '9')
        ++name;
    if (name == digits || (!sameName(name, ".csv") && !sameName(name, "-Batch.txt")))
        return true;

    int number = 0;
    while (digits != name) {
        const int digit = *digits++ - '0';
        if (number > (INT_MAX - digit) / 10)
            return false;
        number = number * 10 + digit;
    }
    if (number == INT_MAX)
        return false;
    if (number >= next)
        next = number + 1;
    return true;
}

inline bool isHistoricalBatch(const char *name, const char *base, int sessionNumber) {
    const char *suffix = nullptr;
    for (const char *cursor = name; *cursor; ++cursor) {
        if (*cursor == '-')
            suffix = cursor;
    }
    int next = 0;
    return suffix && sameName(suffix, "-Batch.txt") && advanceLogNumber(name, base, next) &&
           next > 0 && next <= sessionNumber;
}

enum class RecoveryResult { Selected, Done, ReadError };

// The directory is already positioned at cursor. Resume there after a successful clear;
// do not advance the saved cursor past a failed read. Storage remains entirely on SD.
template <typename Directory, typename FileType, size_t NameSize, typename OnRejected>
RecoveryResult scanRecoveryFiles(Directory &directory, FileType &file, const char *base,
                                 int sessionNumber, uint32_t &cursor,
                                 char (&selectedName)[NameSize], int &selectedCount,
                                 size_t limit, void (*progress)(), OnRejected rejected) {
    char name[NameSize];
    while (file.openNext(&directory)) {
        if (progress)
            progress();
        const uint32_t nextPosition = directory.curPosition();
        if (!file.getName(name, sizeof(name))) {
            file.close();
            return RecoveryResult::ReadError;
        }
        const bool candidate = !file.isDirectory() && isHistoricalBatch(name, base, sessionNumber)
                               && file.fileSize() > 0;
        if (candidate) {
            int records = 0;
            const bool complete = countRecords(file, limit, records, progress);
            const bool readFailed = file.getError() != 0;
            file.close();
            if (readFailed)
                return RecoveryResult::ReadError;
            if (complete && records > 0) {
                memcpy(selectedName, name, strlen(name) + 1);
                selectedCount = records;
                cursor = nextPosition;
                return RecoveryResult::Selected;
            }
            rejected(name); // Leave questionable files untouched; continue to other batches.
        } else {
            file.close();
        }
        cursor = nextPosition;
    }
    return directory.getError() || file.getError() ? RecoveryResult::ReadError
                                                   : RecoveryResult::Done;
}
} // namespace loomSD
