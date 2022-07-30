#include "Loom_Neopixel.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Neopixel::Loom_Neopixel(Manager& man, const bool enableA0, const bool enableA1, const bool enableA2) : Actuator(ACTUATOR_TYPE::NEOPIXEL, 0), manInst(&man), enabledPins( {enableA0, enableA1, enableA2} )
	, pixels( { Adafruit_NeoPixel(1, 14, NEO_GRB + NEO_KHZ800),
				Adafruit_NeoPixel(1, 15, NEO_GRB + NEO_KHZ800),
				Adafruit_NeoPixel(1, 16, NEO_GRB + NEO_KHZ800)
			} )
	, colorVals{}{
    this->enabledPins[0] = enableA0;
    this->enabledPins[1] = enableA1;
    this->enabledPins[2] = enableA2;

	manInst->registerModule((Module*)this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Neopixel::Loom_Neopixel(const bool enableA0, const bool enableA1, const bool enableA2) : Actuator(ACTUATOR_TYPE::NEOPIXEL, 0), enabledPins( {enableA0, enableA1, enableA2} )
	, pixels( { Adafruit_NeoPixel(1, 14, NEO_GRB + NEO_KHZ800),
				Adafruit_NeoPixel(1, 15, NEO_GRB + NEO_KHZ800),
				Adafruit_NeoPixel(1, 16, NEO_GRB + NEO_KHZ800)
			} )
	, colorVals{}{
    this->enabledPins[0] = enableA0;
    this->enabledPins[1] = enableA1;
    this->enabledPins[2] = enableA2;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Neopixel::initialize(){
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

    printModuleName(); Serial.println("Succsessfully initialized Neopixel");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Neopixel::control(JsonArray json){
    //TODO: If using instance number offset all these by one
    set_color(json[0].as<uint8_t>(), json[1].as<uint8_t>(), json[2].as<uint8_t>(), json[3].as<uint8_t>(), json[4].as<uint8_t>());
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Neopixel::set_color(const uint8_t port, const uint8_t chain_num, const uint8_t red, const uint8_t green, const uint8_t blue){
    if ( enabledPins[port] ) {

		// Update color vars
		colorVals[port][0] = (red > 0)   ? ( (red < 255)   ? red   : 255 ) : 0;
		colorVals[port][1] = (green > 0) ? ( (green < 255) ? green : 255 ) : 0;
		colorVals[port][2] = (blue > 0)  ? ( (blue < 255)  ? blue  : 255 ) : 0;

		// Apply color
		pixels[port].setPixelColor(chain_num, pixels[port].Color(colorVals[port][0], colorVals[port][1], colorVals[port][2]));

		// Update colors displayed by Neopixel
		pixels[port].show();
		
        printModuleName();
        Serial.print("Set Neopixel on Port: " + String(port) + ", Chain #: " + String(chain_num));
        Serial.print(" to R: " + String(colorVals[port][0]));
        Serial.print(  ", G: " + String(colorVals[port][1]));
        Serial.println(", B: " + String(colorVals[port][2]));
		
	} else {
		printModuleName(); Serial.println("Neopixel not enabled on port " + String(port));
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Neopixel::enable_pin(const uint8_t port, const bool state){
    enabledPins[port] = state;
	if (state) {
		pinMode(port, OUTPUT);
	}

	printModuleName(); Serial.println("Neopixel state changed on port" + String(port));
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

