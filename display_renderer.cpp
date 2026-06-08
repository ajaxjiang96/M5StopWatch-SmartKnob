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
    , text_overlay_(&M5.Display)
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

    Serial.print("BG sprite: ");
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
// Render text upright into a small temp sprite, then pushRotated onto dst.
// Sprite-to-sprite rotation is purely CPU/memory; it doesn't touch the
// display driver, so it works where pushRotated-to-display failed.
void DisplayRenderer::drawTextRotated(LovyanGFX& dst, const char* str,
                                       const lgfx::IFont* font,
                                       int32_t x, int32_t y, float angle,
                                       uint32_t color) {
    dst.setFont(font);
    int32_t tw = dst.textWidth(str) + 4;
    int32_t th = dst.fontHeight() + 4;
    if (tw < 1 || th < 1) return;

    // Temp sprite (DRAM is fine — text is small)
    LGFX_Sprite tmp(&dst);
    tmp.setColorDepth(16);
    if (tmp.createSprite(tw, th) == nullptr) return;
    tmp.fillScreen(TFT_BLACK);
    tmp.setFont(font);
    tmp.setTextColor(color, TFT_BLACK);
    tmp.setTextDatum(top_left);
    tmp.drawString(str, 2, 2);

    tmp.pushRotateZoom(&dst, x, y, angle, 1.0f, 1.0f);
    tmp.deleteSprite();
}

void DisplayRenderer::render(const KnobState& state, float device_angle) {
    // ---- 1. Draw background elements (arc + bar) onto bg_ sprite ----
    bg_.fillScreen(TFT_BLACK);

    int32_t num_positions = state.config.max_position - state.config.min_position + 1;

    // Adjusted sub-position with log easing at endstops
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

    // Progress bar
    if (num_positions > 1) {
        int32_t h = (state.current_position - state.config.min_position)
                    * DISPLAY_SIZE / (state.config.max_position - state.config.min_position);
        bg_.fillRect(0, DISPLAY_SIZE - h, DISPLAY_SIZE, h, rgb565(COLOR_FILL));
    }

    // Arc (angles offset by device rotation for world stabilization)
    float rot = -fmodf(device_angle, 2.0f * PI);
    drawArc(bg_, state, left_bound + rot, right_bound + rot,
            raw_angle + rot, adjusted_angle + rot, num_positions);

    // ---- 2. Draw text elements with full rotation (position + orientation) ----
    uint16_t text_color = rgb565(COLOR_TEXT);
    char buf[32];

    // Position number at center-top
    snprintf(buf, sizeof(buf), "%d", (int)state.current_position);
    drawTextRotated(bg_, buf, &fonts::Font8,
                    CENTER_X, CENTER_Y - VALUE_Y_OFFSET, rot, text_color);

    // Descriptor lines below center
    int32_t line_y = CENTER_Y + DESCRIPTION_Y_OFFSET;
    const char* start = state.config.descriptor;
    const char* end = start + strlen(state.config.descriptor);
    while (start < end) {
        const char* newline = strchr(start, '\n');
        if (newline == nullptr) newline = end;

        char line[51];
        size_t len = (size_t)(newline - start);
        if (len > sizeof(line) - 1) len = sizeof(line) - 1;
        memcpy(line, start, len);
        line[len] = '\0';

        drawTextRotated(bg_, line, &fonts::Font4,
                        CENTER_X, line_y, rot, text_color);

        bg_.setFont(&fonts::Font4);
        line_y += bg_.fontHeight();
        start = newline + 1;
    }

    // ---- 3. Push to display ----
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
