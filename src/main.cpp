#include <Arduino.h>

#include "EEPROM.h"
#include "Ports.h"

// ms it takes to change by one (255 * this to get time from 0% - 100% brightness)
#define MS_PER_CHANGE 10

// ms to wait after change to save it
#define TIME_TO_SAVE 5000

uint8_t ports[] = {PIN_LED_RED, PIN_LED_GREEN, PIN_LED_BLUE};
uint8_t value[] = {255, 255, 255};

enum mode_t : uint8_t {
    MODE_RED = 0,
    MODE_GREEN = 1,
    MODE_BLUE = 2,
};

mode_t currentMode = MODE_RED;

bool lastChangeSaved = true;
unsigned long lastChange = 0;
unsigned long unusedMillis = 0;

const uint32_t magicValue = 0xAFFEFEFA;

void update() {
    for (uint8_t i = 0; i < 3; ++i) {
        analogWrite(ports[i], value[i]);
        Serial.print(value[i]);
        Serial.write('\t');
    }

    Serial.write('\n');

    lastChange = millis();
    lastChangeSaved = false;
}

void save() {
    digitalWrite(LED_BUILTIN, HIGH);

    lastChangeSaved = true;

    EEPROM.put<uint32_t>(0, magicValue);

    for (uint8_t i = 0; i < 3; ++i) {
        EEPROM.put<uint8_t>(sizeof(uint32_t) + i, value[i]);
    }

    digitalWrite(LED_BUILTIN, LOW);
}

void setup() {
    // Internal LED to show "doing something"
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    Serial.begin(115200);

    // LEDs
    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_BLUE, OUTPUT);

    // Buttons
    pinMode(PIN_BUTTON_RED, INPUT_PULLUP);
    pinMode(PIN_BUTTON_GREEN, INPUT_PULLUP);
    pinMode(PIN_BUTTON_BLUE, INPUT_PULLUP);

    pinMode(PIN_BUTTON_UP, INPUT_PULLUP);
    pinMode(PIN_BUTTON_DOWN, INPUT_PULLUP);

    // Read old values from EEPROM
    uint32_t magic = 0;
    EEPROM.get(0, magic);

    if (magic == magicValue) {
        for (uint8_t i = 0; i < 3; ++i) {
            EEPROM.get<uint8_t>(sizeof(uint32_t) + i, value[i]);
        }
    }

    update();
    digitalWrite(LED_BUILTIN, LOW);
}

void loop() {
    // First, set mode by button pressed
    if (digitalRead(PIN_BUTTON_RED) == LOW) {
        currentMode = MODE_RED;
    } else if (digitalRead(PIN_BUTTON_GREEN) == LOW) {
        currentMode = MODE_GREEN;
    } else if (digitalRead(PIN_BUTTON_BLUE) == LOW) {
        currentMode = MODE_BLUE;
    }

    // Second, check up/down buttons
    unusedMillis += millis();
    unsigned long changeValue = unusedMillis / MS_PER_CHANGE;
    unusedMillis -= changeValue * MS_PER_CHANGE;
    uint8_t* currentValue = value + currentMode;

    if (digitalRead(PIN_BUTTON_UP) == LOW) {
        digitalWrite(LED_BUILTIN, HIGH);

        uint8_t maxChange = 255 - *currentValue;

        if (maxChange < changeValue) {
            *currentValue = 255;
        } else {
            *currentValue += changeValue;
        }

        update();
    } else if (digitalRead(PIN_BUTTON_DOWN) == LOW) {
        digitalWrite(LED_BUILTIN, HIGH);

        uint8_t maxChange = *currentValue;

        if (maxChange < changeValue) {
            *currentValue = 0;
        } else {
            *currentValue -= changeValue;
        }

        update();
    } else {
        digitalWrite(LED_BUILTIN, LOW);
        unusedMillis = 0;
    }

    // And check for save need
    if (!lastChangeSaved) {
        if ((millis() - lastChange) >= TIME_TO_SAVE) {
            save();
        }
    }
}
