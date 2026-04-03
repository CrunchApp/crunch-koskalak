/**
 * Battery Monitor
 *
 * ADC-based battery voltage monitoring for lithium cells.
 * Supports both 1S (3.7V nominal) and 2S (7.4V nominal) configurations
 * via a compile-time flag (KOSKALAK_BATTERY_CELLS).
 *
 * Hardware: Voltage divider on an ADC-capable GPIO.
 *   For 1S: 100K/100K divider (halves voltage, max ~2.1V at ADC)
 *   For 2S: 100K/33K divider (divides by ~4, max ~2.1V at ADC)
 *   ESP32-S3 ADC range: 0-3.1V with 12-bit resolution (default atten)
 *
 * Reports:
 *   - Raw voltage (mV)
 *   - Estimated percentage (linear approximation between cutoff and full)
 *   - Low battery warnings via callback
 *   - Critical battery triggers device sleep
 *
 * Telemetry: battery percentage is included in MQTT heartbeat reports.
 */

#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

/* Compile-time config: 1 or 2 cells in series */
#ifndef KOSKALAK_BATTERY_CELLS
#define KOSKALAK_BATTERY_CELLS 2  /* Default: 2S LiPo (matches 8-DOF servo voltage) */
#endif

/* Voltage thresholds per cell (mV) */
#define CELL_VOLTAGE_FULL_MV     4200
#define CELL_VOLTAGE_NOMINAL_MV  3700
#define CELL_VOLTAGE_LOW_MV      3400  /* ~20% — trigger warning */
#define CELL_VOLTAGE_CRITICAL_MV 3200  /* ~5% — trigger sleep */
#define CELL_VOLTAGE_CUTOFF_MV   3000  /* 0% — hard cutoff */

typedef enum {
    BATTERY_OK,
    BATTERY_LOW,       /* Below 20% — warn user, reduce LED brightness */
    BATTERY_CRITICAL,  /* Below 5% — force sleep mode */
} battery_state_t;

typedef struct {
    uint32_t voltage_mv;       /* Total pack voltage in millivolts */
    uint32_t cell_voltage_mv;  /* Per-cell voltage (total / KOSKALAK_BATTERY_CELLS) */
    uint8_t percentage;        /* 0-100 estimated charge */
    battery_state_t state;
    bool is_charging;          /* True if USB power detected */
} battery_info_t;

typedef void (*battery_state_callback_t)(battery_state_t state, uint8_t percentage);

/**
 * Initialise ADC channel for battery voltage reading.
 *
 * @param adc_gpio  GPIO connected to voltage divider midpoint
 */
esp_err_t battery_monitor_init(int adc_gpio);

/**
 * Start periodic battery monitoring (samples every 10 seconds).
 */
esp_err_t battery_monitor_start(void);

/**
 * Stop monitoring (e.g., before deep sleep).
 */
esp_err_t battery_monitor_stop(void);

/**
 * Get the latest battery reading. Thread-safe.
 */
battery_info_t battery_get_info(void);

/**
 * Register a callback for battery state transitions (OK→LOW, LOW→CRITICAL).
 */
void battery_set_state_callback(battery_state_callback_t callback);

/**
 * Force an immediate reading (outside the normal 10s interval).
 * Useful for telemetry reports.
 */
esp_err_t battery_read_now(battery_info_t *out);
