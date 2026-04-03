/**
 * LED Driver — WS2812 NeoPixel via ESP32-S3 RMT Peripheral
 *
 * Drives an addressable RGB LED ring (or strip) for status indication,
 * mood lighting, and conversation state feedback.
 *
 * LED states mapped to device states:
 *   IDLE       → slow breathing white/blue
 *   LISTENING  → pulsing green (mic active)
 *   THINKING   → spinning amber chase
 *   SPEAKING   → gentle cyan pulse (synced to TTS cadence)
 *   ERROR      → red flash
 *   LOW_BATTERY → slow red pulse
 *   UPDATING   → blue/white alternating
 *   SLEEPING   → all off (or single dim white heartbeat)
 *
 * Uses the RMT (Remote Control Transceiver) peripheral for precise
 * WS2812 timing. Single GPIO, no external driver IC needed.
 */

#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#define LED_MAX_COUNT 12  /* Support up to 12-LED ring */

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} led_color_t;

typedef enum {
    LED_PATTERN_OFF,
    LED_PATTERN_BREATHE,      /* Slow sine-wave brightness on solid colour */
    LED_PATTERN_PULSE,        /* Quick pulse (attention getter) */
    LED_PATTERN_CHASE,        /* Spinning dot around the ring */
    LED_PATTERN_SOLID,        /* Static solid colour */
    LED_PATTERN_FLASH,        /* Rapid on/off (error, alert) */
    LED_PATTERN_RAINBOW,      /* Rotating rainbow (celebration) */
} led_pattern_t;

/**
 * Initialise the RMT peripheral and LED strip driver.
 *
 * @param gpio_num  GPIO connected to WS2812 data line
 * @param led_count Number of LEDs in the strip/ring (max LED_MAX_COUNT)
 */
esp_err_t led_driver_init(int gpio_num, int led_count);

/**
 * Set a pattern with a base colour. The pattern modulates brightness/position.
 */
esp_err_t led_set_pattern(led_pattern_t pattern, led_color_t color);

/**
 * Set the pattern speed. 1.0 = normal, 0.5 = half speed, 2.0 = double.
 */
esp_err_t led_set_speed(float multiplier);

/**
 * Set the overall brightness (0-255). Applied as a multiplier to all patterns.
 */
esp_err_t led_set_brightness(uint8_t brightness);

/**
 * Set a single LED to a specific colour (overrides pattern for that LED).
 * Useful for indicating specific status on individual LEDs.
 */
esp_err_t led_set_pixel(int index, led_color_t color);

/**
 * Turn all LEDs off immediately.
 */
esp_err_t led_off(void);

/* Convenience colours */
#define LED_WHITE   ((led_color_t){255, 255, 255})
#define LED_RED     ((led_color_t){255, 0, 0})
#define LED_GREEN   ((led_color_t){0, 255, 0})
#define LED_BLUE    ((led_color_t){0, 0, 255})
#define LED_AMBER   ((led_color_t){255, 180, 0})
#define LED_CYAN    ((led_color_t){0, 255, 200})
