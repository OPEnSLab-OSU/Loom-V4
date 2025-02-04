#include "DummyModule.h"
#include <Logger.h>
#include <cstdio>


//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Dummy::package(){
    JsonObject json = manInst->get_data_object(getModuleName());

    char output_buffer[200] = {};
    snprintf(output_buffer, sizeof(output_buffer), "packaging module `%s`...", this->getModuleName());
    LOG(output_buffer);

    json["randomData"] = "A very big and long string that should hopefully trigger fragmentation by being long and big";
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
