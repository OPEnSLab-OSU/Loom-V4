/** * In lab use case example for the WeatherChimes project *
 * This project uses SHT31, Teros10, TippingBucket, TSL2591, and two MS5803 sensors to log environment data and logs it to both the SD card and also MQTT/MongoDB *
  * MANAGER MUST BE INCLUDED FIRST IN ALL CODE */ 
// Configurable WeatherChimes 2026 sketch.
#include <Loom_Manager.h>
  #include <Logger.h> 
  #include <Hardware/Loom_Hypnos/Loom_Hypnos.h> 
  #include <Sensors/Loom_Analog/Loom_Analog.h> 
  #include <Sensors/I2C/Loom_SHT31/Loom_SHT31.h> 
  #include <Sensors/I2C/Loom_TSL2591/Loom_TSL2591.h> 
  #include <Sensors/I2C/Loom_MS5803/Loom_MS5803.h> 
  #include <Hardware/Loom_TippingBucket/Loom_TippingBucket.h> 
  #include <Sensors/Analog/Loom_Teros10/Loom_Teros10.h> 
  
  /* ---------------------------------- Hi, this is where we can edit code for each device specifically ---------------------------------------- */ 
  // give the device a name and instance number 
  Manager manager("Device_Name", 1); 

  // use true if the device should use 4G LTE and false if it should not 

  #define USE_LTE true 
  // GLOBAL TOGGLE FOR BATCH MODE 
  #define BATCH_UPLOAD true 

  // Batch tracking 
  uint8_t batchCounter = 0; 
  uint8_t batchcount = 72; 
  
  /* ---------------------------------------- nothing beyond this point should need to change --------------------------------------------------- */ 
  // Pin to have the secondary interrupt triggered from 
  #define INT_PIN A0 
  
  volatile bool sampleFlag = true; 
  volatile bool tipFlag = false; 

  TimeSpan sleepInterval; 

  Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, true); 
  Loom_Analog analog(manager); 
  Loom_SHT31 sht(manager); 
  Loom_TSL2591 tsl(manager); 
  Loom_MS5803 ms_water(manager, 119); 
  Loom_MS5803 ms_air(manager, 118); 
  Loom_TippingBucket bucket(manager, COUNTER_TYPE::MANUAL, 0.01f); 
  Loom_Teros10 teros(manager, A1); 
  
  #if USE_LTE 
    #include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h> 
    #include <Internet/Connectivity/Loom_LTE/Loom_LTE.h> 
    Loom_LTE lte(manager, "hologram","",""); 
    Loom_MongoDB mqtt(manager, lte); 
    #if BATCH_UPLOAD 
      Loom_BatchSD batchSD(hypnos, batchcount); 
    #endif 
  #endif 
  
  float calculateWaterHeight() { 
    return (((ms_water.getPressure()-ms_air.getPressure()) * 100) / (997.77 * 9.81)); 
    } 
  void isrTrigger() { 
    sampleFlag = true; 
    //detachInterrupt(INT_PIN); 
    hypnos.wakeup(); 
    } 

  void tipTrigger() { 
    hypnos.shouldPowerUp = false; 
    tipFlag = true; 
    } 
      
    void setup() { 
      ENABLE_SD_LOGGING; 
      ENABLE_FUNC_SUMMARIES; 
      pinMode(INT_PIN, INPUT); 
      pinMode(LED_BUILTIN, OUTPUT); 
      manager.beginSerial(); 
      #if USE_LTE 
        #if BATCH_UPLOAD 
          lte.setBatchSD(batchSD); 
        #endif 
        hypnos.setNetworkInterface(&lte); 
      #endif 
      
      /////////TESTING///////// 
      // Both power rails should be on when awake 
      hypnos.setWakeConfiguration(POWERRAIL_CONFIG::PR_3V_ON_5V_ON); 
      // Both power rails should remain on when asleep 
      hypnos.setSleepConfiguration(POWERRAIL_CONFIG::PR_3V_ON_5V_ON); 
      /////////////////////// 
      
      hypnos.enable(); 
      sleepInterval = hypnos.getConfigFromSD("SD_config.json"); 
      bucket.setHypnosInstance(hypnos); 
      #if USE_LTE 
        mqtt.loadConfigFromJSON(hypnos.readFile("mqtt_creds.json")); 
      #endif 
      
      manager.initialize(); 
      hypnos.registerInterrupt(isrTrigger); 
      attachInterrupt(INT_PIN, tipTrigger, FALLING); 
      } 
      
      void loop() { 
        if(sampleFlag) { 
          detachInterrupt(INT_PIN); 
          #if BATCH_UPLOAD 
            batchCounter++; 
            if (batchCounter >= batchcount) { 
              TimeSpan extendedSleep = sleepInterval + TimeSpan(0, 0, 2, 0); 
              hypnos.setInterruptDuration(extendedSleep); 
              batchCounter = 0; 
              } 
            else { 
              hypnos.setInterruptDuration(sleepInterval); 
              } 
          #else
            hypnos.setInterruptDuration(sleepInterval); 
          #endif 

          tsl.wake_up();
          
          manager.measure(); 

          tsl.sleep();

          manager.package(); 

          manager.addData("Water", "Height_(m)", calculateWaterHeight());
          manager.display_data(); 
          hypnos.logToSD(); 
          #if USE_LTE 
            #if BATCH_UPLOAD 
              mqtt.publish(batchSD); 
            #else 
              mqtt.publish(); 
            #endif 
          #endif 
          
          hypnos.reattachRTCInterrupt(); 
          attachInterrupt(INT_PIN, tipTrigger, FALLING); sampleFlag = false; 
          } 
          if(tipFlag) { 
            digitalWrite(LED_BUILTIN, HIGH); 
            delay(20); 
            bucket.incrementCount(); 
            tipFlag = false; 
            attachInterrupt(INT_PIN, tipTrigger, FALLING); 
            digitalWrite(LED_BUILTIN, LOW); 
            } 
            
        hypnos.sleep(); 
        }
