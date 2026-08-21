
#include "Loom_OLED.h"
#include "Logger.h"

#include <Adafruit_GFX.h>

namespace {
size_t printTruncated(Adafruit_SSD1306 &display, const char *text, size_t maximumLength) {
    if (text == nullptr)
        return 0;
    size_t i = 0;
    for (; i < maximumLength && text[i] != '\0'; ++i)
        display.write(static_cast<uint8_t>(text[i]));
    return i;
}

void printValueTruncated(Adafruit_SSD1306 &display, JsonVariant value, size_t maximumLength) {
    if (value.is<const char *>()) {
        printTruncated(display, value.as<const char *>(), maximumLength);
        return;
    }

    char buffer[24] = {};
    serializeJson(value, buffer, sizeof(buffer));
    printTruncated(display, buffer, maximumLength);
}

size_t countEntries(JsonArray contents) {
    size_t count = 0;
    for (JsonVariant module : contents)
        count += module["data"].as<JsonObject>().size();
    return count;
}

void printQualifiedKey(Adafruit_SSD1306 &display, const char *moduleName, const char *key,
                       size_t maximumLength) {
    const size_t moduleLength = printTruncated(display, moduleName ? moduleName : "", maximumLength);
    if (moduleLength >= maximumLength)
        return;
    display.write(static_cast<uint8_t>('.'));
    printTruncated(display, key, maximumLength - moduleLength - 1);
}

void printEntryAt(Adafruit_SSD1306 &display, JsonArray contents, size_t targetIndex, int keyX,
                  int valueX, int y, size_t keyLength, size_t valueLength) {
    size_t index = 0;
    for (JsonVariant module : contents) {
        const char *moduleName = module["module"].as<const char *>();
        for (JsonPair entry : module["data"].as<JsonObject>()) {
            if (index++ != targetIndex)
                continue;

            display.setCursor(keyX, y);
            printQualifiedKey(display, moduleName, entry.key().c_str(), keyLength);
            display.setCursor(valueX, y);
            printValueTruncated(display, entry.value(), valueLength);
            return;
        }
    }
}
} // namespace

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_OLED::Loom_OLED(Manager &man, const bool enable_rate_filter, const uint16_t min_filter_delay,
                     const Version type, const byte reset_pin, const Format display_format,
                     const uint16_t scroll_duration, const byte freeze_pin,
                     const FreezeType freeze_behavior)
    : Module("OLED"), manInst(&man), featherwingDisplay(), breakoutDisplay(reset_pin),
      display(nullptr), rateFilterEnabled(enable_rate_filter), min_filter_delay(min_filter_delay),
      version(type), reset_pin(reset_pin), display_format(display_format),
      scroll_duration(scroll_duration), freeze_pin(freeze_pin), freeze_behavior(freeze_behavior),
      lastLogTime(0), previous_time(0) {
    manInst->registerModule(this);

    // Create the correct display module given the OLED version
    display = (version == Version::FEATHERWING) ? &featherwingDisplay : &breakoutDisplay;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_OLED::~Loom_OLED() = default;
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_OLED::initialize() {

    // We need the freeze pin for inputs if freeze isn't disabled
    if (freeze_behavior != FreezeType::DISABLE) {
        pinMode(freeze_pin, INPUT_PULLUP);
    }

    // Start the I2C device on 0x3C for the 128x32 display, address cannot be changed
    if (display == nullptr || !display->begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        moduleInitialized = false;
        ERROR(F("Failed to initialize OLED display."));
        return;
    }

    // Draws to the screen
    display->display();

    // Clears the screen
    display->clearDisplay();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_OLED::display_data() {
    LOG("Attempting to display data on OLED...");
    // If we cant update the display yet return
    if (!canWrite())
        return;

    // If we are trying to write data check if we are freezing the display if so return
    if (freeze_behavior == FreezeType::DATA) {
        if (digitalRead(freeze_pin) == 0)
            return;
    }

    // Set the parameters for writing to the OLED
    display->clearDisplay();
    display->setTextColor(WHITE);
    display->setTextSize(1);

    // Walk the Manager document directly; duplicating it used to reserve another 2 KB JSON pool.
    JsonArray contents = manInst->getDocument()["contents"].as<JsonArray>();
    const size_t size = contents.isNull() ? 0 : countEntries(contents);

    // Write the values to memory in the correct spots
    switch (display_format) {
    case Format::FOUR:
        for (size_t i = 0; i < 4 && i < size; ++i)
            printEntryAt(*display, contents, i, 0, 64, static_cast<int>(i * 8), 8, 8);
        break;

    case Format::EIGHT:
        for (size_t i = 0; i < 8 && i < size; ++i) {
            const bool rightColumn = i >= 4;
            const int keyX = rightColumn ? 64 : 0;
            const int valueX = keyX + 32;
            printEntryAt(*display, contents, i, keyX, valueX, static_cast<int>((i % 4) * 8), 4,
                         4);
        }
        break;

    case Format::SCROLL:
        if (size == 0)
            break;

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

        const unsigned int duration = scroll_duration == 0 ? 1 : scroll_duration;
        const size_t offset = static_cast<size_t>(
            size * (static_cast<float>(time % duration) / static_cast<float>(duration)));

        for (size_t i = 0; i < 5; ++i)
            printEntryAt(*display, contents, (i + offset) % size, 0, 80,
                         static_cast<int>(i * 8), 15, 10);

        break;
    }

    // Write the data to the screen
    display->display();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_OLED::canWrite() {
    const unsigned long now = millis();
    if (rateFilterEnabled && hasDisplayed &&
        static_cast<unsigned long>(now - lastLogTime) < min_filter_delay) {
        LOG(F("Not enough time since last log"));
        return false;
    }

    lastLogTime = now;
    hasDisplayed = true;
    return true;
}
