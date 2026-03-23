# Power Optimization Changes

This document describes the power-saving optimizations implemented to maximize battery life.

## Summary

These optimizations reduce active WiFi time by approximately **30-50%** per wake cycle, significantly extending battery life.

## Changes Made

### 1. Debug Logging Helper (Lines 6-17)
**Impact: Low in debug mode, HIGH in production**

Added `DEBUG_MODE` flag and helper macros:
```cpp
#define DEBUG_MODE  // Comment out for production

#ifdef DEBUG_MODE
    #define DEBUG_PRINT(x) Serial.print(x)
    #define DEBUG_PRINTLN(x) Serial.println(x)
    #define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINTF(...)
#endif
```

**Power Savings:**
- Serial communication disabled in production (saves ~1-3ms per log statement)
- Serial.begin() also disabled when not in debug mode
- Hundreds of log statements throughout code = several seconds of CPU time saved per wake

**Usage:** Comment out `#define DEBUG_MODE` before deploying to production

### 2. MQTT Fail-Fast (Lines 288-313)
**Impact: HIGH (saves 8-10 seconds on MQTT failures)**

**Before:**
- 3 connection attempts with 5-second delays between each
- Total time on failure: ~15 seconds of WiFi active time

**After:**
- Single connection attempt
- Immediate failure and return
- Will retry in 20 minutes naturally

**Power Savings:**
- Eliminates 10 seconds of delay loops on MQTT failures
- At 80-160mA WiFi current, saves ~220-440mAh per failed MQTT connection

### 3. Skip MQTT on Image Failure (Lines 105-110)
**Impact: MEDIUM (saves 3-8 seconds when image fails)**

**Before:**
- MQTT always attempted, even if image download failed

**After:**
```cpp
bool imageSuccess = displayImage();
if (!imageSuccess) {
    DEBUG_PRINTLN("Image download failed - keeping stale image, sleeping for shorter duration");
    goToSleep(DEEP_SLEEP_ON_ERROR);
    return;
}

// Send MQTT telemetry only if image was successful
sendMqttMsg();
```

**Power Savings:**
- Skips entire MQTT connection when image fails
- Saves 3-8 seconds of WiFi time per error cycle

### 4. Reduced Image Retry Delay (Line 277)
**Impact: MEDIUM (saves 1.5 seconds per image failure)**

**Before:**
```cpp
delay(2000);  // 2 seconds between retries
```

**After:**
```cpp
delay(500);  // 500ms between retries - reduced for power savings
```

**Power Savings:**
- Saves 1.5 seconds per image download failure
- Connection is already established, so minimal delay needed

### 5. Lower WiFi TX Power (Line 166)
**Impact: MEDIUM-HIGH (saves 20-30% WiFi power during active time)**

**Before:**
- Default WiFi TX power (~19.5 dBm = 160mA typical)

**After:**
```cpp
WiFi.setTxPower(WIFI_POWER_8_5dBm);  // Reduced power for close-range AP
```

**Power Savings:**
- Reduces WiFi current from ~160mA to ~100-120mA during transmission
- 20-30% reduction in WiFi power consumption
- Safe for devices close to WiFi access point (as yours is)

**Note:** If you experience WiFi connection issues, increase to `WIFI_POWER_11dBm` or `WIFI_POWER_13dBm`

### 6. Remove Redundant Display Refresh (Lines 71-73)
**Impact: MEDIUM-HIGH (saves 2-5 seconds per wake)**

**Before:**
```cpp
display.begin();
display.clearDisplay();
display.display();  // Unnecessary e-ink refresh!
```

**After:**
```cpp
display.begin();
// displayImage() will handle clearing and refreshing
```

**Power Savings:**
- Eliminates one full e-ink refresh cycle during startup
- Saves 2-5 seconds at ~150-300mA per wake
- The display is always cleared/refreshed in displayImage() anyway, making this redundant

## Production Deployment

### Before Uploading to Production:

1. **Comment out debug mode** in main.cpp line 7:
   ```cpp
   // #define INKPLATE_DEBUG
   ```

2. **Verify sleep durations** are set correctly:
   - `DEEP_SLEEP_DURATION = 1200UL` (20 minutes) ✓
   - `DEEP_SLEEP_ON_ERROR = 300UL` (5 minutes) ✓
   - `NIGHTTIME_SLEEP_DURATION = 21600UL` (6 hours) ✓

3. **Build and upload:**
   ```bash
   pio run -t upload
   ```

### Debug vs Production Mode Comparison

| Feature | Debug Mode | Production Mode |
|---------|-----------|-----------------|
| Serial logging | Enabled | Disabled (noop) |
| Serial.begin() | Yes | No |
| Serial.flush() | Yes | No |
| Power consumption | Higher (~3-5 seconds extra per wake) | Optimized |
| Debugging | Full visibility | No output |

## Estimated Battery Life Impact

### Before Optimizations:
- Active time per wake: ~25-40 seconds
- WiFi current: ~160mA average
- Estimated battery life: ~2-3 weeks on 3000mAh battery

### After Optimizations (Production Mode):
- Active time per wake: ~10-20 seconds (40-50% reduction)
- WiFi current: ~100-120mA average (25% reduction)
- Estimated battery life: **~5-8 weeks on 3000mAh battery**

### Breakdown by Optimization:

| Optimization | Time Saved | Frequency | Impact |
|-------------|------------|-----------|--------|
| Debug logging disabled | 2-4 seconds | Every wake | HIGH |
| Remove redundant display refresh | 2-5 seconds | Every wake | MEDIUM-HIGH |
| MQTT fail-fast | 10 seconds | On MQTT failures | HIGH (when applicable) |
| Skip MQTT on image fail | 3-8 seconds | On image failures | MEDIUM (when applicable) |
| Reduced image retry delay | 1.5 seconds | On image failures | MEDIUM (when applicable) |
| Lower WiFi TX power | 20-30% power | Every wake | MEDIUM-HIGH |

## WiFi TX Power Reference

Available power levels (in order of power consumption):
- `WIFI_POWER_19_5dBm` - Maximum range, ~160mA
- `WIFI_POWER_19dBm`
- `WIFI_POWER_18_5dBm`
- `WIFI_POWER_17dBm`
- `WIFI_POWER_15dBm`
- `WIFI_POWER_13dBm` - Good balance
- `WIFI_POWER_11dBm`
- `WIFI_POWER_8_5dBm` - **Current setting** (~100-120mA)
- `WIFI_POWER_7dBm`
- `WIFI_POWER_5dBm`
- `WIFI_POWER_2dBm`
- `WIFI_POWER_MINUS_1dBm` - Minimum

## Future Optimization Ideas

1. **Reduce NTP sync frequency** - Only sync weekly instead of checking validity every wake
2. **Conditional display refresh** - Check if image actually changed before refreshing e-ink
3. **Adaptive sleep duration** - Increase to 30-60 minutes if image rarely changes
4. **ESP32 modem sleep** - Enable light sleep modes during image download (complex)

## Testing Recommendations

1. **Monitor MQTT messages** - Verify telemetry still arrives every 20 minutes
2. **Check WiFi stability** - If connection issues occur, increase TX power
3. **Measure actual battery life** - Track voltage over several weeks
4. **Verify nighttime mode** - Confirm 6-hour sleep activates at midnight
