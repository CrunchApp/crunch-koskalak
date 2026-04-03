/**
 * IMU Driver — MPU-6050 6-Axis Accelerometer/Gyroscope
 *
 * Shares the I2C bus with PCA9685 servo driver (no additional GPIO needed).
 * Provides:
 *   - Raw accelerometer + gyroscope readings
 *   - Orientation estimation (pitch, roll)
 *   - Event detection (pickup, tip-over, shake, surface tilt)
 *
 * Inspired by KT2/TurtleOS's approach: the IMU feeds a closed-loop
 * balance controller that adjusts servo offsets in real time, making
 * gait patterns stable on uneven restaurant tables.
 *
 * Runs as a dedicated FreeRTOS task at 100Hz sample rate on Core 0.
 */

#pragma once

#include "esp_err.h"
#include <stdbool.h>

/* MPU-6050 I2C address (AD0 pin low) */
#define MPU6050_I2C_ADDR 0x68

typedef struct {
    float accel_x;  /* g  (-2..+2) */
    float accel_y;  /* g  (-2..+2) */
    float accel_z;  /* g  (-2..+2) */
    float gyro_x;   /* deg/s (-250..+250) */
    float gyro_y;   /* deg/s (-250..+250) */
    float gyro_z;   /* deg/s (-250..+250) */
    float pitch;    /* degrees, complementary filter output */
    float roll;     /* degrees, complementary filter output */
    float temp_c;   /* die temperature in Celsius */
} imu_data_t;

typedef enum {
    IMU_EVENT_NONE,
    IMU_EVENT_PICKUP,        /* Lifted off surface (freefall detected) */
    IMU_EVENT_PUT_DOWN,      /* Placed back on surface (impact detected) */
    IMU_EVENT_TIP_OVER,      /* Tilt exceeds safe threshold (>45 degrees) */
    IMU_EVENT_SHAKE,         /* Rapid repeated motion (call server gesture) */
    IMU_EVENT_TAP,           /* Single sharp impact (attention gesture) */
    IMU_EVENT_SURFACE_TILT,  /* Surface is uneven (>5 degrees off level) */
} imu_event_t;

typedef void (*imu_event_callback_t)(imu_event_t event);

/**
 * Initialise MPU-6050 on the shared I2C bus.
 * Must be called after I2C master is initialised (done in motion_engine_init).
 * Configures: +/-2g accel range, +/-250 deg/s gyro, 100Hz sample rate, DLPF at 42Hz.
 */
esp_err_t imu_driver_init(void);

/**
 * Start the IMU sampling task. Reads at 100Hz, updates orientation estimate,
 * and fires event callbacks for gesture/state detection.
 */
esp_err_t imu_start(void);

/**
 * Stop the IMU sampling task (e.g., during deep sleep).
 */
esp_err_t imu_stop(void);

/**
 * Get the latest IMU reading. Thread-safe (double-buffered).
 */
imu_data_t imu_get_data(void);

/**
 * Register a callback for IMU events (pickup, shake, tip-over, etc.).
 * Only one callback supported — subsequent calls overwrite the previous.
 */
void imu_set_event_callback(imu_event_callback_t callback);

/**
 * Get the current surface tilt angles for balance correction.
 * Returns pitch and roll in degrees — fed to the motion engine's
 * balance controller to adjust servo offsets.
 */
void imu_get_surface_tilt(float *pitch_deg, float *roll_deg);

/**
 * Returns true if the device is currently being held/carried
 * (not on a surface). Used to suppress gait patterns during handling.
 */
bool imu_is_held(void);

/**
 * Calibrate the IMU on a flat surface. Stores offsets in NVS.
 * Should be run once during manufacturing/first setup.
 */
esp_err_t imu_calibrate(void);
