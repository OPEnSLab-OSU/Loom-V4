#include "Loom_Neopixel.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Neopixel::Loom_Neopixel(Manager& man, const bool enableA0, const bool enableA1, const bool enableA2) : 
    Actuator(ACTUATOR_TYPE::NEOPIXEL, 0), 
    manInst(&man), 
    enabledPins{ enableA0, enableA1, enableA2 },
	pixels{ Adafruit_NeoPixel(1, 14, NEO_RGB + NEO_KHZ800),
            Adafruit_NeoPixel(1, 15, NEO_RGB + NEO_KHZ800),
            Adafruit_NeoPixel(1, 16, NEO_RGB + NEO_KHZ800) }
{
    this->enabledPins[0] = enableA0;
    this->enabledPins[1] = enableA1;
    this->enabledPins[2] = enableA2;

	manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Neopixel::Loom_Neopixel(const bool enableA0, const bool enableA1, const bool enableA2) : 
    Actuator(ACTUATOR_TYPE::NEOPIXEL, 0), 
    enabledPins{ enableA0, enableA1, enableA2 },
	pixels{ Adafruit_NeoPixel(1, 14, NEO_RGB + NEO_KHZ800),
            Adafruit_NeoPixel(1, 15, NEO_RGB + NEO_KHZ800),
            Adafruit_NeoPixel(1, 16, NEO_RGB + NEO_KHZ800) }
{
    this->enabledPins[0] = enableA0;
    this->enabledPins[1] = enableA1;
    this->enabledPins[2] = enableA2;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Neopixel::initialize(){
    FUNCTION_START;
    // Set pin mode on enabled pins (pins A0-A5 = 14-19)
	for (int i = 0; i < 3; i++) {
		if (enabledPins[i]) pinMode(14+i, OUTPUT);
	}

    // Initialize Neopixels
	for (int i = 0; i < 3; i++) {
		if (enabledPins[i]) {
			pixels[i].begin(); // This initializes the NeoPixel library.
			pixels[i].show();  // Initialize all pixels to 'off'
		}
	}

    LOG("Successfully initialized Neopixel");
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Neopixel::package(JsonObject json) {}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Neopixel::control(JsonArray json){
    FUNCTION_START;
    //TODO: If using instance number offset all these by one
    set_color(json[0].as<uint8_t>(), json[1].as<uint8_t>(), json[2].as<uint8_t>(), json[3].as<uint8_t>(), json[4].as<uint8_t>());
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Neopixel::set_color(const uint8_t port, const uint8_t chain_num, const uint8_t red, const uint8_t green, const uint8_t blue){
    FUNCTION_START;
    char output[100];
    if ( enabledPins[port] ) {
		// Apply color
		pixels[port].setPixelColor(chain_num, pixels[port].Color(red, green, blue));

		// Update colors displayed by Neopixel
		pixels[port].show();
		
		/*
        printModuleName();
        Serial.print("Set Neopixel on Port: " + String(port) + ", Chain #: " + String(chain_num));
        Serial.print(" to R: " + String(red));
        Serial.print(  ", G: " + String(green));
        Serial.println(", B: " + String(blue));
		*/
		
	} else {
        snprintf(output, 100, "Neopixel not enabled on port %u", port);
		WARNING(output);
	}
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Neopixel::enable_pin(const uint8_t port, const bool state){
    FUNCTION_START;
    char output[100];
    enabledPins[port] = state;
	if (state) {
		pinMode(port, OUTPUT);
	}
    snprintf(output, 100, "Neopixel state changed on port %u", port);
	LOG(output);
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

