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
  WPM,
  INPUT_MODE,
  NONE
} Menu;

static char* mainMenuText = "\nTELEGRAPH KEY SETTINGS\n\n"
                      "To choose a menu option, tap the corresponding morse sequence.\n"
                      "Hold down the key to quit.\n\n"
                      ".       Reset Input Speed to Default";                      
                      
static char *wpmText = "\nCHANGE WPM\n\n"
                      "Your current input speed is 5 WPM.\n"
                      "To change the speed, enter a number or something.\n";

static char *inputModeText = "\nINPUT MODE\n\n"
                            "Your current mode is \"Morse Keyboard\".\n" 
                            "Tap the correct sequence to choose a new input mode.\n"
                            "Hold the key down to go back.\n\n"

                            ".       Morse Keyboard\n"
                            "        As you key in valid letters in morse code, the device will type the appropriate characters.\n\n"

                            "..      Dots and Dashes\n"
                            "        The device will type \".\" and \"-\" characters for each dit or dash you key.\n\n"

                            "...     Space Bar\n"
                            "        The device will act as a space bar.\n"
                            "        (This is for developers who want raw keyup/keydown events.)\n";

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
bool detectedBackspace = false;

char *currentMorse;
int currentMorseCount;

char lastChar;
char *lastMorse;

uint8_t MODE_ADDR = 0;
uint8_t WPM_ADDR = 1;
uint8_t AUTOSPACE_ADDR = 2; // These 2 are inefficient, but we can spare the extra byte or so
uint8_t CAPITAL_ADDR = 3;

// WPM detector
unsigned long timestamps[8];
int timestampCount = 0; 

const int defaultWPM = 15;
int currentWPM;
char currentWPMString[10];

Mode currentMode = KEYBOARD;
Menu currentMenu = NONE;

bool autoSpace = false;
bool useCapitalLetters = false;

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

  autoSpace = EEPROM.read(AUTOSPACE_ADDR);
  useCapitalLetters = EEPROM.read(CAPITAL_ADDR);
  
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
      if (useCapitalLetters) {
        return uppercaseAscii[i];
      } else {
        return lowercaseAscii[i];
      }
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
  detectedBackspace = false;
  
  lastChar = false;

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
      Keyboard.println("\nExiting the settings menu. Happy keying!");
      Keyboard.println("\n--------------------------------------------------------------------------------");

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
      didChangeMode = true;
      changeMenu(MAINMENU);
    }
    return;
  }

  switch(currentMenu) {
    case MAINMENU:
      loopMainMenu();
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
      didChangeMode = false;
      resetMorse();
      countedCurrentLongPress = false;
      countedCurrentTap = false;
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
      if (autoSpace && timeDiff >= CHAR_DELAY + WORD_DELAY && !countedCurrentSpace) {
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

        if (strcmp(lastMorse, "........") == 0) {
          detectedBackspace = true;  
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
  if (detectedBackspace) {
    Keyboard.press(KEY_BACKSPACE);
    Keyboard.release(KEY_BACKSPACE);
    return; 
    // Short-circuit early since detectedChar = true
    // This can't change, because only keyboard mode cares about backspace
  }  
  
  if (detectedSpace) {
    Keyboard.print(' ');
    Serial.println("SPACE");
  }

  if (detectedChar) {
    Keyboard.write(lastChar);
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
  Menu previousMenu = currentMenu;
  currentMenu = menu;

  Keyboard.println("\n--------------------------------------------------------------------------------");
  
  switch(menu) {
    case WPM:
      enterWPMMode();
      break;
    case INPUT_MODE:
      Keyboard.println(inputModeText);
      break;
    case MAINMENU:
    default:
      Keyboard.println(mainMenuText);
      Keyboard.println("..      Change WPM / Input Speed (Current Speed: " + String(currentWPM) + " WPM)");
      Keyboard.println("...     Change Input Mode");

      if (autoSpace) {
        Keyboard.println("....    Disable Auto-Space");        
      } else {
        Keyboard.println("....    Enable Auto-Space");
      }

      if (useCapitalLetters) {
        Keyboard.println(".....   Switch to Lowercase Letters");        
      } else {
        Keyboard.println(".....   Switch to Uppercase Letters");
      }

      break;
  }
  resetMorse();
}

 void loopMainMenu() {
   if (detectedChar) {
     if (strcmp(lastMorse, ".") == 0) {
         Keyboard.println("Resetting input speed to " + String(defaultWPM) + " WPM.");
         setWPM(defaultWPM);
         changeMenu(MAINMENU);
     } else if (strcmp(lastMorse, "..") == 0) {
       changeMenu(WPM);
     } else if (strcmp(lastMorse, "...") == 0) {
       changeMenu(INPUT_MODE);
     } else if (strcmp(lastMorse, "....") == 0) {
       autoSpace = !autoSpace;
       EEPROM.update(AUTOSPACE_ADDR, autoSpace);
       if (autoSpace) {
         Keyboard.println("Enabling auto-space.");
       } else {
         Keyboard.println("Disabling auto-space.");
       }
       changeMenu(MAINMENU);
     } else if (strcmp(lastMorse, ".....") == 0) {
       useCapitalLetters = !useCapitalLetters; 
       EEPROM.update(CAPITAL_ADDR, useCapitalLetters);
       if (useCapitalLetters) {
         Keyboard.println("Switching to print uppercase letters.");
       } else {
         Keyboard.println("Switching to print lowercase letters.");
       }
       changeMenu(MAINMENU);
     }
   }
 }

void loopInputMode() {
   if (detectedChar) {
     if (strcmp(lastMorse, ".") == 0) {
       EEPROM.update(MODE_ADDR, KEYBOARD);
       Keyboard.println("Setting key to Morse Keyboard mode.");
       changeMenu(MAINMENU);
     } else if (strcmp(lastMorse, "..") == 0) {
       EEPROM.update(MODE_ADDR, DOTDASH);
       Keyboard.println("Setting key to Dots and Dashes mode.");
       changeMenu(MAINMENU);
     } else if (strcmp(lastMorse, "...") == 0) {
       EEPROM.update(MODE_ADDR, SPACEBAR);
       Keyboard.println("Setting key to Space Bar mode.");
       changeMenu(MAINMENU);
     }
   }
}

void loopWPM() {
  if (detectedChar) {
    if (isdigit(lastChar)) {
      Keyboard.write(lastChar);

      int len = strlen(currentWPMString);
      currentWPMString[len] = lastChar;
      currentWPMString[len+ 1] = '\0';
    } else if (strcmp(lastMorse, "........") == 0) {
      Keyboard.press(KEY_BACKSPACE);
      Keyboard.release(KEY_BACKSPACE);

      int len = strlen(currentWPMString);
      currentWPMString[len - 1] = '\0';
    } else if (strcmp(lastMorse, "...-.-") == 0) {
      Keyboard.println("\nSetting the speed to " + String(currentWPMString) + " WPM.");
      
      int newWPM = atoi(currentWPMString);
      
      setWPM(newWPM);
      changeMenu(MAINMENU);
    }
  }
}

void enterWPMMode() {
  currentWPMString[0] = '\0';
  
  Keyboard.println("\nCHANGE INPUT SPEED\n\n"
                    "The current speed is "  + String(currentWPM) + " WPM\n"
                    "Enter a new number by entering the corresponding number in morse\n"
                    "To backspace, tap 8 dots (........)\n"
                    "To accept the current WPM, tap \"...-.-\"\n"
                    "To cancel, hold down the key\n\n");
  Keyboard.write("New WPM: ");
}

void setWPM(int wpm) {
  EEPROM.update(WPM_ADDR, wpm);
  setSpeedFromWPM(wpm);
  currentWPM = wpm;  
}

void resetWPM() {
  Keyboard.println("Resetting WPM to " + String(defaultWPM) + " WPM.");
  setWPM(defaultWPM);
}















