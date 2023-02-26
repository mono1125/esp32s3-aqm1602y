#include <Arduino.h>
#include <ST7032_asukiaaa.h>
#include <WiFi.h>
#include <Wire.h>
#include "MyNTP.h"
#include "secrets.h"

#define SDA_PIN 3
#define SCL_PIN 9

ST7032_asukiaaa lcd;

struct tm timeInfo;

static void beginWiFiSTA(){
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while(WiFi.status() != WL_CONNECTED){
    delay(1000);
  }
}

static void clearLcd(ST7032_asukiaaa *_lcd, int row){
 char buf[16] = {"               "};
 _lcd->setCursor(0, row);
 _lcd->print(buf);
}

static void printTime(ST7032_asukiaaa *_lcd){
  char buf[16];
  if (!getLocalTime(&timeInfo, 5000)){
    return;
  }
  _lcd->setCursor(0, 0);
  sprintf(buf, "%02d:%02d:%02d", timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
  _lcd->print(buf);
}

// hw_timer_t *lcd_timer = NULL;
// volatile SemaphoreHandle_t lcd_semaphore_handle;
// void IRAM_ATTR onLcdTimer(){
//   xSemaphoreGiveFromISR(lcd_semahore_handle, NULL);
// }
// void lcdTimeStampTask(void *pvParameters) {
//   lcd_semaphore_handle = xSemaphoreCreateBinary();
//   lcd_timer = timerBegin(1, getApbFrequency() / 1000000, true)
//   timerAttachInterrupt(lcd_timer, 1000, true);
//   timerAlarmEnable(lcd_timer);
// }

void setup() {
  beginWiFiSTA();
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.setWire(&Wire);
  lcd.begin(16, 2);
  lcd.setContrast(40);
  beginNtp(120000);
  if (WiFi.isConnected()){
    clearLcd(&lcd, 0);
    lcd.setCursor(0, 0);
    lcd.print("WiFi connected!");
    delay(2000);
  }
}

unsigned long prev_time;
void loop() {
  if (!WiFi.isConnected()){
    clearLcd(&lcd, 0);
    lcd.setCursor(0, 0);
    lcd.print("WiFi disconnect!");
  }
  printTime(&lcd);
  lcd.setCursor(8, 0); lcd.print(" ");
  lcd.setCursor(9, 0); lcd.print("SEND:OK");

  lcd.setCursor(0, 1);
  lcd.print("RSSI:");
  lcd.setCursor(5, 1);
  lcd.print(WiFi.RSSI());
}
