/**
 * This is a Loom 4 port of Weed Warden
 * 
 * This is the code for the OPEnS Lab Weed Warden.
 * This code is meant to be uploaded to the feather M0 board and used to detect vegetation. 
 * Default setting for this code are to use the ENDVI algorithm and a threshold of .015 over the calibration value
 * 
 * LOOM MANAGER MUST BE INCLUDED FIRST
 * 
 * original author: Liam Duncan
 * adapted by: Will Richards
*/

#include <Loom_Manager.h>
#include <Logger.h>

#include <Sensors/Loom_Analog/Loom_Analog.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Sensors/I2C/Loom_AS7265X/Loom_AS7265X.h>

Manager manager("WeedWarden", 1);

Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_2, TIME_ZONE::PST, true);
Loom_AS7265X as(manager);
Loom_Analog analog(manager);

/* Defines the available algorithms
 * If custom is slected the user will need to fill out their custom indicator index in the index_algorithm function
*/
enum AlgoChoice{
    endvi,
    ndvi,
    evi,
    psnd,
    custom
};

float threshold = 0;   // variable to hold the calibration threshold
float ndviCal[16];        // array to hold the NDVI values during calibration

// This is the indicator index choice that the sensor will use 
AlgoChoice algorithm_choice = AlgoChoice::endvi;   

/*
    If using an index other than "endvi", this value may need to be changed
    The purpose of this offset value is to ensure that there are minimal false 
    positives by making the calibration threshold value higher than the value of 
    the dirt sample
*/ 
 float offset = .015;   // This value  is the calibration threshold offset 

//=====================================================================
//===                     index_algorithm                          ====
//===                                                              ====
//=== This function will compute the desired indication algorithm  ====
//=== The function currently calculated ENDVI indicator index but  ====
//=== can be changed if desired. to change the index use the       ====
//=== wavelength values to calculate a new index and set it equal  ====
//=== to "index"                                                   ==== 
//=====================================================================
float index_algorithm()
{
    // Pull the wavelength values from the array and 
    // cast them as float values
    uint16_t* values = as.getUV();
    // uint16_t a = (float)values[0];  // 410
    uint16_t b = (float)values[1];  // 435
    uint16_t c = (float)values[2];  // 460
    uint16_t d = (float)values[3];  // 485
    uint16_t e = (float)values[4];  // 510
    uint16_t f = (float)values[5];  // 535

    values = as.getColor();
    uint16_t g = (float)values[0]; // 560
    // uint16_t h = (float)values[1]; // 585
    // uint16_t i = (float)values[2]; // 645
    // uint16_t j = (float)values[3]; // 705
    // uint16_t k = (float)values[4]; // 900
    // uint16_t l = (float)values[5]; // 940
    
    values = as.getNIR();
    // uint16_t r = (float)values[0]; // 610
    uint16_t s = (float)values[1]; // 680
    // uint16_t t = (float)values[2]; // 730
    uint16_t u = (float)values[3]; // 760
    uint16_t v = (float)values[4]; // 810
    uint16_t w = (float)values[5]; // 860

  float index;  // This is the variable that holds the indicator index value

  // This statment will calculate the ENDVI Index
  if(algorithm_choice == AlgoChoice::endvi)
  {
  float en = ((u + v + w + e + f + g) - 2*(b + c + d));  // calculate top half of ENDVI
  float dvi = ((u + v + w + e + f + g) + 2*(b + c + d)); // Calculate bottom half of ENDVI
  index = en/dvi;   // finish calculating the index value
  } // if 

  // This statment will calculte the NDVI index
  if(algorithm_choice == AlgoChoice::ndvi)
    index = (v-s)/(v+s); 

  // This statment will calculate EVI Index 
  if(algorithm_choice == AlgoChoice::evi)
    index = ((2.5*(v-s))/(v+6*s-7.5*b+1)); 

  // This statment will calculate the PSND Index 
  if(algorithm_choice == AlgoChoice::psnd)
    index = (v-f)/(v+f);

  // This statment holds the custom algorithm if the user chooses to have one 
  if(algorithm_choice == AlgoChoice::custom)
    index = 0;  // Custom algorithm defaults to 0. Change this to be the algorithm you would like calculated


  return index;   // return the computed index value
}


//================================================================
//===                     calibrate                           ====
//===                                                         ====
//=== Function will take samples of dirt to find a baseline   ====
//=== NDVI Value that can be used as a threshold              ==== 
//================================================================
void calibrate()
{  
  for(int z=0; z<16; z++) // Loop to take calibration samples
  {
    LOGF("Calibration: %d / 16", z);
    // Collect, package and dispaly data
    manager.measure();
    manager.package();
    manager.display_data();

    ndviCal[z] = index_algorithm();  // Store indicator index values in an array

    LOGF("%.6f", ndviCal[z]);   // print out the calculated indicator index values
   
  } // for loop 

  // loop to sum up the indicator index values 
  for(int i=0; i<16; i++)
    threshold += ndviCal[i];
  
  threshold = (threshold / 16) + offset;  // divide by 16 to average values then add an offset to the value

  // Turn on the HV_Rail_Enable for 6 seconds so that you know the calibration worked
  // In our tests we had the 5V rail hooked up to a 5V buzzer so that we knew when the calibration had finished
  digitalWrite(6, HIGH); 
  delay(6000);               
  digitalWrite(6, LOW); 
} // Calibrate 

// Called initially on startup
void setup(){

    // Wait 20 seconds for the serial monitor to open
    manager.beginSerial();

    // Enable the hypnos, only turn the 3.3v rail on
    hypnos.enable(true, false);

    // Initialize all modules
    manager.initialize();

    // Calibrate the device
    calibrate();    

}

// Called continuously
void loop(){
    manager.measure();
    manager.package();
    manager.display_data();

    LOG("+++++ START +++++");
    SLOG("UV");
    uint16_t* values = as.getUV();
    uint16_t a = (float)values[0]; SLOGF("%u", (unsigned int)a);     // 410
    uint16_t b = (float)values[1]; SLOGF("%u", (unsigned int)b);     // 435
    uint16_t c = (float)values[2]; SLOGF("%u", (unsigned int)c);     // 460
    uint16_t d = (float)values[3]; SLOGF("%u", (unsigned int)d);     // 485
    uint16_t e = (float)values[4]; SLOGF("%u", (unsigned int)e);     // 510
    uint16_t f = (float)values[5]; SLOGF("%u", (unsigned int)f);     // 535

    SLOG("\nColor");
    values = as.getColor();
    uint16_t g = (float)values[0];  SLOGF("%u", (unsigned int)g);    // 560
    uint16_t h = (float)values[1];  SLOGF("%u", (unsigned int)h);    // 585
    uint16_t i = (float)values[2];  SLOGF("%u", (unsigned int)i);    // 645
    uint16_t j = (float)values[3];  SLOGF("%u", (unsigned int)j);    // 705
    uint16_t k = (float)values[4];  SLOGF("%u", (unsigned int)k);    // 900
    uint16_t l = (float)values[5];  SLOGF("%u", (unsigned int)l);    // 940
        
    SLOG("\nNIR");
    values = as.getNIR();
    uint16_t r = (float)values[0];  SLOGF("%u", (unsigned int)r);    // 610
    uint16_t s = (float)values[1];  SLOGF("%u", (unsigned int)s);    // 680
    uint16_t t = (float)values[2];  SLOGF("%u", (unsigned int)t);    // 730
    uint16_t u = (float)values[3];  SLOGF("%u", (unsigned int)u);    // 760
    uint16_t v = (float)values[4];  SLOGF("%u", (unsigned int)v);    // 810
    uint16_t w = (float)values[5];  SLOGF("%u", (unsigned int)w);    // 860

    // Sum up the total of all wavelengths
    float total = a + b + c + d + e + f + g + h + i + j + k + l + r + s + t + u + v + w;
    LOG("++++++++++++++++\n");
    LOG("++++++++++++++++\bTotals");
    float a_w = a/total; LOGF("%.6f", a_w);
    float b_w = b/total; LOGF("%.6f", b_w);
    float c_w = c/total; LOGF("%.6f", c_w);
    float d_w = d/total; LOGF("%.6f", d_w);
    float e_w = b/total; LOGF("%.6f", e_w);
    float f_w = b/total; LOGF("%.6f", f_w);
    float g_w = g/total; LOGF("%.6f", g_w);
    float j_w = j/total; LOGF("%.6f", j_w);
    float k_w = k/total; LOGF("%.6f", k_w);
    float r_w = r/total; LOGF("%.6f", r_w);
    float s_w = s/total; LOGF("%.6f", s_w);
    float t_w = t/total; LOGF("%.6f", t_w);
    float u_w = u/total; LOGF("%.6f", u_w);
    float v_w = v/total; LOGF("%.6f", v_w);
    float w_w = w/total; LOGF("%.6f", w_w);

    // calculate the indicator index Value
    float endvi = index_algorithm(); 

    // Calculate some other indices that are not used

    // PSND
    // float ps = v-f;
    // float nd2 = v+f;
    // float psnd = ps/nd2;

    // EVI   
    float evi = ((2.5*(v-s))/(v+6*s-7.5*b+1));

    // Add Values to the JSON Package so that they will be logged to SD Card
    manager.addData("ENDVI", "ENDVI", endvi);
    manager.addData("EVI", "EVI", evi);
    manager.addData("Threshold", "Threshold", threshold);

    // Calculate normalized ENDVI
    float endvi_w = ((u_w + v_w + w_w + e_w + f_w + g_w) - 2*(b_w + c_w + d_w))/((u_w + v_w + w_w + e_w + f_w + g_w) + 2*(b_w + c_w + d_w));
    
    // Calculate normalized psnd 
    float psnd_w = ((v_w-f_w)/(v_w+f_w));

    // Calculate normalized EVI 
    float evi_w = ((2.5*(v_w-s_w))/(v_w+6*s_w-7.5*b_w+1));

    // Add Values to the JSON Package
    manager.addData("ENDVI_W", "ENDVI_W", endvi_w);
    manager.addData("EVI_W", "EVI_W", evi_w);
    manager.addData("PSND_W", "PSND_W", psnd_w);

    // Log to the SD card
    hypnos.logToSD();

    LOG("++++++++++++++++");
    LOGF("Total=%.6f", total);
    LOGF("EDVI=%.6f", endvi);
    LOGF("Threshold=%.6f", threshold);

    if(threshold < endvi){
        LOG("++++++++++++++++++++++++++++++++++++++");
        LOG("++++++++++ This Is A Target ++++++++++");
        LOG("++++++++++++++++++++++++++++++++++++++");

        digitalWrite(6, HIGH);   // Turn on the HV_Rail_Enable if grass is detected
    }else{
        LOG("----------------------------------");
        LOG("++++++++++ Not A Target ++++++++++");
        LOG("----------------------------------");

        digitalWrite(6, LOW);  // Turn the HV_Rail_Enable off if dirt is detected
    }

    manager.pause(1000);
}
