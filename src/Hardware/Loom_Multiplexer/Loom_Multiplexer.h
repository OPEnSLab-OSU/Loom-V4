#pragma once

#include "../../Loom_Manager.h"
#include "../../Module.h"
#include "../Loom_Hypnos/Loom_Hypnos.h"

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
#include "../../Sensors/I2C/Loom_DFMultiGasSensor/Loom_DFMultiGasSensor.h"
#include "../../Sensors/I2C/Loom_T6793/Loom_T6793.h"
#include "../../Sensors/I2C/Loom_SEN55/Loom_SEN55.h"


/**
 * Adds Hot Swappable functionality for TCA9548 multiplexer
 * 
 * NOTE: This significantly increases flash size and resulting storage used
 * 
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

		struct addrNamePair{
			byte addr;
			const char* name;
		};

        /**
         * Construct a new Multiplexer
         * 
         * @param man Reference to the manager
         */ 
        Loom_Multiplexer(Manager& man);

		/**
         * Construct a new Multiplexer with specified addresses 
         * 
         * @param man Reference to the manager
		 * @param addresses Vector of addresses you want to pass in to known addresses
         */ 
        Loom_Multiplexer(Manager& man, const std::vector<addrNamePair>& addresses);

		/**
		 * @brief Construct a new Loom_Multiplexer object with hypnos object
		 * 
		 * @param man Reference to the manager
		 * @param hypnos hypnos object, will allow for SD card access
		 */
		Loom_Multiplexer(Manager& man, Loom_Hypnos& hypnos, const char* filename);

		// Destructor removes all new sensor instances
		~Loom_Multiplexer();

		/**
         * @brief Load custom multiplexer addresses stored in SD 
         * 
         * @param fileName The file name of the json in root of SD card
         */
        void loadAddressesFromSD(const char* fileName);

        
    private:
        Manager* manInst;                                       // Instance of the manager
		SDManager* sdMan = nullptr; 							// pointer to the SD manager
		const char* sdFile = nullptr;							// name of file on SD card				
		byte activeMuxAddr;										// The port which we want to try to communicate over
		const uint8_t numPorts = 8;								// Number of ports on the multiplexer
		

		std::vector<std::tuple<byte, Module*, int>> sensors;			// List of sensors

        void selectPin(uint8_t pin);                            // Select which pin of the multiplexer to transmit to
		void disableChannels();									// Disables all channels on the Multiplexer
		bool isDeviceConnected(byte addr);						// Check if there is a device at the specified address

		void refreshSensors();									// Checks to see if any new sensors were swapped in allows for hot swapping
		Module* loadSensor(const addrNamePair& sensor);					// Load the correct sensor based on the I2C address

		std::vector<addrNamePair> known_addresses = {};

		// Used to optimize searching for sensors:
		// search addresses in array rather than 0-127 
		const std::vector<addrNamePair> default_addresses = 
		{
			{0x10, "Loom_ZXGesture"}, 		///< ZXGESTURESENSOR
			{0x11,"Loom_ZXGesture"}, 		///< ZXGESTURESENSOR
			{0x15, "Loom_T6793"},			///< T6793
			{0x19,"Loom_LIS3DH"}, 			///< LIS3DH
			{0x1C, "Loom_MMA8541"},			///< MMA8451
			{0x1D, "Loom_MMA8541"},			///< MMA8451
			{0x29, "Loom_TSL2591"},			///< TSL2591
			{0x36, "Loom_STEMMA"},			///< STEMMA
			{0x44, "Loom_SHT31"},			///< SHT31D
			{0x45, "Loom_SHT31"}, 			///< SHT31D
			{0x48, "Loom_ADS1115"},			///< ADS1115
			// {0x49, "Loom_AS7262"},		///< AS726X / AS7265X
			{0x68, "Loom_K30"},				///< K30
			{0x69, "Loom_SEN55"},			///< SEN55
			{0x70, "Loom_MB1232"},			///< MB1232
			{0x74, "Loom_MultiGasSensor"},	///< DFMultiGasSensor
			{0x76, "Loom_MS5803"},			///< MS5803
			{0x77, "Loom_MS5803"}			///< MS5803
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