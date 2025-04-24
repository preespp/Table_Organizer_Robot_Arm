#include "wifi_task.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "camera_task.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"

static httpd_handle_t stream_httpd = NULL;

#define TAG "WIFI"
#define TAG2 "WIFI Handler"

#define WIFI_SSID "BU Guest (unencrypted)" // To be added
#define WIFI_PASSWORD "" // To be added

#define PORT 41234

// Wi-Fi event handler
void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
    // Handle Wi-Fi events here
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect(); // Connect to the AP
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    esp_wifi_connect(); // Reconnect
    ESP_LOGI(TAG2, "Reconnecting to the AP...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(TAG2, "Got IP Address: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

void wifi_init(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_netif_init());

    // Create default event loop
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }

    // Create default Wi-Fi station
    esp_netif_create_default_wifi_sta();

    // Initialize Wi-Fi with default configurations
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handler for Wi-Fi and IP events
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    // Set Wi-Fi station configuration
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID, // Network SSID
            //.password = WIFI_PASSWORD // Network password
            .threshold.authmode = WIFI_AUTH_OPEN  // No password
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); // Wi-Fi station mode
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config)); // Configure Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_start()); // Start Wi-Fi

    ESP_LOGI(TAG, "Wi-Fi initialization completed.");
}

static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    char buf[64];
    const char *boundary = "\r\n--frame\r\n";
    const char *part = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

    httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");

    while (1) {
        fb = camera_capture();
        if (!fb) {
            ESP_LOGW(TAG, "Camera capture failed");
            continue;
        }

        httpd_resp_send_chunk(req, boundary, strlen(boundary));
        int len = snprintf(buf, sizeof(buf), part, fb->len);
        httpd_resp_send_chunk(req, buf, len);
        httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        esp_camera_fb_return(fb);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return ESP_OK;
}

void start_stream_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&stream_httpd, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server!");
        return;
    }
    httpd_start(&stream_httpd, &config);

    httpd_uri_t stream_uri = {
        .uri = "/stream",
        .method = HTTP_GET,
        .handler = stream_handler,
        .user_ctx = NULL
    };

    httpd_register_uri_handler(stream_httpd, &stream_uri);
}
