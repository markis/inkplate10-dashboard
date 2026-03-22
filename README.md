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
- **Nighttime Sleep**: Automatically sleeps for 6 hours (midnight to 6 AM) to save power
- **NTP Time Sync**: Synchronizes time from NTP server, persists with CR2032 RTC battery
- **Error Handling**: Preserves last image on WiFi/network failures, retries with shorter sleep intervals
- **MQTT Telemetry**: Reports temperature and battery voltage
- **Smart Sleep Scheduling**: Adaptive sleep duration based on time of day
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
| `NTP_SERVER` | NTP server address (default: pool.ntp.org) |
| `GMT_OFFSET_SEC` | Timezone offset in seconds (e.g., -28800 for PST) |
| `DAYLIGHT_OFFSET_SEC` | Daylight saving offset in seconds (3600 or 0) |

Public constants in `main.cpp`:
- `DEEP_SLEEP_DURATION`: 1200s (20 minutes) - normal operation
- `DEEP_SLEEP_ON_ERROR`: 300s (5 minutes) - on failures
- `NIGHTTIME_SLEEP_DURATION`: 21600s (6 hours) - midnight to 6 AM
- `NIGHTTIME_START_HOUR`: 0 (midnight, 24-hour format)
- `NIGHTTIME_END_HOUR`: 6 (6 AM, 24-hour format)
- `WIFI_TIMEOUT_MS`: 20000ms (20 seconds)
- `IMAGE_RETRY_ATTEMPTS`: 2

## Hardware

- **Board**: Inkplate 10 (ESP32-based e-paper display)
- **Display**: 9.7" e-ink, 1200x825 pixels
- **Platform**: PlatformIO + ESP32 + Arduino framework
- **CR2032 Battery**: Required for RTC time persistence between deep sleep cycles

## Dependencies

- InkplateLibrary @ 10.2.1
- PubSubClient @ 2.8.0
- NTPClient @ 3.2.1
- ESP32Time @ 2.0.6

## Nighttime Sleep Feature

See [NIGHTTIME_SLEEP.md](NIGHTTIME_SLEEP.md) for detailed information about the nighttime sleep scheduling feature, including:
- How time synchronization works
- Timezone configuration examples
- Power savings calculations
- Troubleshooting guide
