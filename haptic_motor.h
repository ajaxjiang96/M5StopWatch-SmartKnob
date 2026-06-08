#pragma once

#include <stdint.h>

/**
 * Haptic Motor — controls the ERM vibration motor via M5IOE1 PWM (PYG9).
 *
 * Non-blocking pattern sequencer: patterns are triggered and played out
 * over time using millis()-based timing, so the main loop is never blocked.
 */
class HapticMotor {
public:
    /** Haptic pattern types */
    enum Pattern {
        NONE = 0,
        CLICK,         // Short sharp pulse for regular position change
        TICK,          // Very brief pulse for fine-detent position change
        ENDSTOP_THUD,  // Long strong pulse for endstop hit
        DOUBLE_CLICK,  // Two short pulses for button press feel
    };

    HapticMotor();

    /** Initialize the M5IOE1 expander and PWM channel. Call once in setup(). */
    void begin();

    /** Call every loop iteration to advance the pattern state machine. */
    void update();

    /** Trigger a haptic pattern (non-blocking). */
    void trigger(Pattern pattern);

    /** Trigger with scaled intensity based on detent strength. */
    void trigger(Pattern pattern, float strength);

    /** Stop any active pattern immediately. */
    void stop();

    /** Set base intensity (0.0 - 1.0). */
    void setIntensity(float i) { intensity_ = i; }

private:
    void setPWM(uint8_t duty); // 0-255
    void startPattern(Pattern pattern, uint8_t duty);

    Pattern active_pattern_;
    uint8_t active_duty_;
    uint8_t step_;
    uint32_t step_start_ms_;
    bool pattern_running_;
    float intensity_;

    // PWM control
    bool initialized_;
    uint8_t pwm_channel_;

    // Tuning constants
    static constexpr uint16_t PWM_FREQUENCY = 2000;  // Hz for ERM motor
};
