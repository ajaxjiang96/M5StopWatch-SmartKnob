#pragma once

#include "config.h"
#include <M5GFX.h>

class DisplayRenderer {
public:
    DisplayRenderer();
    void begin();
    void render(const KnobState& state, float device_angle = 0);
    void setBrightness(uint8_t brightness);
    bool isReady() const { return ready_; }

private:
    M5Canvas bg_;          // main canvas: arc, progress bar
    M5Canvas text_layer_;  // text overlay: rotated before compositing
    bool ready_;
    uint8_t brightness_;

    static constexpr int32_t W = 466;
    static constexpr int32_t CX = 233;
    static constexpr int32_t CY = 233;
    static constexpr int32_t ARC_R = 210;
    static constexpr int32_t DOT_R = 8;
    static constexpr int32_t VO = 25;
    static constexpr int32_t DO = 70;

    static constexpr uint32_t COLOR_BG      = 0x000000u;
    static constexpr uint32_t COLOR_FILL    = 0x5A1297u;
    static constexpr uint32_t COLOR_DOT     = 0x5064C8u;
    static constexpr uint32_t COLOR_ARC     = 0x424242u;
    static constexpr uint32_t COLOR_ENDSTOP = 0xFFFFFFu;
    static constexpr uint32_t COLOR_TEXT    = 0xFFFFFFu;

    void drawArc(LovyanGFX& gfx, const KnobState& state,
                 float lb, float rb, float ra, float aa, int32_t np);

    static constexpr uint16_t rgb565(uint32_t rgb) {
        return ((rgb >> 19) & 0x1F) << 11 | ((rgb >> 10) & 0x3F) << 5 | ((rgb >> 3) & 0x1F);
    }
};
