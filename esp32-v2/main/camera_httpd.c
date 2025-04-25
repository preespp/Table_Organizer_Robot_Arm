#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "camera_httpd.h"

static const char* TAG = "camera_httpd";

static esp_err_t jpg_stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    char *part_buf[64];
    
    // Set HTTP headers for multipart JPEG stream
    httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=1234567890");

    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            res = ESP_FAIL;
            break;
        }

        // Send multipart boundary
        res = httpd_resp_send_chunk(req, "--1234567890\r\n", strlen("--1234567890\r\n"));
        if (res != ESP_OK) break;

        // Send content headers
        snprintf((char *)part_buf, sizeof(part_buf),
                 "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                 fb->len);
        res = httpd_resp_send_chunk(req, (const char *)part_buf, strlen((const char *)part_buf));
        if (res != ESP_OK) break;

        // Send JPEG image
        res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        if (res != ESP_OK) break;

        // Send line break
        res = httpd_resp_send_chunk(req, "\r\n", strlen("\r\n"));
        if (res != ESP_OK) break;

        esp_camera_fb_return(fb);
        vTaskDelay(30 / portTICK_PERIOD_MS); // ~30 FPS
    }

    esp_camera_fb_return(fb);
    return res;
}

static esp_err_t jpg_capture_handler(httpd_req_t *req) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_send(req, (const char *)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    return ESP_OK;
}

void start_camera_server() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        // JPEG stream endpoint
        httpd_uri_t stream_uri = {
            .uri       = "/stream",
            .method    = HTTP_GET,
            .handler   = jpg_stream_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &stream_uri);

        // Single JPEG capture endpoint
        httpd_uri_t capture_uri = {
            .uri       = "/jpg",
            .method    = HTTP_GET,
            .handler   = jpg_capture_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &capture_uri);

        ESP_LOGI(TAG, "Camera server started");
    } else {
        ESP_LOGE(TAG, "Failed to start HTTP server");
    }
}