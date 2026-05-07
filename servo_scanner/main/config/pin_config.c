#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "driver/ledc.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"


// configurations
void vPinConfig(int TRG_PIN, int ECH_PIN, int SERV_PIN, int BUZ_PIN) {

    gpio_config_t io1_conf = {
        .pin_bit_mask  = (1ULL << TRG_PIN) | (1ULL << SERV_PIN) | (1ULL << BUZ_PIN),
        .mode          = GPIO_MODE_OUTPUT,
        .pull_up_en    = GPIO_PULLUP_DISABLE,
        .pull_down_en  = GPIO_PULLDOWN_DISABLE,
        .intr_type     = GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(gpio_config(&io1_conf));
    ESP_LOGI("OUTPUT", "Configured Successively");


    gpio_config_t io2_conf = {
        .pin_bit_mask  = (1ULL << ECH_PIN),
        .mode          = GPIO_MODE_INPUT,
        .pull_up_en    = GPIO_PULLUP_DISABLE,
        .pull_down_en  = GPIO_PULLDOWN_DISABLE,
        .intr_type     = GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(gpio_config(&io2_conf));
    ESP_LOGI("INPUT", "Configured Successively");

}

// Pin and PWM configurations
void vPWMconfig(int FREQ, int SERVO) {
    // Configure timer for LEDC (high-speed mode)
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_HIGH_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_16_BIT, // high resolution
        .freq_hz          = FREQ,         // 50Hz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
	ESP_LOGI("MOTORTIMER", "Configuration was successively");
    vTaskDelay(pdMS_TO_TICKS(500));

    // Configure channel
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_HIGH_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = SERVO,
        .duty           = 0, // initial duty
        .hpoint         = 0
    };
    
	ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
	ESP_LOGI("MOTORCHANNEL", "Configuration was successively");
    vTaskDelay(pdMS_TO_TICKS(500));

}
