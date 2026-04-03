/**
 * Network Manager
 *
 * Handles WiFi provisioning, connection management, and provides
 * MQTT client and WebSocket client interfaces.
 *
 * WiFi credentials are stored in NVS via BLE provisioning (SmartConfig).
 * MQTT is the persistent connection for telemetry, events, and OTA triggers.
 * WebSocket is opened on-demand for voice streaming sessions only.
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/**
 * Initialise WiFi station mode and event handlers.
 * If credentials exist in NVS, auto-connects. Otherwise waits for provisioning.
 */
esp_err_t network_manager_init(void);

/**
 * Start BLE-based WiFi provisioning. Exposes a BLE GATT service
 * that accepts WiFi SSID/password + device provisioning token.
 */
esp_err_t network_start_provisioning(void);

/**
 * Returns true if WiFi is connected and IP is assigned.
 */
bool network_is_connected(void);

/* --- MQTT --- */

/**
 * Connect to MQTT broker. Topics auto-subscribed:
 *   crunch/{orgId}/{locationId}/{deviceId}/commands
 *   crunch/{orgId}/{locationId}/{deviceId}/config
 *   crunch/{orgId}/{locationId}/{deviceId}/ota
 */
esp_err_t mqtt_client_start(const char *broker_url, const char *device_jwt);

/**
 * Publish telemetry data (battery, signal, uptime).
 * Topic: crunch/{orgId}/{locationId}/{deviceId}/telemetry
 */
esp_err_t mqtt_publish_telemetry(const char *json_payload);

/**
 * Publish a device event (e.g., wake_word_detected, order_confirmed).
 * Topic: crunch/{orgId}/{locationId}/{deviceId}/events
 */
esp_err_t mqtt_publish_event(const char *event_type, const char *json_payload);

/* --- WebSocket (voice sessions) --- */

typedef void (*ws_audio_callback_t)(const uint8_t *data, size_t len);

/**
 * Open a WebSocket connection to server-gadget for a voice session.
 * The callback is invoked when TTS audio frames arrive from the server.
 */
esp_err_t ws_voice_session_open(const char *gateway_url, ws_audio_callback_t on_audio);

/**
 * Send a chunk of PCM16 audio from the microphone to the server.
 */
esp_err_t ws_voice_send_audio(const uint8_t *pcm_data, size_t len);

/**
 * Close the voice session WebSocket.
 */
esp_err_t ws_voice_session_close(void);
