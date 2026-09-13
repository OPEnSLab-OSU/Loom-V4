#pragma once

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

// Small, allocation-free helpers shared by the SD paths and fault-injection tests.
namespace loomSD {
enum class RecordResult { End, Ready, TooLong, ReadError };

// Every iteration must consume a byte or return. available() alone does not detect SD errors.
template <typename FileType>
RecordResult nextRecord(FileType &file, uint32_t &start, size_t &length, size_t limit) {
    length = 0;
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
        if (value == '\r' || value == '\n')
            break;
        if (++length >= limit)
            return RecordResult::TooLong;
    }
    return RecordResult::Ready;
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
} // namespace loomSD
