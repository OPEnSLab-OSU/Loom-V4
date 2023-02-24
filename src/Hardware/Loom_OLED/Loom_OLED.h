#pragma once

#include "Loom_Manager.h"
#include "Module.h"

#include <Adafruit_SSD1306.h>


/**
 * Class for driving the OLED display
 * 
 * @authors Will Richards, Luke Goertzen
 */ 
class Loom_OLED : public Module{
    protected:

        void power_up() override {};
        void power_down() override {}; 
        
        
        // Manager controlled functions
        void measure() override {};
        void initialize() override;
        void package() override {};
        void display_data() override;

    public:

        /** 
         * Different forms of the OLED display
         */
        enum class Version {
            FEATHERWING,	// FeatherWing OLED
            BREAKOUT		// Breakout board
        };

        /** 
         * Different formats to display information in
         */
        enum class Format {
            FOUR,			// 4 Key values
            EIGHT,			// 8 Key values
            SCROLL			// Scrolling
        };

        /**
         * Different freeze behaviors
         */
        enum class FreezeType {
            DISABLE, 		// Freeze disabled
            DATA, 			// Screen freezes
            SCROLL 			// Scroll freezes, data updates
        };

        /**
         * Construct a new OLED interface
         * 
         * @param man Reference to the manager instance
         *
         * @param enable_rate_filter		Whether or not to impose maximum update rate
         * @param min_filter_delay		    Minimum update delay, if enable_rate_filter enabled
         * @param type					    Which version of the OLED is being used (Version::FEATHERWING or Version::BREAKOUT)
         * @param reset_pin				    Which pin should be used for reseting. Only applies to breakout version
         * @param display_format			How to display the key value pairs of a bundle (Format::Scroll, Format::FOUR or Format::EIGHT)
         * @param scroll_duration			The time (ms) to complete full scroll cycle if display_format is SCROLL
         * @param freeze_pin				Which pin should be used to pause the display
         * @param freeze_behavior			How freezing the display should behave (FreezeType::SCROLL, FreezeType::DATA or FreezeType::DISABLE)
        */
        Loom_OLED(
                Manager&            man,
                const bool			enable_rate_filter		= true,
                const uint16_t		min_filter_delay		= 300,
                const Version		type					= Version::FEATHERWING,
                const byte			reset_pin				= A2,
                const Format		display_format			= Format::SCROLL,
                const uint16_t		scroll_duration			= 6000,
                const byte			freeze_pin				= 10,
                const FreezeType	freeze_behavior			= FreezeType::SCROLL
            );

        /* Destructor for the display pointer */
        ~Loom_OLED();

    private:
        bool canWrite();                        // Returns whether or not we can write to the display yet
        void flattenJSONObject(JsonObject json);// Flattens the given JSON object so we can display it

        Manager*            manInst;            // Pointer to the manager

        Adafruit_SSD1306*	display = nullptr;	// Underlying OLED controller
        uint16_t            min_filter_delay;   // Time to wait in between updates
		Version				version;			// What type the OLED is (FeatherWing or breakout)
		byte				reset_pin;			// The reset pin (only applies to breakout version)

		Format				display_format;		// How to display the data on OLED
		uint				scroll_duration;	// The duration to complete a full cycle through a bundle of data (milliseconds)(non-blocking)
		byte				freeze_pin;			// Which pin to check if display should freeze
		FreezeType			freeze_behavior;	// What 'freezing' behavior should be followed

        unsigned long       lastLogTime;        // Value of millis() at the last log
        unsigned long	    previous_time;      // Used to handle scrolling
        DynamicJsonDocument flattenedDoc;  // Flattened object 


};