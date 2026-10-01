#pragma once
#include <cstdint>
#include <cstring>

namespace loomAt {
enum class Reply { Waiting, Accepted, Rejected };
inline Reply classifyLine(const char *line) {
    if (line == nullptr) {
        return Reply::Rejected;
    }
    if (std::strcmp(line, "OK") == 0) {
        return Reply::Accepted;
    }
    if (std::strcmp(line, "ERROR") == 0 || std::strncmp(line, "+CME ERROR:", 11) == 0 ||
        std::strncmp(line, "+CMS ERROR:", 11) == 0) {
        return Reply::Rejected;
    }
    return Reply::Waiting;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// Keep only a small line prefix. "OK" inside an echoed command/URC is not an acknowledgement.
// Oversized/NUL-containing lines cannot become OK; extended error prefixes still reject.
class ReplyReader {
  public:
    Reply consume(char value) {
        if (value == '\r') {
            return Reply::Waiting;
        }
        if (value != '\n') {
            if (value == '\0' || length == sizeof(line) - 1) {
                overflow = true;
            }
            if (!overflow) {
                line[length++] = value;
            }
            return Reply::Waiting;
        }
        line[length] = '\0';
        Reply result = classifyLine(line);
        if (overflow && result == Reply::Accepted) {
            result = Reply::Waiting;
        }
        length = 0;
        overflow = false;
        return result;
    }

  private:
    char line[16] = {};
    uint8_t length = 0;
    bool overflow = false;
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace loomAt
