/**
 * Capacitive Touch Driver — Implementation
 *
 * Uses ESP32-S3 touch_pad_* APIs from ESP-IDF.
 * Touch sampling runs at ~10Hz (low priority) via a FreeRTOS task.
 * Event detection: tap (<500ms), double-tap (two taps within 400ms),
 * long-press (>2s held), hold start/end (for push-to-talk mode).
 */

#include "touch_driver.h"
#include "esp_log.h"
#include "driver/touch_pad.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "touch";

/* GPIO-to-touch-channel mapping (ESP32-S3) */
#define HEAD_TOUCH_CHANNEL   TOUCH_PAD_NUM4
#define BACK_TOUCH_CHANNEL   TOUCH_PAD_NUM5
#define LEFT_TOUCH_CHANNEL   TOUCH_PAD_NUM6
#define RIGHT_TOUCH_CHANNEL  TOUCH_PAD_NUM7

#define TAP_MAX_MS          500
#define DOUBLE_TAP_WINDOW_MS 400
#define LONG_PRESS_MS       2000
#define SAMPLE_INTERVAL_MS  100   /* 10Hz touch polling */
#define TOUCH_THRESHOLD_PCT 80    /* 80% of untouched reading = touched */

static touch_event_callback_t s_callback = NULL;
static TaskHandle_t s_touch_task = NULL;
static bool s_push_to_talk = false;

typedef struct {
    touch_pad_t channel;
    bool active;
    uint32_t press_start_tick;
    uint32_t last_tap_tick;
    int tap_count;
} zone_state_t;

static zone_state_t s_zones[TOUCH_ZONE_COUNT] = {
    [TOUCH_ZONE_HEAD]  = { .channel = HEAD_TOUCH_CHANNEL },
    [TOUCH_ZONE_BACK]  = { .channel = BACK_TOUCH_CHANNEL },
    [TOUCH_ZONE_LEFT]  = { .channel = LEFT_TOUCH_CHANNEL },
    [TOUCH_ZONE_RIGHT] = { .channel = RIGHT_TOUCH_CHANNEL },
};

static void fire_event(touch_zone_t zone, touch_event_type_t type, uint32_t duration_ms)
{
    if (s_callback) {
        touch_event_t ev = {
            .zone = zone,
            .type = type,
            .duration_ms = duration_ms,
        };
        s_callback(ev);
    }
}

static bool read_touch_active(touch_pad_t channel)
{
    uint32_t value = 0;
    touch_pad_read_raw_data(channel, &value);
    /* ESP32-S3 touch: lower value = touched (capacitance increases) */
    /* TODO: Compare against per-channel calibrated baseline threshold */
    (void)value;
    return false; /* Stub — real threshold comparison needed after calibration */
}

static void touch_task(void *arg)
{
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

        for (int z = 0; z < TOUCH_ZONE_COUNT; z++) {
            zone_state_t *zs = &s_zones[z];
            bool currently_touched = read_touch_active(zs->channel);

            if (currently_touched && !zs->active) {
                /* Touch started */
                zs->active = true;
                zs->press_start_tick = now_ms;

                if (s_push_to_talk && z == TOUCH_ZONE_HEAD) {
                    fire_event((touch_zone_t)z, TOUCH_EVENT_HOLD_START, 0);
                }
            } else if (!currently_touched && zs->active) {
                /* Touch released */
                zs->active = false;
                uint32_t duration = now_ms - zs->press_start_tick;

                if (s_push_to_talk && z == TOUCH_ZONE_HEAD) {
                    fire_event((touch_zone_t)z, TOUCH_EVENT_HOLD_END, duration);
                } else if (duration >= LONG_PRESS_MS) {
                    fire_event((touch_zone_t)z, TOUCH_EVENT_LONG_PRESS, duration);
                    zs->tap_count = 0;
                } else if (duration < TAP_MAX_MS) {
                    /* Potential tap — check for double tap */
                    if (zs->tap_count > 0 && (now_ms - zs->last_tap_tick) < DOUBLE_TAP_WINDOW_MS) {
                        fire_event((touch_zone_t)z, TOUCH_EVENT_DOUBLE_TAP, duration);
                        zs->tap_count = 0;
                    } else {
                        zs->tap_count = 1;
                        zs->last_tap_tick = now_ms;
                    }
                }
            }

            /* Flush pending single tap if double-tap window expired */
            if (!zs->active && zs->tap_count == 1 &&
                (now_ms - zs->last_tap_tick) >= DOUBLE_TAP_WINDOW_MS) {
                fire_event((touch_zone_t)z, TOUCH_EVENT_TAP, 0);
                zs->tap_count = 0;
            }
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SAMPLE_INTERVAL_MS));
    }
}

esp_err_t touch_driver_init(void)
{
    ESP_ERROR_CHECK(touch_pad_init());
    ESP_ERROR_CHECK(touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER));

    for (int z = 0; z < TOUCH_ZONE_COUNT; z++) {
        ESP_ERROR_CHECK(touch_pad_config(s_zones[z].channel));
    }

    /* Let the peripheral settle, then calibrate thresholds */
    vTaskDelay(pdMS_TO_TICKS(200));

    /* TODO: Read baseline values and set per-channel touch thresholds
     * at TOUCH_THRESHOLD_PCT of the untouched reading */

    xTaskCreatePinnedToCore(
        touch_task,
        "touch",
        2048,
        NULL,
        2,       /* Low priority — touch is not time-critical */
        &s_touch_task,
        0
    );

    ESP_LOGI(TAG, "Touch driver initialised (%d zones)", TOUCH_ZONE_COUNT);
    return ESP_OK;
}

void touch_set_event_callback(touch_event_callback_t callback)
{
    s_callback = callback;
}

bool touch_is_active(touch_zone_t zone)
{
    if (zone >= TOUCH_ZONE_COUNT) return false;
    return s_zones[zone].active;
}

esp_err_t touch_set_push_to_talk(bool enable)
{
    s_push_to_talk = enable;
    ESP_LOGI(TAG, "Push-to-talk %s", enable ? "enabled" : "disabled");
    return ESP_OK;
}

esp_err_t touch_recalibrate(void)
{
    ESP_LOGI(TAG, "Recalibrating touch thresholds...");
    /* TODO: Re-read baseline values for all channels */
    return ESP_OK;
}
