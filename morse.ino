const int DAH = 130;
const int CHAR_DELAY = 250;
const int WORD_DELAY = 300;


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
bool *currentMorse;
int currentMorseCount;

void setup() {
  Serial.begin(9600);
  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(LED, OUTPUT);
  resetMorse();
}

void resetMorse() {
  currentMorse = (bool *)(malloc(sizeof(bool) * 5));
  currentMorseCount = 0;
}

void loop() {
  // put your main code here, to run repeatedly:
  unsigned long now = millis();
  unsigned long timeDiff = now - start;

  bool pressed = !digitalRead(BUTTON);

  if (wasDown) {
    if (!countedCurrentTap && timeDiff >= DAH) {
      currentMorse[currentMorseCount] = true;
      currentMorseCount++;

      countedCurrentTap = true;
    }

    if (!pressed) { // Just released the button
      if (!countedCurrentTap) {
        currentMorse[currentMorseCount] = false;
        currentMorseCount++;
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
        countedCurrentSpace = true;
      } else if (timeDiff >= CHAR_DELAY && !countedCurrentChar) {
        Serial.print("FINISHED CHAR ");
        for (int i = 0; i < currentMorseCount; i++) {
          Serial.print(currentMorse[i]);
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



