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
    : sprite_(&M5.Display)
    , ready_(false)
    , brightness_(200)
{
}

void DisplayRenderer::begin() {
    M5.Display.setRotation(0);
    M5.Display.setBrightness(brightness_);
    M5.Display.fillScreen(TFT_BLACK);

    // Set pivot to display center for pushRotated
    M5.Display.setPivot(CENTER_X, CENTER_Y);

    // Allocate sprite framebuffer in PSRAM (466x466 RGB565 = ~434KB)
    sprite_.setColorDepth(16);
    bool ok = sprite_.createSprite(DISPLAY_SIZE, DISPLAY_SIZE);
    ready_ = ok;

    Serial.print("Sprite allocation: ");
    Serial.println(ok ? "OK (PSRAM)" : "FAILED — falling back to direct draw");
    Serial.print("Display: ");
    Serial.print(M5.Display.width());
    Serial.print("x");
    Serial.println(M5.Display.height());

    if (ready_) {
        sprite_.fillScreen(TFT_BLACK);
        sprite_.pushSprite(0, 0);
    }
}

void DisplayRenderer::setBrightness(uint8_t brightness) {
    brightness_ = brightness;
    M5.Display.setBrightness(brightness);
}

void DisplayRenderer::render(const KnobState& state, float device_angle) {
    // Choose render target: sprite (tear-free) or direct (fallback)
    auto& gfx = ready_ ? (LovyanGFX&)sprite_ : (LovyanGFX&)M5.Display;

    gfx.fillScreen(TFT_BLACK);

    int32_t num_positions = state.config.max_position - state.config.min_position + 1;

    // ---- Adjusted sub-position with log easing at endstops ----
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
        int32_t height = (state.current_position - state.config.min_position)
                         * DISPLAY_SIZE
                         / (state.config.max_position - state.config.min_position);
        gfx.fillRect(0, DISPLAY_SIZE - height, DISPLAY_SIZE, height, rgb565(COLOR_FILL));
    }

    // ---- Position number (large, centered) ----
    gfx.setTextColor(rgb565(COLOR_TEXT), TFT_BLACK);
    gfx.setFont(&fonts::Font8);
    gfx.setTextDatum(middle_center);

    char num_buf[16];
    snprintf(num_buf, sizeof(num_buf), "%d", (int)state.current_position);
    gfx.drawString(num_buf, CENTER_X, CENTER_Y - VALUE_Y_OFFSET);

    // ---- Descriptor text (multi-line) ----
    gfx.setFont(&fonts::Font4);
    gfx.setTextDatum(top_center);

    int32_t line_y = CENTER_Y + DESCRIPTION_Y_OFFSET;
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

        gfx.drawString(line_buf, CENTER_X, line_y);
        start = newline + 1;
        line_y += gfx.fontHeight();
    }

    // ---- Radial arc ----
    drawArc(gfx, state, left_bound, right_bound,
            raw_angle, adjusted_angle, num_positions);

    // Push sprite to display with world-stabilized counter-rotation.
    // When device rotates +10°, the frame rotates -10° so all elements
    // (text, arc, dots, progress bar) appear fixed in world-space.
    if (ready_) {
        float rot = -fmodf(device_angle, 2.0f * PI);
        sprite_.pushRotated(rot);
    }
}

void DisplayRenderer::drawArc(LovyanGFX& gfx, const KnobState& state,
                               float left_bound, float right_bound,
                               float raw_angle, float adjusted_angle,
                               int32_t num_positions) {
    // Arc track circle
    gfx.drawCircle(CENTER_X, CENTER_Y, ARC_RADIUS, rgb565(COLOR_ARC));

    // Endstop markers
    if (num_positions > 0) {
        int32_t x1 = CENTER_X + ARC_RADIUS * cosf(left_bound);
        int32_t y1 = CENTER_Y - ARC_RADIUS * sinf(left_bound);
        int32_t x2 = CENTER_X + (ARC_RADIUS - 15) * cosf(left_bound);
        int32_t y2 = CENTER_Y - (ARC_RADIUS - 15) * sinf(left_bound);
        gfx.drawLine(x1, y1, x2, y2, rgb565(COLOR_ENDSTOP));

        x1 = CENTER_X + ARC_RADIUS * cosf(right_bound);
        y1 = CENTER_Y - ARC_RADIUS * sinf(right_bound);
        x2 = CENTER_X + (ARC_RADIUS - 15) * cosf(right_bound);
        y2 = CENTER_Y - (ARC_RADIUS - 15) * sinf(right_bound);
        gfx.drawLine(x1, y1, x2, y2, rgb565(COLOR_ENDSTOP));
    }

    // Position indicator dot
    int32_t dot_r = ARC_RADIUS - 10;
    bool past_endstop = num_positions > 0
        && ((state.current_position == state.config.min_position && state.sub_position_unit < 0)
            || (state.current_position == state.config.max_position && state.sub_position_unit > 0));

    if (past_endstop) {
        gfx.fillCircle(CENTER_X + dot_r * cosf(raw_angle),
                       CENTER_Y - dot_r * sinf(raw_angle),
                       5, rgb565(COLOR_DOT));

        float step = 2.0f * PI / 180.0f;
        if (raw_angle < adjusted_angle) {
            for (float a = raw_angle; a <= adjusted_angle; a += step) {
                gfx.fillCircle(CENTER_X + dot_r * cosf(a),
                               CENTER_Y - dot_r * sinf(a),
                               2, rgb565(COLOR_DOT));
            }
        } else {
            for (float a = raw_angle; a >= adjusted_angle; a -= step) {
                gfx.fillCircle(CENTER_X + dot_r * cosf(a),
                               CENTER_Y - dot_r * sinf(a),
                               2, rgb565(COLOR_DOT));
            }
        }
        gfx.fillCircle(CENTER_X + dot_r * cosf(adjusted_angle),
                       CENTER_Y - dot_r * sinf(adjusted_angle),
                       2, rgb565(COLOR_DOT));
    } else {
        gfx.fillCircle(CENTER_X + dot_r * cosf(adjusted_angle),
                       CENTER_Y - dot_r * sinf(adjusted_angle),
                       DOT_RADIUS, rgb565(COLOR_DOT));
    }
}
