/**
 * Display Manager
 *
 * Drives the 2.4" IPS LCD (ST7789, 240x320) via SPI using LVGL.
 * Provides screen management for device states:
 *   - Boot splash
 *   - Provisioning (QR code / instructions)
 *   - Idle (branding + time + location name)
 *   - Listening (waveform animation)
 *   - Thinking (animated dots)
 *   - Speaking (transcript + confirmation)
 *   - Order summary (line items, modifiers, total)
 *   - Error (message + recovery hint)
 */

#pragma once

#include "esp_err.h"

typedef enum {
    SCREEN_BOOT,
    SCREEN_PROVISIONING,
    SCREEN_IDLE,
    SCREEN_LISTENING,
    SCREEN_THINKING,
    SCREEN_SPEAKING,
    SCREEN_ORDER_SUMMARY,
    SCREEN_ERROR,
} screen_id_t;

/**
 * Initialise SPI bus, ST7789 driver, and LVGL. Starts the LVGL tick/refresh task.
 */
esp_err_t display_manager_init(void);

/**
 * Switch to a named screen. Transition is immediate.
 */
esp_err_t display_show_screen(screen_id_t screen);

/**
 * Update the transcript text on the SPEAKING screen.
 */
esp_err_t display_set_transcript(const char *text);

/**
 * Update order summary data on the ORDER_SUMMARY screen.
 * Items are passed as a JSON string for flexibility.
 */
esp_err_t display_set_order_summary(const char *order_json);

/**
 * Set an error message on the ERROR screen.
 */
esp_err_t display_set_error(const char *message);

/**
 * Set the location name and current time shown on the IDLE screen.
 */
esp_err_t display_set_idle_info(const char *location_name, const char *time_str);
