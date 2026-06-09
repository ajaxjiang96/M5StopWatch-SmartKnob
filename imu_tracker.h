#pragma once

#include <stdint.h>

/**
 * IMU Tracker — integrates BMI270 gyro Z (deg/s) to produce a cumulative
 * angle in radians for the detent engine and display.
 *
 * Internal accumulation is in degrees (matching M5Unified's native unit).
 * getAngle() converts to radians for the rest of the system.
 */
class ImuTracker {
public:
    ImuTracker();
    void begin();
    void update();

    /** Cumulative angle in radians (for detent engine, display rotation) */
    float getAngle() const { return angle_deg_ * D2R; }

    /** Current angular velocity in deg/s */
    float getVelocity() const { return gyro_z_; }

    bool isStationary() const { return stationary_; }
    void resetAngle() { angle_deg_ = 0; }
    void setAngleDeg(float deg) { angle_deg_ = deg; }
    void setSensitivity(float s) { sensitivity_ = s; }
    float getSensitivity() const { return sensitivity_; }

    /** Re-zero angle and resample bias. Call on long-press. */
    void calibrate();

private:
    float angle_deg_;         // Cumulative angle in degrees
    float gyro_z_;            // Current Z velocity (deg/s)
    float gyro_bias_;         // Estimated bias (deg/s)
    float velocity_ewma_;     // Smoothed |velocity|
    bool stationary_;
    uint32_t last_us_;
    uint32_t stationary_since_ms_;
    float sensitivity_ = 1.0f;

    static constexpr float D2R = 3.14159265358979323846f / 180.0f;
    static constexpr float BIAS_ALPHA = 0.002f;
    static constexpr float VELOCITY_EWMA_ALPHA = 0.05f;
    static constexpr float STATIONARY_THRESHOLD = 0.5f;   // deg/s
    static constexpr float BIAS_GATE = 0.3f;              // deg/s
    static constexpr uint32_t STATIONARY_TIME_MS = 300;
};
