/**
 * StopWatch Knob — SmartKnob ported to M5Stack StopWatch
 *
 * Rotate the device in your hand to "turn" a virtual haptic knob.
 * The BMI270 IMU gyroscope tracks rotation, and the vibration motor
 * provides detent-click feedback.
 *
 * Hardware: M5Stack StopWatch (ESP32-S3R8, 466x466 AMOLED, BMI270 IMU)
 * Framework: Arduino + M5Unified
 *
 * Adapted from: https://github.com/scottbez1/smartknob
 * Target:       https://docs.m5stack.com/en/core/StopWatch
 */

#include <M5Unified.h>
#include <Preferences.h>
#include <math.h>
#include "config.h"
#include "imu_tracker.h"
#include "detent_engine.h"
#include "haptic_motor.h"
#include "display_renderer.h"
#include "ui_manager.h"

#ifndef PI
#define PI 3.14159265358979323846f
#endif

// ---- Subsystem instances ----
ImuTracker       imu;
DetentEngine     detent;
HapticMotor      haptic;
DisplayRenderer  display;
UiManager        ui;
Preferences      prefs;  // NVS for config persistence

// ---- State ----
static uint8_t   current_config_index = 0;
static uint32_t  last_display_frame_ms = 0;
static uint32_t  last_debug_print_ms = 0;
static uint32_t  loop_count = 0;
static bool      config_changed_this_loop = false;

// ---- Timing ----
static constexpr uint32_t DISPLAY_INTERVAL_MS = 33;  // ~30 fps
static constexpr uint32_t DEBUG_INTERVAL_MS   = 1000; // 1 Hz debug output

// ---- Rotation sensitivity presets ----
// Higher = more virtual angle per physical rotation
static constexpr float SENSITIVITY_DEFAULT = 1.0f;
static constexpr float SENSITIVITY_FINE    = 3.0f;  // 256 positions mode
static constexpr float SENSITIVITY_COARSE  = 0.8f;

// ---- NVS keys ----
static const char* NVS_NAMESPACE = "stopwatch";
static const char* NVS_KEY_CONFIG = "config_idx";

// ---- Forward declarations ----
void applySensitivityForConfig(uint8_t idx);
void saveConfigIndex(uint8_t idx);
uint8_t loadConfigIndex();

void setup() {
    // ---- Initialize M5Stack hardware ----
    auto cfg = M5.config();
    cfg.external_imu = true;   // BMI270 on I2C
    cfg.external_rtc = true;   // RX8130CE
    cfg.output_power = true;   // Enable power management
    M5.begin(cfg);

    // Show splash screen
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setTextDatum(CC_DATUM);
    M5.Display.setFont(&fonts::FreeSansBoldOblique18pt7b);
    M5.Display.drawString("StopWatch", 233, 180);
    M5.Display.setFont(&fonts::FreeSans12pt7b);
    M5.Display.drawString("Knob", 233, 240);
    delay(800);

    // ---- Restore last-used config from NVS ----
    current_config_index = loadConfigIndex();

    // ---- Initialize subsystems ----
    imu.begin();
    detent.setConfig(configs[current_config_index]);
    haptic.begin();
    display.begin();
    ui.begin();
    ui.setConfigIndex(current_config_index);

    // Apply sensitivity for the restored config
    applySensitivityForConfig(current_config_index);

    // Hello world
    Serial.begin(115200);
    Serial.println("StopWatch Knob ready.");
    Serial.print("Restored config ");
    Serial.print(current_config_index);
    Serial.print(": ");
    Serial.println(configs[current_config_index].descriptor);
    Serial.println("KEYA = next config, KEYB = previous config.");
    Serial.println("Touch drag on edge = virtual rotation.");
}

void loop() {
    uint32_t now = millis();
    loop_count++;
    config_changed_this_loop = false;

    // ---- 1. Update M5 internals (button state, touch, power) ----
    M5.update();

    // ---- 2. Read IMU and update virtual angle ----
    imu.update();
    float virtual_angle = imu.getAngle();

    // ---- 3. Check for touch-based angle input ----
    float touch_delta;
    if (ui.getTouchDelta(touch_delta)) {
        // Touch drag adds to virtual angle directly.
        // Bypass IMU integration during touch to avoid conflicting inputs.
        virtual_angle += touch_delta * 0.5f; // touch sensitivity factor
        imu.setAngle(virtual_angle);          // resync IMU to touch position
        detent.setAngle(virtual_angle);       // sync detent engine
    }

    // ---- 4. Update detent engine (position tracking) ----
    bool position_changed = detent.update(virtual_angle);

    // ---- 5. Trigger haptic feedback on position change ----
    if (position_changed) {
        float strength = detent.getConfig().detent_strength_unit;

        // Check if it was an endstop hit
        int32_t num_positions = detent.getConfig().max_position
                                - detent.getConfig().min_position + 1;
        int32_t pos = detent.getPosition();
        bool at_endstop = (num_positions > 0)
            && ((pos == detent.getConfig().min_position)
                || (pos == detent.getConfig().max_position));

        if (at_endstop && detent.getConfig().endstop_strength_unit > 0) {
            haptic.trigger(HapticMotor::ENDSTOP_THUD,
                          detent.getConfig().endstop_strength_unit);
        } else if (strength > 0) {
            // Use CLICK for coarse detents, TICK for fine ones
            if (detent.getConfig().position_width_radians < 3.0f * PI / 180.0f) {
                haptic.trigger(HapticMotor::TICK, strength);
            } else {
                haptic.trigger(HapticMotor::CLICK, strength);
            }
        }
    }

    // ---- 6. Advance haptic pattern state machine ----
    haptic.update();

    // ---- 7. Handle UI input (buttons, touch taps) ----
    uint8_t new_config_idx = ui.update();

    // Long-press calibrates: re-zero IMU angle and bias, reset position
    if (ui.longPress()) {
        imu.calibrate();
        detent.setConfig(configs[current_config_index]);
        haptic.trigger(HapticMotor::DOUBLE_CLICK);
        Serial.println("Calibrated: angle zeroed, bias sampled, position reset");
    }

    // Check for button-press haptic feedback
    if (ui.keyAPressed() || ui.keyBPressed() || ui.tapLeft() || ui.tapRight()) {
        haptic.trigger(HapticMotor::DOUBLE_CLICK);
    }

    // Apply config change if user cycled to a new preset
    if (new_config_idx != current_config_index) {
        current_config_index = new_config_idx;
        detent.setConfig(configs[current_config_index]);
        applySensitivityForConfig(current_config_index);
        saveConfigIndex(current_config_index);
        config_changed_this_loop = true;
    }

    // ---- 8. Render display (throttled to ~30fps) ----
    if (now - last_display_frame_ms >= DISPLAY_INTERVAL_MS) {
        last_display_frame_ms = now;
        KnobState state = detent.getState();
        // Ensure descriptor matches active config
        state.config = configs[current_config_index];
        // Preserve runtime position state
        state.current_position = detent.getPosition();
        state.sub_position_unit = detent.getSubPositionUnit();
        display.render(state, imu.getAngle());
    }

    // ---- 9. Debug output (1Hz) ----
    if (now - last_debug_print_ms >= DEBUG_INTERVAL_MS) {
        last_debug_print_ms = now;

        // Read battery level
        int batt = M5.Power.getBatteryLevel();

        Serial.print("Pos: ");
        Serial.print(detent.getPosition());
        Serial.print("  Sub: ");
        Serial.print(detent.getSubPositionUnit(), 3);
        Serial.print("  Angle: ");
        Serial.print(virtual_angle, 2);
        Serial.print(" rad  Vel: ");
        Serial.print(imu.getVelocity(), 3);
        Serial.print(" rad/s  Stationary: ");
        Serial.print(imu.isStationary() ? "Y" : "N");
        Serial.print("  Config: ");
        Serial.print(current_config_index);
        Serial.print("  Batt: ");
        Serial.print(batt);
        Serial.println("%");
    }

    // ---- 10. Yield for consistent loop timing (~120Hz target) ----
    delay(2);
}

/**
 * Apply rotation sensitivity based on the selected config preset.
 * Fine-position modes need higher sensitivity so small physical rotations
 * map to many virtual positions. Coarse modes use lower sensitivity.
 */
void applySensitivityForConfig(uint8_t idx) {
    KnobConfig& cfg = configs[idx];

    // Fine modes: 256 positions at 1° each => need 3x sensitivity
    // Coarse modes: 32 positions at ~8° each => 0.8x sensitivity
    // Standard modes: 1x sensitivity
    if (cfg.position_width_radians <= 1.5f * PI / 180.0f) {
        imu.setSensitivity(SENSITIVITY_FINE);
    } else if (cfg.position_width_radians >= 15.0f * PI / 180.0f) {
        imu.setSensitivity(SENSITIVITY_COARSE);
    } else {
        imu.setSensitivity(SENSITIVITY_DEFAULT);
    }
}

/**
 * Save the current config index to NVS so it persists across reboots.
 */
void saveConfigIndex(uint8_t idx) {
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putUChar(NVS_KEY_CONFIG, idx);
    prefs.end();
}

/**
 * Load the saved config index from NVS. Returns 0 if no saved value.
 */
uint8_t loadConfigIndex() {
    prefs.begin(NVS_NAMESPACE, true);
    uint8_t idx = prefs.getUChar(NVS_KEY_CONFIG, 0);
    prefs.end();
    if (idx >= CONFIG_COUNT) idx = 0;
    return idx;
}
