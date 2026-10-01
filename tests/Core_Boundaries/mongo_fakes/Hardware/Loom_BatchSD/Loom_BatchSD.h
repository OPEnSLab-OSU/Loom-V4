#pragma once
#include "Arduino.h"

// SD boundary failures only; no claim to emulate SdFat/media or power loss.
struct FakeBatchPlan {
    std::string bytes;
    int count = 2;
    int threshold = 2;
    int openCalls = 0, clearCalls = 0;
    bool openOK = true, closeOK = true, clearOK = true, seekOK = true;
    uint8_t readError = 0;
};
extern FakeBatchPlan batchPlan;

class File : public Stream {
  public:
    explicit operator bool() const { return batchPlan.openOK; }
    int available() override { return static_cast<int>(batchPlan.bytes.size() - position); }
    int read() override {
        return available() ? static_cast<unsigned char>(batchPlan.bytes[position++]) : -1;
    }
    int peek() override {
        return available() ? static_cast<unsigned char>(batchPlan.bytes[position]) : -1;
    }
    uint32_t curPosition() const { return static_cast<uint32_t>(position); }
    bool seekSet(uint32_t next) {
        if (!batchPlan.seekOK || next > batchPlan.bytes.size()) { return false; }
        position = next;
        return true;
    }
    uint8_t getError() const { return batchPlan.readError; }
    bool close() { return batchPlan.closeOK; }
  private:
    size_t position = 0;
};
class Loom_BatchSD {
  public:
    bool shouldPublish() { return batchPlan.count >= batchPlan.threshold; }
    int getCurrentBatch() const { return batchPlan.count; }
    int getBatchSize() const { return batchPlan.threshold; }
    const char *getBatchFilename() const { return "node_0-Batch.txt"; }
    File openBatch() { ++batchPlan.openCalls; return File{}; }
    bool markPublished() {
        ++batchPlan.clearCalls;
        if (!batchPlan.clearOK) { return false; }
        batchPlan.bytes.clear();
        batchPlan.count = 0;
        return true;
    }
};
