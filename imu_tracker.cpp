#include <Arduino.h>
#include "imu_tracker.h"
#include <M5Unified.h>
#include <math.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

ImuTracker::ImuTracker()
    : virtual_angle_(0)
    , gyro_z_(0)
    , gyro_bias_(0)
    , velocity_ewma_(0)
    , stationary_(false)
    , last_update_us_(0)
    , stationary_since_ms_(0)
    , sensitivity_(1.0f)
{
}

void ImuTracker::begin() {
    if (!M5.Imu.isEnabled()) return;

    // Auto-calibrate on boot: zero angle, sample fresh bias
    calibrate();
    Serial.println("IMU auto-calibrated on boot");
}

void ImuTracker::update() {
    if (!M5.Imu.isEnabled()) return;

    uint32_t now_us = micros();
    float dt = (now_us - last_update_us_) * 1e-6f;
    last_update_us_ = now_us;

    // Clamp dt to avoid huge jumps after delays
    if (dt <= 0 || dt > 0.1f) {
        dt = 0.008f; // assume ~120Hz
    }

    // Read gyroscope
    float gx, gy, gz;
    M5.Imu.update();
    if (!M5.Imu.getGyro(&gx, &gy, &gz)) {
        return; // no new data
    }

    // M5.Imu returns gyro in degrees per second (dps).
    // Convert to rad/s for all downstream math (detent engine uses radians).
    float raw_z = gz * (PI / 180.0f);

    // Deadband: suppress noise-floor readings
    if (fabsf(raw_z - gyro_bias_) < GYRO_DEADBAND) {
        gyro_z_ = 0;
    } else {
        gyro_z_ = raw_z - gyro_bias_;
    }

    // Integrate to get virtual angle
    virtual_angle_ += gyro_z_ * dt * sensitivity_;

    // Stationarity detection for bias tracking
    velocity_ewma_ = fabsf(gyro_z_) * VELOCITY_EWMA_ALPHA
                     + velocity_ewma_ * (1.0f - VELOCITY_EWMA_ALPHA);

    if (velocity_ewma_ < STATIONARY_THRESHOLD) {
        if (!stationary_) {
            if (stationary_since_ms_ == 0) {
                stationary_since_ms_ = millis();
            } else if (millis() - stationary_since_ms_ > STATIONARY_TIME_MS) {
                stationary_ = true;
            }
        }
        // Slowly track bias when stationary
        if (stationary_) {
            gyro_bias_ = raw_z * BIAS_ALPHA + gyro_bias_ * (1.0f - BIAS_ALPHA);
        }
    } else {
        stationary_ = false;
        stationary_since_ms_ = 0;
    }
}

void ImuTracker::calibrate() {
    // Re-zero: reset angle and take fresh bias sample
    virtual_angle_ = 0;
    gyro_bias_ = 0;
    velocity_ewma_ = 0;
    stationary_ = false;
    stationary_since_ms_ = 0;

    // Sample bias over ~200ms
    float sum_gz = 0;
    int samples = 0;
    for (int i = 0; i < 30; i++) {
        float gx, gy, gz;
        M5.Imu.update();
        if (M5.Imu.getGyro(&gx, &gy, &gz)) {
            sum_gz += gz * (PI / 180.0f);
            samples++;
        }
        delay(6);
    }
    if (samples > 0) {
        gyro_bias_ = sum_gz / samples;
    }
    last_update_us_ = micros();
}
