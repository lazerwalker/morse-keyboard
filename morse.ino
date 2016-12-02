#include "Keyboard.h"

#include "MorseMapping.h"

const int DAH = 130;
const int CHAR_DELAY = 250;
const int WORD_DELAY = 300;

typedef enum { MODE_TOGGLE } SpecialCode;

int BUTTON = 7;
int LED = 13;

unsigned long start = millis();

// false = up
// true = down
bool wasDown = false;
bool countedCurrentTap = false;
bool countedCurrentChar = false;
bool countedCurrentSpace = false;

// Array of dits/dahs
// false = dit, true = dah
char *currentMorse;
int currentMorseCount;

void setup() {
  Serial.begin(9600);
  Keyboard.begin();
  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(LED, OUTPUT);
  resetMorse();
}

void resetMorse() {
  currentMorse = (char *)(malloc(sizeof(char) * 6));
  currentMorseCount = 0;
}

void addMorse(char m) {
  if (currentMorseCount >= 6) {
    return;
  } else {
    currentMorse[currentMorseCount] = m;
    currentMorseCount++;
  }
}

SpecialCode morseToSpecialCode(char* input) {
    if (strcmp(input, "........") == 0) {
      return MODE_TOGGLE;
    }

    return NULL;
}

char morseToAscii(char* input) {
  for (int i = 0; i < MORSE_CHAR_COUNT; i++) {
    if (strcmp(input, morseMapping[i]) == 0) {
      return ascii[i];
    }
  }
  
  return NULL;
}

void loop() {
  // put your main code here, to run repeatedly:
  unsigned long now = millis();
  unsigned long timeDiff = now - start;

  bool pressed = !digitalRead(BUTTON);

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
        Serial.print("FINISHED CHAR ");
        
        char input[currentMorseCount + 1];
        strncpy(input, input, currentMorseCount);
        truncatedInput[input] = (char)0;

        for (int i = 0; i < currentMorseCount; i++) {
          Serial.print(input[i]);
        }
        Serial.print(": ");

        SpecialCode code = morseToSpecialCode(input);
        if (code != NULL) {
          Serial.println("Special code!");
        }

        char key = morseToAscii(input);
        if (key != NULL) {
          Keyboard.write(key);
          Serial.print(key);
        }
        Serial.print("\n");

        countedCurrentChar = true;
        resetMorse();
      }
    }
  }
  wasDown = pressed;
  delay(10);
}
