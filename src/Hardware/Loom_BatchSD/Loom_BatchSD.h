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
         */ 
        Loom_BatchSD(Loom_Hypnos& hypnos, int batchSize);

        /**
         * Returns if we should publish the data on this batch
         */ 
        bool shouldPublish();

        /**
         * Get the current batch of data as a vector of strings
         */ 
        void getBatch( std::vector<String>& batch);

        /**
         * Get the specified size of the batch
         */ 
        int getBatchSize() { return batchSize; };

        /**
         * Get the current batch we are on
         */ 
        int getCurrentBatch() { return sdMan->getCurrentBatch(); };

    private:
        SDManager* sdMan = nullptr;                 // Pointer to the SD manager
        int batchSize;                              // Batch size to log to
};