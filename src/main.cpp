#include <Arduino.h>
#include <ST7032_asukiaaa.h>
#include <Wire.h>

#define SDA_PIN 3
#define SCL_PIN 46

ST7032_asukiaaa lcd;

void setup() {
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.setWire(&Wire);
  lcd.begin(16, 2);
  lcd.setContrast(40);
  lcd.print("hello, world!");
}

void loop() {
  lcd.setCursor(0, 1);
  lcd.print(millis()/1000);
}
