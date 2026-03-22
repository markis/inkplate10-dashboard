# Agent Guidelines for Inkplate Dashboard

This document provides guidelines for AI coding agents working on this PlatformIO-based ESP32/Arduino C++ project.

## Project Overview

- **Platform**: PlatformIO + ESP32 (Inkplate 10 e-paper display)
- **Framework**: Arduino
- **Language**: C++ (Arduino flavor)
- **Hardware**: ESP32-based Inkplate 10 board with PSRAM

## Build, Upload & Monitor Commands

### Build
```bash
# Clean and build the project
pio run

# Build with verbose output
pio run -v

# Clean build artifacts
pio run --target clean
```

### Upload & Monitor
```bash
# Upload to device
pio run -t upload

# Monitor serial output (115200 baud) - Note: may have terminal issues on some systems
pio device monitor

# If PIO monitor doesn't work, use alternative methods:
# - minicom: minicom -D /dev/cu.usbserial-110 -b 115200
# - screen: screen /dev/cu.usbserial-110 115200
# - cu: cu -l /dev/cu.usbserial-110 -s 115200
```

### Device Management
```bash
# List connected devices
pio device list

# Monitor with specific baud rate
pio device monitor --baud 115200
```

## Testing

**Note**: This project currently has no unit test framework configured. When adding tests:

```bash
# Run tests (once configured)
pio test

# Run specific test
pio test -f test_name

# Run tests with verbose output
pio test -v
```

To add testing, create a `test/` directory and add test files following PlatformIO's testing framework.

## Code Style Guidelines

### File Organization

1. **Preprocessor guards** at the top (board verification)
2. **Includes** grouped logically (Arduino, external libs, project)
3. **Global objects** (display, clients)
4. **Constants** (UPPERCASE with const or #define)
5. **Enums and types**
6. **Forward declarations** for functions
7. **setup() and loop()** functions
8. **Implementation functions**

### Naming Conventions

- **Constants**: `UPPER_SNAKE_CASE` (e.g., `DEEP_SLEEP_DURATION`, `WIFI_SSID`)
- **Functions**: `camelCase` (e.g., `connectWifi()`, `displayImage()`)
- **Variables**: `camelCase` (e.g., `httpCode`, `startAttemptTime`)
- **Enum classes**: `PascalCase` with `PascalCase` values (e.g., `ImageType::JPEG`)
- **Pointers**: descriptive names with `Ptr` suffix where appropriate (e.g., `buffPtr`, `streamPtr`)

### Types and Type Safety

- Use `enum class` instead of plain enums for type safety
- Use explicit types: `unsigned long`, `uint8_t`, `int`, `size_t`
- Use `const` for immutable values
- Use `nullptr` instead of `NULL`
- Always check pointer validity before dereferencing

### Memory Management

- **PSRAM allocation**: Use `ps_malloc()` for large buffers (images, data)
- **Always free**: Every `malloc()`/`ps_malloc()` must have a matching `free()`
- **Check allocations**: Always verify allocation succeeded before use
```cpp
uint8_t* buffer = (uint8_t*)ps_malloc(size);
if (buffer == nullptr) {
    // Handle error
    return;
}
// Use buffer...
free(buffer);
```

### Error Handling

- Use **early returns** for error conditions
- Log errors to Serial with descriptive messages
- Use `Serial.printf()` for formatted output
- Handle network timeouts explicitly
- Implement retry logic with maximum attempts

**Pattern**:
```cpp
if (!success) {
    Serial.println("Operation failed");
    return;
}
```

### Network Operations

- Always set timeouts for WiFi and HTTP operations
- Check connection status before operations
- Use `WiFi.setSleep(false)` for stable connections during downloads
- Re-enable sleep after operations: `WiFi.setSleep(true)`
- Disconnect and turn off WiFi before deep sleep

### Serial Communication

- Initialize Serial at 115200 baud in `setup()`
- Use descriptive log messages
- Use `Serial.printf()` for formatted output
- Flush Serial before sleep: `Serial.flush()`

### Control Flow

- Use `switch` statements for type/enum dispatching
- Always include `default` case in switch statements
- Prefer early returns over deep nesting
- Use descriptive loop conditions

### Comments

- Add comments for non-obvious logic
- Document magic numbers and hardware-specific values
- Use inline comments sparingly, prefer self-documenting code
- Add block comments for function purposes when needed

### Deep Sleep and Power Management

- Isolate GPIO before sleep: `rtc_gpio_isolate(GPIO_NUM_12)`
- Convert sleep duration to microseconds: `seconds * 1000000UL`
- Disconnect WiFi and disable: `WiFi.disconnect(true); WiFi.mode(WIFI_OFF)`
- Flush Serial before entering sleep

### Buffer Operations

- Use fixed-size temporary buffers for streaming (e.g., 512 bytes)
- Use `memcpy()` for buffer copying
- Use `memcmp()` for signature comparison
- Track remaining bytes during streaming operations

## Common Patterns

### Use Inkplate's Built-in Functions
**IMPORTANT**: The Inkplate library provides high-level functions that handle complexity internally:

```cpp
// WiFi connection (preferred over manual WiFi.begin())
display.connectWiFi(ssid, password, timeout_ms);

// Image download and display (handles HTTP, buffering, and decoding automatically)
display.drawImage(url, x, y, dither, invert);  // Auto-detects PNG, JPEG, BMP

// These are much more reliable than manual HTTP/buffer management
```

### WiFi Connection with Timeout (if manual control needed)
```cpp
unsigned long startAttemptTime = millis();
while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(100);
}
```

### MQTT Retry Logic
```cpp
int attempts = 0;
while (!mqttClient.connected() && attempts < 3) {
    // Attempt connection
    attempts++;
}
```

### Image Type Detection
Use magic number comparison for file type detection rather than relying on file extensions.

## Configuration

All user-configurable values should be:
- Declared as `const` at the top of the file
- Clearly commented
- Use descriptive names (e.g., `IMAGE_URL` not `URL`)

## Dependencies

Managed in `platformio.ini`:
- Inkplate Arduino library (from GitHub)
- PubSubClient v2.8+

## Important Notes

- **No dynamic strings**: Minimize String class usage; prefer char arrays and snprintf()
- **Stack size**: Be mindful of large stack allocations; use heap for big buffers
- **Watchdog**: ESP32 watchdog may trigger on long operations; add yields if needed
- **Serial availability**: Always check operations complete before sleep
- **PSRAM required**: This board uses PSRAM; verify availability for large allocations
