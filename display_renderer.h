#pragma once

#include "config.h"
#include <M5GFX.h>

/**
 * Display Renderer — radial gauge UI on 466x466 round AMOLED.
 * Double-buffered: main sprite in PSRAM for the background (arc + bar),
 * text rendered to a small rotated overlay for world-stabilized orientation.
 */
class DisplayRenderer {
public:
    DisplayRenderer();
    void begin();
    void render(const KnobState& state, float device_angle = 0);
    void setBrightness(uint8_t brightness);
    bool isReady() const { return ready_; }

private:
    M5Canvas bg_;        // main framebuffer: arc, progress bar, background
    M5Canvas text_overlay_; // overlay for rotated text
    bool ready_;
    uint8_t brightness_;

    static constexpr int32_t DISPLAY_SIZE = 466;
    static constexpr int32_t CENTER_X = DISPLAY_SIZE / 2;
    static constexpr int32_t CENTER_Y = DISPLAY_SIZE / 2;
    static constexpr int32_t ARC_RADIUS = 210;
    static constexpr int32_t DOT_RADIUS = 8;
    static constexpr int32_t VALUE_Y_OFFSET = 25;
    static constexpr int32_t DESCRIPTION_Y_OFFSET = 70;

    static constexpr uint32_t COLOR_BG      = 0x000000u;
    static constexpr uint32_t COLOR_FILL    = 0x5A1297u;
    static constexpr uint32_t COLOR_DOT     = 0x5064C8u;
    static constexpr uint32_t COLOR_ARC     = 0x424242u;
    static constexpr uint32_t COLOR_ENDSTOP = 0xFFFFFFu;
    static constexpr uint32_t COLOR_TEXT    = 0xFFFFFFu;

    void drawArc(LovyanGFX& gfx, const KnobState& state, float left_bound, float right_bound,
                 float raw_angle, float adjusted_angle, int32_t num_positions);

    /// Draw rotated text: renders upright to a temp sprite, pushes rotated onto dst.
    void drawTextRotated(LovyanGFX& dst, const char* str, const lgfx::IFont* font,
                         int32_t x, int32_t y, float angle, uint32_t color);

    static constexpr uint16_t rgb565(uint32_t rgb) {
        return ((rgb >> 19) & 0x1F) << 11 | ((rgb >> 10) & 0x3F) << 5 | ((rgb >> 3) & 0x1F);
    }
};
