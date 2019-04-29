#include <WiFiManager.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include "PMS.h"

int pm25;
int pm10;
String pm_data;

PMS pms(Serial);
PMS::DATA data;

void setup() {
  Serial.begin(9600);
  
  // === Connect to Internet ===
  WiFiManager wifiManager;
  wifiManager.autoConnect("pavetra");  

  // === Get PM data ===
  pms.wakeUp();
  delay(30*1000);
  if (pms.readUntil(data)) {
    pm25 = data.PM_AE_UG_2_5;
    pm10 = data.PM_AE_UG_10_0;
    pm_data = "{\"sensor_2_5\": " + String(pm25) + ", \"sensor_10\": " + String(pm10) + " }";
  } else {
    pm_data = "{\"sensor_2_5\": 0, \"sensor_10\": 0}";
  }

  Serial.println(pm_data);
  
  // === HTTPS Request ===
  HTTPClient http;
  http.begin("http://pavetra.online/devices/data");
  http.addHeader("Authorization", "Token ***");
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(pm_data);
  http.end();

  
  // === Sleep Mode ===
  pms.sleep();
  ESP.deepSleep(20*60*1000*1000); // Sleep 20 minutes
}

void loop() {
  
}
