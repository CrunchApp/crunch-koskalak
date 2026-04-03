/**
 * Device State Machine — Implementation
 */

#include "device_state.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "device_state";

static device_state_t s_current_state = DEVICE_STATE_BOOTING;
static QueueHandle_t s_state_queue = NULL;
static TaskHandle_t s_state_task = NULL;

/* NVS keys for persisted config */
#define NVS_NAMESPACE "koskalak"
#define NVS_KEY_DEVICE_ID "device_id"
#define NVS_KEY_ORG_ID "org_id"
#define NVS_KEY_LOCATION_ID "location_id"
#define NVS_KEY_JWT "device_jwt"

static void state_machine_task(void *arg)
{
    device_state_t requested_state;

    for (;;) {
        if (xQueueReceive(s_state_queue, &requested_state, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGI(TAG, "State transition: %d → %d", s_current_state, requested_state);
            /* TODO: validate transition, execute entry/exit actions */
            s_current_state = requested_state;
        }

        /* Periodic state-specific processing */
        switch (s_current_state) {
        case DEVICE_STATE_IDLE:
            /* TODO: check for proximity/wake word events */
            break;
        case DEVICE_STATE_SLEEPING:
            /* TODO: enter light sleep, wake on GPIO or timer */
            break;
        default:
            break;
        }
    }
}

esp_err_t device_state_init(void)
{
    s_state_queue = xQueueCreate(8, sizeof(device_state_t));
    if (s_state_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create state queue");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Device state machine initialised");
    return ESP_OK;
}

void device_state_start(void)
{
    xTaskCreatePinnedToCore(
        state_machine_task,
        "state_machine",
        4096,
        NULL,
        5,      /* Priority: above idle, below real-time audio */
        &s_state_task,
        0       /* Core 0 — core 1 reserved for audio pipeline */
    );

    /* Determine initial state based on provisioning status */
    if (device_is_provisioned()) {
        device_state_transition(DEVICE_STATE_CONNECTING);
    } else {
        device_state_transition(DEVICE_STATE_PROVISIONING);
    }
}

esp_err_t device_state_transition(device_state_t new_state)
{
    if (s_state_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xQueueSend(s_state_queue, &new_state, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "State queue full, transition to %d dropped", new_state);
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

device_state_t device_state_get(void)
{
    return s_current_state;
}

bool device_is_provisioned(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return false;
    }

    size_t required_size = 0;
    err = nvs_get_str(handle, NVS_KEY_JWT, NULL, &required_size);
    nvs_close(handle);

    return (err == ESP_OK && required_size > 1);
}
