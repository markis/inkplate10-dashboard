# Inkplate 10 Dashboard

ESP32-based e-ink dashboard that downloads and displays an image from a URL, then sends telemetry via MQTT.

## Setup

### 1. Clone and Install Dependencies

```bash
git clone <your-repo-url>
cd ink-dashboard
pio lib install
```

### 2. Configure Private Settings

Copy the example config file and edit with your credentials:

```bash
cp src/config.h.example src/config.h
```

Edit `src/config.h` with your settings:
- WiFi SSID and password
- MQTT broker address and credentials
- Image URL

**Important:** `src/config.h` is gitignored to keep your credentials private. Never commit this file!

### 3. Build and Upload

```bash
# Build the project
pio run

# Upload to device
pio run -t upload

# Monitor serial output
pio device monitor --baud 115200
```

## Features

- **Image Display**: Downloads JPEG images via HTTPS and displays them on the e-ink screen
- **Error Handling**: Preserves last image on WiFi/network failures, retries with shorter sleep intervals
- **MQTT Telemetry**: Reports temperature and battery voltage
- **Deep Sleep**: Sleeps for 20 minutes between updates (5 minutes on errors)
- **E-ink Friendly**: Only updates display when new image successfully downloads

## Configuration

All private configuration is in `src/config.h`:

| Setting | Description |
|---------|-------------|
| `WIFI_SSID` | Your WiFi network name |
| `WIFI_PASSWORD` | Your WiFi password |
| `MQTT_SERVER` | MQTT broker IP address |
| `MQTT_PORT` | MQTT broker port (default: 1883) |
| `MQTT_USER` | MQTT username |
| `MQTT_PASSWORD` | MQTT password |
| `MQTT_TOPIC` | MQTT topic for telemetry |
| `IMAGE_URL` | URL of the image to display |

Public constants in `main.cpp`:
- `DEEP_SLEEP_DURATION`: 1200s (20 minutes)
- `DEEP_SLEEP_ON_ERROR`: 300s (5 minutes)
- `WIFI_TIMEOUT_MS`: 20000ms (20 seconds)
- `IMAGE_RETRY_ATTEMPTS`: 2

## Hardware

- **Board**: Inkplate 10 (ESP32-based e-paper display)
- **Display**: 9.7" e-ink, 1200x825 pixels
- **Platform**: PlatformIO + ESP32 + Arduino framework

## Dependencies

- InkplateLibrary @ 10.2.1
- PubSubClient @ 2.8.0
