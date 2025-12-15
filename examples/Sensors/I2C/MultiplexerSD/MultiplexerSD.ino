/**
 
This is an example use case for Multiplexer SD functionality
This allows the user to pull sensor addresses from SD card and use for initialize and refreshing
Must pass in manager, hypnos instance, and filename in multiplexer constructor
File must include a json of the following structure:
{
  ...
  ...
  ...
  "sensors": [{"addr": "0x__", "name": "___"}, {"addr": "0x__", "name":"___"}, {"addr": "0x__", "name": "___"}, ...]
  ...
  ...
  }  

Purpose of this example is to make sure filename is recognized, and 
addresses are retrieved. Output should reflect number of addresses in SD

Name usage will be implemented in the future, for now, will just need addresses 

MANAGER MUST BE INCLUDED FIRST IN ALL CODE
*/


#include <Loom_Manager.h>


#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>


#include <Hardware/Loom_Multiplexer/Loom_Multiplexer.h>


Manager manager("Device", 1);


// Create a new Hypnos object setting the version to determine the SD Chip select pin
//Loom_Hypnos(Manager& man, HYPNOS_VERSION version, TIME_ZONE zone, bool use_custom_time = false, bool useSD = true)
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);

// pass in manager, hypnos for pulling file from SD card, and filename
Loom_Multiplexer mux(manager, hypnos, "custom_addresses.json");




void setup() {


  manager.beginSerial();
 
  // Enable the hypnos rails
  hypnos.enable();

  // calls loadAddressesFromSD if mux constructor above was used
  manager.initialize();




 
}


void loop() {
 
}