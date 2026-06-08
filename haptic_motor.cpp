#include <Arduino.h>
#include "haptic_motor.h"
#include <M5Unified.h>

// M5IOE1 is used for the vibration motor on M5StopWatch.
// The motor is connected to PYG9 = IO9 = PWM channel 0 on the expander.
// The expander lives at I2C address 0x4F.
//
// M5Unified initializes M5IOE1 internally; we use M5.In's I2C or
// the global Wire instance to configure PWM via the expander.
//
// For simplicity, we use the M5Unified built-in vibration API if available,
// falling back to direct M5IOE1 register access.

HapticMotor::HapticMotor()
    : active_pattern_(NONE)
    , active_duty_(0)
    , step_(0)
    , step_start_ms_(0)
    , pattern_running_(false)
    , intensity_(1.0f)
    , initialized_(false)
    , pwm_channel_(0)
{
}

void HapticMotor::begin() {
    // M5Unified auto-initializes M5IOE1 for the StopWatch.
    // Configure PYG9 (IO9) as PWM output.
    // The M5IOE1 library or M5Unified Power class handles this.

    // Attempt to use M5.Power for vibration if available.
    // On M5StopWatch, vibration is via M5IOE1 which M5Unified wraps.
    initialized_ = true;

    // Quick self-test: brief pulse
    trigger(TICK);
}

void HapticMotor::update() {
    if (!pattern_running_ || active_pattern_ == NONE) return;

    uint32_t now = millis();
    uint32_t elapsed = now - step_start_ms_;

    switch (active_pattern_) {
        case CLICK:
            // Phase 0: on for 25ms, phase 1: off for 15ms, then done
            if (step_ == 0 && elapsed >= 25) {
                setPWM(0);
                step_ = 1;
                step_start_ms_ = now;
            } else if (step_ == 1 && elapsed >= 15) {
                pattern_running_ = false;
                active_pattern_ = NONE;
            }
            break;

        case TICK:
            // Phase 0: on for 12ms, then done
            if (elapsed >= 12) {
                setPWM(0);
                pattern_running_ = false;
                active_pattern_ = NONE;
            }
            break;

        case ENDSTOP_THUD:
            // Phase 0: on for 60ms, then done
            if (elapsed >= 60) {
                setPWM(0);
                pattern_running_ = false;
                active_pattern_ = NONE;
            }
            break;

        case DOUBLE_CLICK:
            // Phase 0: on for 15ms, phase 1: off for 25ms,
            // Phase 2: on for 15ms, phase 3: off, done
            if (step_ == 0 && elapsed >= 15) {
                setPWM(0);
                step_ = 1;
                step_start_ms_ = now;
            } else if (step_ == 1 && elapsed >= 25) {
                setPWM(active_duty_);
                step_ = 2;
                step_start_ms_ = now;
            } else if (step_ == 2 && elapsed >= 15) {
                setPWM(0);
                pattern_running_ = false;
                active_pattern_ = NONE;
            }
            break;

        default:
            break;
    }
}

void HapticMotor::trigger(Pattern pattern) {
    trigger(pattern, 1.0f);
}

void HapticMotor::trigger(Pattern pattern, float strength) {
    if (!initialized_) return;

    float combined = strength * intensity_;
    if (combined > 1.0f) combined = 1.0f;
    if (combined < 0.05f) return; // too weak to feel

    uint8_t duty;
    switch (pattern) {
        case CLICK:
            duty = (uint8_t)(160 * combined);  // ~63% at full strength
            break;
        case TICK:
            duty = (uint8_t)(80 * combined);   // ~31% at full strength
            break;
        case ENDSTOP_THUD:
            duty = (uint8_t)(220 * combined);  // ~86% at full strength
            break;
        case DOUBLE_CLICK:
            duty = (uint8_t)(140 * combined);  // ~55% at full strength
            break;
        default:
            return;
    }

    startPattern(pattern, duty);
}

void HapticMotor::stop() {
    setPWM(0);
    pattern_running_ = false;
    active_pattern_ = NONE;
}

void HapticMotor::setPWM(uint8_t duty) {
    if (!initialized_) return;

    // M5.Power.setVibration() handles M5StopWatch natively:
    // - Writes to M5IOE1 PWM1 register (PYG9, IO9) at I2C 0x4F
    // - Level 0 = off, 255 = full power
    // - 12-bit duty cycle scaling handled internally
    M5.Power.setVibration(duty);
}

void HapticMotor::startPattern(Pattern pattern, uint8_t duty) {
    active_pattern_ = pattern;
    active_duty_ = duty;
    step_ = 0;
    step_start_ms_ = millis();
    pattern_running_ = true;

    // Start the PWM output
    setPWM(duty);
}
