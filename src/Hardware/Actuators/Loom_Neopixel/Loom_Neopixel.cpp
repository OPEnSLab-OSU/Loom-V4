#include "Loom_Neopixel.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Neopixel::Loom_Neopixel(Manager &man, const bool enableA0, const bool enableA1,
                             const bool enableA2, const neoPixelType colorType)
    : Actuator(ACTUATOR_TYPE::NEOPIXEL, 0), manInst(&man),
      pixels{Adafruit_NeoPixel(1, 14, colorType + NEO_KHZ800),
             Adafruit_NeoPixel(1, 15, colorType + NEO_KHZ800),
             Adafruit_NeoPixel(1, 16, colorType + NEO_KHZ800)},
      enabledPins{enableA0, enableA1, enableA2} {
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Neopixel::Loom_Neopixel(const bool enableA0, const bool enableA1, const bool enableA2,
                             const neoPixelType colorType)
    : Actuator(ACTUATOR_TYPE::NEOPIXEL, 0), manInst(nullptr),
      pixels{Adafruit_NeoPixel(1, 14, colorType + NEO_KHZ800),
             Adafruit_NeoPixel(1, 15, colorType + NEO_KHZ800),
             Adafruit_NeoPixel(1, 16, colorType + NEO_KHZ800)},
      enabledPins{enableA0, enableA1, enableA2} {}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Neopixel::initialize() {
    FUNCTION_START;
    // Set pin mode on enabled pins (pins A0-A5 = 14-19)
    for (int i = 0; i < 3; i++) {
        if (enabledPins[i])
            pinMode(14 + i, OUTPUT);
    }

    // Initialize Neopixels
    for (int i = 0; i < 3; i++) {
        if (enabledPins[i]) {
            pixels[i].begin(); // This initializes the NeoPixel library.
            pixels[i].show();  // Initialize all pixels to 'off'
        }
    }

    LOG(F("Successfully initialized Neopixel"));
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Neopixel::package(JsonObject json) { (void)json; }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Neopixel::control(JsonArray json) {
    FUNCTION_START;
    if (json.size() < 5) {
        ERROR(F("Neopixel command requires port, pixel, red, green, and blue values."));
        FUNCTION_END;
        return;
    }
    set_color(json[0].as<uint8_t>(), json[1].as<uint8_t>(), json[2].as<uint8_t>(),
              json[3].as<uint8_t>(), json[4].as<uint8_t>());
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Neopixel::set_color(const uint8_t port, const uint8_t chain_num, const uint8_t red,
                              const uint8_t green, const uint8_t blue) {
    FUNCTION_START;
    if (port >= 3) {
        ERRORF("Invalid Neopixel port %u; expected 0 through 2.", port);
        FUNCTION_END;
        return;
    }
    if (chain_num >= pixels[port].numPixels()) {
        ERRORF("Invalid Neopixel index %u for port %u.", chain_num, port);
        FUNCTION_END;
        return;
    }
    if (enabledPins[port]) {
        // Apply color
        pixels[port].setPixelColor(chain_num, pixels[port].Color(red, green, blue));

        // Update colors displayed by Neopixel
        pixels[port].show();

    } else {
        WARNINGF("Neopixel not enabled on port %u", port);
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Neopixel::enable_pin(const uint8_t port, const bool state) {
    FUNCTION_START;
    if (port >= 3) {
        ERRORF("Invalid Neopixel port %u; expected 0 through 2.", port);
        FUNCTION_END;
        return;
    }
    enabledPins[port] = state;
    if (state) {
        pinMode(14 + port, OUTPUT);
        pixels[port].begin();
        pixels[port].show();
    }
    LOGF("Neopixel state changed on port %u", port);
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
