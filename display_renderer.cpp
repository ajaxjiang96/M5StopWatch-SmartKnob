#include <Arduino.h>
#include "display_renderer.h"
#include <M5Unified.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

DisplayRenderer::DisplayRenderer()
    : bg_(&M5.Display), text_layer_(&M5.Display), ready_(false), brightness_(200) {}

void DisplayRenderer::begin() {
    M5.Display.setRotation(0);
    M5.Display.setBrightness(brightness_);
    M5.Display.fillScreen(TFT_BLACK);

    bg_.setColorDepth(16);
    text_layer_.setColorDepth(16);
    bool ok1 = bg_.createSprite(W, W);
    bool ok2 = text_layer_.createSprite(W, W);
    ready_ = ok1 && ok2;

    Serial.print("Sprites: bg=");
    Serial.print(ok1 ? "OK" : "FAIL");
    Serial.print(" text=");
    Serial.println(ok2 ? "OK" : "FAIL");

    if (ready_) {
        bg_.fillScreen(TFT_BLACK);
        bg_.pushSprite(0, 0);
    }
}

// Forward: manual pixel rotation (pushRotateZoom is broken on M5GFX 0.2.22)
static void softwareRotateBlit(LGFX_Sprite& dst, LGFX_Sprite& src,
                                float angle, int32_t cx, int32_t cy);

void DisplayRenderer::setBrightness(uint8_t brightness) {
    brightness_ = brightness;
    M5.Display.setBrightness(brightness);
}

void DisplayRenderer::render(const KnobState& state, float device_angle) {
    if (!ready_) return;

    float rot = -fmodf(device_angle, 2.0f * PI);

    int32_t np = state.config.max_position - state.config.min_position + 1;

    // ---- sub-position with log easing at endstops ----
    float adj = state.sub_position_unit * state.config.position_width_radians;
    if (np > 0) {
        if (state.current_position == state.config.min_position && state.sub_position_unit < 0)
            adj = -logf(1.0f - adj / 5.0f / PI * 180.0f) * 5.0f * PI / 180.0f;
        else if (state.current_position == state.config.max_position && state.sub_position_unit > 0)
            adj = logf(1.0f + adj / 5.0f / PI * 180.0f) * 5.0f * PI / 180.0f;
    }

    float lb = PI / 2.0f, rb = 0.0f;
    if (np > 0) {
        float range = (state.config.max_position - state.config.min_position)
                      * state.config.position_width_radians;
        lb = PI / 2.0f + range / 2.0f;
        rb = PI / 2.0f - range / 2.0f;
    }
    float ra = lb - (state.current_position - state.config.min_position) * state.config.position_width_radians;
    float aa = ra - adj;

    // ---- 1. Draw arc + progress bar on bg_ ----
    bg_.fillScreen(TFT_BLACK);
    if (np > 1) {
        int32_t h = (state.current_position - state.config.min_position) * W
                    / (state.config.max_position - state.config.min_position);
        bg_.fillRect(0, W - h, W, h, rgb565(COLOR_FILL));
    }
    drawArc(bg_, state, lb + rot, rb + rot, ra + rot, aa + rot, np);

    // ---- 2. Draw text upright on text_layer_ ----
    text_layer_.fillScreen(TFT_BLACK);
    text_layer_.setTextColor(rgb565(COLOR_TEXT), TFT_BLACK);

    char buf[32];
    snprintf(buf, sizeof(buf), "%d", (int)state.current_position);
    text_layer_.setFont(&fonts::Font8);
    text_layer_.setTextDatum(middle_center);
    text_layer_.drawString(buf, CX, CY - VO);

    text_layer_.setFont(&fonts::Font4);
    text_layer_.setTextDatum(top_center);

    int32_t ly = CY + DO;
    const char* s = state.config.descriptor;
    const char* e = s + strlen(s);
    while (s < e) {
        const char* nl = strchr(s, '\n');
        if (!nl) nl = e;
        char line[51];
        size_t len = (size_t)(nl - s);
        if (len > sizeof(line) - 1) len = sizeof(line) - 1;
        memcpy(line, s, len); line[len] = 0;
        text_layer_.drawString(line, CX, ly);
        ly += text_layer_.fontHeight();
        s = nl + 1;
    }

    // ---- 3. Composite: software-rotate text onto background ----
    // pushRotateZoom is broken on M5GFX 0.2.22; manual pixel rotation instead.
    softwareRotateBlit(bg_, text_layer_, rot, CX, CY);

    // ---- 4. Push final frame to display ----
    bg_.pushSprite(0, 0);
}

// --- Manual pixel rotation: reads text_layer_ buffer, writes rotated to bg_ ---
static void softwareRotateBlit(LGFX_Sprite& dst, LGFX_Sprite& src,
                                float angle, int32_t cx, int32_t cy) {
    uint16_t* sb = (uint16_t*)src.getBuffer();
    uint16_t* db = (uint16_t*)dst.getBuffer();
    if (!sb || !db) return;

    int32_t sw = src.width(), sh = src.height();
    int32_t dw = dst.width(), dh = dst.height();
    float s = sinf(angle), c = cosf(angle);

    // Compute rotated bounding box of source on destination
    float corners[4][2] = {
        {-cx, -cy}, {(float)sw-1-cx, -cy},
        {(float)sw-1-cx, (float)sh-1-cy}, {-cx, (float)sh-1-cy}
    };
    int32_t x0 = dw, x1 = 0, y0 = dh, y1 = 0;
    for (int i = 0; i < 4; i++) {
        int32_t rx = (int32_t)(corners[i][0] * c - corners[i][1] * s) + cx;
        int32_t ry = (int32_t)(corners[i][0] * s + corners[i][1] * c) + cy;
        if (rx < 0) rx = 0; if (rx >= dw) rx = dw-1;
        if (ry < 0) ry = 0; if (ry >= dh) ry = dh-1;
        if (rx < x0) x0 = rx; if (rx > x1) x1 = rx;
        if (ry < y0) y0 = ry; if (ry > y1) y1 = ry;
    }

    // Inverse rotation: destination → source
    for (int32_t dy = y0; dy <= y1; dy++) {
        for (int32_t dx = x0; dx <= x1; dx++) {
            float fx = (dx - cx) * c + (dy - cy) * s;
            float fy = -(dx - cx) * s + (dy - cy) * c;
            int32_t sx = (int32_t)(fx + cx);
            int32_t sy = (int32_t)(fy + cy);
            if ((uint32_t)sx >= (uint32_t)sw || (uint32_t)sy >= (uint32_t)sh) continue;
            uint16_t p = sb[sy * sw + sx];
            if (p) db[dy * dw + dx] = p;  // skip black (transparent)
        }
    }
}

void DisplayRenderer::drawArc(LovyanGFX& gfx, const KnobState& state,
                               float lb, float rb, float ra, float aa, int32_t np) {
    gfx.drawCircle(CX, CY, ARC_R, rgb565(COLOR_ARC));
    if (np > 0) {
        int32_t x1 = CX + (int32_t)(ARC_R * cosf(lb));
        int32_t y1 = CY - (int32_t)(ARC_R * sinf(lb));
        int32_t x2 = CX + (int32_t)((ARC_R - 15) * cosf(lb));
        int32_t y2 = CY - (int32_t)((ARC_R - 15) * sinf(lb));
        gfx.drawLine(x1, y1, x2, y2, rgb565(COLOR_ENDSTOP));
        x1 = CX + (int32_t)(ARC_R * cosf(rb));
        y1 = CY - (int32_t)(ARC_R * sinf(rb));
        x2 = CX + (int32_t)((ARC_R - 15) * cosf(rb));
        y2 = CY - (int32_t)((ARC_R - 15) * sinf(rb));
        gfx.drawLine(x1, y1, x2, y2, rgb565(COLOR_ENDSTOP));
    }
    int32_t dr = ARC_R - 10;
    bool past = np > 0 && ((state.current_position == state.config.min_position && state.sub_position_unit < 0)
                         || (state.current_position == state.config.max_position && state.sub_position_unit > 0));
    if (past) {
        gfx.fillCircle(CX + (int32_t)(dr * cosf(ra)), CY - (int32_t)(dr * sinf(ra)), 5, rgb565(COLOR_DOT));
        float step = 2.0f * PI / 180.0f;
        if (ra < aa) for (float a = ra; a <= aa; a += step)
            gfx.fillCircle(CX + (int32_t)(dr * cosf(a)), CY - (int32_t)(dr * sinf(a)), 2, rgb565(COLOR_DOT));
        else for (float a = ra; a >= aa; a -= step)
            gfx.fillCircle(CX + (int32_t)(dr * cosf(a)), CY - (int32_t)(dr * sinf(a)), 2, rgb565(COLOR_DOT));
        gfx.fillCircle(CX + (int32_t)(dr * cosf(aa)), CY - (int32_t)(dr * sinf(aa)), 2, rgb565(COLOR_DOT));
    } else {
        gfx.fillCircle(CX + (int32_t)(dr * cosf(aa)), CY - (int32_t)(dr * sinf(aa)), DOT_R, rgb565(COLOR_DOT));
    }
}
