#include <Arduino.h>
#include <ST7032_asukiaaa.h>
#include <WiFi.h>
#include <Wire.h>
#include "MyNTP.h"
#include "MyMqtt.h"
#include "secrets.h"

#define SDA_PIN 3
#define SCL_PIN 9

static char TAG[] = "main";

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

static void pubDeviceHealth() {
  static MQTTData pub_heap;
  struct tm       _timeInfo;
  time_t          now;
  if (!getLocalTime(&_timeInfo, 5000)) {
    return;
  }
  unsigned long unixTime = time(&now);
  strncpy(pub_heap.topic, DEVICE_HEALTH_PUB_TOPIC, sizeof(pub_heap.topic) - 1);
  sprintf(pub_heap.data,
          "{\"time_stamp\": \"%04d-%02d-%02d %02d:%02d:%02d\",\"time_serial\": "
          "\"%lu\",\"cpu_temp\":%.2f,\"heap_free\": %lu,\"rssi\":%d}",
          (_timeInfo.tm_year + 1900), (_timeInfo.tm_mon + 1), (_timeInfo.tm_mday), (_timeInfo.tm_hour),
          (_timeInfo.tm_min), (_timeInfo.tm_sec), unixTime, temperatureRead(), esp_get_free_heap_size(), WiFi.RSSI());
  xQueueSend(pubQueue, &pub_heap, 0);
}

void lcdTask(void *pvParams){
  while(true){
    if(xSemaphoreTake(pub_success_semaphore_handle, 0) == pdTRUE){
      ESP_LOGI(TAG, "Take: pub_success_semaphore");
      printTime(&lcd);
      lcd.setCursor(8, 0); lcd.print(" ");
      lcd.setCursor(9, 0); lcd.print("SEND:OK");

      lcd.setCursor(0, 1);
      lcd.print("RSSI:");
      lcd.setCursor(5, 1);
      lcd.print(WiFi.RSSI());
    }
    if (xSemaphoreTake(pub_failed_semaphore_handle, 0) == pdTRUE) {
      ESP_LOGI(TAG, "Take: pub_failed_semaphore");
      printTime(&lcd);
      lcd.setCursor(8, 0); lcd.print(" ");
      lcd.setCursor(9, 0); lcd.print("SEND:NG");

      lcd.setCursor(0, 1);
      lcd.print("RSSI:");
      lcd.setCursor(5, 1);
      lcd.print(WiFi.RSSI());
    }
    delay(500);
  }
}

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
    initMqtt();
    xTaskCreatePinnedToCore(mqttTask, "mqttTask", 8196, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(lcdTask, "lcdTask", 4096, NULL, 3, NULL, 1);
  }
}

void loop() {
  if (!WiFi.isConnected()){
    clearLcd(&lcd, 0);
    lcd.setCursor(0, 0);
    lcd.print("WiFi disconnect!");

    lcd.setCursor(0, 1);
    lcd.print("RSSI:          ");
  } else {
    pubDeviceHealth();
  }
  delay(5000);
}
