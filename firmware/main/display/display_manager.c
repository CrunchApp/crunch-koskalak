/**
 * Display Manager — Implementation stub
 *
 * Full implementation requires LVGL component and ST7789 SPI driver.
 */

#include "display_manager.h"
#include "esp_log.h"

static const char *TAG = "display";

static screen_id_t s_current_screen = SCREEN_BOOT;

esp_err_t display_manager_init(void)
{
    /* TODO: Init SPI bus for ST7789 (240x320)
     * TODO: Init LVGL, register display driver
     * TODO: Create screen objects for each screen_id_t
     * TODO: Start LVGL tick timer (5ms) and refresh task (30ms)
     */
    ESP_LOGI(TAG, "Display manager initialised (stub)");
    return ESP_OK;
}

esp_err_t display_show_screen(screen_id_t screen)
{
    ESP_LOGI(TAG, "Screen transition: %d → %d", s_current_screen, screen);
    s_current_screen = screen;
    /* TODO: Load LVGL screen object, trigger animation */
    return ESP_OK;
}

esp_err_t display_set_transcript(const char *text)
{
    /* TODO: Update LVGL label on SPEAKING screen */
    return ESP_OK;
}

esp_err_t display_set_order_summary(const char *order_json)
{
    /* TODO: Parse JSON, update LVGL list on ORDER_SUMMARY screen */
    return ESP_OK;
}

esp_err_t display_set_error(const char *message)
{
    /* TODO: Update LVGL label on ERROR screen */
    ESP_LOGE(TAG, "Error screen: %s", message);
    return ESP_OK;
}

esp_err_t display_set_idle_info(const char *location_name, const char *time_str)
{
    /* TODO: Update LVGL labels on IDLE screen */
    return ESP_OK;
}
