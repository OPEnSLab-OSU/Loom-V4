#include "Loom_BatchSD.h"
#include "Logger.h"

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
    char* fileOutput = sdMan->readFile(sdMan->getBatchFilename());
    String currentLine = "";

    // Convert each batch into a string and push it into the vector of batches
    for(int i = 0; i < strlen(fileOutput); i++){
        if(fileOutput[i] == '\n'){
            batch.push_back(currentLine);
            currentLine = "";
        }
        else{
            currentLine += fileOutput[i];
        }
    }

    free(fileOutput);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////