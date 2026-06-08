#include <Arduino.h>
#include "detent_engine.h"
#include <math.h>
#include <string.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

// Helper: clamp a value between min and max
static inline float clamp(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

DetentEngine::DetentEngine()
    : current_position_(0)
    , current_detent_center_(0)
    , latest_sub_position_unit_(0)
    , virtual_angle_(0)
    , idle_check_velocity_ewma_(0)
    , last_idle_start_ms_(0)
{
    // Default config: on/off toggle
    config_.position = 0;
    config_.sub_position_unit = 0;
    config_.position_nonce = 0;
    config_.min_position = 0;
    config_.max_position = 1;
    config_.position_width_radians = 60.0f * PI / 180.0f;
    config_.detent_strength_unit = 0;
    config_.endstop_strength_unit = 1;
    config_.snap_point = 1.1f;
    config_.descriptor[0] = '\0';
    config_.detent_positions_count = 0;
    config_.snap_point_bias = 0;
}

void DetentEngine::setConfig(const KnobConfig& config) {
    // Validate
    if (config.detent_strength_unit < 0) return;
    if (config.endstop_strength_unit < 0) return;
    if (config.snap_point < 0.5f) return;
    if (config.snap_point_bias < 0) return;

    KnobConfig new_config = config;

    // Handle explicit position changes
    bool position_updated = false;
    if (new_config.position != config_.position
        || new_config.sub_position_unit != config_.sub_position_unit
        || new_config.position_nonce != config_.position_nonce) {
        current_position_ = new_config.position;
        position_updated = true;
    }

    // Clamp to bounds
    if (new_config.min_position <= new_config.max_position) {
        if (current_position_ < new_config.min_position) {
            current_position_ = new_config.min_position;
        } else if (current_position_ > new_config.max_position) {
            current_position_ = new_config.max_position;
        }
    }

    // Recalculate detent center if position or width changed
    if (position_updated || new_config.position_width_radians != config_.position_width_radians) {
        float new_sub = position_updated ? new_config.sub_position_unit : latest_sub_position_unit_;
        current_detent_center_ = virtual_angle_ + new_sub * new_config.position_width_radians;
    }

    config_ = new_config;
}

bool DetentEngine::update(float virtual_angle) {
    virtual_angle_ = virtual_angle;

    // ---- Idle correction ----
    // When stationary near a detent center, slowly drift center toward actual angle.
    // This prevents continuous micro-corrections (no motor, but reduces jitter).
    float vel = virtual_angle_ - current_detent_center_;
    static float prev_angle = virtual_angle_;
    float instant_vel = (virtual_angle_ - prev_angle); // rad/frame approx
    prev_angle = virtual_angle_;

    idle_check_velocity_ewma_ = fabsf(instant_vel) * IDLE_VELOCITY_EWMA_ALPHA
                                + idle_check_velocity_ewma_ * (1.0f - IDLE_VELOCITY_EWMA_ALPHA);

    if (idle_check_velocity_ewma_ > IDLE_VELOCITY_RAD_PER_SEC) {
        last_idle_start_ms_ = 0;
    } else {
        if (last_idle_start_ms_ == 0) {
            last_idle_start_ms_ = millis();
        }
    }

    if (last_idle_start_ms_ > 0
        && millis() - last_idle_start_ms_ > IDLE_CORRECTION_DELAY_MILLIS
        && fabsf(virtual_angle_ - current_detent_center_) < IDLE_CORRECTION_MAX_ANGLE_RAD) {
        current_detent_center_ = virtual_angle_ * IDLE_CORRECTION_RATE_ALPHA
                                 + current_detent_center_ * (1.0f - IDLE_CORRECTION_RATE_ALPHA);
    }

    // ---- Snap point check ----
    float angle_to_detent_center = virtual_angle_ - current_detent_center_;

    float snap_point_radians = config_.position_width_radians * config_.snap_point;
    float bias_radians = config_.position_width_radians * config_.snap_point_bias;
    float snap_point_radians_decrease = snap_point_radians
        + (current_position_ <= 0 ? bias_radians : -bias_radians);
    float snap_point_radians_increase = -snap_point_radians
        + (current_position_ >= 0 ? -bias_radians : bias_radians);

    int32_t num_positions = config_.max_position - config_.min_position + 1;
    bool position_changed = false;

    if (angle_to_detent_center > snap_point_radians_decrease
        && (num_positions <= 0 || current_position_ > config_.min_position)) {
        current_detent_center_ += config_.position_width_radians;
        angle_to_detent_center -= config_.position_width_radians;
        current_position_--;
        position_changed = true;
    } else if (angle_to_detent_center < snap_point_radians_increase
               && (num_positions <= 0 || current_position_ < config_.max_position)) {
        current_detent_center_ -= config_.position_width_radians;
        angle_to_detent_center += config_.position_width_radians;
        current_position_++;
        position_changed = true;
    }

    latest_sub_position_unit_ = -angle_to_detent_center / config_.position_width_radians;

    return position_changed;
}

KnobState DetentEngine::getState() const {
    KnobState state;
    state.current_position = current_position_;
    state.sub_position_unit = latest_sub_position_unit_;
    state.has_config = true;
    state.config = config_;
    return state;
}
