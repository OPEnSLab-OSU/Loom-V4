#pragma once

#include "Module.h"
#include "Loom_Manager.h"

/**
* Loom Implementation for ACS712 current sensor
*
* @author Zak Watts
**/

class Loom_ACS712 : public Module {

    protected:
	void initialize() override {};
	void power_up() override {};
	void power_down() override {};

    public:

        Loom_ACS712(
		Manager &man, 
		int port = A0, 
		int scaleFactor = 185, 
		int ACSOffset = 2500
		);

        void measure() override;
        void package() override;

    private:

        Manager *manInst;   //Manager instance
        int analogPort;     //Analog pin
        int mVperAmp;       //Scale factor, use: 185 for 5A module, 100 for 20A module, 66 for 30A module
        int rawVal;         //Raw value read from the analog
        int offset;         //ACS offset
        double voltage;     //Voltage value
        double amps;        //Amps value
};

