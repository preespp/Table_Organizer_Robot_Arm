/*  main.c — ESP32-S3 Sense Wi-Fi MJPEG streamer  (no-PSRAM build)  */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "driver/gpio.h"

/* ─────────── Wi-Fi credentials ─────────── */
#define WIFI_SSID   "Group_2"
#define WIFI_PASS   "smartsys"
#define MAX_RETRY   5

static const char *TAG = "CAM_WIFI";
static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT  BIT0
static int s_retry_num = 0;

/* ─────────── Camera pin map (XIAO ESP32S3 Sense) ─────────── */
#define CAM_PIN_PWDN    -1
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    10
#define CAM_PIN_SIOD    40
#define CAM_PIN_SIOC    39
#define CAM_PIN_D7      48
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

/* ─────────── Camera configuration (no-PSRAM) ─────────── */
static camera_config_t camera_config = {
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
    .xclk_freq_hz   = 20000000,
    .ledc_timer     = LEDC_TIMER_0,
    .ledc_channel   = LEDC_CHANNEL_0,
    .pixel_format   = PIXFORMAT_JPEG,
    .frame_size     = FRAMESIZE_QVGA,      /* 320×240 → DRAM fits */
    .jpeg_quality   = 12,                  /* 0(最清)~63 */
    .fb_count       = 1,                   /* 单缓冲 */
    .grab_mode      = CAMERA_GRAB_WHEN_EMPTY,
    .fb_location    = CAMERA_FB_IN_DRAM   // 强制用片上内存
};

/* ─────────── Wi-Fi event handler ─────────── */
static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(TAG, "Retrying to connect… (%d)", s_retry_num);
        } else {
            xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
            ESP_LOGE(TAG, "Connect failed");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void)
{
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t any_id, got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, &any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, &got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {.capable = true, .required = false},
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));   /* 关闭省电模式 */
}

/* ─────────── Camera init ─────────── */
static esp_err_t camera_init(void)
{
    if (CAM_PIN_PWDN != -1) {
        gpio_config_t io = {
            .pin_bit_mask = 1ULL << CAM_PIN_PWDN,
            .mode = GPIO_MODE_OUTPUT,
        };
        gpio_config(&io);
        gpio_set_level(CAM_PIN_PWDN, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) ESP_LOGE(TAG, "camera init failed: %s", esp_err_to_name(err));
    return err;
}

/* ─────────── MJPEG HTTP stream ─────────── */
#define BOUNDARY "123456789000000000000987654321"
static const char *STREAM_CT = "multipart/x-mixed-replace;boundary=" BOUNDARY;
static const char *BOUNDARY_CRLF = "\r\n--" BOUNDARY "\r\n";
static const char *PART_HDR = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static esp_err_t stream_handler(httpd_req_t *req)
{
    camera_fb_t *fb = NULL;
    uint8_t *jpg_buf = NULL;
    size_t jpg_len = 0;
    char hdr_buf[64];
    httpd_resp_set_type(req, STREAM_CT);

    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            return ESP_FAIL;
        }
        if (fb->format != PIXFORMAT_JPEG) {
            bool ok = frame2jpg(fb, 80, &jpg_buf, &jpg_len);
            if (!ok) {
                esp_camera_fb_return(fb);
                ESP_LOGE(TAG, "JPEG encode failed");
                return ESP_FAIL;
            }
        } else {
            jpg_buf = fb->buf;
            jpg_len = fb->len;
        }
        if (httpd_resp_send_chunk(req, BOUNDARY_CRLF, strlen(BOUNDARY_CRLF)) != ESP_OK ||
            httpd_resp_send_chunk(req, hdr_buf,
                snprintf(hdr_buf, sizeof(hdr_buf), PART_HDR, jpg_len)) != ESP_OK ||
            httpd_resp_send_chunk(req, (const char *)jpg_buf, jpg_len) != ESP_OK) {
            esp_camera_fb_return(fb);
            if (fb->format != PIXFORMAT_JPEG) free(jpg_buf);
            ESP_LOGI(TAG, "Stream closed");
            return ESP_OK;
        }
        esp_camera_fb_return(fb);
        if (fb->format != PIXFORMAT_JPEG) free(jpg_buf);
    }
}

static httpd_handle_t start_webserver(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port  = 80;
    cfg.stack_size   = 4096;           /* 无 PSRAM 时压栈 */
    static const httpd_uri_t uri = {
        .uri = "/", .method = HTTP_GET,
        .handler = stream_handler, .user_ctx = NULL
    };
    httpd_handle_t srv = NULL;
    if (httpd_start(&srv, &cfg) == ESP_OK) httpd_register_uri_handler(srv, &uri);
    return srv;
}

/* ─────────── app_main ─────────── */
void app_main(void)
{
    /* 初始化 NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    /* 连接 Wi-Fi */
    wifi_init_sta();
    xEventGroupWaitBits(wifi_event_group,
                        WIFI_CONNECTED_BIT,
                        pdFALSE, pdTRUE, portMAX_DELAY);

    /* 初始化摄像头 */
    if (camera_init() != ESP_OK) {
        ESP_LOGE(TAG, "Rebooting in 2 s");
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    }

    /* 启动 HTTP 图传 */
    start_webserver();
}
