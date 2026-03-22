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
#include "config.h"  // Private configuration file

Inkplate display(INKPLATE_1BIT);

// Public constants
const unsigned long DEEP_SLEEP_DURATION = 1200UL; // 20 minutes in seconds
const unsigned long DEEP_SLEEP_ON_ERROR = 300UL; // 5 minutes on error
const int WIFI_TIMEOUT_MS = 20000; // 20 second WiFi timeout
const int IMAGE_RETRY_ATTEMPTS = 2;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Forward declarations
bool connectWifi();
bool displayImage();
void reconnectMqtt();
void sendMqttMsg();
void goToSleep(unsigned long sleepDuration);

void setup() {
    Serial.begin(115200);

    // Initialize display - this is CRITICAL
    Serial.println("Initializing display...");
    display.begin();

    Serial.println("Display initialized successfully");

    // Try to connect to WiFi
    if (!connectWifi()) {
        Serial.println("WiFi failed - keeping stale image, sleeping for shorter duration");
        goToSleep(DEEP_SLEEP_ON_ERROR);
        return; // Never reached, but good practice
    }

    // Try to display image
    if (!displayImage()) {
        Serial.println("Image download failed - keeping stale image, sleeping for shorter duration");
        goToSleep(DEEP_SLEEP_ON_ERROR);
        return; // Never reached, but good practice
    }

    // Send MQTT telemetry (non-critical, just log if it fails)
    sendMqttMsg();

    // Everything succeeded, normal sleep duration
    goToSleep(DEEP_SLEEP_DURATION);
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
        success = display.drawImage(IMAGE_URL, 0, 0, false, true);

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

