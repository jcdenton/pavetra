#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <InfluxDb.h>
#include <PMS.h>
#include <RemoteDebug.h>
#include <SoftwareSerial.h>
#include <WiFiManager.h>

#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wwrite-strings"

// #define DEBUG_PMS
#define SLEEP_INTERVAL 15 * 60 * 1000
#define SLEEP_INTERVAL_MICROS SLEEP_INTERVAL * 1000

#include <tokens.h>
#if !defined(PAVETRA_TOKEN) || !defined(INFLUX_TOKEN)
#pragma GCC error "Redefine the required constants in your own <tokens.h>"
#endif

int pm25 = 0;
int pm10 = 0;
String pm_data;

RemoteDebug Debug;
SoftwareSerial pmsSerial(D6, D7);
PMS pms(pmsSerial);
PMS::DATA data;

void wakeUpPMS();
void readDataFromPMS(bool retry = true);
void sendDataToPavetra();
void sendDataToInfluxDb();
void printData();

void setup() {
  Serial.begin(74880);
  pmsSerial.begin(PMS::BAUD_RATE);
#ifdef DEBUG_PMS
  Debug.begin("0.0.0.0");
  Debug.setSerialEnabled(true);
#endif

  WiFiManager wifiManager;
#ifndef DEBUG_PMS
  wifiManager.setDebugOutput(false);
#endif
  wifiManager.setConfigPortalTimeout(120);
  wifiManager.setTimeout(120);
  wifiManager.autoConnect(PAVETRA_WIFI_SSID, PAVETRA_WIFI_PASSWORD);
}

void loop() {
  Debug.handle();
  wakeUpPMS();
  readDataFromPMS();
#ifndef DEBUG_PMS
  sendDataToPavetra();
  sendDataToInfluxDb();
#endif

  pms.sleep();
#ifdef DEBUG_PMS
  delay(5 * 1000);
#else
  ESP.deepSleep(SLEEP_INTERVAL_MICROS);
#endif
}

void wakeUpPMS() {
  Debug.printf("%us\tWaking up PMS\n", millis() / 1000);
  pms.wakeUp();
  Debug.printf("%us\tWarming up PMS for 30s\n", millis() / 1000);
  delay(PMS::STEADY_RESPONSE_TIME);
}

void printData() {
  Serial.println(pm25 / 10.0);
  Serial.println(pm10 / 10.0);
}

void readDataFromPMS(bool retry) {
  pm25 = 0;
  pm10 = 0;
  int count_read = 0;
  int count_try_to_read = 0;
  while (count_read < 10 && count_try_to_read < 10) {
    if (pms.readUntil(data, PMS::TOTAL_RESPONSE_TIME)) {
      pm25 += data.PM_AE_UG_2_5;
      pm10 += data.PM_AE_UG_10_0;
      ++count_read;
      if (data.PM_AE_UG_2_5 == 0 || data.PM_AE_UG_10_0 == 0) {
        Debug.printf("%us\tread ZERO (%d/%d): %d %d\n", millis() / 1000, count_read, count_try_to_read, data.PM_AE_UG_2_5, data.PM_AE_UG_10_0);
      } else {
        Debug.printf("%us\tread OK (%d/%d): %d %d\n", millis() / 1000, count_read, count_try_to_read, data.PM_AE_UG_2_5, data.PM_AE_UG_10_0);
      }
    } else {
      ++count_try_to_read;
      Debug.printf("%us\treadUntil timed out (%d/%d)\n", millis() / 1000, count_read, count_try_to_read);
    }
  }
  if (count_read != 0) {
    pm_data = "{\"sensor_2_5\": " + String(static_cast<float>(pm25) / count_read) + ", \"sensor_10\": " + String(static_cast<float>(pm10) / count_read) + " }";
    Debug.printf("%us\t%s\n", millis() / 1000, pm_data.c_str());
  } else {
    pm_data = "{}";
    if (retry) {
      wakeUpPMS();
      readDataFromPMS(false);
    }
  }
}

void sendDataToPavetra() {
  // === HTTPS Request ===
  HTTPClient http;
  http.begin(PAVETRA_ENDPOINT, PAVETRA_HTTPS_FINGERPRINT);
  http.addHeader("Authorization", "Token " PAVETRA_TOKEN);
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(pm_data);
  http.end();
}

void sendDataToInfluxDb() {
  Influxdb influx(INFLUX_HOST, INFLUX_PORT);
#ifdef INFLUX_HTTPS_FINGERPRINT
  influx.setFingerPrint(INFLUX_HTTPS_FINGERPRINT);
#endif
  influx.setToken(INFLUX_TOKEN);
  influx.setVersion(2);
  influx.setOrg(INFLUX_ORG);
  influx.setBucket(INFLUX_BUCKET);

  InfluxData pm25Measurement("pm25");
  pm25Measurement.addTag("location", "k34b");
  pm25Measurement.addTag("sensor", "pms7003");
  pm25Measurement.addValue("value", pm25 / 10.0);
  influx.prepare(pm25Measurement);

  InfluxData pm10Measurement("pm10");
  pm10Measurement.addTag("location", "k34b");
  pm10Measurement.addTag("sensor", "pms7003");
  pm10Measurement.addValue("value", pm10 / 10.0);
  influx.prepare(pm10Measurement);

  influx.write();
}
