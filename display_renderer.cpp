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
    : bg_(&M5.Display)
    , ready_(false)
    , brightness_(200)
{
}

void DisplayRenderer::begin() {
    M5.Display.setRotation(0);
    M5.Display.setBrightness(brightness_);
    M5.Display.fillScreen(TFT_BLACK);

    bg_.setColorDepth(16);
    ready_ = bg_.createSprite(DISPLAY_SIZE, DISPLAY_SIZE);
    Serial.print("Sprite: ");
    Serial.println(ready_ ? "OK" : "FAILED");
    if (ready_) {
        bg_.fillScreen(TFT_BLACK);
        bg_.pushSprite(0, 0);
    }
}

void DisplayRenderer::setBrightness(uint8_t brightness) {
    brightness_ = brightness;
    M5.Display.setBrightness(brightness);
}

// --- drawTextRotated ---
// Render text upright into a tiny sprite, set its pivot to center,
// then pushRotateZoom onto dst. The pivot fix ensures the text center
// lands exactly at (cx, cy) regardless of rotation.
void DisplayRenderer::drawTextRotated(LovyanGFX& dst, const char* str,
                                       const lgfx::v1::IFont* font, int32_t cx, int32_t cy,
                                       float angle, uint32_t rgb888) {
    dst.setFont(font);
    int32_t tw = dst.textWidth(str) + 4;
    int32_t th = dst.fontHeight() + 4;
    if (tw < 4 || th < 4) return;

    LGFX_Sprite tmp(&dst);
    tmp.setColorDepth(16);
    if (tmp.createSprite(tw, th) == nullptr) return;
    tmp.fillScreen(TFT_BLACK);
    tmp.setFont(font);
    tmp.setTextColor(rgb565(rgb888), TFT_BLACK);
    tmp.setTextDatum(top_left);
    tmp.drawString(str, 2, 2);

    // Set pivot to center of the temp sprite so rotation is around the text center
    tmp.setPivot(tw / 2.0f, th / 2.0f);

    tmp.pushRotateZoom(&dst, (float)cx, (float)cy, angle, 1.0f, 1.0f);
    tmp.deleteSprite();
}

void DisplayRenderer::render(const KnobState& state, float device_angle) {
    auto& gfx = ready_ ? (LovyanGFX&)bg_ : (LovyanGFX&)M5.Display;
    gfx.fillScreen(TFT_BLACK);

    // DEBUG: hardcode 30° rotation to verify pushRotateZoom is working.
    // If text rotates: IMU angle is the problem. If not: pushRotateZoom is broken.
    // float rot = -fmodf(device_angle, 2.0f * PI);
    float rot = -30.0f * PI / 180.0f; // -30 degrees
    int32_t num_positions = state.config.max_position - state.config.min_position + 1;

    // Adjusted sub-position with log easing at endstops
    float adj_sub = state.sub_position_unit * state.config.position_width_radians;
    if (num_positions > 0) {
        if (state.current_position == state.config.min_position && state.sub_position_unit < 0) {
            adj_sub = -logf(1.0f - state.sub_position_unit
                * state.config.position_width_radians / 5.0f / PI * 180.0f) * 5.0f * PI / 180.0f;
        } else if (state.current_position == state.config.max_position && state.sub_position_unit > 0) {
            adj_sub = logf(1.0f + state.sub_position_unit
                * state.config.position_width_radians / 5.0f / PI * 180.0f) * 5.0f * PI / 180.0f;
        }
    }

    float left_bound = PI / 2.0f, right_bound = 0.0f;
    if (num_positions > 0) {
        float range = (state.config.max_position - state.config.min_position)
                      * state.config.position_width_radians;
        left_bound  = PI / 2.0f + range / 2.0f;
        right_bound = PI / 2.0f - range / 2.0f;
    }
    float raw_angle = left_bound
        - (state.current_position - state.config.min_position) * state.config.position_width_radians;
    float adjusted_angle = raw_angle - adj_sub;

    // Progress bar
    if (num_positions > 1) {
        int32_t h = (state.current_position - state.config.min_position)
                    * DISPLAY_SIZE / (state.config.max_position - state.config.min_position);
        gfx.fillRect(0, DISPLAY_SIZE - h, DISPLAY_SIZE, h, rgb565(COLOR_FILL));
    }

    // Arc (world-stabilized via angle offset)
    drawArc(gfx, state, left_bound + rot, right_bound + rot,
            raw_angle + rot, adjusted_angle + rot, num_positions);

    // ---- Text with rotation (sprite push with correct pivot) ----
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", (int)state.current_position);
    drawTextRotated(gfx, buf, &fonts::Font8,
                    CENTER_X, CENTER_Y - VALUE_Y_OFFSET, rot, COLOR_TEXT);

    int32_t line_y = CENTER_Y + DESCRIPTION_Y_OFFSET;
    const char* start = state.config.descriptor;
    const char* end = start + strlen(state.config.descriptor);
    gfx.setFont(&fonts::Font4);
    while (start < end) {
        const char* nl = strchr(start, '\n');
        if (nl == nullptr) nl = end;
        char line[51];
        size_t len = (size_t)(nl - start);
        if (len > sizeof(line) - 1) len = sizeof(line) - 1;
        memcpy(line, start, len);
        line[len] = '\0';

        drawTextRotated(gfx, line, &fonts::Font4,
                        CENTER_X, line_y, rot, COLOR_TEXT);
        line_y += gfx.fontHeight();
        start = nl + 1;
    }

    if (ready_) bg_.pushSprite(0, 0);
}

void DisplayRenderer::drawArc(LovyanGFX& gfx, const KnobState& state,
                               float left_bound, float right_bound,
                               float raw_angle, float adjusted_angle,
                               int32_t num_positions) {
    gfx.drawCircle(CENTER_X, CENTER_Y, ARC_RADIUS, rgb565(COLOR_ARC));

    if (num_positions > 0) {
        int32_t x1 = CENTER_X + (int32_t)(ARC_RADIUS * cosf(left_bound));
        int32_t y1 = CENTER_Y - (int32_t)(ARC_RADIUS * sinf(left_bound));
        int32_t x2 = CENTER_X + (int32_t)((ARC_RADIUS - 15) * cosf(left_bound));
        int32_t y2 = CENTER_Y - (int32_t)((ARC_RADIUS - 15) * sinf(left_bound));
        gfx.drawLine(x1, y1, x2, y2, rgb565(COLOR_ENDSTOP));
        x1 = CENTER_X + (int32_t)(ARC_RADIUS * cosf(right_bound));
        y1 = CENTER_Y - (int32_t)(ARC_RADIUS * sinf(right_bound));
        x2 = CENTER_X + (int32_t)((ARC_RADIUS - 15) * cosf(right_bound));
        y2 = CENTER_Y - (int32_t)((ARC_RADIUS - 15) * sinf(right_bound));
        gfx.drawLine(x1, y1, x2, y2, rgb565(COLOR_ENDSTOP));
    }

    int32_t dot_r = ARC_RADIUS - 10;
    bool past = num_positions > 0
        && ((state.current_position == state.config.min_position && state.sub_position_unit < 0)
            || (state.current_position == state.config.max_position && state.sub_position_unit > 0));

    if (past) {
        gfx.fillCircle(CENTER_X + (int32_t)(dot_r * cosf(raw_angle)),
                       CENTER_Y - (int32_t)(dot_r * sinf(raw_angle)),
                       5, rgb565(COLOR_DOT));
        float step = 2.0f * PI / 180.0f;
        if (raw_angle < adjusted_angle) {
            for (float a = raw_angle; a <= adjusted_angle; a += step)
                gfx.fillCircle(CENTER_X + (int32_t)(dot_r * cosf(a)),
                               CENTER_Y - (int32_t)(dot_r * sinf(a)), 2, rgb565(COLOR_DOT));
        } else {
            for (float a = raw_angle; a >= adjusted_angle; a -= step)
                gfx.fillCircle(CENTER_X + (int32_t)(dot_r * cosf(a)),
                               CENTER_Y - (int32_t)(dot_r * sinf(a)), 2, rgb565(COLOR_DOT));
        }
        gfx.fillCircle(CENTER_X + (int32_t)(dot_r * cosf(adjusted_angle)),
                       CENTER_Y - (int32_t)(dot_r * sinf(adjusted_angle)),
                       2, rgb565(COLOR_DOT));
    } else {
        gfx.fillCircle(CENTER_X + (int32_t)(dot_r * cosf(adjusted_angle)),
                       CENTER_Y - (int32_t)(dot_r * sinf(adjusted_angle)),
                       DOT_RADIUS, rgb565(COLOR_DOT));
    }
}
