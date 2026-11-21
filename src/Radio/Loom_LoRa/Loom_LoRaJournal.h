#pragma once

#include "Loom_LoRa.h"
#include "Hardware/Loom_Hypnos/SDManager.h"

class Loom_Hypnos;

// Optionally attach to a scheduled LoRa radio. One record is sent at a time,
// and the node keeps it on SD until the hub confirms that its copy was saved.
class Loom_LoRaJournal {
  public:
    static constexpr uint8_t MaxPayload = 96;
    enum class SendResult { Empty, Confirmed, Deferred, StorageError };
    enum class ReceiveResult { Idle, Saved, Unavailable, StorageError };

    Loom_LoRaJournal(Loom_LoRa &radio, Loom_Hypnos &hypnos, uint8_t payloadBytes);

    // Use a positive, increasing UTC sample ID. A repeat of the latest ID after
    // restart preserves the original recorded payload.
    bool queue(uint32_t sampleUtc, const void *payload);
    SendResult sendNext(uint8_t hub, uint32_t utc);

    // Poll from an awake hub. Supply room for payloadBytes when requesting output.
    // Duplicates are acknowledged from the existing journal and report Idle.
    ReceiveResult receive(uint32_t utc, uint8_t &source, uint32_t &sampleUtc, void *payload);

    // Call after configuring the node schedule and registering its RTC wake.
    // Sleeps between slots. The callback fills payload once per cycle, so keep the buffer alive.
    void sampleAndSend(uint8_t hub, void *payload, void (*measure)());

    // Poll using Hypnos UTC and retry SD initialization after a storage error.
    ReceiveResult pollHub(uint8_t &source, uint32_t &sampleUtc, void *payload);

  private:
    static constexpr uint8_t MaxEntry = MaxPayload + 8;
    Loom_LoRa &radio;
    Loom_Hypnos &hypnos;
    const uint8_t payloadBytes;

    uint32_t sampleCycle = UINT32_MAX;
    uint32_t drainedCycle = UINT32_MAX;
    uint32_t lastStorageRetry = 0;
    bool sampleQueued = false;
    bool reportedStorageError = false;
    bool triedStorage = false;

    bool readClock(DateTime &now);
    SDManager *storage();
    bool recover(const char *name, uint8_t size, uint32_t &end, uint32_t &last,
                 uint32_t *lastChecksum = nullptr);
    bool append(const char *name, const uint8_t *entry, uint8_t size);
};
