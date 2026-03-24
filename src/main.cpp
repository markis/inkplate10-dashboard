#ifndef ARDUINO_INKPLATE10
#error "Wrong board selection for this example, please select Inkplate 10 in the boards menu."
#endif

// #define INKPLATE_DEBUG

#include <Arduino.h>
#include <HTTPClient.h>
#include <Inkplate.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <driver/rtc_io.h>
#include <time.h>

#include "config.h"

#ifdef INKPLATE_DEBUG
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTF(...)
#endif

static constexpr uint32_t WIFI_TIMEOUT_MS = 20000;
static constexpr uint32_t IMAGE_RETRY_ATTEMPTS = 2;
static constexpr uint32_t NORMAL_SLEEP_S = 1200;
static constexpr uint32_t ERROR_SLEEP_S = 300;
static constexpr uint32_t NIGHT_SLEEP_S = 21600;
static constexpr int NIGHT_START_HOUR = 0;
static constexpr int NIGHT_END_HOUR = 6;
static constexpr uint32_t NTP_SYNC_INTERVAL_S = 86400;  // 24 hours

// RTC memory to persist data across deep sleep
RTC_DATA_ATTR time_t lastNtpSync = 0;
RTC_DATA_ATTR char lastModified[64] = "";  // Store Last-Modified header
RTC_DATA_ATTR char etag[64] = "";          // Store ETag header

Inkplate display(INKPLATE_3BIT);

static bool connectWifi();
static void setTimezone();
static bool syncTimeFromNtp();
static bool shouldSyncNtp();
static bool isNighttime();
static uint32_t calculateSleepSeconds();
static bool displayImage();
static void sendMqttMsg();
static void goToSleep(uint32_t sleepSeconds);

void setup() {
#ifdef INKPLATE_DEBUG
  Serial.begin(115200);
#endif

  display.begin();

  if (!connectWifi()) {
    goToSleep(ERROR_SLEEP_S);
  }

  // Smart NTP sync: only when needed to save battery
  if (shouldSyncNtp()) {
    DEBUG_PRINTLN("NTP sync needed");
    if (syncTimeFromNtp()) {
      lastNtpSync = time(nullptr);
      DEBUG_PRINTLN("NTP sync successful");
    } else {
      DEBUG_PRINTLN("NTP sync failed, continuing with RTC time");
      setTimezone();  // Ensure timezone is set even if NTP fails
    }
    delay(100);  // Brief delay to ensure WiFi stack is stable after NTP
  } else {
    DEBUG_PRINTLN("Skipping NTP sync, using RTC time");
    setTimezone();  // Ensure timezone is set when skipping NTP
  }

  struct tm now;
  if (getLocalTime(&now)) {
    DEBUG_PRINTF("Current time: %04d-%02d-%02d %02d:%02d:%02d\n", now.tm_year + 1900, now.tm_mon + 1, now.tm_mday,
                 now.tm_hour, now.tm_min, now.tm_sec);
  }

  if (isNighttime()) {
    goToSleep(NIGHT_SLEEP_S);
  }

  if (!displayImage()) {
    goToSleep(ERROR_SLEEP_S);
  }

  sendMqttMsg();
  goToSleep(calculateSleepSeconds());
}

void loop() {}

static bool connectWifi() {
  DEBUG_PRINT("Connecting to WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);  // Set power before connection for battery savings
  WiFi.disconnect(true, true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_TIMEOUT_MS) {
    delay(100);
  }

  if (WiFi.status() != WL_CONNECTED) {
    DEBUG_PRINTLN("\nWiFi connect failed");
    return false;
  }

  DEBUG_PRINTF("\nConnected, IP: %s\n", WiFi.localIP().toString().c_str());
  return true;
}

static void setTimezone() {
  setenv("TZ", TIMEZONE, 1);
  tzset();
}

static bool syncTimeFromNtp() {
  DEBUG_PRINTLN("Syncing time from NTP...");

  // Configure timezone and sync time from NTP in one call
  configTzTime(TIMEZONE, NTP_SERVER);

  struct tm timeinfo;
  for (int i = 0; i < 20; i++) {
    if (getLocalTime(&timeinfo, 500)) {
      DEBUG_PRINTLN("Time synchronized");
      return true;
    }
  }

  DEBUG_PRINTLN("Failed to obtain time from NTP");
  return false;
}

static bool shouldSyncNtp() {
  struct tm timeinfo;

  // First boot or time invalid - must sync
  if (lastNtpSync == 0 || !getLocalTime(&timeinfo, 100)) {
    return true;
  }

  // Check if time is unreasonably old (before 2026)
  if ((timeinfo.tm_year + 1900) < 2026) {
    return true;
  }

  time_t now = time(nullptr);

  // Sync if it's been more than 24 hours since last sync
  if ((now - lastNtpSync) >= NTP_SYNC_INTERVAL_S) {
    return true;
  }

  return false;
}

static bool isNighttime() {
  struct tm now;
  if (!getLocalTime(&now, 100)) {
    return false;
  }
  return now.tm_hour >= NIGHT_START_HOUR && now.tm_hour < NIGHT_END_HOUR;
}

static uint32_t calculateSleepSeconds() {
  struct tm now;
  if (!getLocalTime(&now, 100)) {
    return NORMAL_SLEEP_S;
  }

  int currentSeconds = now.tm_hour * 3600 + now.tm_min * 60 + now.tm_sec;
  int midnightSeconds = 24 * 3600;
  int secondsUntilMidnight = midnightSeconds - currentSeconds;

  if (secondsUntilMidnight <= (int)NORMAL_SLEEP_S) {
    return secondsUntilMidnight + 300;
  }

  return NORMAL_SLEEP_S;
}

static bool displayImage() {
  WiFi.setSleep(false);

  WiFiClientSecure *client = new WiFiClientSecure;
  if (!client) {
    WiFi.setSleep(true);
    return false;
  }

  client->setInsecure();
  HTTPClient *http = new HTTPClient();

  if (!http->begin(*client, IMAGE_URL)) {
    delete http;
    delete client;
    WiFi.setSleep(true);
    return false;
  }

  http->setConnectTimeout(10000);
  http->setTimeout(10000);

  // Send conditional request headers if we have them
  if (lastModified[0] != '\0') {
    http->addHeader("If-Modified-Since", lastModified);
  }
  if (etag[0] != '\0') {
    http->addHeader("If-None-Match", etag);
  }

  // Tell HTTPClient to collect cache headers
  const char* headerKeys[] = {"Last-Modified", "ETag"};
  http->collectHeaders(headerKeys, 2);

  int httpCode = http->GET();
  DEBUG_PRINTF("HTTP %d\n", httpCode);

  // Image not modified - skip display
  if (httpCode == HTTP_CODE_NOT_MODIFIED) {
    DEBUG_PRINTLN("304 - Skipping refresh");
    http->end();
    delete http;
    delete client;
    WiFi.setSleep(true);
    return true;
  }

  // Precondition failed - clear stale cache
  if (httpCode == 412) {
    DEBUG_PRINTLN("412 - Clearing stale cache");
    lastModified[0] = '\0';
    etag[0] = '\0';
    http->end();
    delete http;
    delete client;
    WiFi.setSleep(true);
    return false;
  }

  // Error - don't clear cache, retry next cycle
  if (httpCode != HTTP_CODE_OK) {
    DEBUG_PRINTF("Error %d\n", httpCode);
    http->end();
    delete http;
    delete client;
    WiFi.setSleep(true);
    return false;
  }

  DEBUG_PRINTLN("200 - Streaming image");

  // Cache new headers for next time
  if (http->hasHeader("Last-Modified")) {
    String lm = http->header("Last-Modified");
    strncpy(lastModified, lm.c_str(), sizeof(lastModified) - 1);
    lastModified[sizeof(lastModified) - 1] = '\0';
  }
  if (http->hasHeader("ETag")) {
    String et = http->header("ETag");
    strncpy(etag, et.c_str(), sizeof(etag) - 1);
    etag[sizeof(etag) - 1] = '\0';
  }

  // Stream image directly to display - no manual buffering!
  int len = http->getSize();
  if (len <= 0) {
    http->end();
    delete http;
    delete client;
    WiFi.setSleep(true);
    return false;
  }

  WiFiClient* stream = http->getStreamPtr();
  display.clearDisplay();
  bool success = display.drawJpegFromWeb(stream, 0, 0, len, false, false);

  http->end();
  delete http;
  delete client;

  if (success) {
    display.display();
    DEBUG_PRINTLN("Image displayed");
  }

  WiFi.setSleep(true);
  return success;
}

static void sendMqttMsg() {
  WiFiClient mqttWifiClient;
  PubSubClient mqttClient(mqttWifiClient);

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);

  if (!mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
    DEBUG_PRINTF("MQTT connect failed, rc=%d\n", mqttClient.state());
    return;
  }

  int temperature = display.readTemperature();
  float voltage = display.readBattery();

  char payload[64];
  snprintf(payload, sizeof(payload), "{\"temperature\":%d,\"voltage\":%.2f}", temperature, voltage);

  mqttClient.publish(MQTT_TOPIC, payload);
  mqttClient.disconnect();
}

static void goToSleep(uint32_t sleepSeconds) {
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);

  rtc_gpio_isolate(GPIO_NUM_12);

  esp_sleep_enable_timer_wakeup((uint64_t)sleepSeconds * 1000000ULL);

#ifdef INKPLATE_DEBUG
  Serial.flush();
#endif

  esp_deep_sleep_start();
}
