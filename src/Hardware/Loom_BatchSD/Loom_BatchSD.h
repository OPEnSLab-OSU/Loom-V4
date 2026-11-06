#pragma once

#include <vector>
#include "../Loom_Hypnos/Loom_Hypnos.h"

/**
 * Basic wrapper for SD to manage batch uploading
 */ 
class Loom_BatchSD{
    public:

        /**
         * Construct a new BatchSD instance
         * 
         * @param hypnos Reference to the hypnos to manage the SD card
         * @param keepUnsent Keep a full batch until clear() is called, so a failed upload is retried with the next
         *                   one instead of being emptied at the next log (call clear() after each successful upload)
         */ 
        Loom_BatchSD(Loom_Hypnos& hypnos, int batchSize, bool keepUnsent = false);

        /**
         * Returns if we should publish the data on this batch
         */ 
        bool shouldPublish();

        /**
         * Return a pointer to the open memory read from arduino
         */ 
        File& getBatch();

        /**
         * Get the specified size of the batch
         */ 
        int getBatchSize() { return batchSize; };

        /**
         * Get the current batch we are on
         */ 
        int getCurrentBatch() { return sdMan->getCurrentBatch(); };

        /**
         * Empty the batch once it has been uploaded, so a reboot doesn't carry it over and upload it again
         */
        void clear() { sdMan->clearBatch(); };

    private:
        SDManager* sdMan = nullptr;                 // Pointer to the SD manager
        int batchSize;                              // Batch size to log to
};