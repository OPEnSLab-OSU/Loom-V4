/**
 * Voltage check example with minimum acceptable voltage and voltage flagging. 
 * Uses analog class as the basis for voltage checks
 * 
 */

 #include <Loom_Manager.h>
 #include <Logger.h>
 #include <Hardware/Loom_Hypnos/Loom_Hypnos.h>


Manager man("Device", 1);

Loom_Hypnos hypnos(man, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, false, false);

void setup(){
ENABLE_SD_LOGGING;
ENABLE_FUNC_SUMMARIES;

// Start the serial monitor
manager.beginSerial();

// Begin Initilization
manager.initialize();
}

void loop(){

// Run voltage check with default args (voltage_min = 0.0, pin = A7, scale = 2.0)
// Scale refers to the voltage divisor that reduces a higher voltage to a lower acceptable voltage. 
// Vout = Vsource x Resistance2 / (Resistance1 + Resistance2)
hypnos.checkVoltage(float vmin = 4.0f, int pin = A7, float scale = 2.0);

manager.pause(5000);
}