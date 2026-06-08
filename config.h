#pragma once

#include <stdint.h>

/**
 * KnobConfig — adapted from PB_SmartKnobConfig in SmartKnob's smartknob.pb.h.
 * Defines the haptic "feel" and position behavior for the virtual knob.
 */
struct KnobConfig {
    int32_t position;              // Target integer position (idempotent apply)
    float sub_position_unit;       // Fractional position within current detent
    uint8_t position_nonce;        // Nonce to force position re-apply

    int32_t min_position;          // Minimum allowed position (max < min = unbounded)
    int32_t max_position;          // Maximum allowed position

    float position_width_radians;  // Angular width of each detent in radians
    float detent_strength_unit;    // Detent strength [0, 1+]
    float endstop_strength_unit;   // Endstop stiffness [0, 1+]
    float snap_point;              // Fraction of width before snapping (0.5-1.5)

    char descriptor[51];           // Human-readable mode label (supports \n)

    uint8_t detent_positions_count; // Number of magnetic detent positions (0 = all)
    int32_t detent_positions[5];    // Which positions have detents (max 5)

    float snap_point_bias;         // Asymmetric snap bias (0 = symmetric)
};

/**
 * KnobState — adapted from PB_SmartKnobState.
 * Published whenever the virtual position changes.
 */
struct KnobState {
    int32_t current_position;      // Current integer detent index
    float sub_position_unit;       // Fractional position within current detent
    bool has_config;               // Whether config is valid
    KnobConfig config;             // Active config at time of snapshot
};

/** Number of preset configurations */
#define CONFIG_COUNT 11

/** Array of preset configs defined in config.cpp */
extern KnobConfig configs[CONFIG_COUNT];
