#include <stdio.h>
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "driver/ledc.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

// Custom Headerfile
#include "pin_config.h"

#define NECK_RANGE 30

// Ultra-Sonic sensor Constants
#define TRIG_PIN   18
#define ECHO_PIN   19
#define SOUND_SPD  0.0343


// Servo-Motor constants
#define MIN_PULSE  1000
#define MAX_PULSE  2000
#define SERVO_PIN  22
#define SERVO_FRQ  50
#define SERVO_MAX  20000
#define MAX_RESOLN 65535
#define MAX_ANGLE  90

// Buzzer constants
#define BUZZ_PIN   4


// Tasks and Queue Handle
TaskHandle_t  thGetDistance = NULL;
TaskHandle_t  thSpinServo   = NULL;
TaskHandle_t  thTrigBuzzer  = NULL;
TaskHandle_t  thMasterTask  = NULL;

QueueHandle_t qhToMaster    = NULL;

// Calculators
int iDistance   (float iDuration);
int iAngleToDuty(int iAngle);



// Tasks Prototypes
void vGetDistance(void *pArg);
void vSpinServo  (void *pArg);
void vTrigBuzzer (void *pArg);
void vMasterTask (void *pArg);



void app_main(void) {
	vPinConfig(TRIG_PIN, ECHO_PIN, SERVO_PIN, BUZZ_PIN);
	vPWMconfig(SERVO_FRQ, SERVO_PIN);

  // Queue Creation
	qhToMaster = xQueueCreate(2, sizeof(int));

	xTaskCreatePinnedToCore(vMasterTask, "Master Task", 2048, NULL, 10, &thMasterTask, 1);
	xTaskCreatePinnedToCore(vGetDistance, "Get Distance", 2048, NULL, 10, &thGetDistance, 1);
	xTaskCreatePinnedToCore(vSpinServo, "Spin Servo-motor", 4096, NULL, 10, &thSpinServo, 1);
	xTaskCreatePinnedToCore(vTrigBuzzer, "Trigger Buzzer", 2048, NULL, 10, &thTrigBuzzer, 1);

}


// Calculators
int iDistance(float iDuration) {
	float iDistance = (iDuration * SOUND_SPD)/2;

	return (int)iDistance;
}

int iAngleToDuty(int iAngle) {
    if(iAngle < 0) iAngle = 0;
    if(iAngle > 90) iAngle = 90;

	int iPulse = MIN_PULSE + ((MAX_PULSE - MIN_PULSE) * iAngle)/ MAX_ANGLE;
	int iDuty  = (iPulse * MAX_RESOLN) / SERVO_MAX;

	return iDuty;
}


// Tasks Implementation
void vGetDistance(void *pArg) {
  // Declaration of some useful Variables
	int iStartTime = 0;
	int iEchoStart = 0;
	int iEchoEnd   = 0;
	int iDuration  = 0;
	int iRange     = 0;

	while(1) {
	  // Trigger Radiations
		ESP_ERROR_CHECK(gpio_set_level(TRIG_PIN, 1));
		esp_rom_delay_us(10);
		ESP_ERROR_CHECK(gpio_set_level(TRIG_PIN, 0));

	  // Start getting readings
		iStartTime = esp_timer_get_time();
		while((gpio_get_level(ECHO_PIN) == 0)) {
			if((esp_timer_get_time() - iStartTime) > 100000) {
				ESP_LOGW("ECHOLOW", "Time out");
				break;
			}
		}

		iEchoStart = esp_timer_get_time();
		while((gpio_get_level(ECHO_PIN) == 1)) {
			if(((esp_timer_get_time() - iEchoStart))  > 300000) {
				ESP_LOGW("ECHOHIGH", "Time out");
				break;
			}
		}
	
		iEchoEnd  = esp_timer_get_time();
		iDuration = iEchoEnd - iEchoStart;
		iRange    = iDistance((float)iDuration);

		ESP_LOGI("USS", "Distance: %d", iRange);
		vTaskDelay(pdMS_TO_TICKS(10));

		xQueueSend(qhToMaster, &iRange, pdMS_TO_TICKS(2));

	}	

}

void vSpinServo  (void *pArg) {
	int iDuty     = 0;

	while(1) {

	// Normal TO and FRO movements
		for(int angle = 0; angle <= 90; angle += 2) {
			iDuty = iAngleToDuty(angle); 
			
	// 	ESP_LOGW("ANGLE", "value: %d", iDuty);
			ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, iDuty);
			ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
			vTaskDelay(pdMS_TO_TICKS(10));
		}

		for(int angle = 90; angle >= 0; angle -= 2) {
			iDuty = iAngleToDuty(angle); 

	//	ESP_LOGW("ANGLE", "value: %d", iDuty);
			ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, iDuty);
			ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
			vTaskDelay(pdMS_TO_TICKS(10));
		}		


	}

}


void vTrigBuzzer (void *pArg) {
	vTaskSuspend(NULL);
	while(1) {
		gpio_set_level(BUZZ_PIN, 1);
		vTaskDelay(pdMS_TO_TICKS(100));
		gpio_set_level(BUZZ_PIN, 0);
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}


void vMasterTask (void *pArg) {
	int iData = 0;

	while(1) {
		if(xQueueReceive(qhToMaster, &iData, pdMS_TO_TICKS(2))) {
			if(iData < NECK_RANGE) {
				ESP_LOGW("MASTER", "Intruder Alert");
				vTaskSuspend(thSpinServo);
				vTaskResume(thTrigBuzzer);
				vTaskDelay(pdMS_TO_TICKS(10));
			}

			if(iData > NECK_RANGE) {
				if(eTaskGetState(thSpinServo) == eSuspended) { 
					vTaskResume(thSpinServo);
					
					gpio_set_level(BUZZ_PIN, 0);  // Shut The Buzzer before suspending it's Task	
					vTaskSuspend(thTrigBuzzer);
				}				
			}
		}
	}
}



























