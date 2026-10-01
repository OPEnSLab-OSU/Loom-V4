#pragma once
#include "Loom_WarningGuards.h"
LOOM_EXTERNAL_INCLUDE_BEGIN
#include <ArduinoJson.h>
LOOM_EXTERNAL_INCLUDE_END
#include "Utilities/Loom_HeartbeatSchedule.h"

class Manager;
class Loom_Hypnos;

////////////////////////////////////////////////////////////////////////////////////////////////////
// UTC deadlines keep time across processor sleep. Sketches choose the transport and wake alarm.
// Heartbeat packaging never changes Manager's packet or measurement packet number.
class Loom_Heartbeat {
  public:
    using Event = loomHeartbeat::Schedule::Event;
    Loom_Heartbeat(uint32_t heartbeatSeconds, uint32_t workSeconds, Manager *manager,
                   Loom_Hypnos *hypnos = nullptr)
        : timers(heartbeatSeconds, workSeconds), manager(manager), hypnos(hypnos) {}
    bool begin(uint32_t nowUtc); // Call after RTC initialization. First normal work is due now.
    Event poll(uint32_t nowUtc); // Normal work wins a tie; heartbeat stays due for the next poll.
    uint32_t secondsUntilNext(uint32_t nowUtc) const;
    // Use a separate document, not Manager::getDocument(). Caller may append small health fields.
    bool createJSONPayload(JsonDocument &output);
    void flashLight(); // Explicit dash-dot LED indicator, never called from an ISR.
  private:
    loomHeartbeat::Schedule timers;
    Manager *manager;
    Loom_Hypnos *hypnos;
};
