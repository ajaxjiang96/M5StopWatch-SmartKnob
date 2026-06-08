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

// Counter-rotate a point around the display center.
// When the device rotates +θ, the element moves to appear fixed in world-space.
static void worldStabilize(int32_t& x, int32_t& y, float angle) {
    float dx = (float)(x - 233);
    float dy = (float)(y - 233);
    float s = sinf(angle);
    float c = cosf(angle);
    x = 233 + (int32_t)(dx * c + dy * s);
    y = 233 + (int32_t)(-dx * s + dy * c);
}

void DisplayRenderer::render(const KnobState& state, float device_angle) {
    auto& gfx = ready_ ? (LovyanGFX&)bg_ : (LovyanGFX&)M5.Display;

    gfx.fillScreen(TFT_BLACK);

    // World-stabilization: negated device rotation angle
    float rot = -fmodf(device_angle, 2.0f * PI);

    int32_t num_positions = state.config.max_position - state.config.min_position + 1;

    // ---- Adjusted sub-position (log easing at endstops) ----
    float adjusted_sub_position = state.sub_position_unit * state.config.position_width_radians;
    if (num_positions > 0) {
        if (state.current_position == state.config.min_position && state.sub_position_unit < 0) {
            adjusted_sub_position = -logf(1.0f - state.sub_position_unit
                * state.config.position_width_radians / 5.0f / PI * 180.0f) * 5.0f * PI / 180.0f;
        } else if (state.current_position == state.config.max_position && state.sub_position_unit > 0) {
            adjusted_sub_position = logf(1.0f + state.sub_position_unit
                * state.config.position_width_radians / 5.0f / PI * 180.0f) * 5.0f * PI / 180.0f;
        }
    }

    // ---- Arc bounds ----
    float left_bound = PI / 2.0f;
    float right_bound = 0.0f;
    if (num_positions > 0) {
        float range_radians = (state.config.max_position - state.config.min_position)
                              * state.config.position_width_radians;
        left_bound = PI / 2.0f + range_radians / 2.0f;
        right_bound = PI / 2.0f - range_radians / 2.0f;
    }
    float raw_angle = left_bound
        - (state.current_position - state.config.min_position) * state.config.position_width_radians;
    float adjusted_angle = raw_angle - adjusted_sub_position;

    // ---- Progress fill bar ----
    if (num_positions > 1) {
        int32_t h = (state.current_position - state.config.min_position)
                    * DISPLAY_SIZE / (state.config.max_position - state.config.min_position);
        gfx.fillRect(0, DISPLAY_SIZE - h, DISPLAY_SIZE, h, rgb565(COLOR_FILL));
    }

    // ---- Position number (center-top, world-stabilized position) ----
    {
        int32_t tx = CENTER_X;
        int32_t ty = CENTER_Y - VALUE_Y_OFFSET;
        worldStabilize(tx, ty, rot);

        gfx.setTextColor(rgb565(COLOR_TEXT), TFT_BLACK);
        gfx.setFont(&fonts::Font8);
        gfx.setTextDatum(middle_center);

        char num_buf[16];
        snprintf(num_buf, sizeof(num_buf), "%d", (int)state.current_position);
        gfx.drawString(num_buf, tx, ty);
    }

    // ---- Descriptor text (below center, world-stabilized position) ----
    {
        gfx.setFont(&fonts::Font4);
        gfx.setTextDatum(top_center);

        int32_t base_ty = CENTER_Y + DESCRIPTION_Y_OFFSET;
        const char* start = state.config.descriptor;
        const char* end = start + strlen(state.config.descriptor);
        while (start < end) {
            const char* newline = strchr(start, '\n');
            if (newline == nullptr) newline = end;

            char line_buf[51];
            size_t len = (size_t)(newline - start);
            if (len > sizeof(line_buf) - 1) len = sizeof(line_buf) - 1;
            memcpy(line_buf, start, len);
            line_buf[len] = '\0';

            int32_t lx = CENTER_X;
            int32_t ly = base_ty;
            worldStabilize(lx, ly, rot);
            gfx.drawString(line_buf, lx, ly);

            start = newline + 1;
            base_ty += gfx.fontHeight();
        }
    }

    // ---- Radial arc (angles offset by device rotation) ----
    drawArc(gfx, state, left_bound + rot, right_bound + rot,
            raw_angle + rot, adjusted_angle + rot, num_positions);

    // Push to display
    if (ready_) {
        bg_.pushSprite(0, 0);
    }
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
    bool past_endstop = num_positions > 0
        && ((state.current_position == state.config.min_position && state.sub_position_unit < 0)
            || (state.current_position == state.config.max_position && state.sub_position_unit > 0));

    if (past_endstop) {
        gfx.fillCircle(CENTER_X + (int32_t)(dot_r * cosf(raw_angle)),
                       CENTER_Y - (int32_t)(dot_r * sinf(raw_angle)),
                       5, rgb565(COLOR_DOT));

        float step = 2.0f * PI / 180.0f;
        if (raw_angle < adjusted_angle) {
            for (float a = raw_angle; a <= adjusted_angle; a += step) {
                gfx.fillCircle(CENTER_X + (int32_t)(dot_r * cosf(a)),
                               CENTER_Y - (int32_t)(dot_r * sinf(a)),
                               2, rgb565(COLOR_DOT));
            }
        } else {
            for (float a = raw_angle; a >= adjusted_angle; a -= step) {
                gfx.fillCircle(CENTER_X + (int32_t)(dot_r * cosf(a)),
                               CENTER_Y - (int32_t)(dot_r * sinf(a)),
                               2, rgb565(COLOR_DOT));
            }
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
