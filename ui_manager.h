#pragma once

#include "config.h"
#include <stdint.h>

/**
 * UI Manager — handles button input, touch gestures, and config cycling.
 *
 * KEYA (GPIO2) = next config
 * KEYB (GPIO1) = previous config
 * Touch drag on edge = virtual knob rotation
 * Touch tap zones = config cycling
 */
class UiManager {
public:
    UiManager();

    /** Initialize buttons and touch. Call once in setup(). */
    void begin();

    /**
     * Process inputs. Call every loop iteration.
     * Returns the current config index (0..CONFIG_COUNT-1).
     */
    uint8_t update();

    /** Get the current config index. */
    uint8_t getConfigIndex() const { return config_index_; }

    /** Set config index directly. */
    void setConfigIndex(uint8_t idx);

    /**
     * Check if a touch-based angle delta is available.
     * Returns true and sets delta_radians if the user is dragging on the edge.
     */
    bool getTouchDelta(float& delta_radians);

    /** Check if KEYA was just pressed this frame. */
    bool keyAPressed() const { return keya_pressed_; }

    /** Check if KEYB was just pressed this frame. */
    bool keyBPressed() const { return keyb_pressed_; }

    /** Check if a tap occurred on the left/right third. */
    bool tapLeft() const { return tap_left_; }
    bool tapRight() const { return tap_right_; }

private:
    uint8_t config_index_;

    // Button state
    bool keya_pressed_;
    bool keyb_pressed_;
    bool last_keya_;
    bool last_keyb_;

    // Touch state
    bool touching_;
    int32_t touch_x_, touch_y_;
    int32_t last_touch_x_, last_touch_y_;
    float touch_angle_;          // current touch angle (radians)
    float last_touch_angle_;     // previous frame touch angle
    float accumulated_delta_;    // delta accumulated this frame (consumed by getTouchDelta)
    bool touch_dragging_;        // true during drag
    uint32_t touch_start_ms_;    // when touch began
    bool tap_handled_;           // prevent multiple taps from one touch

    // Tap zone results (consumed on read)
    bool tap_left_;
    bool tap_right_;

    // Debounce
    uint32_t last_button_check_ms_;

    static constexpr uint32_t BUTTON_DEBOUNCE_MS = 30;
    static constexpr uint32_t TAP_MAX_MS = 300;        // max duration for a "tap"
    static constexpr int32_t TAP_MOVE_THRESHOLD = 20;  // max pixels moved for tap
    static constexpr int32_t DISPLAY_SIZE = 466;
    static constexpr int32_t CENTER_X = DISPLAY_SIZE / 2;
    static constexpr int32_t CENTER_Y = DISPLAY_SIZE / 2;
};
