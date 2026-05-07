#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"

// ---------------- MOTOR CONFIGURATION ----------------

// L298N constants (adjust if wiring is different)
#define LEFT_IN1   25   // Left motor input 1 (+)
#define LEFT_IN2   26   // Left motor input 2 (-)
#define LEFT_ENA   27   // Left motor enable (PWM)

#define RIGHT_IN3  32   // Right motor input 3 (+)
#define RIGHT_IN4  33   // Right motor input 4 (-)
#define RIGHT_ENB  14   // Right motor enable (PWM)

// PWM config
#define LEDC_MODE       LEDC_HIGH_SPEED_MODE
#define LEDC_TIMER      LEDC_TIMER_0
#define LEFT_CHANNEL    LEDC_CHANNEL_0
#define RIGHT_CHANNEL   LEDC_CHANNEL_1
#define LEDC_FREQUENCY  1000                     // 1 KHz PWM frequency
#define LEDC_RESOLUTION LEDC_TIMER_8_BIT         // 8-bit resolution (0–255 duty cycle)

// ---------------- WIFI CONFIGURATION ----------------
#define WIFI_SSID "ESP32-Car"   // SSID of your car Wi-Fi
#define WIFI_PASS "12345678"    // Password (min. 8 chars)

static const char *TAG = "CarController";

// ---------------- MOTOR FUNCTIONS ----------------
void vPinConfig(int iPIN);
void vPWMconfig();

// Movement functions
void vMoveForward();
void vMoveBackward();
void vMoveLeft();
void vMoveRight();
void vStopCar();

// ---------------- HTML PAGE ----------------
const char *HTML_PAGE =
"<!DOCTYPE html>"
"<html>"
"<head><title>ESP32 Car</title></head>"
"<body style='text-align:center; font-family: Arial;'>"
"<h2> RC CAR COTROLLER</h2>"
"<button onclick=\"location.href='/forward'\">Forward</button><br><br>"
"<button onclick=\"location.href='/left'\">Left</button>"
"<button onclick=\"location.href='/stop'\">Stop</button>"
"<button onclick=\"location.href='/right'\">Right</button><br><br>"
"<button onclick=\"location.href='/backward'\">Backward</button>"
"</body></html>";

// ---------------- HTTP SERVER HANDLERS ----------------

// Root page (UI)
esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_send(req, HTML_PAGE, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Motor control endpoints
esp_err_t forward_handler(httpd_req_t *req) {
    vMoveForward();
    return ESP_OK;
}
esp_err_t backward_handler(httpd_req_t *req) {
    vMoveBackward();
    return ESP_OK;
}
esp_err_t left_handler(httpd_req_t *req) {
    vMoveLeft();
    return ESP_OK;
}
esp_err_t right_handler(httpd_req_t *req) {
    vMoveRight();
//    httpd_resp_sendstr(req, "Turning Right");
    return ESP_OK;
}
esp_err_t stop_handler(httpd_req_t *req) {
    vStopCar();
//    httpd_resp_sendstr(req, "Car Stopped");
    return ESP_OK;
}

// ---------------- HTTP SERVER SETUP ----------------
httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        // URI handlers
        httpd_uri_t root = { .uri = "/", .method = HTTP_GET, .handler = root_get_handler };
        httpd_uri_t forward = { .uri = "/forward", .method = HTTP_GET, .handler = forward_handler };
        httpd_uri_t backward = { .uri = "/backward", .method = HTTP_GET, .handler = backward_handler };
        httpd_uri_t left = { .uri = "/left", .method = HTTP_GET, .handler = left_handler };
        httpd_uri_t right = { .uri = "/right", .method = HTTP_GET, .handler = right_handler };
        httpd_uri_t stop = { .uri = "/stop", .method = HTTP_GET, .handler = stop_handler };

        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &forward);
        httpd_register_uri_handler(server, &backward);
        httpd_register_uri_handler(server, &left);
        httpd_register_uri_handler(server, &right);
        httpd_register_uri_handler(server, &stop);
    }
    return server;
}

// ---------------- WIFI INIT (SOFTAP) ----------------
void wifi_init_softap(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t ap_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = 1,
            .password = WIFI_PASS,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    if (strlen(WIFI_PASS) == 0) {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi started. SSID:%s password:%s", WIFI_SSID, WIFI_PASS);
}

// ---------------- MOTOR SETUP ----------------
void vPinConfig(int iPIN) {
    gpio_config_t pin_config = {
        .pin_bit_mask   = (1ULL << iPIN),
        .mode           = GPIO_MODE_OUTPUT,
        .pull_down_en   = GPIO_PULLDOWN_DISABLE,
        .pull_up_en     = GPIO_PULLUP_DISABLE,
        .intr_type      = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&pin_config));
}

void vPWMconfig() {
    // Timer setup
    ledc_timer_config_t timer_config = {
        .timer_num       = LEDC_TIMER_0,
        .clk_cfg         = LEDC_AUTO_CLK,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz         = 1000,
        .speed_mode      = LEDC_HIGH_SPEED_MODE,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_config));

    // Left motor PWM
    ledc_channel_config_t chan_left = {
        .channel    = LEFT_CHANNEL,
        .duty       = 200,
        .gpio_num   = LEFT_ENA,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER_0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&chan_left));

    // Right motor PWM
    ledc_channel_config_t chan_right = {
        .channel    = RIGHT_CHANNEL,
        .duty       = 200,
        .gpio_num   = RIGHT_ENB,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER_0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&chan_right));
}

// ---------------- CAR MOVEMENT ----------------
void vMoveForward() {
    gpio_set_level(LEFT_IN1, 1);
    gpio_set_level(LEFT_IN2, 0);
    gpio_set_level(RIGHT_IN3, 1);
    gpio_set_level(RIGHT_IN4, 0);
}
void vMoveBackward() {
    gpio_set_level(LEFT_IN1, 0);
    gpio_set_level(LEFT_IN2, 1);
    gpio_set_level(RIGHT_IN3, 0);
    gpio_set_level(RIGHT_IN4, 1);
}
void vMoveRight() {
    gpio_set_level(LEFT_IN1, 1);
    gpio_set_level(LEFT_IN2, 0);
    gpio_set_level(RIGHT_IN3, 0);
    gpio_set_level(RIGHT_IN4, 1);
}
void vMoveLeft() {
    gpio_set_level(LEFT_IN1, 0);
    gpio_set_level(LEFT_IN2, 1);
    gpio_set_level(RIGHT_IN3, 1);
    gpio_set_level(RIGHT_IN4, 0);
}
void vStopCar() {
    gpio_set_level(LEFT_IN1, 0);
    gpio_set_level(LEFT_IN2, 0);
    gpio_set_level(RIGHT_IN3, 0);
    gpio_set_level(RIGHT_IN4, 0);
}

// ---------------- APP MAIN ----------------
void app_main(void) {
    // Init NVS (for Wi-Fi driver)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Init motor GPIOs + PWM
    vPWMconfig();
    vPinConfig(LEFT_IN1);
    vPinConfig(LEFT_IN2);
    vPinConfig(RIGHT_IN3);
    vPinConfig(RIGHT_IN4);

    // Start Wi-Fi in AP mode
    wifi_init_softap();

    // Start web server
    start_webserver();
}
