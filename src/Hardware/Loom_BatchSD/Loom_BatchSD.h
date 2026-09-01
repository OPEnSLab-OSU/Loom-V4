#pragma once

#include "../Loom_Hypnos/Loom_Hypnos.h"
#include <vector>

/**
 * Basic wrapper for SD to manage batch uploading
 */
class Loom_BatchSD {
  public:
    /**
     * Construct a new BatchSD instance
     *
     * @param hypnos Reference to the hypnos to manage the SD card
     */
    Loom_BatchSD(Loom_Hypnos &hypnos, int batchSize);

    /**
     * Returns if we should publish the data on this batch
     */
    bool shouldPublish();

    /**
     * Return a pointer to the open memory read from arduino
     */
    File &getBatch();

    /**
     * Open an independent batch reader that ordinary SD logging cannot replace or close.
     */
    File openBatch();

    /**
     * Clear records after a complete successful publish.
     *
     * @return true when the file was truncated and the counter reset
     */
    bool markPublished();

    /**
     * Get the specified size of the batch
     */
    int getBatchSize() const { return batchSize; };

    /**
     * Get the current batch we are on
     */
    int getCurrentBatch() const { return sdMan != nullptr ? sdMan->getCurrentBatch() : 0; };

  private:
    SDManager *sdMan = nullptr; // Pointer to the SD manager
    int batchSize;              // Batch size to log to
};
