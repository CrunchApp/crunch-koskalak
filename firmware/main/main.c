/**
 * Crunch Koskalak — ESP32-S3 Firmware Entry Point
 *
 * Initialises all subsystems and starts the main device state machine.
 * Boot order:
 *   NVS → WiFi → MQTT → Display → LEDs → Audio →
 *   Motion → IMU → Balance → Touch → Battery → State Machine
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "device_state.h"
#include "network_manager.h"
#include "audio_pipeline.h"
#include "display_manager.h"
#include "led_driver.h"
#include "motion_engine.h"
#include "imu_driver.h"
#include "balance_controller.h"
#include "touch_driver.h"
#include "battery_monitor.h"

static const char *TAG = "koskalak";

/* --- Hardware pin assignments (ESP32-S3-DevKitC-1) --- */
#define PIN_LED_DATA    48    /* WS2812 NeoPixel data (GPIO 48) */
#define LED_COUNT       8     /* 8-LED ring around display bezel */
#define PIN_BATTERY_ADC 1     /* ADC1_CH0 (GPIO 1) — battery voltage divider */

/**
 * IMU event handler — routes physical gestures to device state transitions.
 */
static void on_imu_event(imu_event_t event)
{
    switch (event) {
    case IMU_EVENT_PICKUP:
        ESP_LOGI(TAG, "Device picked up — suppressing motion");
        motion_set_gait(GAIT_NONE);
        led_set_pattern(LED_PATTERN_SOLID, LED_CYAN);
        break;
    case IMU_EVENT_PUT_DOWN:
        ESP_LOGI(TAG, "Device placed down — resuming");
        if (device_state_get() == DEVICE_STATE_IDLE) {
            motion_set_gait(GAIT_IDLE_SWAY);
            led_set_pattern(LED_PATTERN_BREATHE, LED_WHITE);
        }
        break;
    case IMU_EVENT_TIP_OVER:
        ESP_LOGW(TAG, "Tip-over detected — crouching for safety");
        motion_crouch();
        led_set_pattern(LED_PATTERN_FLASH, LED_AMBER);
        break;
    case IMU_EVENT_SHAKE:
        ESP_LOGI(TAG, "Shake gesture — calling server");
        led_set_pattern(LED_PATTERN_PULSE, LED_GREEN);
        /* TODO: Emit "call server" event via MQTT */
        break;
    case IMU_EVENT_TAP:
        ESP_LOGI(TAG, "Tap detected — waking up");
        if (device_state_get() == DEVICE_STATE_SLEEPING) {
            device_state_transition(DEVICE_STATE_IDLE);
        }
        break;
    case IMU_EVENT_SURFACE_TILT:
        /* Handled continuously by balance controller — no one-shot action */
        break;
    default:
        break;
    }
}

/**
 * Touch event handler — maps touch interactions to device actions.
 */
static void on_touch_event(touch_event_t event)
{
    switch (event.type) {
    case TOUCH_EVENT_TAP:
        if (event.zone == TOUCH_ZONE_HEAD) {
            ESP_LOGI(TAG, "Head tap — attention");
            if (device_state_get() == DEVICE_STATE_SLEEPING) {
                device_state_transition(DEVICE_STATE_IDLE);
            }
            motion_set_gait(GAIT_GREETING_HOP);
            led_set_pattern(LED_PATTERN_PULSE, LED_CYAN);
        }
        break;
    case TOUCH_EVENT_LONG_PRESS:
        if (event.zone == TOUCH_ZONE_BACK) {
            ESP_LOGI(TAG, "Back long press — entering sleep");
            device_state_transition(DEVICE_STATE_SLEEPING);
        }
        break;
    case TOUCH_EVENT_HOLD_START:
        if (event.zone == TOUCH_ZONE_HEAD) {
            ESP_LOGI(TAG, "Push-to-talk: start");
            /* TODO: Trigger voice capture without wake word */
            led_set_pattern(LED_PATTERN_PULSE, LED_GREEN);
        }
        break;
    case TOUCH_EVENT_HOLD_END:
        if (event.zone == TOUCH_ZONE_HEAD) {
            ESP_LOGI(TAG, "Push-to-talk: end");
            /* TODO: Send end-of-speech signal to voice pipeline */
        }
        break;
    case TOUCH_EVENT_DOUBLE_TAP:
        if (event.zone == TOUCH_ZONE_HEAD) {
            ESP_LOGI(TAG, "Double tap — celebrate!");
            motion_set_gait(GAIT_CELEBRATE);
            led_set_pattern(LED_PATTERN_RAINBOW, LED_WHITE);
        }
        break;
    default:
        break;
    }
}

/**
 * Battery state change handler — manages power-related transitions.
 */
static void on_battery_state(battery_state_t state, uint8_t percentage)
{
    switch (state) {
    case BATTERY_LOW:
        ESP_LOGW(TAG, "Battery low (%u%%) — reducing brightness", percentage);
        led_set_brightness(64);
        /* TODO: Report low battery via MQTT telemetry */
        break;
    case BATTERY_CRITICAL:
        ESP_LOGE(TAG, "Battery critical (%u%%) — entering sleep", percentage);
        led_set_pattern(LED_PATTERN_FLASH, LED_RED);
        vTaskDelay(pdMS_TO_TICKS(2000)); /* Show warning briefly */
        device_state_transition(DEVICE_STATE_SLEEPING);
        break;
    case BATTERY_OK:
        led_set_brightness(128);
        break;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Crunch Koskalak firmware starting... (%d-DOF)", KOSKALAK_DOF);

    /* Initialise NVS — required for WiFi credentials and device config */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Core subsystems */
    ESP_ERROR_CHECK(device_state_init());
    ESP_ERROR_CHECK(network_manager_init());
    ESP_ERROR_CHECK(display_manager_init());
    ESP_ERROR_CHECK(led_driver_init(PIN_LED_DATA, LED_COUNT));
    ESP_ERROR_CHECK(audio_pipeline_init());

    /* Motion + IMU + balance (order matters: motion first, then IMU, then balance) */
    ESP_ERROR_CHECK(motion_engine_init());
    ESP_ERROR_CHECK(imu_driver_init());
    imu_set_event_callback(on_imu_event);
    ESP_ERROR_CHECK(imu_start());

    balance_config_t bal_cfg = BALANCE_CONFIG_DEFAULT;
    ESP_ERROR_CHECK(balance_controller_init(&bal_cfg));
    ESP_ERROR_CHECK(balance_controller_start());

    /* Touch (after all motion/display initialised so callbacks can use them) */
    ESP_ERROR_CHECK(touch_driver_init());
    touch_set_event_callback(on_touch_event);

    /* Battery monitoring */
    ESP_ERROR_CHECK(battery_monitor_init(PIN_BATTERY_ADC));
    battery_set_state_callback(on_battery_state);
    ESP_ERROR_CHECK(battery_monitor_start());

    /* Boot complete — show initial state */
    led_set_pattern(LED_PATTERN_BREATHE, LED_WHITE);

    /* Start the main state machine task */
    device_state_start();

    ESP_LOGI(TAG, "All subsystems initialised. Entering main loop.");
}
