#include <Arduino.h>
#include "imu_tracker.h"
#include <M5Unified.h>
#include <math.h>

ImuTracker::ImuTracker()
    : angle_deg_(0), gyro_z_(0), gyro_bias_(0), velocity_ewma_(0)
    , stationary_(false), last_us_(0), stationary_since_ms_(0) {}

void ImuTracker::begin() {
    if (!M5.Imu.isEnabled()) return;
    calibrate();
    Serial.println("IMU calibrated");
}

void ImuTracker::update() {
    if (!M5.Imu.isEnabled()) return;

    uint32_t now = micros();
    float dt = (now - last_us_) * 1e-6f;
    last_us_ = now;
    if (dt <= 0 || dt > 0.1f) dt = 0.008f;

    float gx, gy, gz;
    bool fresh = M5.Imu.update();
    if (!M5.Imu.getGyro(&gx, &gy, &gz)) return;

    // Diagnostic: track how often the IMU produces fresh data
    static uint32_t calls, fresh_count, last_report;
    calls++;
    if (fresh) fresh_count++;
    if (millis() - last_report > 2000) {
        last_report = millis();
        Serial.print("IMU: fresh="); Serial.print(fresh_count);
        Serial.print("/"); Serial.print(calls);
        Serial.print(" (dt="); Serial.print(dt*1000, 1); Serial.println("ms)");
        calls = fresh_count = 0;
    }

    // gyro Z in deg/s from M5Unified. Deadband rejects noise (~2dps pk-pk)
    // while passing intentional rotation. Integrate in degrees directly.
    float debiased = gz - gyro_bias_;
    if (fabsf(debiased) < 1.0f) {
        gyro_z_ = 0;
    } else {
        gyro_z_ = debiased;
    }
    angle_deg_ += gyro_z_ * dt * sensitivity_;

    // Stationarity detection
    float abs_vel = fabsf(gyro_z_);
    velocity_ewma_ = abs_vel * VELOCITY_EWMA_ALPHA + velocity_ewma_ * (1.0f - VELOCITY_EWMA_ALPHA);

    if (velocity_ewma_ < STATIONARY_THRESHOLD) {
        if (!stationary_) {
            if (stationary_since_ms_ == 0)
                stationary_since_ms_ = millis();
            else if (millis() - stationary_since_ms_ > STATIONARY_TIME_MS)
                stationary_ = true;
        }
        if (stationary_ && abs_vel < BIAS_GATE)
            gyro_bias_ = gz * BIAS_ALPHA + gyro_bias_ * (1.0f - BIAS_ALPHA);
    } else {
        stationary_ = false;
        stationary_since_ms_ = 0;
    }
}

void ImuTracker::calibrate() {
    angle_deg_ = 0;
    gyro_bias_ = 0;
    velocity_ewma_ = 0;
    stationary_ = false;
    stationary_since_ms_ = 0;

    float sum = 0;
    int n = 0;
    for (int i = 0; i < 30; i++) {
        float gx, gy, gz;
        M5.Imu.update();
        if (M5.Imu.getGyro(&gx, &gy, &gz)) { sum += gz; n++; }
        delay(6);
    }
    if (n > 0) gyro_bias_ = sum / n;
    last_us_ = micros();
}
