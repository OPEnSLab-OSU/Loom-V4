#include "SDManager.h"
#include "Utilities/Loom_ResetDiagnostics.h"
#include "Utilities/Loom_SDUtils.h"

// Boot health logging lives separately from sample CSV/batch storage. It uses the same SD
// manager and checked append helper, with local handles and no new heap allocation.
namespace {
constexpr char BOOT_RESET_LOG[] = "/debug/boot-resets.jsonl";
constexpr char RESET_INTENT_FILE[] = "/debug/reset-intent.json";
constexpr size_t RESET_REASON_SIZE = 64;
constexpr size_t RESET_INTENT_SIZE = 256; // Encoded size is checked before opening/truncating.

// The parsed document borrows strings from buffer. Keep them together so the reason stays valid
// until the boot record has been written; nothing in this structure allocates from the heap.
struct SavedResetIntent {
    char buffer[RESET_INTENT_SIZE] = {};
    StaticJsonDocument<JSON_OBJECT_SIZE(4)> document;
    const char *status = "none";
    const char *reason = "";
    bool present = false;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
void readResetIntent(SdFat &sd, SavedResetIntent &intent, const char *serial, uint8_t cause) {
    intent.present = sd.exists(RESET_INTENT_FILE);
    if (!intent.present) {
        return;
    }
    intent.status = "unreadable";
    File input = sd.open(RESET_INTENT_FILE, O_RDONLY);
    if (!input) {
        return;
    }
    const uint32_t length = input.fileSize();
    const bool sizeValid = length > 0 && length < sizeof(intent.buffer);
    const int read = sizeValid ? input.read(intent.buffer, length) : -1;
    const bool closed = input.close();
    if (!sizeValid) {
        intent.status = "invalid";
        return;
    }
    if (read != static_cast<int>(length) || !closed) {
        return;
    }
    const DeserializationError error = deserializeJson(intent.document, intent.buffer);
    const char *savedSerial = intent.document["serial"].as<const char *>();
    const char *savedReason = intent.document["reason"].as<const char *>();
    if (error || !intent.document["version"].is<int>() ||
        intent.document["version"].as<int>() != 1 || savedSerial == nullptr ||
        savedReason == nullptr || savedReason[0] == '\0' ||
        strlen(savedReason) >= RESET_REASON_SIZE) {
        intent.status = "invalid";
        return;
    }

    intent.reason = savedReason;
    // A power cut after saving intent leaves a stale marker. Only the software-reset flag plus
    // matching hardware serial corroborates it; never relabel watchdog/brownout/power-on.
    intent.status = strcmp(savedSerial, serial) == 0 && loomReset::softwareResetOnly(cause)
                        ? "matched"
                        : "stale";
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////
void SDManager::recordBootReset() {
    if (bootResetStatus == SDWriteStatus::Saved || bootResetStatus == SDWriteStatus::Uncertain) {
        // A failed remove must not duplicate the boot record on every wake. Retry just cleanup.
        if (resetIntentCleanupPending && bootResetStatus == SDWriteStatus::Saved) {
            resetIntentCleanupPending =
                sd.exists(RESET_INTENT_FILE) && !sd.remove(RESET_INTENT_FILE);
        }
        return;
    }

    // Read the small intent file locally; readFile() allocates a much larger general JSON buffer.
    SavedResetIntent intent;
    const uint8_t cause = loomReset::bootCause();
    readResetIntent(sd, intent, manInst->get_serial_num(), cause);

    // JSON escaping keeps names/reasons safe without a large formatted text buffer. The document
    // only holds ten object slots; strings point to live buffers until appendRecord() returns.
    StaticJsonDocument<JSON_OBJECT_SIZE(10)> record;
    record["version"] = 1;
    record["device"] = static_cast<const char *>(device_name);
    record["instance"] = manInst->get_instance_num();
    record["serial"] = manInst->get_serial_num();
    record["uptime_ms"] = millis(); // Not UTC: the RTC may not have been initialized yet.
    record["raw_reset_flags"] = cause;
    record["cause"] = loomReset::causeName(cause);
    record["intent"] = intent.status;
    record["reason"] = intent.reason;
    record["csv"] = static_cast<const char *>(fileName);

    File output = sd.open(BOOT_RESET_LOG, O_WRITE | O_CREAT | O_APPEND);
    if (!output) {
        bootResetStatus = SDWriteStatus::Failed;
        printModuleName("Could not open the persistent boot-reset journal; retrying next wake.");
        return;
    }
    bootResetStatus = loomSD::appendRecord(output, record).status;
    if (bootResetStatus != SDWriteStatus::Saved) {
        // An uncertain rollback/close must not be blindly appended again during this boot.
        printModuleName("Could not confirm the boot-reset journal write; reset intent retained.");
        return;
    }

    // Consume only after the boot entry is synced and closed. No marker is created for a default
    // power-on boot; hardware flags are the evidence, not an assumed "lost power" string.
    resetIntentCleanupPending = intent.present && !sd.remove(RESET_INTENT_FILE);
    if (resetIntentCleanupPending) {
        printModuleName("Boot reset recorded, but its intent marker could not be removed.");
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::prepareForReset(const char *reason) {
    if (!sdInitialized || bootResetStatus != SDWriteStatus::Saved || resetIntentCleanupPending ||
        reason == nullptr || reason[0] == '\0') {
        printModuleName("Reset canceled: SD health log unavailable or reset reason missing.");
        return false;
    }
    size_t reasonLength = 0;
    while (reasonLength < RESET_REASON_SIZE && reason[reasonLength] != '\0') {
        ++reasonLength;
    }
    if (reasonLength == RESET_REASON_SIZE) {
        printModuleName("Reset canceled: use a reason of at most 63 bytes.");
        return false;
    }

    StaticJsonDocument<JSON_OBJECT_SIZE(4)> intent;
    intent["version"] = 1;
    intent["device"] = static_cast<const char *>(device_name);
    intent["serial"] = manInst->get_serial_num();
    intent["reason"] = reason;
    const size_t expected = measureJson(intent);
    if (expected + 2 >= RESET_INTENT_SIZE) {
        printModuleName("Reset canceled: escaped intent does not fit its bounded record.");
        return false;
    }
    File output = sd.open(RESET_INTENT_FILE, O_WRITE | O_CREAT | O_TRUNC);
    if (!output) {
        printModuleName("Reset canceled: could not open its SD intent marker.");
        return false;
    }
    output.clearWriteError();
    const size_t written = serializeJson(intent, output);
    const size_t newline = output.println();
    const bool complete =
        written == expected && newline == 2 && !output.getWriteError() && output.sync();
    const bool saved = loomSD::closeAfterWrite(output, complete);
    if (!saved) {
        // A partial/uncertain marker must not be treated as a completed programmatic request.
        resetIntentCleanupPending = sd.exists(RESET_INTENT_FILE) && !sd.remove(RESET_INTENT_FILE);
        printModuleName("Reset canceled: could not confirm its SD intent marker write.");
    }
    return saved;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
