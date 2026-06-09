#pragma once

#include <stdint.h>

class ImuTracker {
public:
    ImuTracker();
    void begin();
    void update();

    float getAngle() const { return virtual_angle_; }
    float getVelocity() const { return gyro_z_; }
    bool isStationary() const { return stationary_; }
    void resetAngle() { virtual_angle_ = 0; }
    void setAngle(float a) { virtual_angle_ = a; }
    void setSensitivity(float s) { sensitivity_ = s; }
    float getSensitivity() const { return sensitivity_; }
    void calibrate();

private:
    float virtual_angle_;
    float gyro_z_;
    float gyro_bias_;
    float velocity_ewma_;
    bool stationary_;
    uint32_t last_update_us_;
    uint32_t stationary_since_ms_;
    float sensitivity_ = 1.0f;

    static constexpr float GYRO_DEADBAND = 0.008f;
    static constexpr float BIAS_ALPHA = 0.0005f;
    static constexpr float VELOCITY_EWMA_ALPHA = 0.01f;
    static constexpr float STATIONARY_THRESHOLD = 0.03f;
    static constexpr uint32_t STATIONARY_TIME_MS = 400;
};
