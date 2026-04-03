/**
 * LED Driver — Implementation
 *
 * Uses ESP-IDF's led_strip component (RMT-based WS2812 driver).
 * A refresh task runs at 30Hz to animate patterns.
 */

#include "led_driver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

static const char *TAG = "led";

static int s_led_count = 0;
static led_pattern_t s_pattern = LED_PATTERN_OFF;
static led_color_t s_base_color = {0};
static float s_speed = 1.0f;
static uint8_t s_brightness = 128;
static TaskHandle_t s_led_task = NULL;

/* LED pixel buffer */
static led_color_t s_pixels[LED_MAX_COUNT] = {0};

static void apply_brightness(led_color_t *c, float factor)
{
    float bf = (s_brightness / 255.0f) * factor;
    c->r = (uint8_t)(s_base_color.r * bf);
    c->g = (uint8_t)(s_base_color.g * bf);
    c->b = (uint8_t)(s_base_color.b * bf);
}

static void led_refresh_task(void *arg)
{
    uint32_t frame = 0;

    for (;;) {
        float t = (frame * s_speed) / 30.0f; /* Time in seconds at current speed */

        switch (s_pattern) {
        case LED_PATTERN_OFF:
            for (int i = 0; i < s_led_count; i++) {
                s_pixels[i] = (led_color_t){0, 0, 0};
            }
            break;

        case LED_PATTERN_BREATHE: {
            /* Sine wave brightness, all LEDs together */
            float factor = (sinf(t * 2.0f * M_PI * 0.3f) + 1.0f) / 2.0f;
            for (int i = 0; i < s_led_count; i++) {
                apply_brightness(&s_pixels[i], factor);
            }
            break;
        }

        case LED_PATTERN_PULSE: {
            /* Quick pulse: bright for 200ms, dim for 800ms */
            float cycle = fmodf(t, 1.0f);
            float factor = (cycle < 0.2f) ? 1.0f : 0.15f;
            for (int i = 0; i < s_led_count; i++) {
                apply_brightness(&s_pixels[i], factor);
            }
            break;
        }

        case LED_PATTERN_CHASE: {
            /* Single bright LED rotates around the ring */
            int active = ((int)(t * 8.0f)) % s_led_count;
            for (int i = 0; i < s_led_count; i++) {
                float factor = (i == active) ? 1.0f : 0.05f;
                apply_brightness(&s_pixels[i], factor);
            }
            break;
        }

        case LED_PATTERN_SOLID:
            for (int i = 0; i < s_led_count; i++) {
                apply_brightness(&s_pixels[i], 1.0f);
            }
            break;

        case LED_PATTERN_FLASH: {
            float cycle = fmodf(t, 0.4f);
            float factor = (cycle < 0.2f) ? 1.0f : 0.0f;
            for (int i = 0; i < s_led_count; i++) {
                apply_brightness(&s_pixels[i], factor);
            }
            break;
        }

        case LED_PATTERN_RAINBOW: {
            for (int i = 0; i < s_led_count; i++) {
                float hue = fmodf((float)i / s_led_count + t * 0.2f, 1.0f);
                /* Simple HSV-to-RGB (S=1, V=brightness) */
                float h6 = hue * 6.0f;
                float f = h6 - (int)h6;
                uint8_t v = s_brightness;
                uint8_t q = (uint8_t)(v * (1.0f - f));
                uint8_t u = (uint8_t)(v * f);
                switch ((int)h6 % 6) {
                case 0: s_pixels[i] = (led_color_t){v, u, 0}; break;
                case 1: s_pixels[i] = (led_color_t){q, v, 0}; break;
                case 2: s_pixels[i] = (led_color_t){0, v, u}; break;
                case 3: s_pixels[i] = (led_color_t){0, q, v}; break;
                case 4: s_pixels[i] = (led_color_t){u, 0, v}; break;
                case 5: s_pixels[i] = (led_color_t){v, 0, q}; break;
                }
            }
            break;
        }
        }

        /* TODO: Write s_pixels[] to RMT-driven WS2812 strip via led_strip API */

        frame++;
        vTaskDelay(pdMS_TO_TICKS(33)); /* ~30Hz refresh */
    }
}

esp_err_t led_driver_init(int gpio_num, int led_count)
{
    if (led_count > LED_MAX_COUNT || led_count <= 0) {
        ESP_LOGE(TAG, "Invalid LED count: %d (max %d)", led_count, LED_MAX_COUNT);
        return ESP_ERR_INVALID_ARG;
    }

    s_led_count = led_count;

    /* TODO: Initialise RMT channel for WS2812 on gpio_num
     * Use led_strip_new_rmt_device() from ESP-IDF led_strip component
     */

    xTaskCreatePinnedToCore(
        led_refresh_task,
        "led",
        2048,
        NULL,
        1,       /* Lowest priority — purely cosmetic */
        &s_led_task,
        0
    );

    ESP_LOGI(TAG, "LED driver initialised (%d LEDs on GPIO %d)", led_count, gpio_num);
    return ESP_OK;
}

esp_err_t led_set_pattern(led_pattern_t pattern, led_color_t color)
{
    s_pattern = pattern;
    s_base_color = color;
    return ESP_OK;
}

esp_err_t led_set_speed(float multiplier)
{
    s_speed = multiplier;
    return ESP_OK;
}

esp_err_t led_set_brightness(uint8_t brightness)
{
    s_brightness = brightness;
    return ESP_OK;
}

esp_err_t led_set_pixel(int index, led_color_t color)
{
    if (index < 0 || index >= s_led_count) return ESP_ERR_INVALID_ARG;
    s_pixels[index] = color;
    return ESP_OK;
}

esp_err_t led_off(void)
{
    s_pattern = LED_PATTERN_OFF;
    return ESP_OK;
}
