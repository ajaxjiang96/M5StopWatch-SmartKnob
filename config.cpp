#include "config.h"
#include <math.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

// Preset configurations — exact values ported from SmartKnob interface_task.cpp.
// Each preset defines a different haptic "feel" for the virtual knob.
KnobConfig configs[CONFIG_COUNT] = {
    // [0] Unbounded, no detents — free spinning
    {
        .position = 0,
        .sub_position_unit = 0,
        .position_nonce = 0,
        .min_position = 0,
        .max_position = -1,               // max < min = unbounded
        .position_width_radians = 10.0f * PI / 180.0f,
        .detent_strength_unit = 0,
        .endstop_strength_unit = 1,
        .snap_point = 1.1f,
        .descriptor = "Unbounded\nNo detents",
        .detent_positions_count = 0,
        .detent_positions = {},
        .snap_point_bias = 0,
    },

    // [1] Bounded 0-10, no detents
    {
        .position = 0,
        .sub_position_unit = 0,
        .position_nonce = 1,
        .min_position = 0,
        .max_position = 10,
        .position_width_radians = 10.0f * PI / 180.0f,
        .detent_strength_unit = 0,
        .endstop_strength_unit = 1,
        .snap_point = 1.1f,
        .descriptor = "Bounded 0-10\nNo detents",
        .detent_positions_count = 0,
        .detent_positions = {},
        .snap_point_bias = 0,
    },

    // [2] Multi-revolution, no detents
    {
        .position = 0,
        .sub_position_unit = 0,
        .position_nonce = 2,
        .min_position = 0,
        .max_position = 72,
        .position_width_radians = 10.0f * PI / 180.0f,
        .detent_strength_unit = 0,
        .endstop_strength_unit = 1,
        .snap_point = 1.1f,
        .descriptor = "Multi-rev\nNo detents",
        .detent_positions_count = 0,
        .detent_positions = {},
        .snap_point_bias = 0,
    },

    // [3] On/off, strong detent
    {
        .position = 0,
        .sub_position_unit = 0,
        .position_nonce = 3,
        .min_position = 0,
        .max_position = 1,
        .position_width_radians = 60.0f * PI / 180.0f,
        .detent_strength_unit = 1,
        .endstop_strength_unit = 1,
        .snap_point = 0.55f,             // past midpoint for clean toggle
        .descriptor = "On/off\nStrong detent",
        .detent_positions_count = 0,
        .detent_positions = {},
        .snap_point_bias = 0,
    },

    // [4] Return-to-center
    {
        .position = 0,
        .sub_position_unit = 0,
        .position_nonce = 4,
        .min_position = 0,
        .max_position = 0,
        .position_width_radians = 60.0f * PI / 180.0f,
        .detent_strength_unit = 0.01f,
        .endstop_strength_unit = 0.6f,
        .snap_point = 1.1f,
        .descriptor = "Return-to-center",
        .detent_positions_count = 0,
        .detent_positions = {},
        .snap_point_bias = 0,
    },

    // [5] Fine values, no detents (256 positions, 1° each)
    {
        .position = 127,
        .sub_position_unit = 0,
        .position_nonce = 5,
        .min_position = 0,
        .max_position = 255,
        .position_width_radians = 1.0f * PI / 180.0f,
        .detent_strength_unit = 0,
        .endstop_strength_unit = 1,
        .snap_point = 1.1f,
        .descriptor = "Fine values\nNo detents",
        .detent_positions_count = 0,
        .detent_positions = {},
        .snap_point_bias = 0,
    },

    // [6] Fine values, with detents (256 positions, 1° each)
    {
        .position = 127,
        .sub_position_unit = 0,
        .position_nonce = 5,
        .min_position = 0,
        .max_position = 255,
        .position_width_radians = 1.0f * PI / 180.0f,
        .detent_strength_unit = 1,
        .endstop_strength_unit = 1,
        .snap_point = 1.1f,
        .descriptor = "Fine values\nWith detents",
        .detent_positions_count = 0,
        .detent_positions = {},
        .snap_point_bias = 0,
    },

    // [7] Coarse values, strong detents (32 positions)
    {
        .position = 0,
        .sub_position_unit = 0,
        .position_nonce = 6,
        .min_position = 0,
        .max_position = 31,
        .position_width_radians = 8.225806452f * PI / 180.0f,
        .detent_strength_unit = 2,
        .endstop_strength_unit = 1,
        .snap_point = 1.1f,
        .descriptor = "Coarse values\nStrong detents",
        .detent_positions_count = 0,
        .detent_positions = {},
        .snap_point_bias = 0,
    },

    // [8] Coarse values, weak detents (32 positions)
    {
        .position = 0,
        .sub_position_unit = 0,
        .position_nonce = 6,
        .min_position = 0,
        .max_position = 31,
        .position_width_radians = 8.225806452f * PI / 180.0f,
        .detent_strength_unit = 0.2f,
        .endstop_strength_unit = 1,
        .snap_point = 1.1f,
        .descriptor = "Coarse values\nWeak detents",
        .detent_positions_count = 0,
        .detent_positions = {},
        .snap_point_bias = 0,
    },

    // [9] Magnetic detents (specific positions have detents)
    {
        .position = 0,
        .sub_position_unit = 0,
        .position_nonce = 7,
        .min_position = 0,
        .max_position = 31,
        .position_width_radians = 7.0f * PI / 180.0f,
        .detent_strength_unit = 2.5f,
        .endstop_strength_unit = 1,
        .snap_point = 0.7f,
        .descriptor = "Magnetic detents",
        .detent_positions_count = 4,
        .detent_positions = {2, 10, 21, 22},
        .snap_point_bias = 0,
    },

    // [10] Return-to-center with detents
    {
        .position = 0,
        .sub_position_unit = 0,
        .position_nonce = 8,
        .min_position = -6,
        .max_position = 6,
        .position_width_radians = 60.0f * PI / 180.0f,
        .detent_strength_unit = 1,
        .endstop_strength_unit = 1,
        .snap_point = 0.55f,
        .descriptor = "Return-to-center\nwith detents",
        .detent_positions_count = 0,
        .detent_positions = {},
        .snap_point_bias = 0.4f,
    },
};
