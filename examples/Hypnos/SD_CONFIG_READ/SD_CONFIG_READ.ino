#include <Loom_Manager.h>
#include <Logger.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>

Manager manager("Device", 1);
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, true, true);

struct StreamSummary {
    size_t totalBytes;
    size_t chunkCount;
};

bool onConfigChunk(const uint8_t *data, size_t bytesRead, size_t fileOffset, size_t chunkIndex,
                   bool eof, void *userCtx) {
    (void)data;

    StreamSummary *summary = (StreamSummary *)userCtx;
    if (summary != nullptr) {
        summary->totalBytes += bytesRead;
        summary->chunkCount++;
    }

    char output[OUTPUT_SIZE];
    snprintf(output, sizeof(output), "chunk=%lu offset=%lu bytes=%lu eof=%s",
             (unsigned long)chunkIndex, (unsigned long)fileOffset, (unsigned long)bytesRead,
             eof ? "true" : "false");
    LOG(output);
    return true;
}

void setup() {
    manager.beginSerial();

    // Hypnos rails must be enabled before SD access.
    hypnos.enable();
    manager.initialize();

    StaticJsonDocument<OUTPUT_SIZE> configDoc;
    MemPool::Handle lease = hypnos.readFileLease("config.json");
    const char *configContents = hypnos.leaseData(lease);

    if (configContents == nullptr) {
        ERROR(F("Failed to read config.json from SD with lease API."));
        return;
    }

    DeserializationError error = deserializeJson(configDoc, configContents);
    if (error != DeserializationError::Ok) {
        char output[OUTPUT_SIZE];
        snprintf(output, sizeof(output), "Failed to parse config.json: %s", error.c_str());
        ERROR(output);
    } else {
        LOG(F("config.json parsed successfully via lease API."));
        if (!configDoc["timezone"].isNull()) {
            char output[OUTPUT_SIZE];
            snprintf(output, sizeof(output), "timezone=%s",
                     configDoc["timezone"].as<const char *>());
            LOG(output);
        }
    }

    if (hypnos.releaseLease(lease)) {
      LOG(F("Released config lease"));
    } else {
        WARNING(F("Failed to release config lease."));
    }
    hypnos.printPoolStats();

    StreamSummary summary = {0, 0};
    if (!hypnos.streamFile("config.json", 128, onConfigChunk, &summary)) {
        ERROR(F("streamFile failed for config.json."));
    } else {
        char output[OUTPUT_SIZE];
        snprintf(output, sizeof(output), "Streaming complete: chunks=%lu totalBytes=%lu",
                 (unsigned long)summary.chunkCount, (unsigned long)summary.totalBytes);
        LOG(output);
    }
    hypnos.printPoolStats();
}

void loop() {}
