/**
 * Battery Monitor — Implementation
 *
 * Reads battery voltage via ADC every 10 seconds.
 * Applies a simple moving average (4 samples) to smooth noise.
 * Maps voltage to percentage using a linear approximation
 * between cutoff and full charge (acceptable for LiPo/Li-ion).
 */

#include "battery_monitor.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "battery";

#define SAMPLE_INTERVAL_MS   10000  /* 10 seconds */
#define MOVING_AVG_SIZE      4
#define DIVIDER_RATIO_1S     2.0f   /* 100K/100K divider */
#define DIVIDER_RATIO_2S     4.03f  /* 100K/33K divider */

static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static adc_cali_handle_t s_cali_handle = NULL;
static int s_adc_channel = -1;
static TaskHandle_t s_monitor_task = NULL;
static battery_info_t s_info = {0};
static battery_state_callback_t s_state_cb = NULL;
static battery_state_t s_prev_state = BATTERY_OK;

/* Moving average buffer */
static uint32_t s_voltage_buf[MOVING_AVG_SIZE] = {0};
static int s_buf_idx = 0;
static int s_buf_count = 0;

static float get_divider_ratio(void)
{
#if KOSKALAK_BATTERY_CELLS == 1
    return DIVIDER_RATIO_1S;
#else
    return DIVIDER_RATIO_2S;
#endif
}

static uint8_t voltage_to_percentage(uint32_t cell_mv)
{
    if (cell_mv >= CELL_VOLTAGE_FULL_MV) return 100;
    if (cell_mv <= CELL_VOLTAGE_CUTOFF_MV) return 0;

    uint32_t range = CELL_VOLTAGE_FULL_MV - CELL_VOLTAGE_CUTOFF_MV;
    uint32_t offset = cell_mv - CELL_VOLTAGE_CUTOFF_MV;
    return (uint8_t)((offset * 100) / range);
}

static battery_state_t classify_state(uint32_t cell_mv)
{
    if (cell_mv <= CELL_VOLTAGE_CRITICAL_MV) return BATTERY_CRITICAL;
    if (cell_mv <= CELL_VOLTAGE_LOW_MV) return BATTERY_LOW;
    return BATTERY_OK;
}

static uint32_t read_voltage_mv(void)
{
    int raw = 0;
    int voltage_mv = 0;

    ESP_ERROR_CHECK(adc_oneshot_read(s_adc_handle, s_adc_channel, &raw));

    if (s_cali_handle) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(s_cali_handle, raw, &voltage_mv));
    } else {
        /* Fallback: rough conversion without calibration (12-bit, ~3100mV range) */
        voltage_mv = (raw * 3100) / 4095;
    }

    /* Scale up by divider ratio to get actual battery voltage */
    return (uint32_t)(voltage_mv * get_divider_ratio());
}

static void update_info(uint32_t pack_mv)
{
    /* Moving average */
    s_voltage_buf[s_buf_idx] = pack_mv;
    s_buf_idx = (s_buf_idx + 1) % MOVING_AVG_SIZE;
    if (s_buf_count < MOVING_AVG_SIZE) s_buf_count++;

    uint32_t sum = 0;
    for (int i = 0; i < s_buf_count; i++) sum += s_voltage_buf[i];
    uint32_t avg_mv = sum / s_buf_count;

    uint32_t cell_mv = avg_mv / KOSKALAK_BATTERY_CELLS;

    s_info.voltage_mv = avg_mv;
    s_info.cell_voltage_mv = cell_mv;
    s_info.percentage = voltage_to_percentage(cell_mv);
    s_info.state = classify_state(cell_mv);
    /* TODO: Detect charging via USB VBUS GPIO */
    s_info.is_charging = false;

    /* Fire callback on state transitions */
    if (s_info.state != s_prev_state && s_state_cb) {
        s_state_cb(s_info.state, s_info.percentage);
        s_prev_state = s_info.state;
    }
}

static void monitor_task(void *arg)
{
    for (;;) {
        uint32_t mv = read_voltage_mv();
        update_info(mv);

        ESP_LOGD(TAG, "Battery: %lumV (cell: %lumV, %u%%, state: %d)",
                 s_info.voltage_mv, s_info.cell_voltage_mv,
                 s_info.percentage, s_info.state);

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_INTERVAL_MS));
    }
}

esp_err_t battery_monitor_init(int adc_gpio)
{
    /* Map GPIO to ADC channel — ESP32-S3 ADC1 channels */
    /* TODO: Proper GPIO-to-channel mapping via adc_oneshot_io_to_channel() */

    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &s_adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,  /* ~0-3100mV range */
        .bitwidth = ADC_BITWIDTH_12,
    };

    /* TODO: Convert gpio to channel properly */
    s_adc_channel = 0; /* Placeholder — use adc_oneshot_io_to_channel() */
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, s_adc_channel, &chan_cfg));

    /* Try to initialise calibration (curve fitting or line fitting) */
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .chan = s_adc_channel,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ADC calibration not available, using raw conversion");
        s_cali_handle = NULL;
    }

    ESP_LOGI(TAG, "Battery monitor initialised (%dS config, GPIO %d)",
             KOSKALAK_BATTERY_CELLS, adc_gpio);
    return ESP_OK;
}

esp_err_t battery_monitor_start(void)
{
    if (s_monitor_task != NULL) return ESP_ERR_INVALID_STATE;

    xTaskCreatePinnedToCore(
        monitor_task,
        "battery",
        2048,
        NULL,
        1,   /* Lowest priority */
        &s_monitor_task,
        0
    );

    ESP_LOGI(TAG, "Battery monitoring started (every %ds)", SAMPLE_INTERVAL_MS / 1000);
    return ESP_OK;
}

esp_err_t battery_monitor_stop(void)
{
    if (s_monitor_task == NULL) return ESP_OK;
    vTaskDelete(s_monitor_task);
    s_monitor_task = NULL;
    return ESP_OK;
}

battery_info_t battery_get_info(void)
{
    return s_info;
}

void battery_set_state_callback(battery_state_callback_t callback)
{
    s_state_cb = callback;
}

esp_err_t battery_read_now(battery_info_t *out)
{
    uint32_t mv = read_voltage_mv();
    update_info(mv);
    if (out) *out = s_info;
    return ESP_OK;
}
