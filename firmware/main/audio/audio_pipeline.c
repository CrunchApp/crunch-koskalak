/**
 * Audio Pipeline — Implementation stub
 *
 * Full implementation requires ESP-ADF and ESP-SR components.
 * This stub provides the interface for integration testing.
 */

#include "audio_pipeline.h"
#include "esp_log.h"

static const char *TAG = "audio";

static wake_word_callback_t s_wake_cb = NULL;
static audio_chunk_callback_t s_chunk_cb = NULL;

esp_err_t audio_pipeline_init(void)
{
    /* TODO: Configure I2S for INMP441 mic (I2S_NUM_0) and MAX98357A speaker (I2S_NUM_1)
     * TODO: Initialise ESP-ADF audio pipeline
     * TODO: Load ESP-SR WakeNet model for "Hey Crunchy" wake word
     */
    ESP_LOGI(TAG, "Audio pipeline initialised (stub)");
    return ESP_OK;
}

void audio_set_wake_word_callback(wake_word_callback_t callback)
{
    s_wake_cb = callback;
}

esp_err_t audio_start_capture(audio_chunk_callback_t chunk_callback)
{
    s_chunk_cb = chunk_callback;
    /* TODO: Start I2S mic DMA capture task on Core 1 */
    ESP_LOGI(TAG, "Mic capture started (stub)");
    return ESP_OK;
}

esp_err_t audio_stop_capture(void)
{
    s_chunk_cb = NULL;
    ESP_LOGI(TAG, "Mic capture stopped");
    return ESP_OK;
}

esp_err_t audio_play_buffer(const int16_t *samples, size_t num_samples)
{
    /* TODO: Write samples to I2S speaker DMA buffer */
    return ESP_OK;
}

esp_err_t audio_stop_playback(void)
{
    /* TODO: Flush I2S speaker buffer */
    return ESP_OK;
}

bool audio_is_playing(void)
{
    /* TODO: Check I2S speaker DMA state */
    return false;
}
