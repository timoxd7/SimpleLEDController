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

bool printActual = false;
bool printDesired = false;

void update() {
    static unsigned long delta = 0;

    unsigned long oldDelta = delta;
    delta = millis();

    // Update actual values to desired values
    unusedMillis += delta - oldDelta;
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

        if (printActual) {
            Serial.print(actualValues[i]);
            Serial.write('\t');
        }
    }

    if (printActual) Serial.write('\n');

    // Print desired values
    if (printDesired) {
        for (uint8_t i = 0; i < 3; ++i) {
            Serial.print(desiredValues[i]);
            Serial.write('\t');
        }

        Serial.write('\n');
    }
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
    static unsigned long lastSave = 0;

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

        Serial.println("Save");

        digitalWrite(LED_BUILTIN, LOW);
    }
}

bool getNumFromChar(uint8_t& num, char charToConv) {
    if (charToConv >= '0' && charToConv <= '9') {
        num = charToConv - '0';
        return true;
    }

    return false;
}

void manageSerial() {
    static bool nextIsNum = false;
    static uint8_t numIndex;
    static uint8_t num;
    static uint8_t commandIndex;

    if (Serial.available()) {
        int currentChar = Serial.read();

        if (currentChar >= 0) {
            switch (currentChar) {
                case 'a':
                case 'A':
                    printActual = !printActual;
                    break;

                case 'd':
                case 'D':
                    printDesired = !printDesired;
                    break;

                case 'r':
                case 'R':
                    numIndex = 0;
                    num = 0;
                    commandIndex = SELECT_RED;
                    nextIsNum = true;
                    break;

                case 'g':
                case 'G':
                    numIndex = 0;
                    num = 0;
                    commandIndex = SELECT_GREEN;
                    nextIsNum = true;
                    break;

                case 'b':
                case 'B':
                    numIndex = 0;
                    num = 0;
                    commandIndex = SELECT_BLUE;
                    nextIsNum = true;
                    break;

                default:
                    if (nextIsNum) {
                        uint8_t currentNum;

                        if (getNumFromChar(currentNum, currentChar)) {
                            num *= 10;
                            num += currentNum;

                            if (++numIndex == 3) {
                                nextIsNum = false;
                                desiredValues[commandIndex] = num;
                                desiredReached = false;

                                Serial.print("Set ");
                                Serial.print(commandIndex);
                                Serial.print(" to ");
                                Serial.println(num);
                            }
                        } else {
                            Serial.println("Not a number (0 - 9)!");
                            nextIsNum = false;
                        }
                    } else {
                        Serial.println("Got wrong command");
                    }
                    break;
            }
        } else {
            Serial.println("Got strange command");
        }
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
            Serial.print(actualValues[i]);
            Serial.write('\t');
        }

        offset += 3;
        Serial.write('\n');

        for (uint8_t i = 0; i < 3; ++i) {
            EEPROM.get<uint8_t>(offset + i, desiredValues[i]);
            Serial.print(desiredValues[i]);
            Serial.write('\t');
        }

        Serial.print("\nSelector: ");
        Serial.println(currentSelector);

        Serial.println("Saved light loaded");
    } else {
        Serial.println("First turn on :)");
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
    }

    // Debounce up/down
    bool oldUpState = upState;
    bool oldDownState = downState;

    upState = upBtn.set(digitalRead(PIN_BUTTON_UP) == LOW);
    downState = downBtn.set(digitalRead(PIN_BUTTON_DOWN) == LOW);

    if (upState != oldUpState) {
        if (upState) {
            Serial.println("-> Up");
        } else {
            Serial.println("<- Up");
        }
    }

    if (downState != oldDownState) {
        if (downState) {
            Serial.println("-> Down");
        } else {
            Serial.println("<- Down");
        }
    }

    // Second, check up/down buttons
    if (currentSelector < SELECT_FADE) {
        fadeSpeed = MS_PER_CHANGE;

        if (upState) {
            desiredValues[currentSelector] = 255;
        } else if (downState) {
            desiredValues[currentSelector] = 0;
        } else {
            if ((upState != oldUpState) || downState != oldDownState) {
                // Only if released -> To fade if set over Serial
                currentIsDesired();
            }
        }
    } else {
        // TODO Fade up/down handling
        calcFade();
    }

    manageSerial();
    update();
    autosave();
}
