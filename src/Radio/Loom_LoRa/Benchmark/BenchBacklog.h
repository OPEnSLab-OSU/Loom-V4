#pragma once

#include <SdFat.h>
#include <Utilities/Loom_SDUtils.h>

namespace bench {
namespace backlog {

constexpr uint8_t BatchKind = 0x45, ReceiptKind = 0x46;
constexpr uint8_t BatchHeader = 7, ReceiptBytes = 10, BatchLimit = 4;
constexpr uint8_t RecordBytes = 4 + CompactBytes, JournalBytes = RecordBytes + 4, CursorBytes = 8;
constexpr uint8_t MaxBatchBytes = BatchHeader + BatchLimit * RecordBytes;
constexpr uint32_t EarliestUtc = 946684800UL;
constexpr uint16_t ReceiptWaitMs = 1500;
constexpr LinkSettings DefaultLink{Transport::RadioHead, MaxRetries, 100, DefaultAckTimeoutMs};
static_assert(MaxBatchBytes <= RH_RF95_MAX_MESSAGE_LEN, "A backlog batch must fit in one datagram");

SdFat card;
char queueName[13], cursorName[13];
uint32_t queueEnd = 0, cursor = 0, newestQueued = 0, newestSaved[MaxNodes];
uint32_t saved = 0, delivered = 0, backfilled = 0, duplicates = 0, confirmed = 0;
uint32_t datagramsAttempted = 0, datagramsRetried = 0, radioRetries = 0;
uint32_t trialStart = 0;
uint8_t trialId = 0;
bool fault = false, durable = false;
LinkSettings link = DefaultLink;

void fileName(char (&name)[13], char kind, uint8_t hub, uint8_t node) {
    snprintf(name, sizeof(name), "B%c%02u%02u%02X.BIN", kind, unsigned(hub), unsigned(node),
             unsigned(trialId));
}

uint32_t checksum(const uint8_t *data, size_t size) {
    uint32_t crc = UINT32_MAX;
    while (size--) {
        crc ^= *data++;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320UL : 0);
        }
    }
    return ~crc;
}

bool readEntry(File &file, uint8_t *entry, uint8_t bytes) {
    return file.read(entry, bytes) == bytes && !file.getError();
}

bool validEntry(const uint8_t *entry, uint8_t bytes) {
    return checksum(entry, bytes - 4) == read32(entry + bytes - 4);
}

// Repair a short trailing write. A full-sized record with a bad checksum is left for inspection.
bool recoverTail(const char *name, uint8_t *tail, uint8_t bytes, uint32_t &size) {
    File file = card.open(name, O_RDWR | O_CREAT);
    if (!file) return false;
    const uint32_t original = file.fileSize();
    size = original - original % bytes;
    bool ok = !size || (file.seekSet(size - bytes) && readEntry(file, tail, bytes) &&
                       validEntry(tail, bytes));
    if (ok && size != original) ok = file.truncate(size) && file.sync();
    return loomSD::closeAfterWrite(file, ok);
}

bool appendEntries(const char *name, const uint8_t *entries, size_t bytes) {
    File file = card.open(name, O_RDWR | O_CREAT | O_APPEND);
    if (!file) return false;
    const uint32_t start = file.fileSize();
    file.clearWriteError();
    const bool wroteAll = bytes <= UINT32_MAX - start && file.write(entries, bytes) == bytes;
    return loomSD::finishAppend(file, start, wroteAll).status == SDWriteStatus::Saved;
}

bool begin(uint8_t nodes, bool useSD, uint16_t ackMs, uint8_t power, uint32_t epoch, uint8_t namespaceId) {
    fault = false;
    durable = useSD;
    trialStart = epoch;
    trialId = namespaceId;
    link = DefaultLink;
    link.ackMs = ackMs;
    saved = delivered = backfilled = duplicates = confirmed = 0;
    datagramsAttempted = datagramsRetried = radioRetries = 0;
    queueEnd = cursor = newestQueued = 0;
    memset(newestSaved, 0, sizeof(newestSaved));
    radio.setModeIdle();
    radio.setTxPower(power, false);
    if (role == Role::Hub) reliableRadio.setThisAddress(hubAddress(hub::hubId));
    if (!durable) return true;
#if BENCH_ENABLE_DENDROMETER
    if (role == Role::Hub) node::dendrometer.setPower(true);
#endif
    if (!card.begin(BENCH_SD_CS_PIN, SD_SCK_MHZ(4))) return false;
    uint8_t tail[JournalBytes];
    if (role == Role::Hub) {
        for (uint8_t id = 1; id <= nodes; ++id) {
            char name[13];
            fileName(name, 'R', hub::hubId, id);
            uint32_t size;
            if (!recoverTail(name, tail, JournalBytes, size)) return false;
            if (size && trialId) return false; // A test namespace must start empty.
            if (size) newestSaved[id - 1] = read32(tail);
        }
        return true;
    }
    fileName(queueName, 'Q', node::preferredHub, node::nodeId);
    fileName(cursorName, 'A', node::preferredHub, node::nodeId);
    if (!recoverTail(queueName, tail, JournalBytes, queueEnd)) return false;
    if (trialId && queueEnd) return false;
    if (queueEnd) newestQueued = read32(tail);
    uint32_t cursorSize;
    if (!recoverTail(cursorName, tail, CursorBytes, cursorSize)) return false;
    if (cursorSize) cursor = read32(tail);
    return cursor <= queueEnd && cursor % JournalBytes == 0;
}

bool prepareReading(uint8_t *record, uint32_t stamp) {
    node::readSensors();
    auto &doc = node::manager.getDocument();
    if (!node::manager.isPacketValid()) return false;
    write32(record, stamp);
    packCompact(doc.as<JsonVariantConst>(), node::nodeId, SensorSample, record + 4);
    return true;
}

bool queueReading(uint32_t stamp) {
    uint8_t entry[JournalBytes];
    if (stamp <= newestQueued || !prepareReading(entry, stamp)) return false;
    write32(entry + RecordBytes, checksum(entry, RecordBytes));
    if (!appendEntries(queueName, entry, sizeof(entry))) return false;
    queueEnd += sizeof(entry);
    newestQueued = stamp;
    return true;
}

bool readBatch(uint8_t *frame, uint8_t records) {
    File file = card.open(queueName);
    if (!file) return false;
    bool ok = file.seekSet(cursor);
    for (uint8_t i = 0; ok && i < records; ++i) {
        uint8_t entry[JournalBytes];
        ok = readEntry(file, entry, sizeof(entry)) && validEntry(entry, sizeof(entry));
        if (ok) memcpy(frame + BatchHeader + i * RecordBytes, entry, RecordBytes);
    }
    return loomSD::closeAfterWrite(file, ok);
}

void sendQueued(uint32_t groupStart, uint32_t slot, uint32_t now, uint32_t slotSeconds,
                uint16_t edgeSeconds, bool capture) {
    const uint32_t preparedAt = millis();
    if (durable && capture && !queueReading(groupStart)) {
        fault = true;
        return;
    }
    // A station that woke late can still save this cycle's reading for its next slot.
    if (now >= slot) return;
    const uint8_t records = durable ? min(uint32_t(BatchLimit), (queueEnd - cursor) / JournalBytes)
                                   : uint8_t(capture);
    if (!records) return;
    uint8_t frame[MaxBatchBytes] = {NetworkId, BatchKind};
    write32(frame + 2, groupStart);
    frame[6] = records;
    const bool prepared = durable ? readBatch(frame, records) : prepareReading(frame + BatchHeader, groupStart);
    if (!prepared) {
        fault = true;
        return;
    }
    const uint32_t sendAfter = (slot - now + edgeSeconds) * 1000UL;
    const uint32_t endAfter = (slot - now + slotSeconds - edgeSeconds) * 1000UL;
    while (millis() - preparedAt < sendAfter) delay(1);
    const uint8_t length = BatchHeader + records * RecordBytes;
    if (millis() - preparedAt + linkFrameMs(length, link) + ReceiptWaitMs > endAfter) return;

    const uint8_t destination = hubAddress(node::preferredHub);
    reliableRadio.setThisAddress(node::nodeId);
    reliableRadio.setRetries(link.retries);
    reliableRadio.setTimeout(link.ackMs);
    radio.cadMs = link.cadMs;
    const uint32_t before = reliableRadio.retransmissions();
    // A missing link ACK does not rule out a durable receipt. Listen in either case.
    reliableRadio.sendtoWait(frame, length, destination);
    radio.cadMs = 0;
    const uint32_t retries = reliableRadio.retransmissions() - before;
    ++datagramsAttempted;
    if (retries) ++datagramsRetried;
    radioRetries += retries;
    const uint32_t waiting = millis();
    const uint32_t lastStamp = read32(frame + BatchHeader + (records - 1) * RecordBytes);
    while (millis() - waiting < ReceiptWaitMs && millis() - preparedAt < endAfter) {
        uint8_t reply[ReceiptBytes + 1];
        uint8_t size = sizeof(reply);
        if (!radio.recv(reply, &size) || size != ReceiptBytes || reply[0] != NetworkId ||
            reply[1] != ReceiptKind || radio.headerFrom() != destination ||
            radio.headerTo() != node::nodeId || read32(reply + 2) != groupStart ||
            read32(reply + 6) != lastStamp) continue;
        if (durable) {
            const uint32_t next = cursor + records * JournalBytes;
            uint8_t entry[CursorBytes];
            write32(entry, next);
            write32(entry + 4, checksum(entry, 4));
            if (!appendEntries(cursorName, entry, sizeof(entry))) {
                fault = true;
                return;
            }
            cursor = next;
        }
        confirmed += records;
        return;
    }
}

bool saveBatch(const uint8_t *body, uint8_t records, uint8_t node, uint32_t groupStart,
               uint32_t &lastStamp) {
    uint8_t journal[BatchLimit * JournalBytes];
    uint8_t newRecords = 0, olderRecords = 0, trialRecords = 0;
    uint32_t previous = EarliestUtc - 1;
    for (uint8_t i = 0; i < records; ++i) {
        const uint8_t *record = body + i * RecordBytes;
        const uint32_t stamp = read32(record);
        if (stamp <= previous || stamp > groupStart || record[4] != CompactVersion ||
            !unpackCompact(record + 4, CompactBytes, node, SensorSample, hub::manager)) return false;
        previous = stamp;
        if (stamp <= newestSaved[node - 1]) continue;
        uint8_t *entry = journal + newRecords++ * JournalBytes;
        memcpy(entry, record, RecordBytes);
        write32(entry + RecordBytes, checksum(entry, RecordBytes));
        if (stamp >= trialStart) {
            ++trialRecords;
            if (stamp < groupStart) ++olderRecords;
        }
    }
    if (newRecords) {
        if (durable) {
            char name[13];
            fileName(name, 'R', hub::hubId, node);
            if (!appendEntries(name, journal, newRecords * JournalBytes)) {
                fault = true;
                return false;
            }
        }
        newestSaved[node - 1] = previous;
        saved += newRecords;
        delivered += trialRecords;
        backfilled += olderRecords;
    }
    duplicates += records - newRecords;
    lastStamp = previous;
    return true;
}

void receiveQueued(uint32_t groupStart, uint32_t windowStart, uint32_t slotMs, uint8_t nodes) {
    while (millis() - windowStart < nodes * slotMs) {
        uint8_t frame[RH_RF95_MAX_MESSAGE_LEN];
        uint8_t length = sizeof(frame), node, destination;
        if (!reliableRadio.recvfromAck(frame, &length, &node, &destination)) continue;
        const uint32_t elapsed = millis() - windowStart;
        if (!node || node > nodes || destination != hubAddress(hub::hubId) ||
            elapsed / slotMs != node - 1 || length < BatchHeader ||
            frame[0] != NetworkId || frame[1] != BatchKind || read32(frame + 2) != groupStart) continue;
        const uint8_t records = frame[6];
        if (!records || records > BatchLimit || length != BatchHeader + records * RecordBytes) continue;
        uint32_t lastStamp;
        if (!saveBatch(frame + BatchHeader, records, node, groupStart, lastStamp)) {
            if (fault) return;
            continue;
        }
        // Let the sender finish processing the RadioHead ACK before sending the receipt.
        if (millis() - windowStart + PingTurnaroundMs + airtimeMs(ReceiptBytes) + FrameMarginMs >=
            node * slotMs) continue;
        delay(PingTurnaroundMs);
        uint8_t reply[ReceiptBytes] = {NetworkId, ReceiptKind};
        write32(reply + 2, groupStart);
        write32(reply + 6, lastStamp);
        sendFrame(radio, node, reply, sizeof(reply));
    }
}

} // namespace backlog
} // namespace bench
