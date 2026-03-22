# Nighttime Sleep Feature

## Overview

The Inkplate dashboard now includes intelligent nighttime sleep scheduling that puts the display into deep sleep from midnight to 6 AM, saving power during hours when the display isn't needed.

## How It Works

### Time Synchronization

1. **Initial Boot**: On first boot or when time is invalid (before Jan 1, 2026), the device connects to an NTP server to sync the current time
2. **RTC Persistence**: With the CR2032 battery installed, the ESP32's RTC maintains accurate time between deep sleep cycles
3. **Automatic Re-sync**: If the RTC time becomes invalid (e.g., battery removed), the device automatically re-syncs on next boot

### Nighttime Detection

Every time the device wakes up:
1. Checks current time from the RTC
2. If current hour is between 00:00-06:00 (midnight to 6 AM), enters 6-hour deep sleep
3. Otherwise, proceeds with normal operation (image update, MQTT telemetry)

### Smart Sleep Duration

The device intelligently calculates sleep duration:

- **Normal Operation** (06:00-23:59): Sleeps for 20 minutes between updates
- **Approaching Midnight**: If within 20 minutes of midnight, sleeps until just after midnight to enter nighttime mode
- **Nighttime Mode** (00:00-06:00): Sleeps for full 6 hours
- **Error Conditions**: Sleeps for 5 minutes if WiFi/image download fails

## Configuration

### Time Zone Settings

Edit `src/config.h` to set your timezone:

```cpp
// NTP Configuration
const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SEC = -28800;  // PST is UTC-8 (change to your timezone)
const int DAYLIGHT_OFFSET_SEC = 3600; // 1 hour for DST (0 if not used)
```

### Time Zone Examples

| Location | GMT_OFFSET_SEC | DAYLIGHT_OFFSET_SEC |
|----------|----------------|---------------------|
| PST (UTC-8) | -28800 | 3600 |
| MST (UTC-7) | -25200 | 3600 |
| CST (UTC-6) | -21600 | 3600 |
| EST (UTC-5) | -18000 | 3600 |
| UTC | 0 | 0 |
| CET (UTC+1) | 3600 | 3600 |

### Nighttime Hours

To customize nighttime hours, edit `src/main.cpp`:

```cpp
const int NIGHTTIME_START_HOUR = 0;  // Midnight (24-hour format)
const int NIGHTTIME_END_HOUR = 6;    // 6 AM
```

### Time Validity Threshold

The device checks if time is valid by comparing against Jan 1, 2026. To change this:

```cpp
const unsigned long TIME_VALIDITY_EPOCH = 1735689600UL; // Unix epoch timestamp
```

## Power Savings

Assuming the display normally updates every 20 minutes:

- **Without nighttime sleep**: ~72 updates per day (every 20 min)
- **With nighttime sleep**: ~54 updates per day (18 hours active)
- **Power savings**: ~25% reduction in wake cycles and WiFi usage

## Hardware Requirements

- **CR2032 Battery**: Required for RTC persistence between deep sleep cycles
- **WiFi Connection**: Required for initial NTP time sync
- **Internet Access**: Must reach NTP server (pool.ntp.org by default)

## Troubleshooting

### Time Not Syncing

Check serial output for:
```
Time is not valid, syncing with NTP...
Failed to sync time from NTP
```

**Solutions**:
- Verify WiFi credentials in `config.h`
- Check internet connectivity
- Try alternative NTP server: `time.google.com` or `time.nist.gov`
- Check firewall isn't blocking NTP (UDP port 123)

### RTC Losing Time

If time resets after deep sleep:
- Check CR2032 battery is properly installed
- Replace CR2032 if voltage is low
- Verify battery holder has good contact

### Display Not Updating During Day

Check if device thinks it's nighttime:
```
Current hour: X, Nighttime hours: 0-6
It's nighttime (00:00-06:00), sleeping for 6 hours
```

**Solutions**:
- Verify timezone settings (GMT_OFFSET_SEC)
- Check if daylight saving offset is correct
- Manually sync time by resetting the device

### Always Sleeping for 6 Hours

The device may think every hour is nighttime if:
- Time sync failed and RTC shows hour 0-5
- CR2032 battery is dead/missing
- Force a time resync by removing CR2032 briefly

## Serial Output Examples

### Normal Operation (Daytime)
```
Initializing display...
Display initialized successfully
Connecting to WiFi...
Connected to WiFi
Current epoch: 1767225600, Valid threshold: 1735689600, Is valid: YES
Current time: 2026-01-01 14:30:00
Current hour: 14, Nighttime hours: 0-6
Downloading and displaying image...
Using normal sleep duration: 1200 seconds
Going to sleep for 1200 seconds (20 minutes)...
```

### Nighttime Detection
```
Current time: 2026-01-01 02:15:00
Current hour: 2, Nighttime hours: 0-6
It's nighttime (00:00-06:00), sleeping for 6 hours
Going to sleep for 21600 seconds (360 minutes)...
```

### First Boot / Time Sync
```
Current epoch: 0, Valid threshold: 1735689600, Is valid: NO
Time is not valid, syncing with NTP...
Syncing time from NTP server...
..........
Time synchronized successfully
Set RTC to: 2026-03-22 10:45:30
```
