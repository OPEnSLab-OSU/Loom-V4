#pragma once
#include <stdint.h>

// One pending bench sleep survives loss of main power. A version tag and
// complements reject an erased row or an incomplete flash write.
struct SleepDeadline {
  static constexpr uint32_t versionTag = 0x4A534C32;
  static constexpr uint32_t rail3 = 1, rail5 = 2, rail12 = 4;
  static constexpr uint32_t railMask = rail3 | rail5 | rail12;
  uint32_t tag, armedAt, wakeAt, rails, armedAtInverse, wakeAtInverse, railsInverse;

  static SleepDeadline make(uint32_t start, uint32_t wake, uint32_t restoreRails) {
    return {versionTag, start, wake, restoreRails, ~start, ~wake, ~restoreRails};
  }

  bool valid(uint32_t maxSeconds) const {
    return tag == versionTag && armedAtInverse == ~armedAt && wakeAtInverse == ~wakeAt &&
           railsInverse == ~rails && !(rails & ~railMask) &&
           armedAt >= 946684800u && wakeAt < 4102444800u &&
           wakeAt > armedAt && wakeAt - armedAt <= maxSeconds;
  }

  bool alarmDue(uint32_t now, uint8_t status) const {
    // Require A1F from the retained alarm, a running clock, and the saved deadline.
    return (status & 0x81) == 0x01 && now >= wakeAt;
  }
};
