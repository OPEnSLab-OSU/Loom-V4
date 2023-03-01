#include "Loom_OLED.h"
#include "Logger.h"

#include <Adafruit_GFX.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_OLED::Loom_OLED(Manager& man,
                     const bool enable_rate_filter, 
                     const uint16_t min_filter_delay, 
                     const Version type,
                     const byte reset_pin, 
                     const Format display_format, 
                     const uint16_t scroll_duration, 
                     const byte freeze_pin, 
                     const FreezeType freeze_behavior
                    ) : Module("OLED"), manInst(&man), min_filter_delay(min_filter_delay), version(type), reset_pin(reset_pin), display_format(display_format), scroll_duration(scroll_duration), freeze_behavior(freeze_behavior), freeze_pin(freeze_pin), flattenedDoc(2000){
                        manInst->registerModule(this);

                        // Create the correct display module given the OLED version
                        display = (version == Version::FEATHERWING) ? new Adafruit_SSD1306() : new Adafruit_SSD1306(reset_pin);
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_OLED::~Loom_OLED(){
    delete display;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_OLED::initialize(){

    // We need the freeze pin for inputs if freeze isn't disabled
    if(freeze_behavior != FreezeType::DISABLE){
        pinMode(freeze_pin, INPUT_PULLUP);
    }

    // Start the I2C device on 0x3C for the 128x32 display, address cannot be changed
    display->begin(SSD1306_SWITCHCAPVCC, 0x3C);

    // Draws to the screen
    display->display();

    // Clears the screen
    display->clearDisplay();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_OLED::display_data(){
    // If we cant update the display yet return
    if(!canWrite())
        return;
    
    // If we are trying to write data check if we are freezing the display if so return
    if(freeze_behavior == FreezeType::DATA){
        if(digitalRead(freeze_pin) == 0)
            return;
    }

    // Set the parameters for writing to the OLED
    display->clearDisplay();
    display->setTextColor(WHITE);
    display->setTextSize(1);

    // Flatten the main JSON document
    flattenJSONObject(manInst->getDocument().as<JsonObject>());

    // Get the size of the flattened object
    JsonObject data = flattenedDoc["flatObj"].as<JsonObject>();
    int size = data.size();
    String keys[size], vals[size];

    int i = 0;
	for (auto kv : data) {
		keys[i] = kv.key().c_str();
		vals[i] = kv.value().as<String>();
		i++;
	}

    // Write the values to memory in the correct spots
	switch (display_format) {
		case Format::FOUR:
			for (int i = 0; i < 4 && i < size; i++) {
				display->setCursor(0, i*8);
				display->print(keys[i].substring(0,8));

				display->setCursor(64, i*8);
				display->print(vals[i].substring(0,8));
			}
			break;

		case Format::EIGHT:
			for (int i = 0; i < 4 && i < size; i++) {
				display->setCursor(0, i*8);
				display->print(keys[i].substring(0,4));
        
				display->setCursor(32, i*8);
				display->print(vals[i].substring(0,4));
			}
			for (int i = 0; i < 4 && i < size; i++) {
				display->setCursor(64, i*8);
				display->print(keys[i+4].substring(0,4));
        
				display->setCursor(96, i*8);
				display->print(vals[i+4].substring(0,4));
			}
			break;

		case Format::SCROLL:

			unsigned long time;
      
			if (freeze_behavior == FreezeType::SCROLL) {
				if (digitalRead(freeze_pin) == 0) {
					time = previous_time;
				} else {
					time = millis();
					previous_time = time;
				}
			} else {
				time = millis();
			}

			int offset = size*( float(time%(scroll_duration)) / (float)(scroll_duration) );

			for (int i = 0; i < 5; i++) {
				display->setCursor(0, i*8);
				display->print(keys[(i+offset)%size].substring(0,8));
        
				display->setCursor(64, i*8);
				display->print(vals[(i+offset)%size].substring(0,8));
			}

			break;
	}

    // Write the data to the screen
    display->display();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_OLED::canWrite(){
    if ( (millis() > min_filter_delay) && ( (millis()-lastLogTime) < min_filter_delay ) ) {
	    LOG(F("Not enough time since last log"));
		return false;
	} else {
		lastLogTime = millis();
		return true;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_OLED::flattenJSONObject(JsonObject json){

    // Get the contents array
    JsonArray contents = json["contents"].as<JsonArray>();

    // Check if there is actual data
    if(contents.isNull()) return;

    // Using our temp doc we convert it to and object and create a new nested object that is called flatObj
    JsonObject flatData = flattenedDoc.to<JsonObject>().createNestedObject("flatObj");

    // Flatten the data into the flattenedDoc
    JsonObject data;
    for (auto module_obj : contents) {
		for (JsonPair kv : module_obj["data"].as<JsonObject>()) {
			flatData[kv.key()] = kv.value();
		}
	}

}
//////////////////////////////////////////////////////////////////////////////////////////////////////