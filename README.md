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
- **Smart NTP Sync**: Synchronizes time from NTP server only every 24 hours to conserve battery
- **Timezone Support**: Full POSIX timezone configuration with automatic DST adjustment
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
| `MQTT_CLIENT_ID` | MQTT client identifier (default: Inkplate) |
| `MQTT_USER` | MQTT username |
| `MQTT_PASSWORD` | MQTT password |
| `MQTT_TOPIC` | MQTT topic for telemetry |
| `IMAGE_URL` | URL of the image to display |
| `NTP_SERVER` | NTP server address (default: pool.ntp.org) |
| `TIMEZONE` | POSIX timezone string (e.g., "EST5EDT,M3.2.0/2,M11.1.0/2" for Eastern Time with DST) |

Public constants in `main.cpp`:
- `NORMAL_SLEEP_S`: 1200s (20 minutes) - normal operation
- `ERROR_SLEEP_S`: 300s (5 minutes) - on failures
- `NIGHT_SLEEP_S`: 21600s (6 hours) - midnight to 6 AM
- `NIGHT_START_HOUR`: 0 (midnight, 24-hour format)
- `NIGHT_END_HOUR`: 6 (6 AM, 24-hour format)
- `WIFI_TIMEOUT_MS`: 20000ms (20 seconds)
- `IMAGE_RETRY_ATTEMPTS`: 2
- `NTP_SYNC_INTERVAL_S`: 86400s (24 hours) - NTP resync interval

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
- Timezone @ 1.2.4

## Nighttime Sleep Feature

See [NIGHTTIME_SLEEP.md](docs/NIGHTTIME_SLEEP.md) for detailed information about the nighttime sleep scheduling feature, including:
- How time synchronization works
- Timezone configuration examples
- Power savings calculations
- Troubleshooting guide
