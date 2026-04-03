/**
 * MQTT Client — Persistent connection for telemetry, events, and commands
 */

#include "network_manager.h"
#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAG = "mqtt";

static esp_mqtt_client_handle_t s_mqtt_client = NULL;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected to MQTT broker");
        /* TODO: Subscribe to command, config, and OTA topics */
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Disconnected from MQTT broker");
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT data received on topic: %.*s",
                 event->topic_len, event->topic);
        /* TODO: Route to command handler, config handler, or OTA handler */
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error type: %d", event->error_handle->error_type);
        break;
    default:
        break;
    }
}

esp_err_t mqtt_client_start(const char *broker_url, const char *device_jwt)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = broker_url,
        .credentials.username = "device",
        .credentials.authentication.password = device_jwt,
        .session.keepalive = 30,
        .network.reconnect_timeout_ms = 5000,
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "Failed to init MQTT client");
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(esp_mqtt_client_register_event(
        s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));

    return esp_mqtt_client_start(s_mqtt_client);
}

esp_err_t mqtt_publish_telemetry(const char *json_payload)
{
    if (s_mqtt_client == NULL) return ESP_ERR_INVALID_STATE;
    /* TODO: Build topic from device config: crunch/{org}/{loc}/{device}/telemetry */
    int msg_id = esp_mqtt_client_publish(s_mqtt_client, "crunch/telemetry",
                                          json_payload, 0, 1, 0);
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t mqtt_publish_event(const char *event_type, const char *json_payload)
{
    if (s_mqtt_client == NULL) return ESP_ERR_INVALID_STATE;
    /* TODO: Build topic with event_type suffix */
    int msg_id = esp_mqtt_client_publish(s_mqtt_client, "crunch/events",
                                          json_payload, 0, 1, 0);
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}
