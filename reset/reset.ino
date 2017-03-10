#include <EEPROM.h>

uint8_t MODE_ADDR = 0;
uint8_t WPM_ADDR = 1;
uint8_t AUTOSPACE_ADDR = 2; // These 2 are inefficient, but we can spare the extra byte or so
uint8_t CAPITAL_ADDR = 3;

void setup() {
  Serial.begin(9600);

  while(!Serial) {}

  Serial.println("Current State:");
  
  int mode = EEPROM.read(MODE_ADDR);
  Serial.println("Mode:");
  Serial.println(mode);

  int wpm = EEPROM.read(WPM_ADDR);
  Serial.println("WPM: ");
  Serial.println(wpm);

  bool autoSpace = EEPROM.read(AUTOSPACE_ADDR);
  Serial.println("Use autospace: ");
  Serial.println(autoSpace);
  
  bool useCapitalLetters = EEPROM.read(CAPITAL_ADDR);
  Serial.println("Use capital letters:");
  Serial.println(useCapitalLetters);

  Serial.println("-----");
  Serial.println("Resetting to default state...");
  EEPROM.update(MODE_ADDR, 0);
  EEPROM.update(WPM_ADDR, 15);
  EEPROM.update(AUTOSPACE_ADDR, false);
  EEPROM.update(CAPITAL_ADDR, false);
  Serial.println("Reset! You should reinstall the actual telegraph key arduino sketch now");
}

void loop() {

}
