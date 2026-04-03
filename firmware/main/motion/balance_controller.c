/**
 * Balance Controller — Implementation
 *
 * Runs as a standalone FreeRTOS task at 50Hz on Core 0.
 * Reads pitch/roll from IMU, computes proportional-derivative corrections,
 * and writes to a shared offset buffer that motion_engine reads.
 *
 * Servo mapping for balance correction:
 *   8-DOF (miniKame): pitch adjusts all 4 hip servos, roll adjusts left vs right hips
 *   4-DOF (KT2-style): pitch adjusts front vs back leg, roll adjusts left vs right leg
 */

#include "balance_controller.h"
#include "imu_driver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

static const char *TAG = "balance";

#define BALANCE_RATE_HZ  50
#define BALANCE_STACK    3072
#define BALANCE_PRIORITY 5  /* Above motion (4), below IMU (6) */

static balance_config_t s_config;
static bool s_enabled = false;
static float s_offsets[BALANCE_NUM_SERVOS] = {0};
static TaskHandle_t s_task = NULL;

/* Previous tilt for derivative term */
static float s_prev_pitch = 0;
static float s_prev_roll = 0;

static float clamp(float val, float limit)
{
    if (val > limit) return limit;
    if (val < -limit) return -limit;
    return val;
}

static void compute_offsets(float pitch, float roll)
{
    float dt = 1.0f / BALANCE_RATE_HZ;

    /* Apply deadband */
    float p = (fabsf(pitch) < s_config.deadband_deg) ? 0 : pitch;
    float r = (fabsf(roll) < s_config.deadband_deg) ? 0 : roll;

    /* PD controller */
    float pitch_rate = (p - s_prev_pitch) / dt;
    float roll_rate = (r - s_prev_roll) / dt;

    float pitch_corr = -(p * s_config.p_gain + pitch_rate * s_config.d_gain);
    float roll_corr = -(r * s_config.p_gain + roll_rate * s_config.d_gain);

    pitch_corr = clamp(pitch_corr, s_config.max_correction);
    roll_corr = clamp(roll_corr, s_config.max_correction);

    s_prev_pitch = p;
    s_prev_roll = r;

#if KOSKALAK_DOF == 8
    /* 8-DOF servo mapping (miniKame layout):
     * Channels: FL_HIP(0), FL_KNEE(1), FR_HIP(2), FR_KNEE(3),
     *           BL_HIP(4), BL_KNEE(5), BR_HIP(6), BR_KNEE(7)
     *
     * Pitch: front hips up, back hips down (or vice versa)
     * Roll: left hips up, right hips down (or vice versa)
     * Knees: no direct correction (follow gait)
     */
    s_offsets[0] = pitch_corr - roll_corr;  /* FL hip */
    s_offsets[1] = 0;                        /* FL knee */
    s_offsets[2] = pitch_corr + roll_corr;  /* FR hip */
    s_offsets[3] = 0;                        /* FR knee */
    s_offsets[4] = -pitch_corr - roll_corr; /* BL hip */
    s_offsets[5] = 0;                        /* BL knee */
    s_offsets[6] = -pitch_corr + roll_corr; /* BR hip */
    s_offsets[7] = 0;                        /* BR knee */

#elif KOSKALAK_DOF == 4
    /* 4-DOF servo mapping (KT2-style, one actuator per leg):
     * Channels: FL(0), FR(1), BL(2), BR(3)
     *
     * Pitch: front legs up, back legs down
     * Roll: left legs up, right legs down
     */
    s_offsets[0] = pitch_corr - roll_corr;  /* FL */
    s_offsets[1] = pitch_corr + roll_corr;  /* FR */
    s_offsets[2] = -pitch_corr - roll_corr; /* BL */
    s_offsets[3] = -pitch_corr + roll_corr; /* BR */

#else
#error "KOSKALAK_DOF must be 4 or 8"
#endif
}

static void balance_task(void *arg)
{
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        if (s_enabled && !imu_is_held()) {
            float pitch, roll;
            imu_get_surface_tilt(&pitch, &roll);
            compute_offsets(pitch, roll);
        } else {
            /* Zero all offsets when disabled or device is held */
            memset(s_offsets, 0, sizeof(s_offsets));
            s_prev_pitch = 0;
            s_prev_roll = 0;
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000 / BALANCE_RATE_HZ));
    }
}

esp_err_t balance_controller_init(const balance_config_t *config)
{
    if (config) {
        s_config = *config;
    } else {
        balance_config_t defaults = BALANCE_CONFIG_DEFAULT;
        s_config = defaults;
    }

    memset(s_offsets, 0, sizeof(s_offsets));
    ESP_LOGI(TAG, "Balance controller initialised (P=%.2f, D=%.3f, max=%.1f deg, %d-DOF)",
             s_config.p_gain, s_config.d_gain, s_config.max_correction, KOSKALAK_DOF);
    return ESP_OK;
}

esp_err_t balance_controller_start(void)
{
    if (s_task != NULL) return ESP_ERR_INVALID_STATE;

    xTaskCreatePinnedToCore(
        balance_task,
        "balance",
        BALANCE_STACK,
        NULL,
        BALANCE_PRIORITY,
        &s_task,
        0  /* Core 0 */
    );

    s_enabled = true;
    ESP_LOGI(TAG, "Balance controller started (%dHz)", BALANCE_RATE_HZ);
    return ESP_OK;
}

esp_err_t balance_controller_stop(void)
{
    if (s_task == NULL) return ESP_OK;
    s_enabled = false;
    vTaskDelete(s_task);
    s_task = NULL;
    memset(s_offsets, 0, sizeof(s_offsets));
    return ESP_OK;
}

esp_err_t balance_set_enabled(bool enable)
{
    s_enabled = enable;
    if (!enable) {
        memset(s_offsets, 0, sizeof(s_offsets));
    }
    ESP_LOGI(TAG, "Balance correction %s", enable ? "enabled" : "disabled");
    return ESP_OK;
}

void balance_get_offsets(float *offsets_out)
{
    memcpy(offsets_out, s_offsets, sizeof(float) * BALANCE_NUM_SERVOS);
}

esp_err_t balance_set_config(const balance_config_t *config)
{
    if (!config) return ESP_ERR_INVALID_ARG;
    s_config = *config;
    ESP_LOGI(TAG, "Balance config updated (P=%.2f, D=%.3f)", s_config.p_gain, s_config.d_gain);
    return ESP_OK;
}

bool balance_is_enabled(void)
{
    return s_enabled;
}
