#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_psram.h"

#include "esp_camera.h"
#include "camera_httpd.h"

#define CAM_PIN_PWDN    -1
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    10
#define CAM_PIN_SIOD    40
#define CAM_PIN_SIOC    39

#define CAM_PIN_D7      9
#define CAM_PIN_D6      11
#define CAM_PIN_D5      12
#define CAM_PIN_D4      14
#define CAM_PIN_D3      16
#define CAM_PIN_D2      18
#define CAM_PIN_D1      17
#define CAM_PIN_D0      15

#define CAM_PIN_VSYNC   38
#define CAM_PIN_HREF    47
#define CAM_PIN_PCLK    13

static const char *TAG = "camera_app";

// #define WIFI_SSID "Pree" // To be added
// #define WIFI_PASS "preepree"

// #define WIFI_SSID "BUMRC Members"
// #define WIFI_PASS ""

#define WIFI_SSID "Group_2/3"
#define WIFI_PASS "smartsys"

// Event handler
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Disconnected. Reconnecting...");
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

// Initialize WiFi
static void wifi_init_sta(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.static_tx_buf_num = 8;
    cfg.dynamic_tx_buf_num = 16;
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL) );
    ESP_ERROR_CHECK( esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL) );

    wifi_config_t wifi_cfg = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            //.threshold.authmode = WIFI_AUTH_WPA2_PSK,
            //.threshold.authmode = WIFI_AUTH_OPEN  // No password
        },
    };
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_cfg) );
    ESP_ERROR_CHECK( esp_wifi_start() );

    ESP_LOGI(TAG, "WiFi initialization complete.");
}

void app_main(void)
{
    ESP_LOGI(TAG, "Free PSRAM: %d, Largest block: %d", 
        heap_caps_get_free_size(MALLOC_CAP_SPIRAM), heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
    // Configure camera
    static camera_config_t config = {
        .pin_pwdn       = CAM_PIN_PWDN,
        .pin_reset      = CAM_PIN_RESET,
        .pin_xclk       = CAM_PIN_XCLK,
        .pin_sccb_sda   = CAM_PIN_SIOD,
        .pin_sccb_scl   = CAM_PIN_SIOC,
        .pin_d7         = CAM_PIN_D7,
        .pin_d6         = CAM_PIN_D6,
        .pin_d5         = CAM_PIN_D5,
        .pin_d4         = CAM_PIN_D4,
        .pin_d3         = CAM_PIN_D3,
        .pin_d2         = CAM_PIN_D2,
        .pin_d1         = CAM_PIN_D1,
        .pin_d0         = CAM_PIN_D0,
        .pin_vsync      = CAM_PIN_VSYNC,
        .pin_href       = CAM_PIN_HREF,
        .pin_pclk       = CAM_PIN_PCLK,

        .xclk_freq_hz = 20000000,//EXPERIMENTAL: Set to 16MHz on ESP32-S2 or ESP32-S3 to enable EDMA mode
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
    
        .pixel_format = PIXFORMAT_JPEG,//YUV422,GRAYSCALE,RGB565,JPEG
        .frame_size = FRAMESIZE_QVGA,
    
        .jpeg_quality = 12, //0-63, for OV series camera sensors, lower number means higher quality
        .fb_count = 1, //When jpeg mode is used, if fb_count more than one, the driver will work in continuous mode.
        .grab_mode      = CAMERA_GRAB_WHEN_EMPTY,
        //.fb_location    = CAMERA_FB_IN_PSRAM
        .fb_location = CAMERA_FB_IN_DRAM
    };

    // init camera
    esp_err_t ret = esp_camera_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed (%x)", ret);
        return;
    }

    // NVS flash 
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // init wifi after camera to give more memory PSRAM to camera
    wifi_init_sta();

    sensor_t *s = esp_camera_sensor_get();
    if (s->id.PID == OV3660_PID) {
        s->set_vflip(s, 1);
        s->set_brightness(s, 1);
        s->set_saturation(s, -2);
    }
    if (config.pixel_format == PIXFORMAT_JPEG) {
        s->set_framesize(s, FRAMESIZE_QVGA);
    }

    #if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
      s->set_vflip(s, 1);
      s->set_hmirror(s, 1);
    #endif

    #if defined(CAMERA_MODEL_ESP32S3_EYE)
      s->set_vflip(s, 1);
    #endif

    // // 6) optional LED flash setup
    // #if defined(LED_GPIO_NUM)
    //   gpio_pad_select_gpio(LED_GPIO_NUM);
    //   gpio_set_direction(LED_GPIO_NUM, GPIO_MODE_OUTPUT);
    // #endif

    // start HTTP camera server task
    xTaskCreate(capture_and_send_task, "capture_send", 4096, NULL, 5, NULL);

    //start_camera_server();
    // Delay a bit to ensure Wi-Fi connection is ready
    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(TAG, "Camera Ready!");

    vTaskDelete(NULL);
}