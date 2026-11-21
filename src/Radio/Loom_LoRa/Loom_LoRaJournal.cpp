#include "Loom_LoRaJournal.h"

#include "Hardware/Loom_Hypnos/Loom_Hypnos.h"
#include "Logger.h"
#include "Utilities/Loom_LoRaJournalFormat.h"
#include "Utilities/Loom_SDUtils.h"

namespace {
using namespace loomLoRa::journal;
constexpr uint16_t ReceiptWaitMs = 1500, ReceiptTurnaroundMs = 20;

void journalName(char (&name)[13], char kind, uint8_t address, uint8_t size) {
    snprintf(name, sizeof(name), "L%c%02X%02X.BIN", kind, unsigned(address), unsigned(size));
}
} // namespace

Loom_LoRaJournal::Loom_LoRaJournal(Loom_LoRa &radio, Loom_Hypnos &hypnos, uint8_t payloadBytes)
    : radio(radio), hypnos(hypnos), payloadBytes(payloadBytes) {}

bool Loom_LoRaJournal::readClock(DateTime &now) {
    if (hypnos.tryGetCurrentTime(now)) {
        return true;
    }
    WARNING(F("LoRa needs a valid UTC clock. Set the Hypnos RTC before using scheduled LoRa."));
    delay(5000);
    hypnos.enable();
    return false;
}

void Loom_LoRaJournal::sampleAndSend(uint8_t hub, void *payload, void (*measure)()) {
    const auto *schedule = radio.schedule;
    if (payload == nullptr || measure == nullptr || schedule == nullptr || !schedule->isEnabled()) {
        delay(1000);
        return;
    }
    DateTime now;
    if (!readClock(now)) {
        return;
    }
    const uint32_t utc = now.unixtime();
    const uint32_t cycle = schedule->cycleNumber(utc);
    const uint32_t untilSlot = schedule->secondsUntilWake(
        radio.deviceAddress, utc, drainedCycle == cycle);
    if (untilSlot > 0) {
        if (untilSlot != UINT32_MAX &&
            hypnos.scheduleWake(TimeSpan(static_cast<int32_t>(untilSlot)))) {
            hypnos.sleep();
        } else {
            delay(1000);
        }
        return;
    }

    if (sampleCycle != cycle) {
        measure();
        sampleCycle = cycle;
        sampleQueued = false;
        reportedStorageError = false;
    }
    if (!sampleQueued) {
        sampleQueued = queue(schedule->cycleStart(utc), payload);
    }
    // SD and sensor work can use up the slot. Check the clock again before sending.
    const auto result = hypnos.tryGetCurrentTime(now) ? sendNext(hub, now.unixtime())
                                                     : SendResult::Deferred;
    if (result == SendResult::Empty && sampleQueued) {
        drainedCycle = cycle;
        return;
    }
    if ((!sampleQueued || result == SendResult::StorageError) && !reportedStorageError) {
        WARNING(F("Node LoRa journal could not be read or saved."));
        reportedStorageError = true;
    }
    if (result != SendResult::Confirmed) {
        delay(1000);
    }
}

Loom_LoRaJournal::ReceiveResult Loom_LoRaJournal::pollHub(uint8_t &source, uint32_t &sampleUtc,
                                                         void *payload) {
    DateTime now;
    if (!readClock(now)) {
        return ReceiveResult::Unavailable;
    }
    const auto result = receive(now.unixtime(), source, sampleUtc, payload);
    if (result == ReceiveResult::StorageError &&
        (!triedStorage || millis() - lastStorageRetry >= 5000)) {
        triedStorage = true;
        lastStorageRetry = millis();
        WARNING(F("Hub LoRa journal unavailable. Retrying SD initialization."));
        if (auto *sd = hypnos.getSDManager()) {
            sd->begin();
        }
    }
    return result;
}

SDManager *Loom_LoRaJournal::storage() {
    SDManager *sd = hypnos.getSDManager();
    return payloadBytes > 0 && payloadBytes <= MaxPayload && sd && sd->hasSDInitialized() ? sd
                                                                                          : nullptr;
}

bool Loom_LoRaJournal::recover(const char *name, uint8_t size, uint32_t &end, uint32_t &last,
                               uint32_t *lastChecksum) {
    File file = storage()->openFile(name, O_RDWR | O_CREAT);
    uint8_t entry[MaxEntry];
    if (!loomSD::recoverFixedRecords(file, entry, size, end, validEntry)) {
        return false;
    }
    last = 0;
    if (end != 0) {
        last = read32(entry);
        if (lastChecksum) {
            *lastChecksum = read32(entry + size - 4);
        }
    }
    return true;
}

bool Loom_LoRaJournal::append(const char *name, const uint8_t *entry, uint8_t size) {
    File file = storage()->openFile(name, O_RDWR | O_CREAT | O_APPEND);
    if (!file) {
        return false;
    }
    const uint32_t start = file.fileSize();
    file.clearWriteError();
    const bool written = size <= UINT32_MAX - start && file.write(entry, size) == size;
    return loomSD::finishAppend(file, start, written).status == SDWriteStatus::Saved;
}

bool Loom_LoRaJournal::queue(uint32_t sampleUtc, const void *payload) {
    if (!storage() || !payload || sampleUtc == 0) {
        return false;
    }
    char name[13];
    journalName(name, 'Q', radio.deviceAddress, payloadBytes);
    const uint8_t body = payloadBytes + 4;
    uint32_t end, latest;
    if (!recover(name, body + 4, end, latest) || sampleUtc < latest) {
        return false;
    }
    if (sampleUtc == latest) {
        return true;
    }
    uint8_t entry[MaxEntry];
    write32(entry, sampleUtc);
    memcpy(entry + 4, payload, payloadBytes);
    write32(entry + body, checksum(entry, body));
    return append(name, entry, body + 4);
}

Loom_LoRaJournal::SendResult Loom_LoRaJournal::sendNext(uint8_t hub, uint32_t utc) {
    const uint32_t started = millis();
    if (!radio.moduleInitialized || !radio.schedule || !radio.schedule->isEnabled() ||
        hub == radio.deviceAddress || hub == RH_BROADCAST_ADDRESS ||
        !loomLoRa::Schedule::sameGroup(radio.deviceAddress, hub)) {
        return SendResult::Deferred;
    }
    // Budget for every radio TX and randomized wait, plus SD and the storage receipt.
    constexpr uint16_t sendBudget = Loom_RF95::InterruptTimeoutMs;
    const uint32_t budget = (radio.radioManager.retries() + 1UL) *
                            (sendBudget + 2UL * radio.retryTimeout) + ReceiptWaitMs + 2000UL;
    const uint32_t slotMs = radio.schedule->secondsLeftInSlot(radio.deviceAddress, utc) * 1000UL;
    if (slotMs <= budget) {
        return SendResult::Deferred;
    }
    if (!storage()) {
        return SendResult::StorageError;
    }

    char queueName[13], cursorName[13];
    journalName(queueName, 'Q', radio.deviceAddress, payloadBytes);
    journalName(cursorName, 'A', radio.deviceAddress, payloadBytes);
    const uint8_t entrySize = payloadBytes + 8;
    uint32_t end, latest, cursor;
    if (!recover(queueName, entrySize, end, latest)) {
        return SendResult::StorageError;
    }
    File checkpoint = storage()->openFile(cursorName, O_RDWR | O_CREAT);
    if (!recoverCursor(checkpoint, end, entrySize, cursor)) {
        return SendResult::StorageError;
    }
    if (cursor == end) {
        return SendResult::Empty;
    }

    File file = storage()->openFile(queueName);
    uint8_t entry[MaxEntry];
    if (!file || !loomSD::closeAfterWrite(
                     file, loomSD::readFixedRecord(file, cursor, entry, entrySize) &&
                               validEntry(entry, entrySize))) {
        return SendResult::StorageError;
    }
    const uint32_t stamp = read32(entry);
    uint8_t frame[HeaderSize + MaxPayload];
    writeHeader(frame, Reading, stamp);
    memcpy(frame + HeaderSize, entry + 4, payloadBytes);
    uint8_t receipt[HeaderSize];
    writeHeader(receipt, Receipt, stamp);
    const uint32_t elapsed = millis() - started;
    if (elapsed >= slotMs - budget) {
        return SendResult::Deferred; // SD reads consumed the time reserved for the exchange.
    }
    const bool confirmed = radio.radioManager.sendWithReceipt(
        frame, HeaderSize + payloadBytes, hub, receipt, sizeof(receipt),
        ReceiptWaitMs, slotMs - elapsed - 1000UL, sendBudget);
    radio.radioDriver.sleep();
    if (!confirmed) {
        return SendResult::Deferred;
    }
    write32(entry, cursor + entrySize);
    write32(entry + 4, checksum(entry, 4));
    return append(cursorName, entry, CursorSize) ? SendResult::Confirmed : SendResult::StorageError;
}

Loom_LoRaJournal::ReceiveResult Loom_LoRaJournal::receive(uint32_t utc, uint8_t &source,
                                                         uint32_t &sampleUtc, void *payload) {
    if (!radio.moduleInitialized || !radio.schedule || !radio.schedule->isEnabled()) {
        return ReceiveResult::Unavailable;
    }
    const uint32_t started = millis();
    uint8_t frame[HeaderSize + MaxPayload + 1], length = sizeof(frame), id, from;
    if (!radio.radioManager.receiveForStorage(frame, length, from, id) ||
        length != HeaderSize + payloadBytes || frame[0] != Network || frame[1] != Reading ||
        from == radio.deviceAddress || !loomLoRa::Schedule::sameGroup(radio.deviceAddress, from) ||
        !radio.schedule->mayTransmit(from, utc)) {
        return ReceiveResult::Idle;
    }
    const uint32_t stamp = read32(frame + 2);
    if (stamp == 0 || stamp > utc) {
        return ReceiveResult::Idle;
    }
    const uint32_t slotMs = radio.schedule->secondsLeftInSlot(from, utc) * 1000UL;
    if (slotMs <= 2000) {
        return ReceiveResult::Idle;
    }
    radio.radioManager.acknowledgeFrame(from, id);
    if (!storage()) {
        return ReceiveResult::StorageError;
    }

    char name[13];
    journalName(name, 'R', from, payloadBytes);
    const uint8_t body = payloadBytes + 4;
    uint32_t end, latest, storedChecksum = 0;
    if (!recover(name, body + 4, end, latest, &storedChecksum)) {
        return ReceiveResult::StorageError;
    }
    uint8_t entry[MaxEntry];
    memcpy(entry, frame + 2, body);
    const uint32_t entryChecksum = checksum(entry, body);
    write32(entry + body, entryChecksum);
    if (stamp > latest) {
        if (!append(name, entry, body + 4)) {
            return ReceiveResult::StorageError;
        }
    } else if (stamp < latest) {
        File file = storage()->openFile(name);
        uint8_t stored[MaxEntry];
        const auto result = findStoredReading(file, end, body + 4, entry, stored);
        if (!file.close() || result == StoredReading::Error) {
            return ReceiveResult::StorageError;
        }
        if (result == StoredReading::Missing) {
            return ReceiveResult::Idle;
        }
    } else if (entryChecksum != storedChecksum) {
        return ReceiveResult::Idle;
    }

    // Let the node switch from the link-ACK wait to the application receipt.
    if (millis() - started + ReceiptTurnaroundMs + 2000UL < slotMs) {
        delay(ReceiptTurnaroundMs);
        uint8_t receipt[HeaderSize];
        writeHeader(receipt, Receipt, stamp);
        radio.radioManager.setHeaderFlags(0, RH_FLAGS_ACK | RH_FLAGS_RETRY);
        if (radio.radioManager.sendto(receipt, sizeof(receipt), from)) {
            radio.radioDriver.waitPacketSent();
        }
    }
    if (stamp <= latest) {
        return ReceiveResult::Idle;
    }
    source = from;
    sampleUtc = stamp;
    if (payload) {
        memcpy(payload, frame + HeaderSize, payloadBytes);
    }
    return ReceiveResult::Saved;
}
