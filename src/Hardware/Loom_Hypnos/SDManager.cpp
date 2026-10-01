#include "SDManager.h"
#include "Logger.h"
#include "Utilities/Loom_SDUtils.h"
#include "Utilities/Loom_CsvUtils.h"
#include "Utilities/Loom_CsvChecksum.h"

namespace {
constexpr size_t MAX_SD_READ_BYTES = 4999;

// SdFat and Arduino Print keep different write-error flags. Observe the actual write count
// rather than asking a Print reference for a flag that SdFat's write() does not update.
class CheckedCsvOutput : public Print {
  public:
    explicit CheckedCsvOutput(Print &output) : output(output) {}
    size_t write(uint8_t value) override {
        if (getWriteError() || output.write(value) != 1) {
            setWriteError(1);
            return 0;
        }
        checksum = static_cast<uint16_t>(checksum + value);
        return 1;
    }

    uint16_t getChecksum() const { return checksum; }

  private:
    uint16_t checksum = 0;
    Print &output;
};

// Compare generated header bytes directly with the file. A layout change is detected exactly,
// including column order, without keeping the header in RAM or relying on hash collisions.
class CsvHeaderComparison : public Print {
  public:
    explicit CsvHeaderComparison(File &file) : file(file) {}
    size_t write(uint8_t value) override {
        if (!matches || file.read() != value) {
            matches = false;
            return 0;
        }
        return 1;
    }
    bool matched() const { return matches; }

  private:
    File &file;
    bool matches = true;
};

bool writeCsvJsonValue(Print &file, JsonVariant value) {
    if (value.is<const char *>()) {
        const JsonString text = value.as<JsonString>();
        return loomCsv::writeText(file, text.c_str(), text.size());
    }
    const size_t expected = measureJson(value);
    if (value.is<JsonArray>() || value.is<JsonObject>()) {
        if (file.write('"') != 1) {
            return false;
        }
        loomCsv::QuotedJsonWriter<Print> escaped(file);
        const bool complete = serializeJson(value, escaped) == expected;
        const bool closedQuote = file.write('"') == 1;
        return complete && closedQuote;
    }
    return serializeJson(value, file) == expected;
}

void copyBounded(char *destination, size_t destinationSize, const char *source,
                 size_t maxSourceCharacters) {
    if (destinationSize == 0) {
        return;
    }

    size_t index = 0;
    if (source != nullptr) {
        while (index < destinationSize - 1 && index < maxSourceCharacters &&
               source[index] != '\0') {
            destination[index] = source[index];
            ++index;
        }
    }
    destination[index] = '\0';
}

void appendLiteral(char *destination, size_t destinationSize, const char *suffix) {
    if (destinationSize == 0 || suffix == nullptr) {
        return;
    }

    size_t destinationIndex = strlen(destination);
    size_t suffixIndex = 0;
    while (destinationIndex < destinationSize - 1 && suffix[suffixIndex] != '\0') {
        destination[destinationIndex++] = suffix[suffixIndex++];
    }
    destination[destinationIndex] = '\0';
}

void buildNumberedName(char *destination, size_t destinationSize, const char *base, int number,
                       const char *suffix) {
    char numberText[12];
    snprintf(numberText, sizeof(numberText), "%i", number);

    const size_t reserved = strlen(numberText) + strlen(suffix) + 1;
    const size_t maxBaseCharacters = destinationSize > reserved ? destinationSize - reserved : 0;
    copyBounded(destination, destinationSize, base, maxBaseCharacters);
    appendLiteral(destination, destinationSize, numberText);
    appendLiteral(destination, destinationSize, suffix);
}

bool writeCsvTimestamp(Print &file, JsonString timestamp) {
    if (timestamp.isNull()) {
        return true;
    }
    // Loom timestamps are 19 characters plus optional Z. Keep the established T-to-space
    // normalization, but reject embedded NUL/overlong text and escape unusual cell contents.
    if (timestamp.size() > 20 || memchr(timestamp.c_str(), '\0', timestamp.size()) != nullptr) {
        return false;
    }
    char normalized[21];
    size_t length = 0;
    while (length < timestamp.size() && timestamp.c_str()[length] != 'Z') {
        normalized[length] = length == 10 ? ' ' : timestamp.c_str()[length];
        ++length;
    }
    return loomCsv::writeText(file, normalized, length);
}
} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////
SDManager::SDManager(Manager *man, int sd_chip_select)
    : Module("SD Manager"), manInst(man), chip_select(sd_chip_select) {
    strncpy(device_name, manInst->get_device_name(), sizeof(device_name) - 1);
    device_name[sizeof(device_name) - 1] = '\0';
    memset(batchFileName, '\0', sizeof(batchFileName));
    memset(fileName, '\0', sizeof(fileName));
    memset(overrideFileName, '\0', sizeof(overrideFileName));
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::writeLineToFile(const char *filename, const char *content) {
    if (filename == nullptr || filename[0] == '\0' || content == nullptr) {
        lastDebugWriteStatus = SDWriteStatus::Rejected;
        printModuleName("Cannot write a null/empty filename or null content!");
        return false;
    }

    if (!canWriteDebugLogs()) {
        // Optional debug output must never stall startup or every instrumented function.
        return false;
    }
    if (writeDebug) {
        Serial.print(F("[SD DEBUG] Opening SD file: "));
        Serial.println(filename);
    }

    // Open the given file for writing
    // Keep this handle local. A batch publisher may concurrently retain SDManager::myFile for
    // streaming; reassigning that shared object here invalidates the reader.
    File outputFile = sd.open(filename, O_RDWR | O_CREAT | O_APPEND);

    // Check if the file was actually opened, if so write the content to the file
    if (outputFile) {
        if (writeDebug) {
            Serial.println(F("[SD DEBUG] Writing the record"));
        }

        const uint32_t start = outputFile.fileSize();
        outputFile.clearWriteError();
        const size_t contentLength = strlen(content);
        const bool wroteContent = outputFile.print(content) == contentLength;
        const bool wroteNewline = outputFile.println() == 2;
        const bool writeComplete = wroteContent && wroteNewline && !outputFile.getWriteError();
        return finishDebugWrite(outputFile, start, writeComplete);
    }
    if (writeDebug) {
        Serial.println(F("[SD DEBUG] Could not open the file"));
    }
    lastDebugWriteStatus = SDWriteStatus::Failed;
    printModuleName("Failed to Open File!");
    return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::finishDebugWrite(File &file, uint32_t start, bool wroteAll) {
    if (writeDebug) {
        Serial.println(F("[SD DEBUG] Saving the file; rolling back a partial write if needed; closing"));
    }
    lastDebugWriteStatus = loomSD::finishAppend(file, start, wroteAll).status;
    const bool complete = lastDebugWriteStatus == SDWriteStatus::Saved;
    if (lastDebugWriteStatus == SDWriteStatus::Uncertain) {
        debugAppendBlocked = true;
        // Report directly: sending this through Logger would attempt another SD write.
        Serial.println(F("[SD DEBUG] The append result is uncertain; SD debug logging is paused until reboot."));
    }
    if (writeDebug) {
        Serial.println(complete ? F("[SD DEBUG] File saved successfully") : F("[SD DEBUG] File save failed"));
    }
    if (!complete) {
        printModuleName("Failed while writing or closing debug file!");
    }
    return complete;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::writeDebugRecords(const char *filename, bool (*writer)(Print &, void *),
                                  void *context) {
    if (!canWriteDebugLogs() || filename == nullptr || filename[0] == '\0' || writer == nullptr) {
        return false;
    }
    File file = sd.open(filename, O_RDWR | O_CREAT | O_APPEND);
    if (!file) {
        lastDebugWriteStatus = SDWriteStatus::Failed;
        return false;
    }
    const uint32_t start = file.fileSize();
    file.clearWriteError();
    const bool complete = writer(file, context) && !file.getWriteError();
    return finishDebugWrite(file, start, complete);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::writeJsonToFile(const char *filename, const DynamicJsonDocument &document) {
    if (!canWriteDebugLogs()) {
        return false;
    }
    if (filename == nullptr || filename[0] == '\0' || document.overflowed() || document.isNull()) {
        lastDebugWriteStatus = SDWriteStatus::Rejected;
        return false;
    }
    if (writeDebug) {
        Serial.print(F("[SD DEBUG] Opening SD file: "));
        Serial.println(filename);
    }
    File outputFile = sd.open(filename, O_RDWR | O_CREAT | O_APPEND);
    if (!outputFile) {
        if (writeDebug) {
            Serial.println(F("[SD DEBUG] Could not open the file"));
        }
        lastDebugWriteStatus = SDWriteStatus::Failed;
        printModuleName("Failed to open JSON debug file!");
        return false;
    }
    if (writeDebug) {
        Serial.println(F("[SD DEBUG] Writing the JSON record"));
    }
    const uint32_t start = outputFile.fileSize();
    outputFile.clearWriteError();
    const size_t expected = measureJsonPretty(document);
    const size_t written = serializeJsonPretty(document, outputFile);
    const bool newlineComplete = outputFile.println() == 2;
    return finishDebugWrite(outputFile, start,
                            written == expected && newlineComplete && !outputFile.getWriteError());
}
////////////////////////////////////////////////////////////////////////////////////////////////////

bool SDManager::appendDebugTrace(const char *filename, bool (*writer)(Print &, void *),
                                  void *context) {
    if (!canWriteDebugLogs() || filename == nullptr || writer == nullptr) {
        return false;
    }
    File file = sd.open(filename, O_RDWR);
    if (!file) {
        lastDebugWriteStatus = SDWriteStatus::Failed;
        return false;
    }
    const loomSD::AppendResult result = loomSD::appendTraceEvents(
        file, [writer, context](File &output) { return writer(output, context); });
    lastDebugWriteStatus = result.status;
    if (result.status == SDWriteStatus::Uncertain) {
        debugAppendBlocked = true;
        Serial.println(F("[TRACE] uncertain JSON append; SD debug logging paused until reboot"));
    }
    return result.status == SDWriteStatus::Saved;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::writeHeaders(Print &output) {
    CheckedCsvOutput checked(output);
    // Preserve the legacy byte layout: the serial text contained an LF before println() added its
    // normal line ending, leaving the established blank separator without a 513-byte stack array.
    checked.print(manInst->get_serial_num());
    checked.write('\n');
    checked.println();

    JsonObject document = manInst->getDocument().as<JsonObject>();

    // Write the first header directly. Building both headers in RAM previously consumed 1 KB of
    // stack on a Cortex-M0 just before an SD write.
    if (csvIdentityColumns) {
        checked.print(F("ID,,"));
    }

    // If there is a key that contains timestamp data when need to include that separately
    if (document.containsKey("timestamp")) {
        checked.print(F("timestamp,,"));
    }

    JsonArray contentsArray = document["contents"].as<JsonArray>();
    for (JsonVariant v : contentsArray) {
        const JsonString module = v.as<JsonObject>()["module"].as<JsonString>();
        if (!loomCsv::writeText(checked, module.c_str(), module.size())) {
            return false;
        }
        size_t fieldCount = v.as<JsonObject>()["data"].as<JsonObject>().size();
        while (fieldCount > 0) {
            checked.print(',');
            --fieldCount;
        }
    }
    if (csvChecksums) {
        checked.print(F("checksum"));
    }
    checked.println();

    // The second header is a separate pass over the small in-memory JSON tree.
    if (csvIdentityColumns) {
        checked.print(F("name,instance,"));
    }
    if (document.containsKey("timestamp")) {
        checked.print(F("time_utc,time_local,"));
    }
    for (JsonVariant v : contentsArray) {
        for (JsonPair keyValue : v.as<JsonObject>()["data"].as<JsonObject>()) {
            const JsonString key = keyValue.key();
            if (!loomCsv::writeText(checked, key.c_str(), key.size())) {
                return false;
            }
            checked.print(',');
        }
    }
    if (csvChecksums) {
        checked.print(F("checksum"));
    }
    checked.println();
    return !checked.getWriteError();
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::log(DateTime currentTime) {
    FUNCTION_START(this);
    lastLogResult = SDLogResult{};
    lastLogPacket = manInst->get_packet_number();
    if (!manInst->isPacketValid()) {
        lastLogResult.csv = SDWriteStatus::Rejected;
        lastLogResult.batch = SDWriteStatus::Rejected;
        printModuleName("Refusing to save an empty or overflowed JSON packet!");
        return false;
    }
    lastLogResult.csv = logCsv(currentTime);
    if (lastLogResult.csv != SDWriteStatus::Saved) {
        return false;
    }
    if (batch_size <= 0) {
        return true;
    }
    lastLogResult.batch = logBatch();
    return lastLogResult.batch == SDWriteStatus::Saved;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::retryBatch() {
    FUNCTION_START(this);
    if (!loomSD::canRetryBatch(lastLogResult, lastLogPacket, manInst->get_packet_number())) {
        return false;
    }
    lastLogResult.batch = logBatch();
    return lastLogResult.batch == SDWriteStatus::Saved;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
SDWriteStatus SDManager::logCsv(DateTime currentTime) {
    FUNCTION_START(this);
    // An overflowed document is syntactically valid but incomplete. Reject it
    // before creating headers, appending a row, or incrementing the batch.
    if (!manInst->isPacketValid()) {
        printModuleName("Refusing to save an empty or overflowed JSON packet!");
        return SDWriteStatus::Rejected;
    }
    if (!sdInitialized) {
        printModuleName("Failed to log! SD card not Initialized!");
        return SDWriteStatus::Failed;
    }
    const uint32_t utcDay = currentTime.unixtime() / 86400UL;
    if (csvChecksums && lastChecksumDay != 0 && lastChecksumDay != utcDay) {
        if (!verifyCsvChecksums(fileName)) {
            printModuleName("Daily checksum/read check failed; preserving the CSV and rotating.");
            if (!selectNextCsvFile()) {
                return SDWriteStatus::Failed;
            }
        }
        // Preserve any damaged file; future rows use a new CSV, while batches stay queued.
        lastChecksumDay = utcDay;
    }
    if (csvAppendBlocked) {
        if (!selectNextCsvFile()) {
            return SDWriteStatus::Uncertain;
        }
        csvAppendBlocked = false;
    }
    myFile = sd.open(fileName, O_RDWR | O_CREAT | O_APPEND);
    if (!myFile) {
        printModuleName("Failed to open log file!");
        return SDWriteStatus::Failed;
    }

    myFile.clearWriteError();

    if (myFile.fileSize() > 0) {
        if (!myFile.seekSet(0)) {
            myFile.close();
            printModuleName("Cannot read the CSV header; refusing to append a row!");
            return SDWriteStatus::Failed;
        }
        CsvHeaderComparison comparison(myFile);
        const bool sameHeaders = writeHeaders(comparison) && comparison.matched();
        const bool readFailed = myFile.getError() != 0;
        if (readFailed) {
            myFile.close();
            printModuleName("SD read failed during CSV header comparison!");
            return SDWriteStatus::Failed;
        }
        if (!sameHeaders) {
            if (!myFile.close() || !selectNextCsvFile()) {
                return SDWriteStatus::Failed;
            }
            printModuleName("CSV layout changed; using a new file and keeping the upload batch.");
            myFile = sd.open(fileName, O_RDWR | O_CREAT | O_APPEND);
            if (!myFile || myFile.fileSize() != 0) {
                myFile.close();
                return SDWriteStatus::Failed;
            }
        }
    }

    // If this file has never been written to before we need to create and write the proper
    // headers to the file
    // available() measures unread bytes, not file size; append handles may already be at EOF.
    if (myFile.fileSize() == 0) {
        const uint32_t originalSize = myFile.fileSize();
        // Set the date created timestamp of the File
        myFile.timestamp(T_CREATE, currentTime.year(), currentTime.month(), currentTime.day(),
                         currentTime.hour(), currentTime.minute(), currentTime.second());

        if (!writeHeaders(myFile) || !myFile.sync()) {
            const bool rolledBack = myFile.truncate(originalSize) && myFile.sync();
            const bool closed = myFile.close();
            csvAppendBlocked = !rolledBack || !closed;
            printModuleName("Failed while writing CSV headers!");
            return csvAppendBlocked ? SDWriteStatus::Uncertain : SDWriteStatus::Failed;
        }
    }

    const uint32_t recordStart = myFile.fileSize();
    myFile.clearWriteError();

    CheckedCsvOutput row(myFile);
    // Checksum covers exactly the same streamed bytes as storage, including identity.
    bool fieldsComplete = true;
    if (csvIdentityColumns) {
        fieldsComplete = loomCsv::writeText(row, manInst->get_device_name());
        row.print(',');
        row.print(manInst->get_instance_num());
        row.print(',');
    }

    JsonObject document = manInst->getDocument().as<JsonObject>();

    // If there is a key that contains timestamp data when need to include that separately
    if (document.containsKey("timestamp")) {
        fieldsComplete =
            writeCsvTimestamp(row, document["timestamp"]["time_utc"].as<JsonString>()) &&
            fieldsComplete;
        row.print(',');
        fieldsComplete =
            writeCsvTimestamp(row, document["timestamp"]["time_local"].as<JsonString>()) &&
            fieldsComplete;
        row.print(',');
    }

    // Stream fields in the same order as the CSV headers.
    JsonArray contentsArray = document["contents"].as<JsonArray>();

    for (JsonVariant v : contentsArray) {

        for (JsonPair keyValue : v.as<JsonObject>()["data"].as<JsonObject>()) {
            fieldsComplete = writeCsvJsonValue(row, keyValue.value()) && fieldsComplete;
            row.print(',');
        }
    }

    if (csvChecksums) {
        const uint16_t checksum = row.getChecksum();
        row.print(checksum);
    }
    row.println();

    const bool recordComplete =
        fieldsComplete && !row.getWriteError() && !myFile.getWriteError() && myFile.sync();

    // Set the last modified date
    if (recordComplete) {
        myFile.timestamp(T_WRITE, currentTime.year(), currentTime.month(), currentTime.day(),
                         currentTime.hour(), currentTime.minute(), currentTime.second());
    }

    const bool rolledBack = recordComplete || (myFile.truncate(recordStart) && myFile.sync());

    // Close the file
    const bool closed = myFile.close();
    csvAppendBlocked = !rolledBack || !closed;
    if (csvAppendBlocked) {
        printModuleName("CSV write/close is uncertain; preserving this file before the next row.");
        return SDWriteStatus::Uncertain;
    }

    if (!recordComplete) {
        printModuleName("Failed while writing CSV data!");
        return SDWriteStatus::Failed;
    }

    // Inform the user that we have successfully written to the file
    LOGF("Successfully logged data to %s", fileName);
    if (csvChecksums) {
        lastChecksumDay = utcDay;
    }
    return SDWriteStatus::Saved;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::verifyCsvChecksums(const char *filename) {
    if (!sdInitialized || filename == nullptr) {
        return false;
    }
    File input = sd.open(filename, O_RDONLY);
    if (!input) {
        return false;
    }
    loomCsv::ChecksumVerifier verifier;
    const uint32_t length = input.fileSize();
    bool valid = true;
    uint8_t bytes[32]; // Small fixed chunks reduce SD calls without buffering a whole CSV row.
    uint32_t offset = 0;
    while (offset < length && valid) {
        const uint32_t remaining = length - offset;
        const size_t count = remaining < sizeof(bytes) ? remaining : sizeof(bytes);
        if (input.read(bytes, count) != static_cast<int>(count)) {
            valid = false;
            break;
        }
        for (size_t i = 0; i < count; ++i) {
            if (!verifier.consume(bytes[i])) {
                valid = false;
                break;
            }
        }
        offset += count;
        LOOM_FEED_WATCHDOG(); // A bounded chunk was read; a stalled SDK read remains guarded.
    }
    valid = valid && verifier.finish() && input.getError() == 0;
    const bool closed = input.close();
    return valid && closed;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::selectNextCsvFile() {
    if (!root.open("/", O_RDONLY)) {
        return false;
    }
    const char *base = overrideFileName[0] ? overrideFileName : device_name;
    char candidate[LOG_FILENAME_SIZE];
    int nextNumber = 0;
    while (scanningFile.openNext(&root)) {
        const bool named = scanningFile.getName(candidate, sizeof(candidate));
        const bool closed = scanningFile.close();
        if (!named || !closed || !loomSD::advanceLogNumber(candidate, base, nextNumber)) {
            root.close();
            return false;
        }
        LOOM_FEED_WATCHDOG(); // A directory entry was consumed; the scan made forward progress.
    }
    const bool scanFailed = root.getError() != 0 || scanningFile.getError() != 0;
    const bool closed = root.close();
    if (scanFailed || !closed) {
        return false;
    }
    buildNumberedName(candidate, sizeof(candidate), base, nextNumber, ".csv");
    if (sd.exists(candidate)) {
        return false;
    }
    memcpy(fileName, candidate, strlen(candidate) + 1);
    csvFileNumber = nextNumber;
    // batchSessionNumber, batchFileName, recovery cursor and batch counters describe the upload
    // queue. They intentionally remain unchanged when only the CSV schema changes.
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::begin() {

    // Card reachability can be lost for one wake while the selected log file
    // must persist for the whole MCU boot session. Keeping these states
    // separate prevents a transient SD initialization failure from advancing
    // to a new numbered CSV on the following wake.
    const bool recoveringExistingLog = logFileSelected && !sdInitialized;

    pinMode(8, OUTPUT);
    digitalWrite(8, HIGH); // Disable LoRa

    printModuleName("Initializing SD Card...");

    /*
     Start the SD card with the fastest SPI speed
     Changing to 4MHz because 24MHz has occasional stability issues on different devices.
     SD_SCK_MHZ is from the SDFat library which takes the an integer parameter that sets the Serial
     clock frequency that SPI uses to communicate between the SD and the MCU(m0) Setting it to 50 is
     essentially asking the m0 to set the SCK freq to the max, up to 50MHz. (for an m0 the max is
     24MHz)

     Other possible settings can be multiples of 48MHz(m0 CPU Freq) such as,
     24MHz
     16MHz
     12MHz
     8MHz
     6MHz
     4MHz
   */
    if (!sd.begin(chip_select, SD_SCK_MHZ(4))) {
        sdInitialized = false;
        printModuleName("Failed to Initialize SD Card! SD Card functionality will be disabled, is "
                        "there an SD card inserted into the device?");
        return false;
    } else {
        // Make a debug folder if it doesn't already exist
        if (!sd.exists("debug")) {
            sd.mkdir("debug");
        }

        printModuleName("Successfully initialized SD Card!");
    }

    // Choose a numbered log file only once per MCU boot, or after setLogName()
    // explicitly changes the requested base name. A normal wake or recovery
    // from a temporary sd.begin() failure resumes the existing file.
    if (!logFileSelected) {
        // Try to open the root of the file system so we can get the files on the device
        if (!root.open("/", O_RDONLY)) {
            sdInitialized = false;
            printModuleName("ERROR");
            ERROR(F("Failed to open root file system on SD Card!"));
            printModuleName("After ERROR");
            return false;
        }
        if (!updateCurrentFileName()) {
            sdInitialized = false;
            root.close();
            return false;
        }
        logFileSelected = true;
    }

    sdInitialized = true;
    recordBootReset(); // Independent of ENABLE_SD_LOGGING; does not alter sample/batch files.

    if (batchClearPending) {
        clearBatch(); // Idempotent: no new records may be appended to this selected file yet.
    }
    if (batch_size > 0 && !batchClearPending && !hasRecoveredBatch() && recoveryScanPending) {
        findRecoveryBatch(); // Failure must not disable logging of new samples; retry next wake.
    }

    if (recoveringExistingLog) {
        printModuleName("SD card recovered; resuming data log:");
        printModuleName(fileName);
    }
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::updateCurrentFileName() {
    char f_name[LOG_FILENAME_SIZE];
    const char *base = overrideFileName[0] ? overrideFileName : device_name;
    batchSessionNumber = 0;

    // Select above the highest existing CSV OR batch number. Counting files reuses names
    // after a deletion or a failed half-pair write, and mixes old records with a new RAM counter.
    while (scanningFile.openNext(&root)) {
        const bool named = scanningFile.getName(f_name, sizeof(f_name));
        scanningFile.close();
        if (!named || !loomSD::advanceLogNumber(f_name, base, batchSessionNumber)) {
            printModuleName("Cannot safely select a new SD log number!");
            return false;
        }
    }

    const bool scanFailed = root.getError() != 0 || scanningFile.getError() != 0;
    root.close();
    if (scanFailed) {
        printModuleName("SD directory read failed; log filename was not selected!");
        return false;
    }

    buildNumberedName(fileName, sizeof(fileName), base, batchSessionNumber, ".csv");
    buildNumberedName(batchFileName, sizeof(batchFileName), base, batchSessionNumber, "-Batch.txt");
    if (sd.exists(fileName) || sd.exists(batchFileName)) {
        printModuleName("Selected SD log name already exists; refusing to mix sessions!");
        return false;
    }
    current_batch = 0;
    csvFileNumber = batchSessionNumber;
    csvAppendBlocked = false;

    printModuleName("Data will be logged to:");
    printModuleName(fileName);
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
const char *SDManager::getBatchFilename() {
    return hasRecoveredBatch() ? recoveryFileName : batchFileName;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::findRecoveryBatch() {
    // One filename and a directory cursor, not a heap-resident list of files or records.
    if (!root.open("/", O_RDONLY)) {
        return false;
    }
    if (!root.seekSet(recoveryScanPosition)) {
        root.close();
        return false;
    }
    const char *base = overrideFileName[0] ? overrideFileName : device_name;
    const loomSD::RecoveryResult result = loomSD::scanRecoveryFiles(
        root, scanningFile, base, batchSessionNumber, recoveryScanPosition, recoveryFileName,
        recoveryCount, MAX_JSON_SIZE, loomResetWatchdogIfEnabled, [this](const char *name) {
            printModuleName("Batch has incomplete/oversized records; preserved for inspection:");
            printModuleName(name);
        });
    root.close();
    if (result == loomSD::RecoveryResult::Done) {
        recoveryScanPending = false;
    }
    if (result == loomSD::RecoveryResult::Selected) {
        printModuleName("Recovered pending batch:");
        printModuleName(recoveryFileName);
    }
    if (result == loomSD::RecoveryResult::ReadError) {
        printModuleName("SD read failed during recovery; will retry on wake.");
    }
    return result != loomSD::RecoveryResult::ReadError;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
char *SDManager::readFile(const char *fileName) {
    FUNCTION_START(this);
    if (!sdInitialized) {
        printModuleName("Failed to read! SD card not Initialized!");
        return nullptr;
    }

    myFile = sd.open(fileName);
    if (!myFile) {
        printModuleName("Failed to open file!");
        return nullptr;
    }

    const size_t fileSize = myFile.size();
    if (fileSize > MAX_SD_READ_BYTES) {
        printModuleName("File is too large to read safely into memory!");
        myFile.close();
        return nullptr;
    }

    char *fileContents = static_cast<char *>(malloc(fileSize + 1));
    if (fileContents == nullptr) {
        printModuleName("Failed to allocate memory for file contents!");
        myFile.close();
        return nullptr;
    }

    size_t index = 0;
    while (index < fileSize) {
        const int value = myFile.read();
        if (value < 0) {
            break;
        }
        fileContents[index++] = static_cast<char>(value);
    }
    fileContents[index] = '\0';
    myFile.close();

    if (index != fileSize) {
        free(fileContents);
        printModuleName("Failed to read the complete file!");
        return nullptr;
    }
    return fileContents;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
SDWriteStatus SDManager::logBatch() {
    FUNCTION_START(this);
    if (!sdInitialized || batch_size <= 0) {
        return SDWriteStatus::Failed;
    }
    if (batchClearPending) {
        clearBatch(); // Also recover in always-awake sketches that do not call begin() again.
    }
    if (batchAppendBlocked || (batchClearPending && !hasRecoveredBatch())) {
        printModuleName("Batch append blocked by an uncertain write or pending clear; CSV kept!");
        return SDWriteStatus::Uncertain;
    }
    // Validate encoded bytes separately from the JSON pool before changing the queue.
    if (!loomJsonFitsWire(manInst->getDocument(), MAX_JSON_SIZE)) {
        printModuleName(
            "Packet exceeds batch wire limit or is incomplete; CSV kept, batch rejected!");
        return SDWriteStatus::Rejected;
    }
    if (current_batch == INT_MAX) {
        return SDWriteStatus::Rejected;
    }
    // Never discard an unsent batch here. The publisher clears it explicitly, and only after
    // every record succeeds. If a network outage lasts for several samples, the file grows on SD
    // instead of consuming SRAM or silently dropping the older records.
    myFile = sd.open(batchFileName, O_WRITE | O_CREAT | O_APPEND);

    // Check if the file has been opened properly and write the JSON packet to one line.
    if (myFile) {
        const loomSD::AppendResult result = loomSD::appendRecord(myFile, manInst->getDocument());
        if (result.committed) {
            current_batch++;
        }
        if (result.status == SDWriteStatus::Failed) {
            printModuleName("Failed while writing batch data!");
        }
        if (result.status == SDWriteStatus::Uncertain) {
            batchAppendBlocked = true;
            printModuleName("Uncertain batch write; preserving file and blocking further appends!");
        }
        return result.status;
    } else {
        printModuleName("Failed to open file!");
        return SDWriteStatus::Failed;
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::clearBatch() {
    FUNCTION_START(this);
    if (!sdInitialized || batch_size <= 0 || batchFileName[0] == '\0') {
        printModuleName("Cannot clear batch because SD batch logging is unavailable!");
        return false;
    }

    const bool recovered = hasRecoveredBatch();
    batchClearPending = true;
    myFile = sd.open(getBatchFilename(), O_WRITE | O_CREAT | O_TRUNC);
    if (!myFile) {
        printModuleName("Failed to clear the published batch file!");
        return false;
    }

    if (!myFile.close()) {
        printModuleName("Failed to close cleared batch file; clear result is uncertain!");
        return false;
    }
    batchClearPending = false;
    if (recovered) {
        recoveryFileName[0] = '\0';
        recoveryCount = 0;
        findRecoveryBatch();
    } else {
        current_batch = 0;
        batchAppendBlocked = false;
    }
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
