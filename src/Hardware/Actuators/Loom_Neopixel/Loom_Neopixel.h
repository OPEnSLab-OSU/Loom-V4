#pragma once

#include "../../../Loom_Manager.h"
#include "../../../Actuators.h"

#include <Adafruit_NeoPixel.h>


/**
 *  Neopixel LED Strip Controller
 * 
 *  @author Will Richards
 */
class Loom_Neopixel : public Actuator{
    public:

        /**
         * Constructs a new Neopixel instance
         * 
         * @param man Reference to the manager
         * @param enableA0 Enable the first Neopixel
         * @param enableA1 Enable the second Neopixel
         * @param enableA2 Enable the third Neopixel
         */ 
        Loom_Neopixel(
                    Manager& man,
                    const bool enableA0 = false, 
                    const bool enableA1 = false, 
                    const bool enableA2 = true
                );
        
        /**
         * Constructs a new Neopixel instance
         * 
         * @param enableA0 Enable the first Neopixel
         * @param enableA1 Enable the second Neopixel
         * @param enableA2 Enable the third Neopixel
         */ 
        Loom_Neopixel(
                    const bool enableA0 = false, 
                    const bool enableA1 = false, 
                    const bool enableA2 = true
                );
        
        /* Initialize the module per the manager */
        void initialize() override;

        /* Allows for Max control of the neopixel */
        void control(JsonArray json) override;

        /**
         * Manually set the color of a specific Neopixel
         * 
         * @param port The port of the neopixel we want to control (0 - 2)
         * @param chain_num The number of the neopixel if daisy chained
         * @param red The red value (0-255)
         * @param green The green value (0-255)
         * @param blue The blue value (0-255)
         */ 
        void set_color(const uint8_t port, const uint8_t chain_num, const uint8_t red, const uint8_t green, const uint8_t blue);

        /**
         * Enable or disable a given Neopixel pin
         * 
         * @param port Port that we want to change (A0-A2)
         * @param state State that we want to change the port too
         */ 
        void enable_pin(const uint8_t port, const bool state);

    private:
        Manager* manInst;                       // Pointer to a manager 

        Adafruit_NeoPixel pixels[3];             // Allows users to control up to 3 different LED strips at once

        bool enabledPins[3];                    // Which of the three control pins is enabled
        uint8_t colorVals[3][3];                // RGB colors for each of the led strips
};