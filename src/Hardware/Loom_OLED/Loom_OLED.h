#pragma once

#include "Loom_Manager.h"
#include "Module.h"

#include <Adafruit_SSD1306.h>


/**
 * Class for driving the OLED display
 * 
 * @author Will Richards
 */ 
class Loom_OLED : public Module{
    protected:

        void power_up() override {};
        void power_down() override {}; 
        void print_measurements() override {};
        
        // Manager controlled functions
        void measure() override;
        void initialize() override;
        void package() override;

    public:

        /** 
         * Different forms of the OLED display
         */
        enum class Version {
            FEATHERWING,	///< FeatherWing OLED
            BREAKOUT		///< Breakout board
        };

        /** 
         * Different formats to display information in
         */
        enum class Format {
            FOUR,			///< 4 Key values
            EIGHT,			///< 8 Key values
            SCROLL			///< Scrolling
        };

        /**
         * Different freeze behaviors
         */
        enum class FreezeType {
            DISABLE, 		///< Freeze disabled
            DATA, 			///< Screen freezes
            SCROLL 			///< Scroll freezes, data updates
        };

        /**
         * Construct a new OLED interface
         * 
         * @param man Reference to the manager instancce
         * 
         * OLED module constructor.
         * 
         * @param[in]	enable_rate_filter		Bool | <true> | {true, false} | Whether or not to impose maximum update rate
         * @param[in]	min_filter_delay		Int | <300> | [50-5000] | Minimum update delay, if enable_rate_filter enabled
         * @param[in]	type					Set(Version) | <0> | {0("Featherwing"), 1("Breakout")} | Which version of the OLED is being used
         * @param[in]	reset_pin				Set(Int) | <14> | {5, 6, 9, 10, 11, 12, 13, 14("A0"), 15("A1"), 16("A2"), 17("A3"), 18("A4"), 19("A5")} | Which pin should be used for reseting. Only applies to breakout version
         * @param[in]	display_format			Set(Format) | <A0> | {0("4 pairs"), 1("8 pairs"), 2("Scrolling")} | How to display the key value pairs of a bundle
         * @param[in]	scroll_duration			Int | <6000> | [500-30000] | The time (ms) to complete full scroll cycle if display_format is SCROLL
         * @param[in]	freeze_pin				Set(Int) | <10> | {5, 6, 9, 10, 11, 12, 13, 14("A0"), 15("A1"), 16("A2"), 17("A3"), 18("A4"), 19("A5")} | Which pin should be used to pause the display
         * @param[in]	freeze_behavior			Set(FreezeType) | <2> | {O("Disable"), 1("Pause Data"), 2("Pause Data and Scroll")} | How freezing the display should behave
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

    private:
        Manager*            manInst;            ///< Pointer to the manager

        Adafruit_SSD1306	display;			///< Underlying OLED controller
		Version				version;			///< What type the OLED is (FeatherWing or breakout)
		byte				reset_pin;			///< The reset pin (only applies to breakout version)

		Format				display_format;		///< How to display the data on OLED
		uint				scroll_duration;	///< The duration to complete a full cycle through a bundle of data (milliseconds)(non-blocking)
		byte				freeze_pin;			///< Which pin to check if display should freeze
		FreezeType			freeze_behavior;	///< What 'freezing' behavior should be followed

};