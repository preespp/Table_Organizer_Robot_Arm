/*  main.c — ESP32-S3 auto-adaptive PSRAM / DRAM video streaming               *
 *  • If PSRAM is detected (≥ 4 MB) → use SVGA 800 × 600, double buffering,     *
 *    frame buffers in PSRAM                                                   *
 *  • If no PSRAM is detected → fall back to QVGA 320 × 240, single buffer,    *
 *    frame buffer in DRAM                                                     */

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
 #include "esp_psram.h"              /* ★ used for automatic PSRAM detection */
 
 #define WIFI_SSID  "Group_2/3"
 #define WIFI_PASS  "smartsys"
 #define MAX_RETRY  5
 
 static const char *TAG = "CAM_AUTO";
 static EventGroupHandle_t wifi_event_group;
 #define WIFI_CONNECTED_BIT  BIT0
 static int s_retry_num = 0;
 
 /* ───────── Camera GPIO mapping (XIAO ESP32-S3 Sense; edit here if needed) ───────── */
 #define CAM_PIN_PWDN   -1
 #define CAM_PIN_RESET  -1
 #define CAM_PIN_XCLK   10
 #define CAM_PIN_SIOD   40
 #define CAM_PIN_SIOC   39
 #define CAM_PIN_D7     48
 #define CAM_PIN_D6     11
 #define CAM_PIN_D5     12
 #define CAM_PIN_D4     14
 #define CAM_PIN_D3     16
 #define CAM_PIN_D2     18
 #define CAM_PIN_D1     17
 #define CAM_PIN_D0     15
 #define CAM_PIN_VSYNC  38
 #define CAM_PIN_HREF   47
 #define CAM_PIN_PCLK   13
 
 /* ───────── Camera base configuration ───────── */
 static camera_config_t cam = {
     .pin_pwdn   = CAM_PIN_PWDN,
     .pin_reset  = CAM_PIN_RESET,
     .pin_xclk   = CAM_PIN_XCLK,
     .pin_sccb_sda = CAM_PIN_SIOD,
     .pin_sccb_scl = CAM_PIN_SIOC,
     .pin_d7 = CAM_PIN_D7, .pin_d6 = CAM_PIN_D6, .pin_d5 = CAM_PIN_D5,
     .pin_d4 = CAM_PIN_D4, .pin_d3 = CAM_PIN_D3, .pin_d2 = CAM_PIN_D2,
     .pin_d1 = CAM_PIN_D1, .pin_d0 = CAM_PIN_D0,
     .pin_vsync = CAM_PIN_VSYNC, .pin_href = CAM_PIN_HREF, .pin_pclk = CAM_PIN_PCLK,
     .xclk_freq_hz = 20000000,
     .ledc_timer   = LEDC_TIMER_0,
     .ledc_channel = LEDC_CHANNEL_0,
     .pixel_format = PIXFORMAT_JPEG,
     /* The next three fields are overwritten at runtime
        depending on whether PSRAM is present */
     .frame_size   = FRAMESIZE_QVGA,
     .jpeg_quality = 12,
     .fb_count     = 1,
     .grab_mode    = CAMERA_GRAB_WHEN_EMPTY,
     .fb_location  = CAMERA_FB_IN_DRAM
 };
 
 /* ───────── Wi-Fi event handler ───────── */
 static void wifi_event_handler(void *arg, esp_event_base_t eb,
                                int32_t id, void *data)
 {
     if (eb == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
         esp_wifi_connect();
     } else if (eb == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
         if (s_retry_num < MAX_RETRY) {
             esp_wifi_connect(); s_retry_num++;
             ESP_LOGW(TAG, "Retry Wi-Fi… (%d)", s_retry_num);
         } else {
             xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
         }
     } else if (eb == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
         ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
         ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&event->ip_info.ip));
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
 
     wifi_config_t wc = { .sta = {
         .ssid = WIFI_SSID, .password = WIFI_PASS,
         .threshold.authmode = WIFI_AUTH_WPA2_PSK,
         .pmf_cfg = { .capable = true, .required = false } } };
     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
     ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
     ESP_ERROR_CHECK(esp_wifi_start());
     ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
 }
 
 /* ───────── Camera initialization (after dynamic configuration) ───────── */
 static esp_err_t camera_init(void)
 {
     esp_err_t err = esp_camera_init(&cam);
     if (err != ESP_OK) {
         ESP_LOGE(TAG, "Camera init failed: %s", esp_err_to_name(err));
     } else {
         ESP_LOGI(TAG, "Camera init OK: %s %d×%d, fb=%d, loc=%s",
                  cam.frame_size == FRAMESIZE_QVGA ? "QVGA" : "SVGA",
                  cam.frame_size == FRAMESIZE_QVGA ? 320 : 800,
                  cam.frame_size == FRAMESIZE_QVGA ? 240 : 600,
                  cam.fb_count,
                  cam.fb_location == CAMERA_FB_IN_PSRAM ? "PSRAM" : "DRAM");
     }
     return err;
 }
 
 /* ───────── MJPEG stream HTTP handler ───────── */
 #define BOUNDARY "123456789000000000000987654321"
 static const char *CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" BOUNDARY;
 static const char *BOUNDARY_CRLF = "\r\n--" BOUNDARY "\r\n";
 static const char *PART_HDR = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";
 
 static esp_err_t stream_handler(httpd_req_t *req)
 {
     camera_fb_t *fb; char head[64];
     httpd_resp_set_type(req, CONTENT_TYPE);
 
     while (true) {
         fb = esp_camera_fb_get();
         if (!fb) { ESP_LOGE(TAG, "Frame buffer NULL"); return ESP_FAIL; }
 
         if (httpd_resp_send_chunk(req, BOUNDARY_CRLF, strlen(BOUNDARY_CRLF)) != ESP_OK ||
             httpd_resp_send_chunk(req, head,
                  snprintf(head, sizeof(head), PART_HDR, fb->len)) != ESP_OK ||
             httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len) != ESP_OK) {
             esp_camera_fb_return(fb);
             return ESP_OK;        /* client disconnected */
         }
         esp_camera_fb_return(fb);
     }
 }
 
 static httpd_handle_t start_server(void)
 {
     httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
     cfg.stack_size = 8192;
     httpd_handle_t srv = NULL;
 
     if (httpd_start(&srv, &cfg) == ESP_OK) {
         static const httpd_uri_t root = { .uri="/", .method=HTTP_GET,
                                           .handler=stream_handler };
         static const httpd_uri_t stream = { .uri="/stream", .method=HTTP_GET,
                                             .handler=stream_handler };
         httpd_register_uri_handler(srv, &root);
         httpd_register_uri_handler(srv, &stream);
     }
     return srv;
 }
 
 /* ───────── app_main ───────── */
 void app_main(void)
 {
     /* Initialize NVS */
     esp_err_t ret = nvs_flash_init();
     if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
         ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
         ESP_ERROR_CHECK(nvs_flash_erase());
         ESP_ERROR_CHECK(nvs_flash_init());
     }
 
     /* Dynamically detect PSRAM */
     size_t psram = esp_psram_get_size();   /* 0 = none / initialization failed */
     if (psram >= 2*1024*1024) {            /* ≥ 2 MB → treat as having PSRAM */
         ESP_LOGI(TAG, "Detected PSRAM: %u bytes → use SVGA", psram);
         cam.frame_size  = FRAMESIZE_SVGA;
         cam.fb_count    = 2;
         cam.grab_mode   = CAMERA_GRAB_LATEST;
         cam.fb_location = CAMERA_FB_IN_PSRAM;
     } else {
         ESP_LOGW(TAG, "No PSRAM detected → fallback to QVGA");
         cam.frame_size  = FRAMESIZE_QVGA;
         cam.fb_count    = 1;
         cam.grab_mode   = CAMERA_GRAB_WHEN_EMPTY;
         cam.fb_location = CAMERA_FB_IN_DRAM;
     }
 
     wifi_init_sta();
     xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT,
                         pdFALSE, pdTRUE, portMAX_DELAY);
 
     if (camera_init() != ESP_OK) {
         ESP_LOGE(TAG, "Restarting…"); vTaskDelay(pdMS_TO_TICKS(2000));
         esp_restart();
     }
     start_server();
 }
 
 