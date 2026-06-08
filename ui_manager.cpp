#include <Arduino.h>
#include "ui_manager.h"
#include <M5Unified.h>
#include <math.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

static float unwrapAngle(float current, float previous) {
    float diff = current - previous;
    while (diff > PI)  diff -= 2.0f * PI;
    while (diff < -PI) diff += 2.0f * PI;
    return previous + diff;
}

UiManager::UiManager()
    : config_index_(0)
    , keya_pressed_(false)
    , keyb_pressed_(false)
    , last_keya_(false)
    , last_keyb_(false)
    , touching_(false)
    , touch_x_(0), touch_y_(0)
    , last_touch_x_(0), last_touch_y_(0)
    , touch_angle_(0)
    , last_touch_angle_(0)
    , touch_dragging_(false)
    , touch_start_ms_(0)
    , tap_handled_(false)
    , tap_left_(false)
    , tap_right_(false)
    , last_button_check_ms_(0)
{
}

void UiManager::begin() {
    // Buttons are on GPIO1 (KEYB) and GPIO2 (KEYA) with internal pull-ups.
    // M5Unified initializes these, but we configure the pins explicitly.
    pinMode(1, INPUT_PULLUP);  // KEYB
    pinMode(2, INPUT_PULLUP);  // KEYA

    // Touch is initialized by M5Unified (CST820B via I2C).
    last_button_check_ms_ = millis();
}

uint8_t UiManager::update() {
    uint32_t now = millis();

    // Reset per-frame flags
    keya_pressed_ = false;
    keyb_pressed_ = false;
    tap_left_ = false;
    tap_right_ = false;
    accumulated_delta_ = 0;  // consumed each frame by getTouchDelta

    // ---- Button handling (with debounce) ----
    if (now - last_button_check_ms_ >= BUTTON_DEBOUNCE_MS) {
        last_button_check_ms_ = now;

        bool keya = !digitalRead(2); // active low
        bool keyb = !digitalRead(1);

        if (keya && !last_keya_) {
            keya_pressed_ = true;
            config_index_ = (config_index_ + 1) % CONFIG_COUNT;
        }
        if (keyb && !last_keyb_) {
            keyb_pressed_ = true;
            if (config_index_ == 0) {
                config_index_ = CONFIG_COUNT - 1;
            } else {
                config_index_--;
            }
        }

        last_keya_ = keya;
        last_keyb_ = keyb;
    }

    // ---- Touch handling ----
    M5.update(); // refresh touch state
    auto touch = M5.Touch.getDetail();

    if (touch.state & 0x01) { // touched
        if (!touching_) {
            // Touch began
            touching_ = true;
            touch_x_ = touch.x;
            touch_y_ = touch.y;
            last_touch_x_ = touch_x_;
            last_touch_y_ = touch_y_;
            touch_start_ms_ = now;
            tap_handled_ = false;
            touch_dragging_ = false;

            // Compute initial touch angle relative to center
            float dx = touch_x_ - CENTER_X;
            float dy = CENTER_Y - touch_y_; // screen Y inverted
            touch_angle_ = atan2f(dy, dx);
            last_touch_angle_ = touch_angle_;
        } else {
            // Touch continued
            last_touch_x_ = touch_x_;
            last_touch_y_ = touch_y_;
            touch_x_ = touch.x;
            touch_y_ = touch.y;

            float dx = touch_x_ - CENTER_X;
            float dy = CENTER_Y - touch_y_;
            float new_angle = atan2f(dy, dx);
            // Unwrap across the -PI/PI boundary
            float unwrapped = unwrapAngle(new_angle, last_touch_angle_);

            // Accumulate the delta from this frame
            accumulated_delta_ += unwrapped - touch_angle_;
            touch_angle_ = unwrapped;

            // Determine if dragging (moved enough)
            int32_t total_move = abs(touch_x_ - (int32_t)(touch.state & 0x02 ? touch.x : last_touch_x_))
                               + abs(touch_y_ - (int32_t)(touch.state & 0x02 ? touch.y : last_touch_y_));
            // Actually compute correctly:
            int32_t move_dist = abs(touch.x - last_touch_x_) + abs(touch.y - last_touch_y_);
            if (move_dist > TAP_MOVE_THRESHOLD && !touch_dragging_) {
                touch_dragging_ = true;
            }

            last_touch_angle_ = touch_angle_;
            last_touch_x_ = touch.x;
            last_touch_y_ = touch.y;
        }
    } else {
        // Touch released
        if (touching_ && !tap_handled_) {
            uint32_t duration = now - touch_start_ms_;
            if (duration < TAP_MAX_MS && !touch_dragging_) {
                // It was a tap — determine zone
                if (touch_x_ < DISPLAY_SIZE / 3) {
                    tap_left_ = true;
                    // Previous config
                    if (config_index_ == 0) {
                        config_index_ = CONFIG_COUNT - 1;
                    } else {
                        config_index_--;
                    }
                } else if (touch_x_ > DISPLAY_SIZE * 2 / 3) {
                    tap_right_ = true;
                    // Next config
                    config_index_ = (config_index_ + 1) % CONFIG_COUNT;
                }
                // Center tap is a no-op (could be used for info overlay)
            }
            tap_handled_ = true;
        }
        touching_ = false;
        touch_dragging_ = false;
    }

    return config_index_;
}

void UiManager::setConfigIndex(uint8_t idx) {
    if (idx < CONFIG_COUNT) {
        config_index_ = idx;
    }
}

bool UiManager::getTouchDelta(float& delta_radians) {
    if (!touching_ || !touch_dragging_) return false;

    delta_radians = accumulated_delta_;
    accumulated_delta_ = 0;  // consumed
    return fabsf(delta_radians) > 0.001f;
}
