# Home Assistant Integration for Inkplate Dashboard

This directory contains configuration files to integrate your Inkplate Dashboard with Home Assistant via MQTT.

## What Gets Published

Your Inkplate device publishes telemetry data every wake cycle (every 20 minutes during daytime) to the MQTT topic configured in `src/config.h` (default: `home/inkplate`).

**Message Format:**
```
inkplate temperature=<temp_celsius>,voltage=<battery_voltage>
```

**Example:**
```
inkplate temperature=23,voltage=3.85
```

## Sensors Created

The YAML configuration creates three sensors in Home Assistant:

1. **Inkplate Temperature** - Temperature in Celsius from the onboard sensor
2. **Inkplate Battery Voltage** - Raw battery voltage in volts
3. **Inkplate Battery Level** - Calculated battery percentage (0-100%)

## Installation

### Option 1: Include in configuration.yaml (Recommended)

1. Copy `inkplate-dashboard.yaml` to your Home Assistant config directory
2. Edit your `configuration.yaml` and add:
   ```yaml
   mqtt: !include inkplate-dashboard.yaml
   ```
3. Restart Home Assistant
4. Check Developer Tools > States for `sensor.inkplate_*` entities

### Option 2: Merge with existing MQTT configuration

If you already have an `mqtt:` section in your configuration:

1. Open `inkplate-dashboard.yaml`
2. Copy the sensor definitions (lines under `mqtt: sensor:`)
3. Paste them into your existing MQTT sensor configuration
4. Reload MQTT integration or restart Home Assistant

### Option 3: Use device grouping (optional)

For better organization in the UI, uncomment the "Alternative" section at the bottom of `inkplate-dashboard.yaml`. This groups all sensors under a single "Inkplate Dashboard" device.

## Customization

### Adjust MQTT Topic

If you changed `MQTT_TOPIC` in your `src/config.h`, update the `state_topic` and `availability_topic` in the YAML file to match.

### Adjust Battery Percentage Calculation

The default battery percentage calculation assumes:
- 4.2V = 100% (fully charged LiPo)
- 3.7V = ~50% (nominal voltage)
- 3.0V = 0% (discharged)

If you're using a different battery type, adjust the voltage thresholds in the "Inkplate Battery Level" sensor's `value_template`.

### Temperature Units

To convert to Fahrenheit in Home Assistant:
1. Keep the sensor as-is (it will remain in Celsius)
2. Use Home Assistant's unit conversion in the UI or create a template sensor:
   ```yaml
   template:
     - sensor:
         - name: "Inkplate Temperature F"
           state: "{{ (states('sensor.inkplate_temperature') | float * 9/5 + 32) | round(1) }}"
           unit_of_measurement: "°F"
   ```

## Troubleshooting

### Sensors not appearing

1. Check that your MQTT broker is running and accessible
2. Verify MQTT configuration in Home Assistant (Settings > Devices & Services > MQTT)
3. Use an MQTT client (like MQTT Explorer) to confirm messages are being published to the correct topic
4. Check Home Assistant logs for errors: Settings > System > Logs

### Sensors showing "unavailable"

1. Verify the Inkplate device is successfully connecting to WiFi and MQTT
2. Check the device is waking from sleep (should publish every 20 minutes during daytime, except midnight-6am)
3. Use serial monitor to check MQTT publish success: `pio device monitor`
4. Verify topic names match between device config and Home Assistant YAML

### Wrong values

1. Temperature values are in Celsius - this is normal
2. Battery percentage calculation may need adjustment for your specific battery type
3. Check raw MQTT messages using MQTT Explorer to verify data format

## Automation Ideas

Once integrated, you can create automations based on these sensors:

### Low Battery Alert
```yaml
automation:
  - alias: "Inkplate Low Battery Alert"
    trigger:
      - platform: numeric_state
        entity_id: sensor.inkplate_battery_level
        below: 20
    action:
      - service: notify.mobile_app
        data:
          message: "Inkplate battery is low ({{ states('sensor.inkplate_battery_level') }}%)"
```

### Temperature Monitoring
```yaml
automation:
  - alias: "Inkplate Temperature Alert"
    trigger:
      - platform: numeric_state
        entity_id: sensor.inkplate_temperature
        above: 35
    action:
      - service: notify.mobile_app
        data:
          message: "Inkplate is getting hot: {{ states('sensor.inkplate_temperature') }}°C"
```

### Device Offline Detection
```yaml
automation:
  - alias: "Inkplate Offline Alert"
    trigger:
      - platform: state
        entity_id: sensor.inkplate_temperature
        to: "unavailable"
        for:
          minutes: 30
    action:
      - service: notify.mobile_app
        data:
          message: "Inkplate dashboard hasn't reported in 30+ minutes"
```

## Lovelace Card Example

Display Inkplate stats in your dashboard:

```yaml
type: entities
title: Inkplate Dashboard
entities:
  - entity: sensor.inkplate_temperature
  - entity: sensor.inkplate_battery_voltage
  - entity: sensor.inkplate_battery_level
    icon: mdi:battery
```

Or use a more visual gauge card for battery:

```yaml
type: gauge
entity: sensor.inkplate_battery_level
min: 0
max: 100
name: Inkplate Battery
severity:
  green: 50
  yellow: 30
  red: 0
```
