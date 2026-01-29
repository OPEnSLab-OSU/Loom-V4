#pragma once

#include "Adapter.h"
#include "../../Logging/Loom_MongoDB/Loom_MongoDB.h"


class MongoDB_Adapter : public Adapter {
    
    public:
    MongoDB_Adapter(Loom_MongoDB* passedMongoDBModule){
        mongoDBModule = passedMongoDBModule;

    }

    virtual bool sendHeartbeat(const char* metadata) override {
        return mongoDbModule->publishMetadata(metadata);

    }


    private:
    Loom_MongoDB* mongoDBModule = nullptr;

}
