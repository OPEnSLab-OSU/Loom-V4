#pragma once

#include "../../Loom_Manager.h"
#include "../../Module.h"

#include <vector>
#include <tuple>
#include <algorithm>
#include "Wire.h"

// I2C Sensors Used by Loom
#include "../../Sensors/I2C/Loom_ADS1115/Loom_ADS1115.h"
#include "../../Sensors/I2C/Loom_MPU6050/Loom_MPU6050.h"
#include "../../Sensors/I2C/Loom_MS5803/Loom_MS5803.h"
#include "../../Sensors/I2C/Loom_SHT31/Loom_SHT31.h"
#include "../../Sensors/I2C/Loom_TSL2591/Loom_TSL2591.h"
#include "../../Sensors/I2C/Loom_STEMMA/Loom_STEMMA.h"
#include "../../Sensors/I2C/Loom_MB1232/Loom_MB1232.h"
#include "../../Sensors/I2C/Loom_K30/Loom_K30.h"
#include "../../Sensors/I2C/Loom_MMA8451/Loom_MMA8451.h"
#include "../../Sensors/I2C/Loom_ZXGesture/Loom_ZXGesture.h"

/**
 * Adds Hot Swappable functionality for TCA9548 multiplexer
 * 
 * NOTE: This significantly increases flash size and resulting storage used
 * 
 * @author Will Richards
 */ 
class Loom_Multiplexer : public Module{
	protected:

        /* These aren't used with the Mux */
        
       

    public:

		/* Loomified generalized calls*/
		void initialize() override;
		void measure() override;
		void package() override;
		void power_down() override; 
		void power_up() override;

        /**
         * Construct a new Multiplexer
         * 
         * @param man Reference to the manager
         */ 
        Loom_Multiplexer(Manager& man);

		// Destructor removes all new sensor instances
		~Loom_Multiplexer();
        
    private:
        Manager* manInst;                                       // Instance of the manager
		byte activeMuxAddr;										// The port which we want to try to communicate over
		const uint8_t numPorts = 8;								// Number of ports on the multiplexer

		std::vector<std::tuple<byte, Module*, int>> sensors;			// List of sensors

        void selectPin(uint8_t pin);                            // Select which pin of the multiplexer to transmit to
		void disableChannels();									// Disables all channels on the Multiplexer
		bool isDeviceConnected(byte addr);						// Check if there is a device at the specified address

		void refreshSensors();									// Checks to see if any new sensors were swapped in allows for hot swapping
		Module* loadSensor(const byte addr);					// Load the correct sensor based on the I2C address

		// Used to optimize searching for sensors:
		// search addresses in array rather than 0-127 
		const std::array<byte, 21> known_addresses = 
		{
			0x10, ///< ZXGESTURESENSOR
			0x11, ///< ZXGESTURESENSOR
			0x19, ///< LIS3DH
			0x1C, ///< MMA8451
			0x1D, ///< MMA8451
			0x29, ///< TSL2591
			0x36, ///< STEMMA
			0x44, ///< SHT31D
			0x45, ///< SHT31D
			0x48, ///< ADS1115
			0x49, ///< AS726X / AS7265X
			0x68, ///< K30
			0x69, ///< MPU6050
			0x70, ///< MB1232
			0x76, ///< MS5803
			0x77  ///< MS5803
		};

		/**
		 * Possible alternate addresses for the TCA9548
		 */ 
		const std::array<byte, 9>  alt_addresses = {
			0x70,
			0x71,
			0x72,
			0x73,
			0x74,
			0x75,
			0x76,
			0x77,
			0x78
		};

};