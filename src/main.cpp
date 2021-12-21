#include <Arduino.h>

#include "EEPROM.h"
#include "Ports.h"
#include "Steroido.h"

// ms it takes to change by one (255 * this to get time from 0% - 100% brightness)
#define MS_PER_CHANGE 10

// ms to wait after change to save it
#define TIME_TO_SAVE 5000

uint8_t ports[] = {PIN_LED_RED, PIN_LED_GREEN, PIN_LED_BLUE};

// The values currently shown
uint8_t actualValues[] = {255, 255, 255};

// The values that should be shown right now
uint8_t desiredValues[] = {255, 255, 255};

// For value calculation
unsigned long unusedMillis = 0;
bool fade = false;
unsigned long fadeSpeed = MS_PER_CHANGE;
bool desiredReached = true;

// For EEPROM
const uint32_t magicValue = 0xAFFEFEFA;

// For up/down debounce
DelayedSwitch upBtn;
DelayedSwitch downBtn;

enum selector_t : uint8_t {
    SELECT_RED = 0,
    SELECT_GREEN = 1,
    SELECT_BLUE = 2,
    SELECT_FADE = 3,
};

selector_t currentSelector = SELECT_RED;

void update() {
    // Update actual values to desired values
    unusedMillis += millis();
    unsigned long changeValue = unusedMillis / fadeSpeed;
    unusedMillis -= changeValue * fadeSpeed;

    desiredReached = true;
    for (uint8_t i = 0; i < 3; ++i) {
        // TODO implement better fade to reach all colors desired state at same time
        uint8_t* desiredValue = desiredValues + i;
        uint8_t* actualValue = actualValues + i;

        if (*desiredValue > *actualValue) {
            desiredReached = false;

            // -> Fade up
            uint8_t maxVal = *desiredValue - *actualValue;
            if (maxVal < changeValue) {
                *actualValue = *desiredValue;
            } else {
                *actualValue += changeValue;
            }
        } else if (*desiredValue < *actualValue) {
            desiredReached = false;

            // -> Fade down
            uint8_t maxVal = *actualValue - *desiredValue;
            if (maxVal < changeValue) {
                *actualValue = *desiredValue;
            } else {
                *actualValue -= changeValue;
            }
        }
    }

    // Set (and print) actual values
    for (uint8_t i = 0; i < 3; ++i) {
        analogWrite(ports[i], actualValues[i]);
        Serial.print(actualValues[i]);
        Serial.write('\t');
    }

    Serial.write('\n');

    // Print desired values
    for (uint8_t i = 0; i < 3; ++i) {
        Serial.print(desiredValues[i]);
        Serial.write('\t');
    }

    Serial.write('\n');
}

void calcFade() {
    // -> not implemented yet
}

void currentIsDesired() {
    desiredReached = true;

    for (uint8_t i = 0; i < 3; ++i) {
        desiredValues[i] = actualValues[i];
    }
}

void autosave() {
    unsigned long lastSave = 0;

    if ((millis() - lastSave) >= TIME_TO_SAVE) {
        digitalWrite(LED_BUILTIN, HIGH);

        lastSave = millis();

        EEPROM.put<uint32_t>(0, magicValue);
        uint8_t offset = sizeof(uint32_t);

        EEPROM.put<uint8_t>(offset, currentSelector);
        offset += sizeof(uint8_t);

        for (uint8_t i = 0; i < 3; ++i) {
            EEPROM.put<uint8_t>(offset + i, actualValues[i]);
            EEPROM.put<uint8_t>(offset + 3 + i, desiredValues[i]);
        }

        digitalWrite(LED_BUILTIN, LOW);
    }
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
    pinMode(PIN_BUTTON_FADE, INPUT_PULLUP);

    pinMode(PIN_BUTTON_UP, INPUT_PULLUP);
    pinMode(PIN_BUTTON_DOWN, INPUT_PULLUP);

    // Read old values from EEPROM
    uint32_t magic = 0;
    EEPROM.get<uint32_t>(0, magic);
    uint8_t offset = sizeof(uint32_t);

    if (magic == magicValue) {
        EEPROM.get<uint8_t>(offset, (uint8_t&)currentSelector);
        offset += sizeof(uint8_t);

        for (uint8_t i = 0; i < 3; ++i) {
            EEPROM.get<uint8_t>(offset + i, actualValues[i]);
            EEPROM.get<uint8_t>(offset + 3 + i, desiredValues[i]);
        }
    }

    desiredReached = true;
    for (uint8_t i = 0; i < 3; ++i) {
        if (desiredValues[i] != actualValues[i]) {
            desiredReached = false;
            break;
        }
    }

    upBtn.setEnableTime(100);
    downBtn.setEnableTime(100);
    upBtn.setDisableTime(50);
    downBtn.setDisableTime(50);

    digitalWrite(LED_BUILTIN, LOW);
}

// Vars for loop
bool upState;
bool downState;

void loop() {
    // First, set mode by button pressed
    if (digitalRead(PIN_BUTTON_RED) == LOW) {
        currentSelector = SELECT_RED;
    } else if (digitalRead(PIN_BUTTON_GREEN) == LOW) {
        currentSelector = SELECT_GREEN;
    } else if (digitalRead(PIN_BUTTON_BLUE) == LOW) {
        currentSelector = SELECT_BLUE;
    } else if (digitalRead(PIN_BUTTON_FADE) == LOW) {
        currentSelector = SELECT_FADE;
    } else {
        goto done;
    }

    // Debounce up/down
    upState = upBtn.set(digitalRead(PIN_BUTTON_UP) == LOW);
    downState = downBtn.set(digitalRead(PIN_BUTTON_DOWN) == LOW);

    // Second, check up/down buttons
    if (currentSelector < SELECT_FADE) {
        fadeSpeed = MS_PER_CHANGE;

        if (upState) {
            desiredValues[currentSelector] = 255;
        } else if (downState) {
            desiredValues[currentSelector] = 0;
        } else {
            currentIsDesired();
        }
    } else {
        // TODO Fade up/down handling
    }

done:
    if (fade) calcFade();

    update();
    autosave();
}
