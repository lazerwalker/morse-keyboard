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
  INPUT_MODE,
  NONE
} Menu;

static char* mainMenuText = "\nTELEGRAPH KEY SETTINGS\n\n"
                      "Tap the correct sequence to choose a menu option.\n"
                      "Hold down the key to quit.\n"
                      "Confused? Just quickly tap the telegraph key once for instructions.\n\n"
                      ".       How To Use\n"
                      "..      Change WPM / Input Speed\n"
                      "...     Change Input Mode";

static char *helpText = "\nHOW TO USE\n\n"
                        "This is a morse code keyboard. You use it to type morse code!\n"
                        "There are three input modes:\n\n"
                        "1. Morse Keyboard\n"
                        "This is the default mode. As you key in valid morse code letters, the device will type the corresponding characters as if you'd typed them yourself on a normal keyboard.\n" 
                        "For example, if you key in \"...\", then \"---\", then \"...\", your computer will act as if you just typed the letters \"SOS\".\n\n"

                        "2. Dots and Dashes\n"
                        "Every time you key in a short press (a \"dot\") or a long press (a \"dash\"), the device will type a \".\" or \"-\" character.\n" 
                        "This is useful if you want to send messages in morse code to your friends!\n\n"

                        "3. Space Bar\n"
                        "In this mode, the telegraph key just acts like a space bar. You can think of it like a one-button keyboard.\n"
                        "This mode is mostly useful if you're a developer who wants to build a custom experience using the hardware,\n"
                        "as it's the only mode that triggers discrete keydown and keyup events.\n";
  
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
bool detectedSpecialCode = false;

char *currentMorse;
int currentMorseCount;

char lastChar;
char *lastMorse;
int lastMorseCount;

uint8_t MODE_ADDR = 0;
uint8_t WPM_ADDR = 1;
uint8_t AUTOSPACE_ADDR = 2; // These 2 are inefficient, but we can spare the extra byte or so
uint8_t CAPITAL_ADDR = 3;

// WPM detector
unsigned long timestamps[8];
int timestampCount = 0; 

const int defaultWPM = 15;
int currentWPM;
char *currentWPMString;

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
  Menu previousMenu = currentMenu;
  currentMenu = menu;
  
  // TODO: Counts are slightly off, and this is way too slow.
  /*
   switch(previousMenu) {
    case MAINMENU:
      for (int i = 0; i < strlen(mainMenuText); i++) {
        Keyboard.press(KEY_BACKSPACE);
        Keyboard.release(KEY_BACKSPACE);
      }
      break;
    case HELP:
      for (int i = 0; i < strlen(helpText); i++) {
        Keyboard.press(KEY_BACKSPACE);
        Keyboard.release(KEY_BACKSPACE);
      }
      break; 
    case WPM:
      break;
    case INPUT_MODE:
      break; 
  }
  */
  
  switch(menu) {
    case HELP:
      Keyboard.println(helpText);
      break;
    case WPM:
      Keyboard.println(wpmText);
      break;
    case INPUT_MODE:
      Keyboard.println(inputModeText);
      break;
    case MAINMENU:
    default:
      Keyboard.println(mainMenuText);
      if (autoSpace) {
        Keyboard.println("....     Disable Auto-Space");        
      } else {
        Keyboard.println("....     Enable Auto-Space");
      }

      if (useCapitalLetters) {
        Keyboard.println(".....    Switch to Lowercase Letters\n");        
      } else {
        Keyboard.println(".....    Switch to Uppercase Letters\n");
      }

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
       autoSpace = !autoSpace;
       EEPROM.update(AUTOSPACE_ADDR, autoSpace);
       if (autoSpace) {
         Keyboard.println("Enabling auto-space.");
       } else {
         Keyboard.println("Disabling auto-space.");
       }
     } else if (strcmp(lastMorse, ".....") == 0) {
       useCapitalLetters = !useCapitalLetters; 
       EEPROM.update(CAPITAL_ADDR, useCapitalLetters);
       if (useCapitalLetters) {
         Keyboard.println("Switching to print uppercase letters.");
       } else {
         Keyboard.println("Switching to print lowercase letters.");
       }
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













