# M5StopWatch-SmartKnob

A virtual haptic knob for the [M5Stack StopWatch](https://docs.m5stack.com/en/core/StopWatch) — rotate the device in your hand to "turn" a dial, with vibration feedback at each detent.

Ported from [scottbez1/smartknob](https://github.com/scottbez1/smartknob), replacing the BLDC motor/encoder with the BMI270 IMU gyroscope and the ERM vibration motor.

## How It Works

- **Twist the device** — the IMU gyroscope tracks rotation, converting it into a virtual knob position
- **Feel the clicks** — vibration motor pulses at each detent boundary
- **9 preset modes** — free spin, bounded ranges, on/off toggle, fine/coarse values, magnetic detents
- **World-stabilized UI** — the arc and text counter-rotate to stay fixed in space while you twist

## Hardware

- [M5Stack StopWatch](https://docs.m5stack.com/en/core/StopWatch) (ESP32-S3R8, 466×466 AMOLED, BMI270 IMU)
- No additional hardware required — uses the built-in IMU and vibration motor

## Build & Flash

### Arduino IDE

1. Install **ESP32** board package by Espressif (v3.x) via Boards Manager
2. Install libraries via Library Manager:
   - **M5Unified** by M5Stack
   - **M5GFX** by M5Stack
3. Select board: **ESP32-S3 Dev Module**
4. Settings: `PSRAM: OPI PSRAM`, `Flash Size: 16MB`, `USB CDC On Boot: Enabled`
5. Open `M5StopWatch-SmartKnob.ino` and flash

### arduino-cli

```bash
arduino-cli compile \
  --fqbn esp32:esp32:esp32s3 \
  --board-options "PSRAM=opi,FlashMode=qio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB" \
  M5StopWatch-SmartKnob

arduino-cli upload -p /dev/cu.usbmodem* \
  --fqbn esp32:esp32:esp32s3 \
  --board-options "PSRAM=opi,FlashMode=qio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB" \
  M5StopWatch-SmartKnob
```

## Controls

| Input | Action |
|-------|--------|
| Rotate device | Turn the virtual knob |
| KEYA (yellow) short press | Next config preset |
| KEYB (blue) short press | Previous config preset |
| KEYA or KEYB long press (>0.8s) | Calibrate (zero IMU, reset position) |
| Touch drag on screen edge | Virtual rotation (alternative to twisting) |

## Config Presets

| # | Mode | Description |
|---|------|-------------|
| 0 | Unbounded, no detents | Free spinning, no haptic feedback |
| 1 | Bounded 0–10, no detents | 11 positions with endstops |
| 2 | Multi-rev, no detents | 73 positions across multiple revolutions |
| 3 | On/off, strong detent | 2-position toggle with strong snap |
| 4 | Fine values, no detents | 256 positions (1° per step), smooth |
| 5 | Fine values, with detents | 256 positions with click feedback |
| 6 | Coarse values, strong detents | 32 positions with strong clicks |
| 7 | Coarse values, weak detents | 32 positions with subtle clicks |
| 8 | Magnetic detents | Only positions 2, 10, 21, 22 have detents |

## Architecture

```
M5StopWatch-SmartKnob.ino   # Main loop: IMU → detent engine → haptics → display
config.h / config.cpp        # KnobConfig presets and data structures
imu_tracker.h / .cpp         # BMI270 gyro Z integration + drift compensation
detent_engine.h / .cpp       # Virtual position tracking (from SmartKnob motor_task)
haptic_motor.h / .cpp        # Vibration patterns via M5.Power.setVibration()
display_renderer.h / .cpp    # M5GFX double-canvas radial gauge UI
ui_manager.h / .cpp          # Button, touch, and config cycling input
```

## License

Apache 2.0 — matching the original [SmartKnob](https://github.com/scottbez1/smartknob) project.

Derived from [scottbez1/smartknob](https://github.com/scottbez1/smartknob) (Apache 2.0) by Scott Bezek.
