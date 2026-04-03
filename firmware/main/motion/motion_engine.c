/**
 * Motion Engine — Implementation
 *
 * Reads gait oscillator parameters and balance offsets, computes final
 * servo positions, and writes to PCA9685 via I2C at ~50Hz.
 *
 * Balance offsets come from the standalone balance_controller task.
 * The motion engine does NOT own the balance logic — it just applies
 * the correction offsets to each servo's oscillation centre.
 */

#include "motion_engine.h"
#include "balance_controller.h"
#include "imu_driver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "motion";

static gait_id_t s_current_gait = GAIT_NONE;
static float s_speed_multiplier = 1.0f;
static TaskHandle_t s_motion_task = NULL;

/* PCA9685 I2C address (default) */
#define PCA9685_I2C_ADDR 0x40

#if KOSKALAK_DOF == 8
/* 8-DOF servo channel mapping: 4 legs x 2 servos */
#define SERVO_FL_HIP  0
#define SERVO_FL_KNEE 1
#define SERVO_FR_HIP  2
#define SERVO_FR_KNEE 3
#define SERVO_BL_HIP  4
#define SERVO_BL_KNEE 5
#define SERVO_BR_HIP  6
#define SERVO_BR_KNEE 7
#elif KOSKALAK_DOF == 4
/* 4-DOF servo channel mapping: 1 actuator per leg */
#define SERVO_FL 0
#define SERVO_FR 1
#define SERVO_BL 2
#define SERVO_BR 3
#else
#error "KOSKALAK_DOF must be 4 or 8"
#endif

static void motion_task(void *arg)
{
    float balance_offsets[KOSKALAK_DOF] = {0};

    for (;;) {
        /* If device is being held, suppress all motion */
        if (imu_is_held()) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        /* Read balance correction offsets from standalone controller */
        balance_get_offsets(balance_offsets);

        if (s_current_gait == GAIT_NONE) {
            /* No gait active — still apply balance offsets to neutral pose */
            /* TODO: Write (neutral_position + balance_offsets) to PCA9685 */
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        /* TODO: Compute servo positions from oscillator parameters:
         *
         * For each servo i:
         *   gait_params = get_gait_params(s_current_gait)
         *   base_pos = gait_params.offset[i]
         *              + gait_params.amplitude[i] * sin(2*PI*t / (gait_params.period * s_speed_multiplier) + gait_params.phase[i])
         *   final_pos = base_pos + balance_offsets[i]
         *   clamp final_pos to [0, 180]
         *   write final_pos to PCA9685 channel i
         */

        vTaskDelay(pdMS_TO_TICKS(20)); /* ~50Hz servo update rate */
    }
}

esp_err_t motion_engine_init(void)
{
    /* TODO: Init I2C master for PCA9685 + MPU-6050 (shared bus)
     * TODO: Configure PCA9685 PWM frequency (~50Hz for servos)
     * TODO: Move all servos to neutral
     */

    xTaskCreatePinnedToCore(
        motion_task,
        "motion",
        4096,
        NULL,
        4,
        &s_motion_task,
        0  /* Core 0 */
    );

    ESP_LOGI(TAG, "Motion engine initialised (%d-DOF)", KOSKALAK_DOF);
    return ESP_OK;
}

esp_err_t motion_set_gait(gait_id_t gait)
{
    ESP_LOGI(TAG, "Gait: %d → %d", s_current_gait, gait);
    s_current_gait = gait;
    return ESP_OK;
}

gait_id_t motion_get_gait(void)
{
    return s_current_gait;
}

esp_err_t motion_set_speed(float multiplier)
{
    s_speed_multiplier = multiplier;
    return ESP_OK;
}

esp_err_t motion_stand_neutral(void)
{
    s_current_gait = GAIT_NONE;
    /* TODO: Write neutral + balance offsets to all servo channels */
    return ESP_OK;
}

esp_err_t motion_crouch(void)
{
    s_current_gait = GAIT_NONE;
    /* TODO: Write crouch positions to all servo channels */
    return ESP_OK;
}

int motion_get_servo_count(void)
{
    return KOSKALAK_DOF;
}

#ifdef KOSKALAK_SERVO_FEEDBACK
esp_err_t motion_read_servo_position(int servo_index, float *angle_out)
{
    if (servo_index < 0 || servo_index >= KOSKALAK_DOF || !angle_out) {
        return ESP_ERR_INVALID_ARG;
    }
    /* TODO: Read ADC channel mapped to servo's feedback wire
     * Convert ADC reading to angle (calibrated per-servo) */
    *angle_out = 90.0f; /* Stub */
    return ESP_OK;
}
#endif
