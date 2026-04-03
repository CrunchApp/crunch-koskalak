/**
 * Device State Machine
 *
 * Manages the top-level device state and transitions:
 *   BOOTING → PROVISIONING → CONNECTING → IDLE → LISTENING → THINKING → SPEAKING → IDLE
 *                                          ↓
 *                                       SLEEPING
 *
 * Also handles: OTA updates, config persistence, watchdog, error recovery.
 */

#pragma once

#include "esp_err.h"

typedef enum {
    DEVICE_STATE_BOOTING,       /* Hardware init in progress */
    DEVICE_STATE_PROVISIONING,  /* Awaiting WiFi/auth via BLE */
    DEVICE_STATE_CONNECTING,    /* Establishing MQTT + backend connection */
    DEVICE_STATE_IDLE,          /* Displaying idle screen, awaiting wake word */
    DEVICE_STATE_LISTENING,     /* Wake word detected, streaming audio to cloud */
    DEVICE_STATE_THINKING,      /* Waiting for AI response */
    DEVICE_STATE_SPEAKING,      /* Playing TTS response + expressive animation */
    DEVICE_STATE_SLEEPING,      /* Low-power mode, wake on proximity or timer */
    DEVICE_STATE_UPDATING,      /* OTA firmware update in progress */
    DEVICE_STATE_ERROR,         /* Recoverable error, will retry */
} device_state_t;

/**
 * Initialise the device state machine. Must be called after NVS init.
 * Loads persisted config (device ID, org/location, server URLs).
 */
esp_err_t device_state_init(void);

/**
 * Start the state machine FreeRTOS task. Call after all subsystems are initialised.
 */
void device_state_start(void);

/**
 * Request a state transition. Thread-safe (uses FreeRTOS queue).
 * Invalid transitions are logged and ignored.
 */
esp_err_t device_state_transition(device_state_t new_state);

/**
 * Get the current device state. Thread-safe.
 */
device_state_t device_state_get(void);

/**
 * Check if the device has been provisioned (has WiFi creds + device JWT).
 */
bool device_is_provisioned(void);
