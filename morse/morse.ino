#include "MorseMapping.h"

const int DAH = 130;
const int CHAR_DELAY = 250;
const int WORD_DELAY = 300;

int BUTTON = 7;
int LED = 13;

unsigned long start = millis();

typedef enum { NONE, TOGGLE_INPUT_MODE } SpecialCode;
typedef enum { KEYBOARD, DOTDASH, SPACEBAR } Mode;

// false = up
// true = down
bool wasDown = false;

bool countedCurrentTap = false;
bool countedCurrentChar = false;
bool countedCurrentSpace = false;

char *currentMorse;
int currentMorseCount;

Mode currentMode = KEYBOARD;

void setup() {
  Serial.begin(9600);
  Keyboard.begin();
  
  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(LED, OUTPUT);
  resetMorse();
}

void resetMorse() {
  currentMorse = (char *)(malloc(sizeof(char) * 8));
  currentMorseCount = 0;
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
  if (strcmp(input, "........") == 0) {
    return TOGGLE_INPUT_MODE;
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

void loop() {
  unsigned long now = millis();
  unsigned long timeDiff = now - start;

  bool pressed = !digitalRead(BUTTON);

  if (currentMode == KEYBOARD) {
      loopKeyboard(pressed, now, timeDiff);
  } else if (currentMode == DOTDASH) {
      loopDotDash(pressed, now, timeDiff);
  } else if (currentMode == SPACEBAR) {
      loopSpaceBar(pressed, now, timeDiff);
  }
  
  wasDown = pressed;
  delay(10);
}

void loopKeyboard(bool pressed, unsigned long now, unsigned long timeDiff) {
  if (wasDown) {
    if (!countedCurrentTap && timeDiff >= DAH) {
      addMorse('-');
      countedCurrentTap = true;
    }

    if (!pressed) { // Just released the button
      if (!countedCurrentTap) {
        addMorse('.');
      }

      // Time for a new tap
      countedCurrentTap = false;
      start = now;
    }
  } else { // !wasDown
    if (pressed) {
      start = millis();
      countedCurrentChar = false;
      countedCurrentSpace = false;
    } else {
      if (timeDiff >= CHAR_DELAY + WORD_DELAY && !countedCurrentSpace) {
        Serial.println("SPACE");
        Keyboard.write(" ");
        countedCurrentSpace = true;
      } else if (timeDiff >= CHAR_DELAY && !countedCurrentChar) {
        char input[currentMorseCount + 1];
        strncpy(input, currentMorse, currentMorseCount);
        input[currentMorseCount] = (char)0;

        SpecialCode code = morseToSpecialCode(input);
        if (code != NONE) {
          Serial.println("Special Code!");
          countedCurrentSpace = true;
        } else {
          Serial.print("FINISHED CHAR ");
          for (int i = 0; i < currentMorseCount; i++) {
            Serial.print(currentMorse[i]);
          }
          Serial.print(": ");

          char key = morseToAscii(input);
          if (key == NULL) {
            countedCurrentSpace = true;
          } else {
            Keyboard.write(key);
            Serial.print(key);
          }

          Serial.print("\n");
        }
        countedCurrentChar = true;
        resetMorse();
      }
    }
  }
}

void loopDotDash(bool pressed, unsigned long now, unsigned long timeDiff) {
  if (wasDown) {
    if (!countedCurrentTap && timeDiff >= DAH) {
      Keyboard.write("-");
      Serial.print("-");
      countedCurrentTap = true;
    }

    if (!pressed) { // Just released the button
      if (!countedCurrentTap) {
        Keyboard.write('.');
        Serial.print('.');
      }

      // Time for a new tap
      countedCurrentTap = false;
      start = now;
    }
  } else { // !wasDown
    if (pressed) {
      start = millis();
      countedCurrentChar = false;
      countedCurrentSpace = false;
    } else {
      if (timeDiff >= CHAR_DELAY + WORD_DELAY && !countedCurrentSpace) {
        Serial.println("SPACE");
        Keyboard.write(" ");
        countedCurrentSpace = true;
      }
    }
  }  
}

void loopSpaceBar(bool pressed, unsigned long now, unsigned long timeDiff) {
  if (pressed && !wasDown) {
    Keyboard.press(' ');
    Serial.println("Key down");
  }

  if (!pressed && wasDown) {
    Keyboard.release(' ');
    Serial.println("Key up");
  }
}


