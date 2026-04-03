/**
 * IMU Driver — MPU-6050 Implementation
 *
 * Complementary filter for orientation (pitch/roll) at 100Hz.
 * Event detection thresholds tuned for tabletop restaurant use:
 *   - Pickup: sustained low-g (<0.3g) for >100ms
 *   - Tip-over: pitch or roll > 45 degrees
 *   - Shake: gyro magnitude > 200 deg/s, 3+ times in 1 second
 *   - Tap: accel spike > 2g, duration < 50ms
 *   - Surface tilt: static pitch/roll > 5 degrees
 */

#include "imu_driver.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <math.h>
#include <string.h>

static const char *TAG = "imu";

#define I2C_MASTER_NUM      I2C_NUM_0
#define IMU_SAMPLE_RATE_HZ  100
#define IMU_TASK_STACK_SIZE 4096
#define IMU_TASK_PRIORITY   6  /* Above motion engine (4), below audio (7) */

/* Complementary filter coefficient (0.98 = trust gyro mostly, correct with accel) */
#define COMP_FILTER_ALPHA   0.98f

/* Event detection thresholds */
#define PICKUP_G_THRESHOLD      0.3f    /* Below this = freefall / held */
#define PICKUP_DURATION_MS      100
#define TIPOVER_ANGLE_DEG       45.0f
#define SHAKE_GYRO_THRESHOLD    200.0f  /* deg/s */
#define SHAKE_COUNT_THRESHOLD   3
#define SHAKE_WINDOW_MS         1000
#define TAP_ACCEL_THRESHOLD     2.0f    /* g */
#define SURFACE_TILT_DEG        5.0f

/* Calibration offsets (loaded from NVS) */
static float s_accel_offset[3] = {0};
static float s_gyro_offset[3] = {0};

/* Double-buffered data */
static imu_data_t s_data = {0};
static imu_event_callback_t s_event_cb = NULL;
static TaskHandle_t s_imu_task = NULL;
static bool s_is_held = false;

/* MPU-6050 register addresses */
#define MPU6050_REG_PWR_MGMT_1     0x6B
#define MPU6050_REG_SMPLRT_DIV     0x19
#define MPU6050_REG_CONFIG         0x1A
#define MPU6050_REG_GYRO_CONFIG    0x1B
#define MPU6050_REG_ACCEL_CONFIG   0x1C
#define MPU6050_REG_ACCEL_XOUT_H   0x3B
#define MPU6050_REG_WHO_AM_I       0x75

static esp_err_t mpu6050_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = {reg, value};
    return i2c_master_write_to_device(I2C_MASTER_NUM, MPU6050_I2C_ADDR,
                                       buf, sizeof(buf), pdMS_TO_TICKS(100));
}

static esp_err_t mpu6050_read_regs(uint8_t start_reg, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(I2C_MASTER_NUM, MPU6050_I2C_ADDR,
                                         &start_reg, 1, data, len,
                                         pdMS_TO_TICKS(100));
}

static void fire_event(imu_event_t event)
{
    if (s_event_cb) {
        s_event_cb(event);
    }
}

static void imu_sample_task(void *arg)
{
    uint8_t raw[14]; /* 6 accel + 2 temp + 6 gyro */
    float dt = 1.0f / IMU_SAMPLE_RATE_HZ;
    float pitch = 0, roll = 0;

    /* Shake detection state */
    uint32_t shake_timestamps[SHAKE_COUNT_THRESHOLD];
    int shake_idx = 0;
    memset(shake_timestamps, 0, sizeof(shake_timestamps));

    /* Pickup detection state */
    uint32_t low_g_start = 0;
    bool was_held = false;

    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        esp_err_t err = mpu6050_read_regs(MPU6050_REG_ACCEL_XOUT_H, raw, 14);
        if (err != ESP_OK) {
            vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000 / IMU_SAMPLE_RATE_HZ));
            continue;
        }

        /* Parse raw data (big-endian 16-bit signed) */
        int16_t ax_raw = (raw[0] << 8) | raw[1];
        int16_t ay_raw = (raw[2] << 8) | raw[3];
        int16_t az_raw = (raw[4] << 8) | raw[5];
        int16_t temp_raw = (raw[6] << 8) | raw[7];
        int16_t gx_raw = (raw[8] << 8) | raw[9];
        int16_t gy_raw = (raw[10] << 8) | raw[11];
        int16_t gz_raw = (raw[12] << 8) | raw[13];

        /* Convert to physical units and apply calibration */
        float ax = (ax_raw / 16384.0f) - s_accel_offset[0];   /* +/-2g → g */
        float ay = (ay_raw / 16384.0f) - s_accel_offset[1];
        float az = (az_raw / 16384.0f) - s_accel_offset[2];
        float gx = (gx_raw / 131.0f) - s_gyro_offset[0];     /* +/-250 deg/s */
        float gy = (gy_raw / 131.0f) - s_gyro_offset[1];
        float gz = (gz_raw / 131.0f) - s_gyro_offset[2];
        float temp = (temp_raw / 340.0f) + 36.53f;

        /* Complementary filter for orientation */
        float accel_pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * (180.0f / M_PI);
        float accel_roll = atan2f(ay, az) * (180.0f / M_PI);

        pitch = COMP_FILTER_ALPHA * (pitch + gy * dt) + (1.0f - COMP_FILTER_ALPHA) * accel_pitch;
        roll = COMP_FILTER_ALPHA * (roll + gx * dt) + (1.0f - COMP_FILTER_ALPHA) * accel_roll;

        /* Update shared data */
        s_data.accel_x = ax;
        s_data.accel_y = ay;
        s_data.accel_z = az;
        s_data.gyro_x = gx;
        s_data.gyro_y = gy;
        s_data.gyro_z = gz;
        s_data.pitch = pitch;
        s_data.roll = roll;
        s_data.temp_c = temp;

        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        /* --- Event detection --- */

        float accel_mag = sqrtf(ax * ax + ay * ay + az * az);
        float gyro_mag = sqrtf(gx * gx + gy * gy + gz * gz);

        /* Pickup detection: sustained low-g */
        if (accel_mag < PICKUP_G_THRESHOLD) {
            if (low_g_start == 0) low_g_start = now;
            if (!was_held && (now - low_g_start) > PICKUP_DURATION_MS) {
                s_is_held = true;
                was_held = true;
                fire_event(IMU_EVENT_PICKUP);
            }
        } else {
            if (was_held) {
                s_is_held = false;
                was_held = false;
                fire_event(IMU_EVENT_PUT_DOWN);
            }
            low_g_start = 0;
        }

        /* Tip-over detection */
        if (fabsf(pitch) > TIPOVER_ANGLE_DEG || fabsf(roll) > TIPOVER_ANGLE_DEG) {
            fire_event(IMU_EVENT_TIP_OVER);
        }

        /* Shake detection: count high-gyro spikes in rolling window */
        if (gyro_mag > SHAKE_GYRO_THRESHOLD) {
            shake_timestamps[shake_idx % SHAKE_COUNT_THRESHOLD] = now;
            shake_idx++;

            if (shake_idx >= SHAKE_COUNT_THRESHOLD) {
                uint32_t oldest = shake_timestamps[(shake_idx) % SHAKE_COUNT_THRESHOLD];
                if ((now - oldest) < SHAKE_WINDOW_MS) {
                    fire_event(IMU_EVENT_SHAKE);
                    shake_idx = 0; /* Reset to avoid repeat fires */
                }
            }
        }

        /* Tap detection: sharp accel spike */
        if (accel_mag > TAP_ACCEL_THRESHOLD && !s_is_held) {
            fire_event(IMU_EVENT_TAP);
        }

        /* Surface tilt detection (only when not held) */
        if (!s_is_held && (fabsf(pitch) > SURFACE_TILT_DEG || fabsf(roll) > SURFACE_TILT_DEG)) {
            fire_event(IMU_EVENT_SURFACE_TILT);
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000 / IMU_SAMPLE_RATE_HZ));
    }
}

esp_err_t imu_driver_init(void)
{
    /* Verify MPU-6050 is present */
    uint8_t who_am_i = 0;
    esp_err_t err = mpu6050_read_regs(MPU6050_REG_WHO_AM_I, &who_am_i, 1);
    if (err != ESP_OK || who_am_i != 0x68) {
        ESP_LOGE(TAG, "MPU-6050 not found (WHO_AM_I: 0x%02x)", who_am_i);
        return ESP_ERR_NOT_FOUND;
    }

    /* Wake up (clear sleep bit) */
    ESP_ERROR_CHECK(mpu6050_write_reg(MPU6050_REG_PWR_MGMT_1, 0x00));
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Sample rate: 1kHz / (1 + 9) = 100Hz */
    ESP_ERROR_CHECK(mpu6050_write_reg(MPU6050_REG_SMPLRT_DIV, 9));

    /* DLPF: 42Hz bandwidth (good noise rejection for servo vibration) */
    ESP_ERROR_CHECK(mpu6050_write_reg(MPU6050_REG_CONFIG, 0x03));

    /* Gyro: +/- 250 deg/s */
    ESP_ERROR_CHECK(mpu6050_write_reg(MPU6050_REG_GYRO_CONFIG, 0x00));

    /* Accel: +/- 2g */
    ESP_ERROR_CHECK(mpu6050_write_reg(MPU6050_REG_ACCEL_CONFIG, 0x00));

    /* Load calibration offsets from NVS */
    nvs_handle_t handle;
    if (nvs_open("imu_cal", NVS_READONLY, &handle) == ESP_OK) {
        size_t size = sizeof(s_accel_offset);
        nvs_get_blob(handle, "accel_off", s_accel_offset, &size);
        size = sizeof(s_gyro_offset);
        nvs_get_blob(handle, "gyro_off", s_gyro_offset, &size);
        nvs_close(handle);
        ESP_LOGI(TAG, "Calibration loaded from NVS");
    } else {
        ESP_LOGW(TAG, "No calibration data — run imu_calibrate() on flat surface");
    }

    ESP_LOGI(TAG, "MPU-6050 initialised (100Hz, +/-2g, +/-250 deg/s)");
    return ESP_OK;
}

esp_err_t imu_start(void)
{
    if (s_imu_task != NULL) return ESP_ERR_INVALID_STATE;

    xTaskCreatePinnedToCore(
        imu_sample_task,
        "imu",
        IMU_TASK_STACK_SIZE,
        NULL,
        IMU_TASK_PRIORITY,
        &s_imu_task,
        0  /* Core 0 — same as motion engine, feeds balance data to it */
    );

    ESP_LOGI(TAG, "IMU sampling started at %dHz", IMU_SAMPLE_RATE_HZ);
    return ESP_OK;
}

esp_err_t imu_stop(void)
{
    if (s_imu_task == NULL) return ESP_OK;
    vTaskDelete(s_imu_task);
    s_imu_task = NULL;
    ESP_LOGI(TAG, "IMU sampling stopped");
    return ESP_OK;
}

imu_data_t imu_get_data(void)
{
    return s_data;
}

void imu_set_event_callback(imu_event_callback_t callback)
{
    s_event_cb = callback;
}

void imu_get_surface_tilt(float *pitch_deg, float *roll_deg)
{
    *pitch_deg = s_data.pitch;
    *roll_deg = s_data.roll;
}

bool imu_is_held(void)
{
    return s_is_held;
}

esp_err_t imu_calibrate(void)
{
    ESP_LOGI(TAG, "Calibrating — keep device flat and still for 2 seconds...");

    float accel_sum[3] = {0}, gyro_sum[3] = {0};
    int samples = IMU_SAMPLE_RATE_HZ * 2; /* 2 seconds */
    uint8_t raw[14];

    for (int i = 0; i < samples; i++) {
        esp_err_t err = mpu6050_read_regs(MPU6050_REG_ACCEL_XOUT_H, raw, 14);
        if (err != ESP_OK) continue;

        accel_sum[0] += (int16_t)((raw[0] << 8) | raw[1]) / 16384.0f;
        accel_sum[1] += (int16_t)((raw[2] << 8) | raw[3]) / 16384.0f;
        accel_sum[2] += (int16_t)((raw[4] << 8) | raw[5]) / 16384.0f;
        gyro_sum[0] += (int16_t)((raw[8] << 8) | raw[9]) / 131.0f;
        gyro_sum[1] += (int16_t)((raw[10] << 8) | raw[11]) / 131.0f;
        gyro_sum[2] += (int16_t)((raw[12] << 8) | raw[13]) / 131.0f;

        vTaskDelay(pdMS_TO_TICKS(1000 / IMU_SAMPLE_RATE_HZ));
    }

    /* Accel offsets (expect 0, 0, 1g when flat) */
    s_accel_offset[0] = accel_sum[0] / samples;
    s_accel_offset[1] = accel_sum[1] / samples;
    s_accel_offset[2] = (accel_sum[2] / samples) - 1.0f; /* Subtract gravity */

    /* Gyro offsets (expect 0, 0, 0 when still) */
    s_gyro_offset[0] = gyro_sum[0] / samples;
    s_gyro_offset[1] = gyro_sum[1] / samples;
    s_gyro_offset[2] = gyro_sum[2] / samples;

    /* Persist to NVS */
    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open("imu_cal", NVS_READWRITE, &handle));
    ESP_ERROR_CHECK(nvs_set_blob(handle, "accel_off", s_accel_offset, sizeof(s_accel_offset)));
    ESP_ERROR_CHECK(nvs_set_blob(handle, "gyro_off", s_gyro_offset, sizeof(s_gyro_offset)));
    ESP_ERROR_CHECK(nvs_commit(handle));
    nvs_close(handle);

    ESP_LOGI(TAG, "Calibration complete. Accel offset: [%.3f, %.3f, %.3f] Gyro offset: [%.1f, %.1f, %.1f]",
             s_accel_offset[0], s_accel_offset[1], s_accel_offset[2],
             s_gyro_offset[0], s_gyro_offset[1], s_gyro_offset[2]);

    return ESP_OK;
}
