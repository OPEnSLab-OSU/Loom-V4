#include "Loom_BatchSD.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_BatchSD::Loom_BatchSD(Loom_Hypnos& hypnos, int batchSize) : batchSize(batchSize){
    sdMan = hypnos.getSDManager();
    sdMan->setBatchSize(batchSize);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_BatchSD::shouldPublish(){
    return (sdMan->getCurrentBatch() == batchSize);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_BatchSD::getBatch(std::vector<String>& batch){
    batch.clear();
    Serial.println("[BatchSD] Opening Batch File");
    String fileOutput = sdMan->readFile(sdMan->getBatchFilename());
    Serial.println("[BatchSD] Batch File opened");

    String currentLine = "";

    Serial.println("[BatchSD] Reading lines");
    // Convert each batch into a string and push it into the vector of batches
    for(int i = 0; i < fileOutput.length(); i++){
        if(fileOutput[i] == '\n'){
            Serial.println("[BatchSD] Line: " + currentLine);
            batch.push_back(currentLine);
            currentLine = "";
        }
        else{
            currentLine += fileOutput[i];
        }
    }
    Serial.println("[BatchSD] Done Reading lines");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////