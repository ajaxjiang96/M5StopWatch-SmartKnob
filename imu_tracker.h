#pragma once

#include <stdint.h>

/**
 * IMU Tracker — reads BMI270 gyroscope and integrates Z-axis angular velocity
 * to produce a cumulative virtual angle that replaces the motor shaft angle
 * from the original SmartKnob.
 *
 * Drift compensation: deadband → bias EWMA when stationary → gravity reference.
 */
class ImuTracker {
public:
    ImuTracker();

    /** Initialize IMU hardware. Call once in setup(). */
    void begin();

    /**
     * Update the tracker. Call every loop iteration (~120Hz).
     * Reads gyro, integrates angle, updates drift compensation.
     */
    void update();

    /** Get the cumulative virtual angle in radians (unbounded). */
    float getAngle() const { return virtual_angle_; }

    /** Get the instantaneous angular velocity in rad/s. */
    float getVelocity() const { return gyro_z_; }

    /** Check if the device is currently stationary. */
    bool isStationary() const { return stationary_; }

    /** Reset the virtual angle to zero. */
    void resetAngle() { virtual_angle_ = 0; }

    /** Set the virtual angle to a specific value (e.g., after touch input). */
    void setAngle(float a) { virtual_angle_ = a; }

    /** Set sensitivity multiplier for angle mapping (default 1.0). */
    void setSensitivity(float s) { sensitivity_ = s; }
    float getSensitivity() const { return sensitivity_; }

    /** Re-zero: reset angle to 0 and recalibrate bias. Call on long-press. */
    void calibrate();

private:
    float virtual_angle_;       // Cumulative integrated angle (radians)
    float gyro_z_;              // Current Z angular velocity (rad/s)
    float gyro_bias_;           // Estimated gyro bias (rad/s)
    float velocity_ewma_;       // EWMA of velocity for stationarity detection
    bool stationary_;           // Currently stationary?

    uint32_t last_update_us_;   // Micros at last update
    uint32_t stationary_since_ms_; // How long we've been stationary
    float sensitivity_;         // Angle sensitivity multiplier

    // Tuning constants (BMI270 noise floor ~2 deg/s = 0.035 rad/s)
    static constexpr float GYRO_DEADBAND = 0.005f;       // rad/s for bias update gating only
    static constexpr float BIAS_ALPHA = 0.002f;           // bias EWMA alpha
    static constexpr float VELOCITY_EWMA_ALPHA = 0.02f;   // velocity smoothing
    static constexpr float STATIONARY_THRESHOLD = 0.025f; // rad/s (~1.4 deg/s)
    static constexpr uint32_t STATIONARY_TIME_MS = 300;
};
