#include "Loom_MPU6050.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MPU6050::Loom_MPU6050(Manager& man, bool useMux, const bool autoCalibrate) : I2CDevice("MPU6050"), manInst(&man), mpu(Wire), autoCali(autoCalibrate){ 
    if(!useMux)
        manInst->registerModule(this); 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MPU6050::initialize(){
    Wire.begin();
    mpu.begin();

    // If we want to auto calibrate the gyro on initialize
    if(autoCali){
        calibrate();
    }

    LOG(F("Successfully initialized sensor!"));
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

/*

MPU6050 appears to have the ability to set accelerometer(±2G, ±4G, ±6G, ±8G) and gyro ranges(±250deg/s, ±500deg/s, ±1000deg/s, ± 2000deg/s). 
https://github.com/tockn/MPU6050_tockn/blob/master/src/MPU6050_tockn.cpp defaults to 2G and 500 deg/s
Should add to issues to fix in the future.

@author Reid Pettibone
*/


//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MPU6050::measure(){

    // Pull new data from the sensor
    mpu.update();

    // Get acceleration values
    acc[0] = mpu.getAccX();
    acc[1] = mpu.getAccY();
    acc[2] = mpu.getAccZ();

    // Rate of rotation of each axis
    rate[0] = mpu.getGyroX();
    rate[1] = mpu.getGyroY();
    rate[2] = mpu.getGyroZ();

    angle[0] = mpu.getAngleX();
    angle[1] = mpu.getAngleY();
    angle[2] = mpu.getAngleZ();


}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MPU6050::package(){
    JsonObject json = manInst->get_data_object(getModuleName());

    // Acceleration
    json["ax_g"] = acc[0];
    json["ay_g"] = acc[1];
    json["az_g"] = acc[2];

    // Rotation Rate
    json["gx_°/s"] = rate[0]; 
    json["gy_°/s"] = rate[1];                      
    json["gz_°/s"] = rate[2];

    // Angle
    json["roll_x"] = angle[0];
    json["pitch_y"] = angle[1];
    json["yaw_z"] = angle[2];
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MPU6050::calibrate(){
    LOG(F("Calibrating Gyroscope..."));

    mpu.calcGyroOffsets(true);
    Serial.println();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////