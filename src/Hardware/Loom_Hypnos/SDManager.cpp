#include "SDManager.h"
#include "Logger.h"

namespace {
struct CsvEscapedBufferWriter {
    char *out;
    size_t outSize;
    bool ok;

    CsvEscapedBufferWriter(char *output, size_t outputSize)
        : out(output), outSize(outputSize), ok(true) {}

    bool appendRaw(char c) {
        size_t used = strlen(out);
        if (used + 1 >= outSize) {
            ok = false;
            return false;
        }
        out[used] = c;
        out[used + 1] = '\0';
        return true;
    }

    size_t write(uint8_t c) {
        if (c == '"') {
            if (!appendRaw('"') || !appendRaw('"')) {
                return 0;
            }
            return 1; // One input byte consumed.
        }

        return appendRaw((char)c) ? 1 : 0;
    }

    size_t write(const uint8_t *buffer, size_t size) {
        size_t i = 0;
        for (; i < size; i++) {
            if (!write(buffer[i])) {
                break;
            }
        }
        return i;
    }
};

void poolLeaseDumpSink(const char *line, void *ctx) {
    SDManager *manager = static_cast<SDManager *>(ctx);
    if (manager != nullptr && line != nullptr) {
        manager->printModuleName(line);
    }
}
} // namespace

//////////////////////////////////////////////////////////////////////////////////////////////////////
SDManager::SDManager(Manager *man, int sd_chip_select)
    : manInst(man), Module("SD Manager"), chip_select(sd_chip_select) {
    snprintf(device_name, sizeof(device_name), "%s", manInst->get_device_name());
    memset(overrideFileName, '\0', 260);
    manInst->getPool().setFreeRamProvider(freeMemory);
} // Disables Lora so we can use the SD card on hypnos
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::writeLineToFile(const char *filename, const char *content) {

    // Check if the SD card is actually functional
    if (sdInitialized) {
        // Open the given file for writing
        myFile = sd.open(filename, O_RDWR | O_CREAT | O_APPEND);

        // Check if the file was actually opened, if so write the content to the file
        if (myFile) {
            myFile.println(content);
            myFile.close();
            return true;
        }
        printModuleName("Failed to Open File!");
        return false;
    }

    /* Wait for a bit so the user has time to read it */
    printModuleName(
        "SD Card was improperly initialized and as such this functionality was disabled!");
    delay(5000);
    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void SDManager::writeHeaders() {
    // Allocates 576 bytes per header within this scope (size returns 513)
    // Capacity returns 576
    MemPool::Lease headerLease1 = manInst->getPool().allocLease(513, "sd_hdr1");
    MemPool::Lease headerLease2 = manInst->getPool().allocLease(513, "sd_hdr2");

    if (!headerLease1 || !headerLease2) {
        printModuleName("Failed to allocate SD header buffers!");
        return;
    }

    char *header1 = headerLease1.chars();
    char *header2 = headerLease2.chars();

    auto appendHeader = [](char *dst, size_t dstSize, const char *text) -> bool {
        if (dst == nullptr) {
            return false;
        }
        if (text == nullptr) {
            text = "";
        }

        size_t used = strlen(dst);
        if (used >= dstSize) {
            return false;
        }

        size_t remaining = dstSize - used - 1;
        strncat(dst, text, remaining);
        return strlen(text) <= remaining;
    };
    
    // Append the serial number to the top of the CSV file, reset the header1 array
    snprintf_P(header1, headerLease1.size(), PSTR("%s"), manInst->get_serial_num());
    myFile.println(header1);

    // Clear both arrays
    header1[0] = '\0';
    header2[0] = '\0';

    JsonObject document = manInst->getDocument().as<JsonObject>();
    // count = size - contents - null terminator
    appendHeader(header1, headerLease1.size(), "ID,,");
    appendHeader(header2, headerLease2.size(), "name,instance,");

    // If there is a key that contains timestamp data when need to include that separately
    if (document.containsKey("timestamp")) {
        appendHeader(header1, headerLease1.size(), "timestamp,,");
        appendHeader(header2, headerLease2.size(), "time_utc,time_local,");
    }

    // Get the contents containing the reset of the sensor data
    JsonArray contentsArray = document["contents"].as<JsonArray>();

    // Loop over each
    for (JsonVariant v : contentsArray) {
        // Get the module name
        appendHeader(header1, headerLease1.size(), v.as<JsonObject>()["module"] | "");

        // Get all JSON keys
        for (JsonPair keyValue : v.as<JsonObject>()["data"].as<JsonObject>()) {
            appendHeader(header2, headerLease2.size(), keyValue.key().c_str());
            appendHeader(header2, headerLease2.size(), ",");
            appendHeader(header1, headerLease1.size(), ",");
        }
    }
    // Write the headers to the file
    myFile.println(header1);
    myFile.println(header2);
} // Leases release here

//////////////////////////////////////////////////////////////////////////////////////////////////////
void SDManager::buildSchemaHashes(uint64_t &hash1, uint64_t &hash2) {
    JsonObject document = manInst->getDocument().as<JsonObject>();

    hash1 = fnv1a64_init();
    hash2 = fnv1a64_init() ^ 0x9E3779B97F4A7C15ULL;

    auto updateHashPair = [&](const char *token) {
        if (!token)
            token = "";

        hash1 = fnv1a64_update_cstr(hash1, token);

        size_t len = strlen(token);
        uint8_t lenBytes[2] = {(uint8_t)(len & 0xFF), (uint8_t)((len >> 8) & 0xFF)};
        hash2 = fnv1a64_update(hash2, lenBytes, sizeof(lenBytes));
        hash2 = fnv1a64_update_cstr(hash2, token);
    };

    updateHashPair("ID:name,instance;");
    if (document.containsKey("timestamp")) {
        updateHashPair("timestamp:time_utc,time_local;");
    }

    JsonArray contentsArray = document["contents"].as<JsonArray>();
    for (JsonVariant v : contentsArray) {
        const char *module = v.as<JsonObject>()["module"] | "";

        updateHashPair("[");
        updateHashPair(module);
        updateHashPair("]:");

        for (JsonPair keyValue : v.as<JsonObject>()["data"].as<JsonObject>()) {
            updateHashPair(keyValue.key().c_str());
            updateHashPair(",");
        }

        updateHashPair(";");
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void SDManager::setCurrentLogFileNames() {
    if (strlen(overrideFileName) > 0) {
        snprintf_P(fileName, 260, PSTR("%s%i.csv"), overrideFileName, getCurrentFileNumber());
        snprintf_P(fileNameNoExtension, 260, PSTR("%s%i"), overrideFileName,
                   getCurrentFileNumber());
    } else {
        snprintf_P(fileName, 260, PSTR("%s%i.csv"), device_name, getCurrentFileNumber());
        snprintf_P(fileNameNoExtension, 260, PSTR("%s%i"), device_name, getCurrentFileNumber());
    }
    getBatchFilename();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::log(DateTime currentTime) {
    char output[MAX_JSON_SIZE + 1];
    output[0] = '\0';
    bool truncated = false;
    bool time_error = false;
    bool success = true;

    /* Compare current modules with existing modules */
    uint64_t newHash1 = 0;
    uint64_t newHash2 = 0;
    buildSchemaHashes(newHash1, newHash2);

    /* Safe method of adding data to output */
    auto append = [&](const char *s) -> bool {
        if (!s)
            s = "";
        size_t used = strlen(output);
        size_t rem = sizeof(output) - used - 1; // Must leave room for terminator
        if (rem == 0)
            return false;
        strncat(output, s, rem);
        return strlen(s) <= rem; // False when truncated
    };

    auto appendCsvEscaped = [&](const char *s) -> bool {
        if (!s)
            s = "";

        bool needsEscaping = false;
        for (const char *p = s; *p != '\0'; p++) {
            if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r') {
                needsEscaping = true;
                break;
            }
        }

        if (!needsEscaping)
            return append(s);

        if (!append("\""))
            return false;

        for (const char *p = s; *p != '\0'; p++) {
            if (*p == '"') {
                if (!append("\"\""))
                    return false;
            } else {
                char oneChar[2] = {*p, '\0'};
                if (!append(oneChar))
                    return false;
            }
        }

        return append("\"");
    };

    /**
     * Type agnostic append for json values
     * Eliminates arduino temp string usage
     * Supported Types:
     * bool, string, float/double, int/uint, long/ulong
     */
    auto appendJsonValue = [&](JsonVariantConst v) -> bool {
        char buf[64];

        if (v.isNull())
            return append("");

        if (v.is<bool>())
            return append(v.as<bool>() ? "true" : "false");

        if (v.is<const char *>()) {
            const char *s = v.as<const char *>();
            return appendCsvEscaped(s);
        }

        if (v.is<float>() || v.is<double>()) {
            snprintf(buf, sizeof(buf), "%.6g", v.as<double>());
            return append(buf);
        }

        if (v.is<long>()) {
            snprintf(buf, sizeof(buf), "%ld", v.as<long>());
            return append(buf);
        }

        if (v.is<unsigned long>()) {
            snprintf(buf, sizeof(buf), "%lu", v.as<unsigned long>());
            return append(buf);
        }

        // Fallback for arrays/objects/other types.
        // Stream serialized JSON directly into output, escaping quotes for CSV.
        if (!append("\""))
            return false;

        CsvEscapedBufferWriter writer(output, sizeof(output));
        size_t n = serializeJson(v, writer);
        if (!writer.ok || n == 0)
            return false;

        return append("\"");
    };

    auto appendComma = [&]() -> bool { return append(","); };

    if (!sdInitialized) {
        printModuleName("Failed to log! SD card not Initialized!");
        success = false;
        return success;
    }

    // Open the file in read/write mode, create if needed, append to end.
    myFile = sd.open(fileName, O_RDWR | O_CREAT | O_APPEND);
    if (!myFile) {
        printModuleName("Failed to open log file!");
        success = false;
        return success;
    }

    bool shouldWriteHeaders = myFile.available() <= 3;

    // Keep each CSV schema fixed: rotate to a new CSV when module/key schema changes.
    if (!shouldWriteHeaders && schemaHashInitialized &&
        (newHash1 != currentSchemaHash1 || newHash2 != currentSchemaHash2)) {
        myFile.close();

        file_count++;
        current_batch = 0;
        setCurrentLogFileNames();

        myFile = sd.open(fileName, O_RDWR | O_CREAT | O_APPEND);
        if (!myFile) {
            printModuleName("Failed to open rotated log file!");
            success = false;
            return success;
        }

        shouldWriteHeaders = true;

        char schemaOutput[OUTPUT_SIZE];
        snprintf_P(schemaOutput, OUTPUT_SIZE, PSTR("CSV schema changed, rotating to %s"), fileName);
        printModuleName(schemaOutput);
    }

    // If this file has never been written to before, create headers and pin schema hashes.
    if (shouldWriteHeaders) {
        if (!myFile.timestamp(T_CREATE, currentTime.year(), currentTime.month(), currentTime.day(),
                              currentTime.hour(), currentTime.minute(), currentTime.second()))
            time_error = true;

        writeHeaders();
        currentSchemaHash1 = newHash1;
        currentSchemaHash2 = newHash2;
        schemaHashInitialized = true;
    } else if (!schemaHashInitialized) {
        currentSchemaHash1 = newHash1;
        currentSchemaHash2 = newHash2;
        schemaHashInitialized = true;
    }

    if (!append(manInst->get_device_name()))
        truncated = true;
    if (!appendComma())
        truncated = true;

    /* To cast int to a const char* we use snprintf */
    char inst[12];
    snprintf(inst, sizeof(inst), "%d", manInst->get_instance_num());
    if (!append(inst))
        truncated = true;
    if (!appendComma())
        truncated = true;

    // Write the Instance data that isn't included in the JSON packet
    if (!myFile.print(output))
        success = false;
    output[0] = '\0'; // Start a new string keeping the total buffer length available

    JsonObject document = manInst->getDocument().as<JsonObject>();

    // If there is a key that contains timestamp data then include that separately
    if (document.containsKey("timestamp")) {
        char utcArr[21];
        char localArr[21];
        const char *utcSrc = document["timestamp"]["time_utc"] | "";
        const char *localSrc = document["timestamp"]["time_local"] | "";
        snprintf(utcArr, sizeof(utcArr), "%s", utcSrc);
        snprintf(localArr, sizeof(localArr), "%s", localSrc);

        // Format date with spaces when logging to SD
        char *indexPointer = strchr(utcArr, 'Z');
        if (indexPointer) {
            if (strlen(utcArr) > 10)
                utcArr[10] = ' ';
            *indexPointer = '\0';
        }

        // Format date with spaces when logging to SD
        indexPointer = strchr(localArr, 'Z');
        if (indexPointer) {
            if (strlen(localArr) > 10)
                localArr[10] = ' ';
            *indexPointer = '\0';
        }

        // Format the timestamp in the CSV file
        if (!append(utcArr))
            truncated = true;
        if (!appendComma())
            truncated = true;
        if (!append(localArr))
            truncated = true;
        if (!appendComma())
            truncated = true;
    }

    // Get the contents containing the rest of the sensor data
    JsonArray contentsArray = document["contents"].as<JsonArray>();

    for (JsonVariant v : contentsArray) {
        for (JsonPair keyValue : v.as<JsonObject>()["data"].as<JsonObject>()) {
            if (!appendJsonValue(keyValue.value()))
                truncated = true;
            if (!appendComma())
                truncated = true;
        }
    }

    // Write the matching data into the CSV file
    if (!myFile.println(output))
        success = false;

    // Set the last modified date
    if (!myFile.timestamp(T_WRITE, currentTime.year(), currentTime.month(), currentTime.day(),
                          currentTime.hour(), currentTime.minute(), currentTime.second()))
        time_error = true;

    if (!myFile.close())
        success = false;

    snprintf_P(output, sizeof(output), PSTR("Logged data to %s: success=%s: truncated=%s: time=%s"),
               fileName, success ? "true" : "false", truncated ? "true" : "false",
               time_error ? "failed" : "success");
    LOG(output);

    if (batch_size > 0)
        logBatch();

    return success && !truncated;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::begin() {

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
        printModuleName("Failed to Initialize SD Card! SD Card functionality will be disabled, is "
                        "there an SD card inserted into the device?");
        return false;
    } else {
        // Make a debug folder if it doesn't already exist
        if (!sd.exists("debug"))
            sd.mkdir("debug");

        printModuleName("Successfully initialized SD Card!");
    }

    // Only should be run on the first initialize not when it wakes up from sleep
    if (!sdInitialized) {
        // Try to open the root of the file system so we can get the files on the device
        if (!root.open("/", O_RDONLY)) {
            printModuleName("ERROR");
            ERROR(F("Failed to open root file system on SD Card!"));
            printModuleName("After ERROR");
            return false;
        }
        updateCurrentFileName();
    }

    // Once the SD card has initialized the first round through we don't want to update the file
    // name
    sdInitialized = true;

    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::updateCurrentFileName() {
    char f_name[260];
    const char *csvBase = (strlen(overrideFileName) > 0) ? overrideFileName : device_name;
    const size_t csvBaseLen = strlen(csvBase);
    int maxCsvIndex = -1;

    // What number we need to append to the file name
    file_count = 0;

    // While there is a next file to open, open it
    while (scanningFile.openNext(&root)) {
        scanningFile.getName(f_name, sizeof(f_name));

        // Match only files of the exact form: <base><number>.csv
        if (strncmp(f_name, csvBase, csvBaseLen) == 0) {
            const char *indexStart = f_name + csvBaseLen;

            // Parse contiguous decimal digits after the base.
            int parsedIndex = 0;
            int digitCount = 0;
            while (*indexStart >= '0' && *indexStart <= '9') {
                parsedIndex = (parsedIndex * 10) + (*indexStart - '0');
                digitCount++;
                indexStart++;
            }

            // Valid CSV candidate only if digits were present and extension is exactly ".csv".
            if (digitCount > 0 && strcmp(indexStart, ".csv") == 0) {
                if (parsedIndex > maxCsvIndex) {
                    maxCsvIndex = parsedIndex;
                }
            }
        }
        scanningFile.close();
    }

    file_count = maxCsvIndex + 1;

    setCurrentLogFileNames();
    currentSchemaHash1 = 0;
    currentSchemaHash2 = 0;
    schemaHashInitialized = false;

    // Close the root file after we have decided what to name the next file
    root.close();

    char output[OUTPUT_SIZE];
    snprintf_P(output, OUTPUT_SIZE, PSTR("Data will be logged to %s"), fileName);
    printModuleName(output);

    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

MemPool::Lease SDManager::readFileLease(const char *fileName) {
    if (!sdInitialized) {
        printModuleName("Failed to read! SD card not Initialized!");
        return MemPool::Lease();
    }

    File file = sd.open(fileName, O_RDONLY);
    if (!file) {
        printModuleName("Failed to open file!");
        return MemPool::Lease();
    }

    size_t byteCount = 0;
    while (file.available()) {
        if (file.read() < 0) {
            file.close();
            printModuleName("Failed to count file contents!");
            return MemPool::Lease();
        }
        byteCount++;
    }
    file.close();

    MemPool::Lease lease = manInst->getPool().allocLease(byteCount + 1, "sd_file");
    if (!lease) {
        printModuleName("Failed to allocate memory-pool lease for file read!");
        return MemPool::Lease();
    }

    uint8_t *buffer = lease.bytes();
    if (buffer == nullptr) {
        return MemPool::Lease();
    }

    file = sd.open(fileName, O_RDONLY);
    if (!file) {
        printModuleName("Failed to open file!");
        return MemPool::Lease();
    }

    size_t index = 0;
    while (file.available() && index < byteCount) {
        int value = file.read();
        if (value < 0) {
            file.close();
            printModuleName("Failed while reading file into lease!");
            return MemPool::Lease();
        }

        buffer[index] = (uint8_t)value;
        index++;
    }
    file.close();

    if (index != byteCount) {
        printModuleName("Failed to read full file into lease!");
        return MemPool::Lease();
    }

    buffer[index] = '\0';
    return lease;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
DeserializationError SDManager::deserializeJsonFile(const char *fileName, JsonDocument &doc) {
    MemPool::Lease lease = readFileLease(fileName);
    if (!lease || lease.chars() == nullptr) {
        return DeserializationError::EmptyInput;
    }
    const char *json = lease.chars();
    return deserializeJson(doc, json );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool SDManager::streamFile(const char *fileName, size_t chunkBytes, StreamChunkCallback cb,
                           void *userCtx) {
    if (!sdInitialized) {
        printModuleName("Failed to stream! SD card not Initialized!");
        return false;
    }

    if (chunkBytes == 0 || cb == nullptr) {
        printModuleName("Failed to stream! Invalid chunk size or callback.");
        return false;
    }

    File file = sd.open(fileName, O_RDONLY);
    if (!file) {
        printModuleName("Failed to open file for streaming!");
        return false;
    }

    size_t fileOffset = 0;
    size_t chunkIndex = 0;

    while (true) {
        MemPool::Lease lease = manInst->getPool().allocLease(chunkBytes, "sd_stream");
        if (!lease) {
            file.close();
            printModuleName("Failed to allocate streaming chunk from pool!");
            return false;
        }

        uint8_t *buffer = lease.bytes();
        if (buffer == nullptr) {
            file.close();
            return false;
        }

        size_t bytesRead = 0;
        while (bytesRead < chunkBytes && file.available()) {
            int value = file.read();
            if (value < 0) {
                file.close();
                printModuleName("Streaming read failed!");
                return false;
            }

            buffer[bytesRead] = (uint8_t)value;
            bytesRead++;
        }

        bool eof = !file.available();
        if (bytesRead == 0) {
            if (eof) {
                break;
            }
            file.close();
            printModuleName("Streaming stalled before EOF!");
            return false;
        }

        bool shouldContinue = cb(buffer, bytesRead, fileOffset, chunkIndex, eof, userCtx);
        if (!lease.release()) {
            file.close();
            return false;
        }

        fileOffset += bytesRead;
        chunkIndex++;

        if (!shouldContinue) {
            file.close();
            return false;
        }

        if (eof) {
            break;
        }
    }

    file.close();
    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
MemPool::Stats SDManager::getPoolStats() const { return manInst->getPool().stats(); }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void SDManager::printPoolStats() {
    MemPool::Stats stats = getPoolStats();

    char output[OUTPUT_SIZE];
    snprintf(output, sizeof(output), "Pool blocks used/free/total: %u/%u/%u",
             (unsigned int)stats.usedBlocks, (unsigned int)stats.freeBlocks,
             (unsigned int)stats.totalBlocks);
    printModuleName(output);

    snprintf(output, sizeof(output), "Pool bytes used/free/total: %u/%u/%u",
             (unsigned int)stats.bytesUsed, (unsigned int)stats.bytesFree,
             (unsigned int)stats.bytesTotal);
    printModuleName(output);

    snprintf(output, sizeof(output), "Pool leases=%u highWater=%u failedAllocs=%u",
             (unsigned int)stats.activeLeases, (unsigned int)stats.highWaterBlocks,
             (unsigned int)stats.failedAllocs);
    printModuleName(output);

    snprintf(output, sizeof(output),
             "Pool calls alloc/release/read/write/clear: %lu/%lu/%lu/%lu/%lu",
             (unsigned long)stats.allocCalls, (unsigned long)stats.releaseCalls,
             (unsigned long)stats.readCalls, (unsigned long)stats.writeCalls,
             (unsigned long)stats.clearCalls);
    printModuleName(output);

    snprintf(output, sizeof(output), "Pool bytes req/granted/released: %lu/%lu/%lu",
             (unsigned long)stats.bytesRequested, (unsigned long)stats.bytesGranted,
             (unsigned long)stats.bytesReleased);
    printModuleName(output);

    snprintf(output, sizeof(output), "Pool alloc fails tooLarge/noRun/noSlot: %u/%u/%u",
             (unsigned int)stats.failedTooLarge, (unsigned int)stats.failedNoContiguousRun,
             (unsigned int)stats.failedNoLeaseSlot);
    printModuleName(output);

    snprintf(output, sizeof(output), "Pool release fails invalid/corrupt: %u/%u",
             (unsigned int)stats.failedReleaseInvalid, (unsigned int)stats.failedReleaseCorrupt);
    printModuleName(output);

    snprintf(output, sizeof(output), "Pool max contiguous free blocks: %u",
             (unsigned int)stats.maxContiguousFreeBlocks);
    printModuleName(output);

    snprintf(output, sizeof(output), "System RAM free/total: %u/%u",
             (unsigned int)stats.systemRamFreeBytes, (unsigned int)stats.systemRamTotalBytes);
    printModuleName(output);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void SDManager::dumpActiveLeases() {
    size_t count = manInst->getPool().dumpActiveLeases(poolLeaseDumpSink, this);
    if (count == 0) {
        printModuleName("No active pool leases.");
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void SDManager::logBatch() {
    MemPool::Lease jsonLease = manInst->getPool().allocLease(MAX_JSON_SIZE, "sd_batch");
    if (!jsonLease) {
        printModuleName("Failed to allocate memory-pool lease for batch log!");
        return;
    }

    const char *f_name = getBatchFilename();
    // We want to clear the file after the batch size has been exceeded
    if (current_batch >= batch_size) {
        current_batch = 0;
        myFile = sd.open(f_name, O_WRITE | O_TRUNC | O_APPEND);
    } else {
        myFile = sd.open(f_name, O_WRITE | O_CREAT | O_APPEND);
    }
    // Check if the file has been opened properly and write the JSON packet to one line
    if (myFile) {

        manInst->getJSONString(jsonLease.chars());
        myFile.println(jsonLease.chars());
        myFile.close();
        current_batch++;

    } else {
        printModuleName("Failed to open file!");
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
