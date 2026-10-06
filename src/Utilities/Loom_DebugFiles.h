#pragma once

#include <stddef.h>
#include <stdio.h>
#include <string.h>

// Shared, allocation-free naming for Logger text, function summaries and trace files.
namespace loomDebugFiles {
constexpr size_t NAME_SIZE = 64;
constexpr size_t PATH_SIZE = NAME_SIZE + 40;
enum class Kind { Text, Summaries, TraceRecords, TraceTimeline };

// Restrict the prefix to one portable filename component. Trim trailing separators
// so Manager("Deploy_Test_", ...) produces Deploy_Test_debug_N.log.
inline bool copyName(char *destination, size_t capacity, const char *name) {
    if (!destination || capacity == 0) return false;
    destination[0] = '\0';
    size_t length = 0;
    for (const char *cursor = name ? name : ""; *cursor; ++cursor) {
        if (length + 1 >= capacity) {
            destination[0] = '\0';
            return false; // Never silently truncate a custom prefix into another name.
        }
        const char value = *cursor;
        destination[length++] =
            (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
            (value >= '0' && value <= '9') || value == '-' || value == '_' ? value : '_';
    }
    while (length && destination[length - 1] == '_') --length;
    destination[length] = '\0';
    return true;
}

inline const char *role(Kind kind) {
    return kind == Kind::Text ? "debug" : kind == Kind::Summaries ? "funcSummaries" : "trace";
}
inline const char *suffix(Kind kind) {
    return kind == Kind::TraceRecords ? ".ndjson" :
           kind == Kind::TraceTimeline ? ".perfetto.json" : ".log";
}

inline bool buildBase(char *destination, size_t capacity, const char *prefix, Kind kind) {
    if (!destination || capacity == 0) return false;
    destination[0] = '\0';
    char safeName[NAME_SIZE];
    if (!copyName(safeName, sizeof(safeName), prefix)) return false;
    const int length = snprintf(destination, capacity, "%s_%s_",
                                safeName[0] ? safeName : "device", role(kind));
    if (length < 0 || static_cast<size_t>(length) >= capacity) {
        destination[0] = '\0';
        return false;
    }
    return true;
}

inline bool buildPath(char *destination, size_t capacity, const char *prefix, Kind kind,
                      int session) {
    if (!destination || capacity == 0) return false;
    destination[0] = '\0';
    char base[NAME_SIZE + 16];
    if (session < 0 || !buildBase(base, sizeof(base), prefix, kind)) return false;
    const int length = snprintf(destination, capacity, "/debug/%s%d%s", base, session, suffix(kind));
    if (length < 0 || static_cast<size_t>(length) >= capacity) {
        destination[0] = '\0';
        return false;
    }
    return true;
}
} // namespace loomDebugFiles
