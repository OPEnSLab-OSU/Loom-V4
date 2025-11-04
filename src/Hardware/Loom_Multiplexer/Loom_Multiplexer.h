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
 * TODO:Rory Issue #209- Change array to struct array for default sensors, pull custom address 
 * from SD, but would like a serial interface input for custom addresses as an option. 
 * , call Change sesnor address to new one. Will need
 * to go through existing code that uses these addresses and change method
 * for indexing. ALSO, measure memory usage by looking at the JSON data.
 * @author Will Richards
 */ 
class Loom_Multiplexer : public Module{
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
        Loom_Multiplexer(Manager& man, bool useSD = false);

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
		
		/**
		 * 
		 * @brief function that pulls config file from SD, may have return value be bool, which will indicate if we want to use known_addresses
		 * vs default_addresses
		 * @author Rory L. Groom
		 */
		void loadConfigFromSD();

		
		struct Sensor {
			char name[20];
			byte addr[4];
		}

		// used for indexing array. Will want to use address array by doing default_addresses[zxgesturesensor].addr for custom cases
		const enum sensorIndex = {
			ZXGESTURESENSOR
			LIS3DH
			MMA8451
			TSL2591
			STEMMA
			SHT31D
			ADS1115
			AS726X_AS7265X
			K30
			MPU6050
			MB1232
			MS5803
		};
		
		//use following line of code IFF using custom addresses, may put in .cpp 
		//	std::array<sensor, 21> custom_addresses = default_addresses; 
		
		
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
		 * @brief array to use if using custom addresses, maps addresses to sensornames. 
		 * With the use of a look up table, having names with the addresses may be redundant,
		 * could potentially save some space. 
		 * @author Rory L. Groom
		 */
		const std::array<sensor, 21> default_addresses = 
		{
			{{"ZXGESTURESENSOR"}, {0x10, 0x11}}, 
			{{"LIS3DH"}, {0x19}},
			{{"MMA8451"}, {0x1C, 0x1D}},
			{{"TSL2591"}, {0x29}},
			{{"STEMMA"}, {0x36}},
			{{"SHT31D"}, {0x44, 0x45}},
			{{"ADS1115"}, {0x48}},
			{{"AS726X / AS7265X"}, {0x49}},
			{{"K30"}, {0x68}},
			{{"MPU6050"}, {0x69}},
			{{"MB1232"}, {0x70}},
			{{"MS5803"}, {0x76, 0x77}}

		};
		

		/**
		 * Possible alternate addresses for the TCA9548
		 */ 
		const std::array<byte, 9>  alt_addresses = {
			0x71,
			0x72,
			0x73,
			0x74,
			0x75,
			0x78
		};

};