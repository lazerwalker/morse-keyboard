#include <EEPROM.h>

#include "MorseMapping.h"

int DASH = 130;
int CHAR_DELAY = 250;
int WORD_DELAY = 300;
int LONG_PRESS = 300;

int BUTTON = 7;
int LED = 13;

unsigned long start = millis();

typedef enum { NONE, TOGGLE_WPM_MODE, RESET_WPM } SpecialCode;
typedef enum { 
  KEYBOARD = 0, 
  DOTDASH, 
  SPACEBAR,
  NO_MODE,
  WPM  
} Mode;

// false = up
// true = down
bool wasPressed = false;

// Stateful tracking of phrase
bool countedCurrentTap = false;
bool countedCurrentChar = false;
bool countedCurrentSpace = false;
bool countedCurrentLongPress = false;

// Tracking for a given frame
// TODO: Should probably be placed in a data structure and passed around as a non-global
bool detectedDown = false;
bool detectedDot = false;
bool detectedDash = false;
bool detectedChar = false;
bool detectedSpace = false;
bool detectedLongPress = false;
bool detectedSpecialCode = false;

char *currentMorse;
int currentMorseCount;

SpecialCode lastCode;
char lastChar;
char *lastMorse;
int lastMorseCount;

uint8_t MODE_ADDR = 0;
uint8_t WPM_ADDR = 1;

// WPM detector
unsigned long timestamps[8];
int timestampCount = 0; 

const int defaultWPM = 15;
int currentWPM;
char *currentWPMString;

Mode currentMode = KEYBOARD;

void setup() {
  Serial.begin(9600);
  Keyboard.begin();

  char mode = EEPROM.read(MODE_ADDR);
  if (mode != 255) {
    currentMode = (Mode)mode;
  }

  currentWPM = EEPROM.read(WPM_ADDR);
  if (currentWPM == 255) { // Never manually set, use the default
    currentWPM = defaultWPM;
  }
  setSpeedFromWPM(currentWPM);
  
  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(LED, OUTPUT);
  resetMorse();
}

void resetMorse() {
  currentMorse = (char *)(malloc(sizeof(char) * 8));
  currentMorseCount = 0;
  countedCurrentLongPress = false;
}

void addMorse(char m) {
  if (currentMorseCount >= 8) {
    return;
  } else {
    currentMorse[currentMorseCount] = m;
    currentMorseCount++;
  }
}

SpecialCode morseToSpecialCode(char *input) {
  if (strcmp(input, ".......-") == 0) {
    return TOGGLE_WPM_MODE;
  } else if (strcmp(input, "------.") == 0) {
    return RESET_WPM;
  }

  return NONE;
}

char morseToAscii(char *input) {
  for (int i = 0; i < MORSE_CHAR_COUNT; i++) {
    if (strcmp(input, morseMapping[i]) == 0) {
      return ascii[i];
    }
  }
  
  return NULL;
}

void setSpeedFromWPM(int wpm) {
  int dot = 1200.0 / wpm;
  DASH = 3 * dot;
  CHAR_DELAY = 3 * dot;
  WORD_DELAY = 7 * dot;
  LONG_PRESS = 9 * dot;
}

void loop() {
  unsigned long now = millis();
  unsigned long timeDiff = now - start;

  bool pressed = !digitalRead(BUTTON);

  detectedDown = false;
  detectedDot = false;
  detectedDash = false;
  detectedChar = false;
  detectedSpace = false;
  detectedLongPress = false;
  detectedSpecialCode = false;
  
  lastChar = false;
  lastMorseCount = 0;
  lastCode = NONE;

  parseMorse(pressed, now, timeDiff);

  if (detectedSpecialCode) {
    if (lastCode == TOGGLE_WPM_MODE) {
      currentMode = WPM;
      enterWPMMode();
    } else if (lastCode == RESET_WPM) {
      resetWPM();
    }
  }

  if (detectedLongPress && currentMode != WPM) {
    // Toggle mode
    currentMode = (Mode)(currentMode + 1);
    if (currentMode == NO_MODE) {
      currentMode = KEYBOARD;
    }
    Keyboard.println("\nTelegraph Key input mode changed!");
    EEPROM.update(MODE_ADDR, currentMode);
  }

  if (currentMode == KEYBOARD) {
    loopKeyboard();
  } else if (currentMode == DOTDASH) {
    loopDotDash();
  } else if (currentMode == SPACEBAR) {
    loopSpaceBar(pressed);
  } else if (currentMode == WPM) {
    loopWPM();
  }
  
  wasPressed = pressed;
  delay(10);
}

void parseMorse(bool pressed, unsigned long now, unsigned long timeDiff) {
  if (wasPressed) {
    if (!countedCurrentLongPress  && timeDiff >= LONG_PRESS) {
      detectedLongPress = true;
      countedCurrentLongPress = true;
      countedCurrentTap = true;
    } else if (!countedCurrentTap && timeDiff >= DASH) {
      detectedDash = true;
      addMorse('-');
      countedCurrentTap = true;
    }

    if (!pressed) { // Just released the button
      if (!countedCurrentTap) {
        addMorse('.');
        detectedDot = true;
      }

      // Time for a new tap
      countedCurrentTap = false;
      start = now;
    }
  } else { // !wasPressed
    if (pressed) {
      start = millis();
      countedCurrentChar = false;
      countedCurrentSpace = false;
      detectedDown = true;
    } else {
      if (timeDiff >= CHAR_DELAY + WORD_DELAY && !countedCurrentSpace) {
        detectedSpace = true;
        countedCurrentSpace = true;
      } else if (timeDiff >= CHAR_DELAY && !countedCurrentChar) {
        char input[currentMorseCount + 1];
        strncpy(input, currentMorse, currentMorseCount);
        input[currentMorseCount] = (char)0;

        SpecialCode code = morseToSpecialCode(input);
        if (code != NONE) {
          detectedSpecialCode = true;
          lastCode = code;
          countedCurrentSpace = true;
        } else {
          detectedChar = true;
          lastChar = morseToAscii(input);
          strncpy(currentMorse, lastMorse, currentMorseCount);
          
          if (lastChar == NULL) {
            countedCurrentSpace = true;
          }
        }
        countedCurrentChar = true;
        resetMorse();
      }
    }
  }
}
void loopKeyboard() {
  if (detectedSpace) {
    Keyboard.print(' ');
    Serial.println("SPACE");
  }

  if (detectedChar) {
    Keyboard.write(lastChar);
    Serial.print("FINISHED CHAR ");
    for (int i = 0; i < lastMorseCount; i++) {
      Serial.print(lastMorse[i]);
    }
    Serial.print(": ");
    Serial.print(lastChar);
    Serial.print("\n");
  }
}

void loopDotDash() {
  if (detectedDot) {
    Keyboard.write('.');
    Serial.print('.');
  }

  if (detectedDash) {
    Keyboard.write("-");
    Serial.print("-");
  }

  if (detectedSpace) {
    Serial.println("SPACE");
    Keyboard.write(" ");
  }  
}

void loopSpaceBar(bool pressed) {
  if (pressed && !wasPressed) {
    Keyboard.press(' ');
    Serial.println("Key down");
  }

  if (!pressed && wasPressed) {
    Keyboard.release(' ');
    Serial.println("Key up");
  }
}

void loopWPM() {
  if (detectedChar) {
    // TODO: Only do something if it's a number
    currentWPMString += detectedChar;
    Keyboard.write(detectedChar);
    
  }
  if (detectedSpecialCode && detectedSpecialCode == TOGGLE_WPM_MODE) {
    int wpm = atoi(currentWPMString);
    if (wpm != 0) {
      setWPM(wpm);
      Keyboard.println("\nSetting keyboard to " + String(currentWPMString) + " WPM.");
    } else {
      Keyboard.println("\nAn error occurred, exiting WPM setting mode without doing anything");
    }
    currentMode = (Mode)EEPROM.read(MODE_ADDR);
  }
}

void enterWPMMode() {
  currentWPMString = "";
  Keyboard.println("Welcome to WPM Mode! Current speed is "  + String(currentWPM) + " WPM. Enter a new number by entering the corresponding number in morse.");
}

void setWPM(int wpm) {
  setSpeedFromWPM(defaultWPM);
  EEPROM.update(WPM_ADDR, defaultWPM);
  currentWPM = defaultWPM;  
}

void resetWPM() {
  Keyboard.println("Resetting WPM to " + String(defaultWPM) + " WPM.");
  setWPM(defaultWPM);
}

