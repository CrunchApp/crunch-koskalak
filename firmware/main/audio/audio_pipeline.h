/**
 * Audio Pipeline
 *
 * Manages the I2S microphone (INMP441) and speaker (MAX98357A) via ESP-ADF.
 * Provides:
 *   - Continuous mic capture with ring buffer
 *   - Wake word detection via ESP-SR (WakeNet)
 *   - Audio playback from TTS response stream
 *   - Acoustic echo cancellation (AEC) for barge-in support
 *
 * Audio runs on Core 1 to avoid contention with the state machine on Core 0.
 */

#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

typedef void (*wake_word_callback_t)(void);
typedef void (*audio_chunk_callback_t)(const int16_t *samples, size_t num_samples);

/**
 * Initialise I2S peripherals, ESP-ADF pipeline, and ESP-SR wake word model.
 */
esp_err_t audio_pipeline_init(void);

/**
 * Register callback invoked when the wake word is detected.
 */
void audio_set_wake_word_callback(wake_word_callback_t callback);

/**
 * Start capturing audio from the microphone.
 * The chunk_callback receives PCM16 audio chunks suitable for streaming.
 */
esp_err_t audio_start_capture(audio_chunk_callback_t chunk_callback);

/**
 * Stop microphone capture.
 */
esp_err_t audio_stop_capture(void);

/**
 * Queue PCM16 audio data for playback through the speaker.
 * Used to play TTS response audio received from the server.
 */
esp_err_t audio_play_buffer(const int16_t *samples, size_t num_samples);

/**
 * Stop any ongoing playback immediately.
 */
esp_err_t audio_stop_playback(void);

/**
 * Returns true if TTS audio is currently playing.
 */
bool audio_is_playing(void);
