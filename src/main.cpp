// Next 3 lines are a precaution, you can ignore those, and the example would also work without them
#ifndef ARDUINO_INKPLATE10
#error "Wrong board selection for this example, please select Inkplate 10 in the boards menu."
#endif

#include <Arduino.h>
#include <HTTPClient.h>
#include <Inkplate.h>
#include <WiFi.h>
#include <driver/rtc_io.h>
#include <PubSubClient.h>
#include <ESP32Time.h>
#include <Timezone.h>
#include <time.h>
#include "config.h"  // Private configuration file

Inkplate display(INKPLATE_3BIT);
ESP32Time rtc(0);  // Internal RTC, offset in seconds (0 = UTC, we'll set timezone properly)

// US Eastern Time Zone with DST rules
TimeChangeRule usDST = {"EDT", Second, Sun, Mar, 2, -240};  // UTC-4 (Daylight time begins 2nd Sunday in March at 2am)
TimeChangeRule usSTD = {"EST", First, Sun, Nov, 2, -300};   // UTC-5 (Standard time begins 1st Sunday in November at 2am)
Timezone usEastern(usDST, usSTD);

// Public constants
const unsigned long DEEP_SLEEP_DURATION = 1200UL; // 20 minutes in seconds
const unsigned long DEEP_SLEEP_ON_ERROR = 300UL; // 5 minutes on error
const unsigned long NIGHTTIME_SLEEP_DURATION = 21600UL; // 6 hours in seconds
const int NIGHTTIME_START_HOUR = 0;  // Midnight
const int NIGHTTIME_END_HOUR = 6;    // 6 AM
const int WIFI_TIMEOUT_MS = 20000; // 20 second WiFi timeout
const int IMAGE_RETRY_ATTEMPTS = 2;
const unsigned long TIME_VALIDITY_EPOCH = 1735689600UL; // Jan 1, 2026 00:00:00 UTC

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Forward declarations
bool connectWifi();
bool syncTimeFromNTP();
bool isTimeValid();
bool isNighttime();
unsigned long calculateSleepDuration();
bool displayImage();
void reconnectMqtt();
void sendMqttMsg();
void goToSleep(unsigned long sleepDuration);

void setup() {
    Serial.begin(115200);

    // Initialize display - this is CRITICAL
    Serial.println("Initializing display...");
    display.begin();
    
    // Clear the display buffer and push to screen (required for proper initialization)
    display.clearDisplay();
    display.display();

    Serial.println("Display initialized successfully");

    // Try to connect to WiFi
    if (!connectWifi()) {
        Serial.println("WiFi failed - keeping stale image, sleeping for shorter duration");
        goToSleep(DEEP_SLEEP_ON_ERROR);
        return; // Never reached, but good practice
    }

    // Check if time is valid, if not sync with NTP
    if (!isTimeValid()) {
        Serial.println("Time is not valid, syncing with NTP...");
        if (!syncTimeFromNTP()) {
            Serial.println("Failed to sync time from NTP");
            // Continue anyway, we'll try again next cycle
        }
    }

    // Display current time
    Serial.printf("Current time: %s\n", rtc.getTime("%Y-%m-%d %H:%M:%S").c_str());

    // Check if it's nighttime (midnight to 6am)
    if (isNighttime()) {
        Serial.println("It's nighttime (00:00-06:00), sleeping for 6 hours");
        goToSleep(NIGHTTIME_SLEEP_DURATION);
        return;
    }

    // Try to display image
    if (!displayImage()) {
        Serial.println("Image download failed - keeping stale image, sleeping for shorter duration");
        goToSleep(DEEP_SLEEP_ON_ERROR);
        return; // Never reached, but good practice
    }

    // Send MQTT telemetry (non-critical, just log if it fails)
    sendMqttMsg();

    // Calculate sleep duration based on time
    unsigned long sleepDuration = calculateSleepDuration();
    goToSleep(sleepDuration);
}

void loop() {
    // Empty loop as we're using deep sleep
}

bool connectWifi() {
    Serial.print("Connecting to WiFi...");

    // Use Inkplate's built-in WiFi connection with timeout
    if (!display.connectWiFi(WIFI_SSID, WIFI_PASSWORD, WIFI_TIMEOUT_MS)) {
        Serial.println("\nFailed to connect to WiFi");
        return false;
    }

    Serial.println("\nConnected to WiFi");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    return true;
}

bool syncTimeFromNTP() {
    Serial.println("Syncing time from NTP server...");
    
    // Configure time with NTP (use UTC, we'll convert with Timezone library)
    configTime(0, 0, NTP_SERVER);  // 0 offset = UTC
    
    // Wait for time to be set
    struct tm timeinfo;
    int attempts = 0;
    while (!getLocalTime(&timeinfo) && attempts < 10) {
        Serial.print(".");
        delay(500);
        attempts++;
    }
    
    if (attempts >= 10) {
        Serial.println("\nFailed to obtain time from NTP");
        return false;
    }
    
    Serial.println("\nTime synchronized successfully");
    
    // Get UTC time as epoch
    time_t utcTime = mktime(&timeinfo);
    
    // Convert to local time using Timezone library (handles DST automatically)
    time_t localTime = usEastern.toLocal(utcTime);
    struct tm* localTimeInfo = localtime(&localTime);
    
    // Set ESP32Time RTC with the local time
    rtc.setTime(localTimeInfo->tm_sec, localTimeInfo->tm_min, localTimeInfo->tm_hour,
                localTimeInfo->tm_mday, localTimeInfo->tm_mon + 1, localTimeInfo->tm_year + 1900);
    
    // Check if we're in DST or standard time
    const char* tzName = usEastern.utcIsDST(utcTime) ? "EDT" : "EST";
    Serial.printf("Set RTC to: %s %s\n", rtc.getTime("%Y-%m-%d %H:%M:%S").c_str(), tzName);
    
    return true;
}

bool isTimeValid() {
    // Check if the current time is after Jan 1, 2026
    unsigned long currentEpoch = rtc.getEpoch();
    bool valid = currentEpoch >= TIME_VALIDITY_EPOCH;
    
    Serial.printf("Current epoch: %lu, Valid threshold: %lu, Is valid: %s\n",
                  currentEpoch, TIME_VALIDITY_EPOCH, valid ? "YES" : "NO");
    
    return valid;
}

bool isNighttime() {
    int currentHour = rtc.getHour(true);  // true = 24-hour format
    
    Serial.printf("Current hour: %d, Nighttime hours: %d-%d\n",
                  currentHour, NIGHTTIME_START_HOUR, NIGHTTIME_END_HOUR);
    
    // Check if current hour is between midnight (0) and 6am
    return (currentHour >= NIGHTTIME_START_HOUR && currentHour < NIGHTTIME_END_HOUR);
}

unsigned long calculateSleepDuration() {
    int currentHour = rtc.getHour(true);
    int currentMinute = rtc.getMinute();
    
    // Calculate minutes until midnight
    int minutesUntilMidnight = (24 - currentHour - 1) * 60 + (60 - currentMinute);
    
    // If we're close to midnight (within normal sleep duration), sleep until just past midnight
    if (minutesUntilMidnight <= (DEEP_SLEEP_DURATION / 60)) {
        unsigned long sleepSeconds = (minutesUntilMidnight * 60) + 300; // Add 5 minutes buffer
        Serial.printf("Close to midnight, sleeping for %lu seconds to enter nighttime mode\n", sleepSeconds);
        return sleepSeconds;
    }
    
    // Otherwise, normal sleep duration
    Serial.printf("Using normal sleep duration: %lu seconds\n", DEEP_SLEEP_DURATION);
    return DEEP_SLEEP_DURATION;
}

bool displayImage() {
    Serial.println("Downloading and displaying image...");

    // Disable WiFi sleep for stable connection during download
    WiFi.setSleep(false);

    bool success = false;

    // Try multiple times to download the image
    for (int attempt = 1; attempt <= IMAGE_RETRY_ATTEMPTS; attempt++) {
        Serial.printf("Image download attempt %d/%d...\n", attempt, IMAGE_RETRY_ATTEMPTS);

        // Clear display buffer
        display.clearDisplay();

        // drawImage(url, x, y, dither, invert)
        // Set invert=true to fix black/white inversion
        success = display.drawImage(IMAGE_URL, 0, 0, false, false);

        if (success) {
            display.display();
            WiFi.setSleep(true);
            Serial.println("Image displayed successfully");
            return true;
        }

        Serial.printf("Attempt %d failed\n", attempt);

        // Wait before retry (except on last attempt)
        if (attempt < IMAGE_RETRY_ATTEMPTS) {
            delay(2000);
        }
    }

    // All attempts failed
    WiFi.setSleep(true);
    Serial.println("Failed to download image after all attempts");
    return false;
}

void reconnectMqtt() {
    int attempts = 0;
    while (!mqttClient.connected() && attempts < 3) {
        Serial.print("Attempting MQTT connection...");
        if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
            Serial.println("connected");
            return;
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" retrying in 5 seconds");
            delay(5000);
            attempts++;
        }
    }

    if (!mqttClient.connected()) {
        Serial.println("Failed to connect to MQTT after 3 attempts");
    }
}

void sendMqttMsg() {
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);

    if (!mqttClient.connected()) {
        reconnectMqtt();
    }

    if (mqttClient.connected()) {
        int temperature = display.readTemperature();
        float voltage = display.readBattery();

        char message[60];
        snprintf(message, sizeof(message), "inkplate temperature=%d,voltage=%.2f", temperature, voltage);

        if (mqttClient.publish(MQTT_TOPIC, message)) {
            Serial.println("MQTT message sent successfully");
        } else {
            Serial.println("Failed to send MQTT message");
        }

        mqttClient.disconnect();
    }
}

void goToSleep(unsigned long sleepDuration) {
    // Clean up WiFi
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    // Isolate GPIO for deep sleep
    rtc_gpio_isolate(GPIO_NUM_12);

    // Configure wakeup timer
    esp_sleep_enable_timer_wakeup(sleepDuration * 1000000UL);

    Serial.printf("Going to sleep for %lu seconds (%lu minutes)...\n", 
                  sleepDuration, sleepDuration / 60);
    Serial.flush();

    // Enter deep sleep
    esp_deep_sleep_start();
}

