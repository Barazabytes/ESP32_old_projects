#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_err.h"

// ADC Configuration
#define ADC_WIDTH       ADC_WIDTH_BIT_12
#define ADC_POT_CHANNEL ADC1_CHANNEL_0 
#define ADC_ATTENUATION ADC_ATTEN_DB_12
#define ADC_LM_CHANNEL  ADC1_CHANNEL_6
#define WIDTH_VAL       4095

// LEDC Configuration
#define RESOLN_VALUE    8191
#define LEDC_TIMER      LEDC_TIMER_0
#define LEDC_SPEED      LEDC_HIGH_SPEED_MODE
#define DATA_COUNT      80
#define LEDC_CHANNEL    LEDC_CHANNEL_0
#define CHANNEL_GPIO    23

// LED config Constants
#define COLD_LED        2
#define HOT_LED         4
#define BUZZER_PIN      23
#define MEDIUM_TEMP     17

void fLedConfig();
void fPot_adcConfig();
void fLm35_adcConfig();
void fLedcTimerConfig();
void fLedcChannelConfig();
int  fToCelcius();
int  fToPitchVal(int iRaw);
void fBlinkLEDs(int fCheckTemp);



void app_main(void) {
    int iRaw  = 0;
    int iTemp = 0;
    int iDuty = 0;
    
    fLedConfig();
    fPot_adcConfig();
    fLm35_adcConfig();
    fLedcTimerConfig();
    fLedcChannelConfig();
    
    while(1) {
        iTemp = fToCelcius();
        printf("Temp : %d\n", iTemp);
        fBlinkLEDs(iTemp);
        
        iRaw  = adc1_get_raw(ADC_POT_CHANNEL);
        iDuty = fToPitchVal(iRaw);
        
        ledc_set_duty(LEDC_SPEED, LEDC_CHANNEL, iDuty);
        ledc_update_duty(LEDC_SPEED, LEDC_CHANNEL);
        vTaskDelay(pdMS_TO_TICKS(800));
    }
    
}





void fLedConfig() {
    gpio_reset_pin(HOT_LED);
    gpio_reset_pin(COLD_LED);
    
    int pin_mask = (1ULL << HOT_LED) | (1ULL << COLD_LED);
    
    gpio_config_t io_conf = {
        .pin_bit_mask   = pin_mask,
        .mode           = GPIO_MODE_OUTPUT,
        .pull_up_en     = GPIO_PULLUP_DISABLE,
        .pull_down_en   = GPIO_PULLDOWN_DISABLE,
        .intr_type      = GPIO_INTR_DISABLE
    };
    
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_LOGI("LED", "configuration was Successiful");
}


void fBlinkLEDs(int fCheckTemp) {
    
    if(fCheckTemp > MEDIUM_TEMP) {
        gpio_set_level(COLD_LED,   0);
        gpio_set_level(HOT_LED,    1);
        gpio_set_level(BUZZER_PIN, 1);  // Buzzer
     
    } else if(fCheckTemp < MEDIUM_TEMP) {
        gpio_set_level(COLD_LED,   1);
        gpio_set_level(HOT_LED,    0);
        gpio_set_level(BUZZER_PIN, 0);  // Buzzer
     
    }

}


void fPot_adcConfig() {
    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH));
    ESP_ERROR_CHECK(adc1_config_channel_atten(ADC_LM_CHANNEL, ADC_ATTENUATION));
    ESP_LOGI("ADC_POT", "CONFIGURATION WAS SUCCESSIFUL");
}

void fLm35_adcConfig() {
    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH));
    ESP_ERROR_CHECK(adc1_config_channel_atten(ADC_LM_CHANNEL, ADC_ATTENUATION));
    ESP_LOGI("ADC_LM35", "CONFIGURATION WAS SUCCESSIFUL");
}

void fLedcTimerConfig() {
    ledc_timer_config_t timer_conf = {
        .speed_mode       = LEDC_SPEED,
        .duty_resolution  = LEDC_TIMER_13_BIT,
        .freq_hz          = 2000,            // for a Buzzer
        .timer_num        = LEDC_TIMER,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    
    ESP_ERROR_CHECK(ledc_timer_config(&timer_conf));
    ESP_LOGI("TIMER_BUZ", "CONFIGURATION WAS SUCCESSIFUL");
}


void fLedcChannelConfig() {
    ledc_channel_config_t channel_conf = {
        .channel    = LEDC_CHANNEL,
        .gpio_num   = BUZZER_PIN,
        .intr_type  = LEDC_INTR_DISABLE,
        .duty       = 0,
        .hpoint     = 0,  
        .timer_sel  = LEDC_TIMER,
        .speed_mode = LEDC_SPEED
    };
    
    ESP_ERROR_CHECK(ledc_channel_config(&channel_conf));
    ESP_LOGI("CHANNEL_BUZ", "CONFIGURATION WAS SUCCESSIFUL");
}

int  fToCelcius() {
    int iRaw     = 0;
    int iVoltage = 0;
    
    for(int i = 0; i < DATA_COUNT; i++) {
        iRaw += adc1_get_raw(ADC_LM_CHANNEL);
    }
    
    // Data Average
    iRaw /= DATA_COUNT;
    
    // Temperature Deg Calculation
    iVoltage = (iRaw * 3300)/WIDTH_VAL;
    
    return iVoltage/10;
}


int  fToPitchVal(int iRaw) {
    return (iRaw * RESOLN_VALUE) / WIDTH_VAL;
}


