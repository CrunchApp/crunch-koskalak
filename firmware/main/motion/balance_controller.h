/**
 * Balance Controller — Standalone FreeRTOS Task
 *
 * Extracted from motion_engine.c to match the TurtleOS pattern:
 * the balance layer runs independently and continuously adjusts
 * servo offsets, invisible to gait/animation code above it.
 *
 * Architecture:
 *   IMU task (100Hz) → writes pitch/roll to shared data
 *   Balance task (50Hz) → reads IMU data → computes offset corrections
 *                       → writes to shared offset buffer
 *   Motion task (50Hz) → reads gait + offset buffer → writes to servos
 *
 * The balance controller is a proportional controller with optional
 * derivative term for damping. It compensates for:
 *   - Static surface tilt (uneven restaurant tables)
 *   - Dynamic disturbances (guest bumps the gadget)
 *   - Weight asymmetry (display/speaker mounted off-centre)
 *
 * When the IMU reports the device is being held (freefall), the
 * controller zeroes all offsets to prevent servo fighting.
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>

/* Number of servos the balance controller adjusts */
#ifndef KOSKALAK_DOF
#define KOSKALAK_DOF 8  /* 8-DOF default (miniKame); set to 4 for KT2-style */
#endif

#define BALANCE_NUM_SERVOS KOSKALAK_DOF

/**
 * Tuning parameters for the balance controller.
 */
typedef struct {
    float p_gain;            /* Proportional gain (deg servo correction per deg tilt) */
    float d_gain;            /* Derivative gain (damping, reduces oscillation) */
    float max_correction;    /* Max offset in degrees (clamp) */
    float deadband_deg;      /* Ignore tilt below this threshold */
} balance_config_t;

/* Default config: tuned for restaurant tables (2-5 degree typical tilt) */
#define BALANCE_CONFIG_DEFAULT { \
    .p_gain = 1.5f,             \
    .d_gain = 0.05f,            \
    .max_correction = 15.0f,    \
    .deadband_deg = 1.0f,       \
}

/**
 * Initialise the balance controller. Does NOT start the task.
 */
esp_err_t balance_controller_init(const balance_config_t *config);

/**
 * Start the balance controller FreeRTOS task (50Hz, Core 0).
 * Call after both imu_start() and motion_engine_init().
 */
esp_err_t balance_controller_start(void);

/**
 * Stop the balance controller task.
 */
esp_err_t balance_controller_stop(void);

/**
 * Enable or disable balance correction.
 * When disabled, all offsets are zeroed.
 */
esp_err_t balance_set_enabled(bool enable);

/**
 * Get the current servo offset corrections computed by the balance controller.
 * Array size: BALANCE_NUM_SERVOS. Values in degrees.
 * Called by motion_engine at each servo update cycle.
 */
void balance_get_offsets(float *offsets_out);

/**
 * Update tuning parameters at runtime (e.g., from device config push).
 */
esp_err_t balance_set_config(const balance_config_t *config);

/**
 * Returns true if balance correction is currently active.
 */
bool balance_is_enabled(void);
