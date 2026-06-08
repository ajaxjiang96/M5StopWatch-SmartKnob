#pragma once

#include "config.h"
#include <M5GFX.h>

/**
 * Display Renderer — renders the radial gauge UI on the 466x466 round AMOLED.
 * Uses an LGFX_Sprite framebuffer in PSRAM for tear-free double-buffered rendering.
 */
class DisplayRenderer {
public:
    DisplayRenderer();

    /** Allocate the sprite framebuffer. Call once in setup(). */
    void begin();

    /** Render a frame from the current knob state. Call at ~30fps. */
    void render(const KnobState& state);

    /** Set display brightness (0-255). */
    void setBrightness(uint8_t brightness);

    /** True if the framebuffer was allocated. */
    bool isReady() const { return ready_; }

private:
    M5Canvas sprite_;   // M5Canvas extends LGFX_Sprite with PSRAM enabled
    bool ready_;
    uint8_t brightness_;

    // Display geometry
    static constexpr int32_t DISPLAY_SIZE = 466;
    static constexpr int32_t CENTER_X = DISPLAY_SIZE / 2;
    static constexpr int32_t CENTER_Y = DISPLAY_SIZE / 2;
    static constexpr int32_t ARC_RADIUS = 210;
    static constexpr int32_t DOT_RADIUS = 8;

    // Layout offsets
    static constexpr int32_t VALUE_Y_OFFSET = 25;
    static constexpr int32_t DESCRIPTION_Y_OFFSET = 70;

    // Colors (RGB565)
    static constexpr uint32_t COLOR_BG      = 0x000000u;
    static constexpr uint32_t COLOR_FILL    = 0x5A1297u;
    static constexpr uint32_t COLOR_DOT     = 0x5064C8u;
    static constexpr uint32_t COLOR_ARC     = 0x424242u;
    static constexpr uint32_t COLOR_ENDSTOP = 0xFFFFFFu;
    static constexpr uint32_t COLOR_TEXT    = 0xFFFFFFu;

    void drawArc(LovyanGFX& gfx, const KnobState& state, float left_bound, float right_bound,
                 float raw_angle, float adjusted_angle, int32_t num_positions);

    static constexpr uint16_t rgb565(uint32_t rgb) {
        return ((rgb >> 19) & 0x1F) << 11 | ((rgb >> 10) & 0x3F) << 5 | ((rgb >> 3) & 0x1F);
    }
};
