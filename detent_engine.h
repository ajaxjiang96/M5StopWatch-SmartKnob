#pragma once

#include "config.h"
#include <stdint.h>

/**
 * Detent Engine — virtual position tracking adapted from SmartKnob's
 * motor_task.cpp position state machine.
 *
 * Takes a virtual angle from the IMU tracker and computes:
 *  - Which integer detent position the knob is at
 *  - The fractional sub_position within the current detent
 *  - Whether a position change occurred (to trigger haptic feedback)
 */
class DetentEngine {
public:
    DetentEngine();

    /**
     * Apply a new config. Handles position updates and detent center
     * recalculation (mirrors the SmartKnob CONFIG command handler).
     */
    void setConfig(const KnobConfig& config);

    /**
     * Update the detent engine with a new virtual angle reading.
     * Returns true if the integer position changed (trigger haptic).
     */
    bool update(float virtual_angle);

    /** Get the current combined state. */
    KnobState getState() const;

    /** Get the current integer position. */
    int32_t getPosition() const { return current_position_; }

    /** Get the sub-position unit (fractional position within detent). */
    float getSubPositionUnit() const { return latest_sub_position_unit_; }

    /** Force-set the virtual angle (e.g., from touch input). */
    void setAngle(float angle) { virtual_angle_ = angle; }

    /** Access the active config. */
    const KnobConfig& getConfig() const { return config_; }

private:
    KnobConfig config_;
    int32_t current_position_;
    float current_detent_center_;
    float latest_sub_position_unit_;
    float virtual_angle_;

    // Idle correction state
    float idle_check_velocity_ewma_;
    uint32_t last_idle_start_ms_;

    // Tuning constants (from SmartKnob motor_task.cpp)
    static constexpr float IDLE_VELOCITY_EWMA_ALPHA = 0.001f;
    static constexpr float IDLE_VELOCITY_RAD_PER_SEC = 0.05f;
    static constexpr uint32_t IDLE_CORRECTION_DELAY_MILLIS = 500;
    static constexpr float IDLE_CORRECTION_MAX_ANGLE_RAD = 5.0f * 3.14159265358979323846f / 180.0f;
    static constexpr float IDLE_CORRECTION_RATE_ALPHA = 0.0005f;
};
