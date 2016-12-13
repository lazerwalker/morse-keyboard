#include <EEPROM.h>

#include "MorseMapping.h"

int DASH = 130;
int CHAR_DELAY = 250;
int WORD_DELAY = 300;
int LONG_PRESS = 300;

int BUTTON = 7;
int LED = 13;

unsigned long start = millis();

typedef enum { 
  KEYBOARD = 0, 
  DOTDASH, 
  SPACEBAR,
  NO_MODE,
  MENU
} Mode;

typedef enum {
  MAINMENU = 0,
  HELP,
  WPM,
  INPUT_MODE
} Menu;

// false = up
// true = down
bool wasPressed = false;
bool isPressed = false;

// Stateful tracking of phrase
bool countedCurrentTap = false;
bool countedCurrentChar = false;
bool countedCurrentSpace = false;
bool countedCurrentLongPress = false;
bool didChangeMode = false;

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
Menu currentMenu = MAINMENU;

void setup() {
  Serial.begin(9600);
  Keyboard.begin();

  char mode = EEPROM.read(MODE_ADDR);
  if (mode != 255) {
    currentMode = (Mode)mode;
  }
  if (currentMode == MENU) {
    // This shouldn't ever happen!
    currentMode = KEYBOARD;
    EEPROM.update(MODE_ADDR, KEYBOARD);
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
}

void addMorse(char m) {
  if (currentMorseCount >= 8) {
    return;
  } else {
    currentMorse[currentMorseCount] = m;
    currentMorseCount++;
  }
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

  isPressed = !digitalRead(BUTTON);

  detectedDown = false;
  detectedDot = false;
  detectedDash = false;
  detectedChar = false;
  detectedSpace = false;
  detectedLongPress = false;
  
  lastChar = false;
  lastMorseCount = 0;

  parseMorse(isPressed, now, timeDiff);

  if (currentMode == MENU) {
    loopMenu();
  } else {
    loopInput();
  }
 
  wasPressed = isPressed;
  delay(10);
}

void loopInput() {
  if (detectedLongPress) {
    Keyboard.println("Entering menu");
    currentMode = MENU;
    didChangeMode = true;
    changeMenu(MAINMENU);
    return;
  }

  switch(currentMode) {
    case KEYBOARD:
      loopKeyboard();
      break;
    case DOTDASH:
      loopDotDash();
      break;
    case SPACEBAR:
      loopSpaceBar(isPressed);
      break;
  }
}

void loopMenu() {
  if (detectedLongPress) {
    if (currentMenu == MAINMENU) {
      didChangeMode = true;
      Keyboard.println("Exiting main menu");
      // Exit the menu, restore saved mode
      char mode = EEPROM.read(MODE_ADDR);
      if (mode != 255) {
        currentMode = (Mode)mode;
      } else {
        currentMode = KEYBOARD;
      }
      EEPROM.update(MODE_ADDR, currentMode);
    } else {
      // Go back to the menu
      Keyboard.println("Returning to main menu");
      changeMenu(MAINMENU);
    }
    return;
  }

  switch(currentMenu) {
    case MAINMENU:
      loopMainMenu();
      break;
    case HELP:
      break;
    case WPM:
      loopWPM();
      break;
    case INPUT_MODE:
      loopInputMode();
      break;
  }
}

void parseMorse(bool pressed, unsigned long now, unsigned long timeDiff) {
  if (didChangeMode) {
    if(wasPressed && !pressed) {
      Keyboard.println("Exit didChangeMode");
      didChangeMode = false;
      resetMorse();
      countedCurrentLongPress = false;
    }
    return;      
  }
  
  if (wasPressed) {   
    if (!countedCurrentLongPress && timeDiff >= LONG_PRESS) {
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
        currentMorse[currentMorseCount] = '\0';
        
        free(lastMorse);
        lastMorse = new char[currentMorseCount];
        strcpy(lastMorse, currentMorse);

        detectedChar = true;
        lastChar = morseToAscii(lastMorse);
        
        if (lastChar == NULL) {
          countedCurrentSpace = true;
        }

        countedCurrentChar = true;
        resetMorse();
      }
    }
  }
}

/* ******************
* Keyboard Modes
********************/

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

/* ******************
* Menu Modes
********************/

void changeMenu(Menu menu) {
  Keyboard.println("Changing menu to " + String(menu));
  currentMenu = menu;
  switch(menu) {
    case HELP:
      break;
    case WPM:
      break;
    case INPUT_MODE:
      break;
    case MAINMENU:
    default:
      Keyboard.println("\nTELEGRAPH KEY SETTINGS\n\n"
                      "Tap the correct sequence to choose a menu option.\n"
                      "Hold down the key to quit.\n"
                      "Confused? Just quickly tap the telegraph key once for instructions.\n\n"
                      ".       How To Use\n"
                      "..      Change WPM / Input Speed\n"
                      "...     Change Input Mode\n"
                      "....     [Enable / Disable] Auto-Space\n"
                      ".....    Switch to [Uppercase]\n");
      break;
  }
  resetMorse();
}

 void loopMainMenu() {
   if (detectedChar) {
     if (strcmp(lastMorse, ".") == 0) {
       changeMenu(HELP);
     } else if (strcmp(lastMorse, "..") == 0) {
       changeMenu(WPM);
     } else if (strcmp(lastMorse, "...") == 0) {
       changeMenu(INPUT_MODE);
     } else if (strcmp(lastMorse, "....") == 0) {
       // Enable/disable autospace    
     } else if (strcmp(lastMorse, ".....") == 0) {
       // Toggle lowercase/uppercase
     }  

   }
 }

void loopInputMode() {

}

void loopWPM() {
  if (detectedChar) {
    // TODO: Only do something if it's a number
    currentWPMString += detectedChar;
    Keyboard.write(detectedChar);
  }
  // TODO: Check if we should exit  
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













