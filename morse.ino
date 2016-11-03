const int DAH = 130;
const int CHAR_DELAY = 250;
const int WORD_DELAY = 300;

int currentMorse[5];

int BUTTON = 7;
int LED = 13;

unsigned long start = millis();

// false = up
// true = down
bool wasDown = false;
bool countedCurrentTap = false;
bool countedCurrentChar = false;
bool countedCurrentSpace = false;

void setup() {
  Serial.begin(9600);
  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(LED, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  unsigned long now = millis();
  unsigned long timeDiff = now - start;

  bool pressed = !digitalRead(BUTTON);

  if (wasDown) {


    if (!countedCurrentTap && timeDiff >= DAH) {
      Serial.println("DAH");
      countedCurrentTap = true;
    }

    if (!pressed) { // Just released the button
      if (!countedCurrentTap) {
        Serial.println("DIT");
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
        Serial.println("FINISHED CHAR");
        countedCurrentChar = true;
      }
    }
  }
  wasDown = pressed;
  delay(10);
}
