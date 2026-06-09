#include <Arduino.h>
#include "imu_tracker.h"
#include <M5Unified.h>
#include <math.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

ImuTracker::ImuTracker()
    : virtual_angle_(0), gyro_z_(0), gyro_bias_(0), velocity_ewma_(0)
    , stationary_(false), last_update_us_(0), stationary_since_ms_(0) {}

void ImuTracker::begin() {
    if (!M5.Imu.isEnabled()) return;
    calibrate();
}

void ImuTracker::update() {
    if (!M5.Imu.isEnabled()) return;

    uint32_t now_us = micros();
    float dt = (now_us - last_update_us_) * 1e-6f;
    last_update_us_ = now_us;
    if (dt <= 0 || dt > 0.1f) dt = 0.008f;

    float gx, gy, gz;
    // Let getGyro call update() internally — avoids double-read race
    if (!M5.Imu.getGyro(&gx, &gy, &gz)) return;

    // M5.Imu returns gyro in degrees per second. Convert to rad/s.
    float raw_z = gz * (PI / 180.0f);

    if (fabsf(raw_z - gyro_bias_) < GYRO_DEADBAND) {
        gyro_z_ = 0;
    } else {
        gyro_z_ = raw_z - gyro_bias_;
    }

    virtual_angle_ += gyro_z_ * dt * sensitivity_;

    velocity_ewma_ = fabsf(gyro_z_) * VELOCITY_EWMA_ALPHA
                     + velocity_ewma_ * (1.0f - VELOCITY_EWMA_ALPHA);

    if (velocity_ewma_ < STATIONARY_THRESHOLD) {
        if (!stationary_) {
            if (stationary_since_ms_ == 0)
                stationary_since_ms_ = millis();
            else if (millis() - stationary_since_ms_ > STATIONARY_TIME_MS)
                stationary_ = true;
        }
        if (stationary_)
            gyro_bias_ = raw_z * BIAS_ALPHA + gyro_bias_ * (1.0f - BIAS_ALPHA);
    } else {
        stationary_ = false;
        stationary_since_ms_ = 0;
    }
}

void ImuTracker::calibrate() {
    virtual_angle_ = 0;
    gyro_bias_ = 0;
    velocity_ewma_ = 0;
    stationary_ = false;
    stationary_since_ms_ = 0;

    float sum = 0;
    int n = 0;
    for (int i = 0; i < 30; i++) {
        float gx, gy, gz;
        if (M5.Imu.getGyro(&gx, &gy, &gz)) { sum += gz * (PI / 180.0f); n++; }
        delay(6);
    }
    if (n > 0) gyro_bias_ = sum / n;
    last_update_us_ = micros();
}
