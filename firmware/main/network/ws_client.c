/**
 * WebSocket Client — On-demand voice session streaming
 *
 * Opened when wake word is detected, closed after conversation ends.
 * Sends PCM16 audio frames from mic, receives TTS audio from server.
 */

#include "network_manager.h"
#include "esp_log.h"
#include "esp_websocket_client.h"

static const char *TAG = "ws_voice";

static esp_websocket_client_handle_t s_ws_client = NULL;
static ws_audio_callback_t s_on_audio_cb = NULL;

static void ws_event_handler(void *handler_args, esp_event_base_t base,
                             int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Voice session WebSocket connected");
        break;
    case WEBSOCKET_EVENT_DATA:
        if (data->op_code == 0x02 && s_on_audio_cb != NULL) {
            /* Binary frame: TTS audio data from server */
            s_on_audio_cb((const uint8_t *)data->data_ptr, data->data_len);
        }
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Voice session ended");
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG, "WebSocket error");
        break;
    default:
        break;
    }
}

esp_err_t ws_voice_session_open(const char *gateway_url, ws_audio_callback_t on_audio)
{
    if (s_ws_client != NULL) {
        ESP_LOGW(TAG, "Voice session already open");
        return ESP_ERR_INVALID_STATE;
    }

    s_on_audio_cb = on_audio;

    esp_websocket_client_config_t ws_cfg = {
        .uri = gateway_url,
        .buffer_size = 4096,
    };

    s_ws_client = esp_websocket_client_init(&ws_cfg);
    if (s_ws_client == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_websocket_register_events(s_ws_client, WEBSOCKET_EVENT_ANY,
                                  ws_event_handler, NULL);

    return esp_websocket_client_start(s_ws_client);
}

esp_err_t ws_voice_send_audio(const uint8_t *pcm_data, size_t len)
{
    if (s_ws_client == NULL || !esp_websocket_client_is_connected(s_ws_client)) {
        return ESP_ERR_INVALID_STATE;
    }
    int sent = esp_websocket_client_send_bin(s_ws_client, (const char *)pcm_data, len,
                                              pdMS_TO_TICKS(1000));
    return (sent >= 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t ws_voice_session_close(void)
{
    if (s_ws_client == NULL) return ESP_OK;

    esp_err_t ret = esp_websocket_client_close(s_ws_client, pdMS_TO_TICKS(5000));
    esp_websocket_client_destroy(s_ws_client);
    s_ws_client = NULL;
    s_on_audio_cb = NULL;
    return ret;
}
