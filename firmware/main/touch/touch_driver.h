/**
 * Capacitive Touch Driver
 *
 * Uses the ESP32-S3's built-in capacitive touch peripheral (no external IC).
 * The S3 has 14 touch-capable GPIOs; we use 3-4 for interaction zones.
 *
 * Touch zones and their mapped interactions:
 *   HEAD_PAT   — Sustained touch on top → happy animation, acknowledge gesture
 *   TAP_WAKE   — Quick tap → wake from sleep, attention
 *   HOLD_SLEEP — Long press (>2s) → enter sleep mode
 *   TALK       — Touch-and-hold → push-to-talk fallback for noisy environments
 *                (bypasses wake word when voice recognition is unreliable)
 *
 * The ESP32-S3 touch peripheral handles debouncing and threshold detection
 * in hardware, with interrupt-driven notification to firmware.
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TOUCH_ZONE_HEAD,    /* Top of chassis — head pat, general interaction */
    TOUCH_ZONE_BACK,    /* Rear of chassis — hold to sleep */
    TOUCH_ZONE_LEFT,    /* Left side — future use / gesture */
    TOUCH_ZONE_RIGHT,   /* Right side — future use / gesture */
    TOUCH_ZONE_COUNT,
} touch_zone_t;

typedef enum {
    TOUCH_EVENT_NONE,
    TOUCH_EVENT_TAP,          /* Quick touch (<500ms) */
    TOUCH_EVENT_DOUBLE_TAP,   /* Two taps within 400ms */
    TOUCH_EVENT_LONG_PRESS,   /* Held >2 seconds */
    TOUCH_EVENT_HOLD_START,   /* Touch begins (for push-to-talk) */
    TOUCH_EVENT_HOLD_END,     /* Touch released (end push-to-talk) */
} touch_event_type_t;

typedef struct {
    touch_zone_t zone;
    touch_event_type_t type;
    uint32_t duration_ms;     /* How long the touch lasted */
} touch_event_t;

typedef void (*touch_event_callback_t)(touch_event_t event);

/**
 * Initialise the ESP32-S3 touch peripheral.
 * Configures touch GPIOs, sets thresholds, and starts the touch FSM task.
 *
 * GPIO mapping (configurable via menuconfig or compile-time defines):
 *   TOUCH_ZONE_HEAD  → GPIO 4  (Touch 4)
 *   TOUCH_ZONE_BACK  → GPIO 5  (Touch 5)
 *   TOUCH_ZONE_LEFT  → GPIO 6  (Touch 6)
 *   TOUCH_ZONE_RIGHT → GPIO 7  (Touch 7)
 */
esp_err_t touch_driver_init(void);

/**
 * Register a callback for touch events. Replaces any previous callback.
 */
void touch_set_event_callback(touch_event_callback_t callback);

/**
 * Returns true if the specified zone is currently being touched.
 * Useful for push-to-talk: check touch_is_active(TOUCH_ZONE_HEAD)
 * to know if the user is still holding.
 */
bool touch_is_active(touch_zone_t zone);

/**
 * Enable or disable push-to-talk mode.
 * When enabled, HOLD_START/HOLD_END events are generated for the HEAD zone
 * instead of TAP/LONG_PRESS. Used as a fallback in noisy environments
 * where wake word detection is unreliable.
 */
esp_err_t touch_set_push_to_talk(bool enable);

/**
 * Recalibrate touch thresholds. Run when the device is not being touched.
 * Useful after environmental changes (temperature, humidity on restaurant floor).
 */
esp_err_t touch_recalibrate(void);
