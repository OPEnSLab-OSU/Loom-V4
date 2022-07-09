#include "Loom_MPU6050.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MPU6050::Loom_MPU6050(Manager& man, const bool autoCalibrate) : Module("MPU6050"), manInst(&man), mpu(Wire), autoCali(autoCalibrate){ manInst->registerModule(this); }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MPU6050::initialize(){
    Wire.begin();
    mpu.begin();

    // If we want to auto calibrate the gyro on initialize
    if(autoCali){
        printModuleName(); Serial.println("Calibrating Gyroscope...");
        printModuleName();
        mpu.calcGyroOffsets(true);
        Serial.println();
    }

    printModuleName(); Serial.println("Successfully initialized sensor!");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

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
    json["ax"] = acc[0];
    json["ay"] = acc[1];
    json["az"] = acc[2];

    // Rotation Rate
    json["gx"] = rate[0];
    json["gy"] = rate[1];
    json["gz"] = rate[2];

    // Angle
    json["roll"] = angle[0];
    json["pitch"] = angle[1];
    json["yaw"] = angle[2];
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MPU6050::calibrate(){
    printModuleName(); Serial.println("Calibrating Gyroscope...");
    printModuleName();
    mpu.calcGyroOffsets(true);
    Serial.println();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////