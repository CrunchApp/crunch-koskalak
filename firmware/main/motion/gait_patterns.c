/**
 * Gait Pattern Definitions
 *
 * Each gait is defined by oscillator parameters for 8 servos:
 *   - amplitude: maximum deflection from offset (degrees)
 *   - offset:    centre position (degrees, 90 = neutral)
 *   - phase:     phase offset relative to reference (radians)
 *   - period:    oscillation period (milliseconds)
 *
 * Ported from miniKame's gait library with additions for
 * expressive animations (greeting, thinking, speaking, etc.)
 */

#include "motion_engine.h"
#include <math.h>

#define NUM_SERVOS 8

typedef struct {
    float amplitude[NUM_SERVOS];
    float offset[NUM_SERVOS];
    float phase[NUM_SERVOS];
    float period;
} gait_params_t;

/* Servo order: FL_HIP, FL_KNEE, FR_HIP, FR_KNEE, BL_HIP, BL_KNEE, BR_HIP, BR_KNEE */

static const gait_params_t GAIT_PARAMS[] = {
    [GAIT_IDLE_SWAY] = {
        .amplitude = { 5,  3,  5,  3,  5,  3,  5,  3},
        .offset    = {90, 90, 90, 90, 90, 90, 90, 90},
        .phase     = { 0,  0, 3.14, 3.14, 3.14, 3.14, 0, 0},
        .period    = 3000,
    },
    [GAIT_GREETING_HOP] = {
        .amplitude = {20, 30, 20, 30, 20, 30, 20, 30},
        .offset    = {90, 70, 90, 70, 90, 70, 90, 70},
        .phase     = { 0,  0,  0,  0,  0,  0,  0,  0},
        .period    = 500,
    },
    [GAIT_LISTENING_TILT] = {
        .amplitude = {10,  5,  5,  5,  5,  5, 10,  5},
        .offset    = {95, 90, 85, 90, 85, 90, 95, 90},
        .phase     = { 0,  0,  0,  0, 3.14, 3.14, 3.14, 3.14},
        .period    = 2000,
    },
    [GAIT_THINKING_WIGGLE] = {
        .amplitude = {15,  0, 15,  0, 15,  0, 15,  0},
        .offset    = {90, 90, 90, 90, 90, 90, 90, 90},
        .phase     = { 0,  0, 3.14, 0, 3.14, 0, 0, 0},
        .period    = 800,
    },
    [GAIT_SPEAKING_BOUNCE] = {
        .amplitude = { 3, 10,  3, 10,  3, 10,  3, 10},
        .offset    = {90, 85, 90, 85, 90, 85, 90, 85},
        .phase     = { 0,  0,  0,  0,  0,  0,  0,  0},
        .period    = 400,
    },
    [GAIT_SLEEP_BREATHE] = {
        .amplitude = { 2,  5,  2,  5,  2,  5,  2,  5},
        .offset    = {90, 60, 90, 60, 90, 60, 90, 60},
        .phase     = { 0,  0,  0,  0,  0,  0,  0,  0},
        .period    = 4000,
    },
    [GAIT_WALK_FORWARD] = {
        .amplitude = {30, 30, 30, 30, 30, 30, 30, 30},
        .offset    = {90, 90, 90, 90, 90, 90, 90, 90},
        .phase     = { 0,  1.57, 3.14, 4.71, 3.14, 4.71, 0, 1.57},
        .period    = 1000,
    },
    [GAIT_CELEBRATE] = {
        .amplitude = {25, 35, 25, 35, 25, 35, 25, 35},
        .offset    = {90, 75, 90, 75, 90, 75, 90, 75},
        .phase     = { 0, 0.5, 1.57, 2.07, 3.14, 3.64, 4.71, 5.21},
        .period    = 600,
    },
};

/**
 * Get the oscillator parameters for a given gait.
 * Returns NULL for GAIT_NONE or invalid gait IDs.
 */
const gait_params_t *gait_get_params(gait_id_t gait)
{
    if (gait >= GAIT_NONE || gait < 0) {
        return NULL;
    }
    return &GAIT_PARAMS[gait];
}
