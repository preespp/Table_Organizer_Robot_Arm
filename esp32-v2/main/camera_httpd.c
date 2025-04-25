#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "camera_httpd.h"
#include "lwip/sockets.h"
#include <arpa/inet.h>  // For htonl

#define SERVER_IP   "192.168.8.106"  // Replace with your PC IP
#define SERVER_PORT 8080

static const char* TAG = "camera_httpd";

static esp_err_t jpg_stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    char *part_buf[64];

    ESP_LOGI(TAG, "Stream request started");

    // Set HTTP headers for multipart JPEG stream
    //httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=1234567890");
    httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=--frame");

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Connection", "keep-alive");

    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            res = ESP_FAIL;
            break;
        }

        ESP_LOGI(TAG, "Captured frame: %u bytes", fb->len);

        // Send multipart boundary
        // res = httpd_resp_send_chunk(req, "--1234567890\r\n", strlen("--1234567890\r\n"));
        res = httpd_resp_send_chunk(req, "--frame\r\n", strlen("--frame\r\n"));
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send boundary");
            break;
        }

        // Send content headers
        int header_len = snprintf((char *)part_buf, sizeof(part_buf),
                 "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                 fb->len);
        res = httpd_resp_send_chunk(req, (const char *)part_buf, header_len);
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send headers");
            break;
        }

        // Send JPEG image
        res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send JPEG frame");
            break;
        }

        // Send line break
        res = httpd_resp_send_chunk(req, "\r\n", strlen("\r\n"));
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send line break");
            break;
        }

        esp_camera_fb_return(fb);
        fb = NULL;

        // **Lower FPS to reduce bandwidth**
        vTaskDelay(100 / portTICK_PERIOD_MS); // ~5 FPS
    }

    if (fb) {
        esp_camera_fb_return(fb);
    }

    ESP_LOGI(TAG, "Stream request ended");
    return res;
}

static esp_err_t jpg_capture_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Capture request started");

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Captured frame: %u bytes", fb->len);

    httpd_resp_set_type(req, "image/jpeg");
    esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    esp_camera_fb_return(fb);

    ESP_LOGI(TAG, "Capture request ended");
    return res;
}

void tcp_send_frame(camera_fb_t *fb) {
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE("TCP", "Unable to create socket");
        return;
    }

    struct sockaddr_in dest_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(SERVER_PORT),
        .sin_addr.s_addr = inet_addr(SERVER_IP),
    };

    if (connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
        ESP_LOGE("TCP", "Socket connection failed");
        close(sock);
        return;
    }

    uint32_t frame_size_net = htonl(fb->len);
    if (send(sock, &frame_size_net, sizeof(frame_size_net), 0) != sizeof(frame_size_net)) {
        ESP_LOGE("TCP", "Failed to send frame size");
        close(sock);
        return;
    }

    size_t total_sent = 0;
    while (total_sent < fb->len) {
        int sent_now = send(sock, fb->buf + total_sent, fb->len - total_sent, 0);
        if (sent_now < 0) {
            ESP_LOGE("TCP", "Failed to send frame data");
            break;
        }
        total_sent += sent_now;
    }

    ESP_LOGI("TCP", "Frame sent: %u bytes", fb->len);
    close(sock);
}

void capture_and_send_task(void *pvParameters) {
    while (1) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE("Camera", "Capture failed");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        tcp_send_frame(fb);
        esp_camera_fb_return(fb);

        vTaskDelay(2000 / portTICK_PERIOD_MS);  // Capture every 2 seconds
    }
}

void start_camera_server() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.recv_wait_timeout = 10; // 10 seconds receive timeout
    config.send_wait_timeout = 10; // 10 seconds send timeout
    config.stack_size = 12288;  // **Increase HTTPD stack size if needed**
    config.keep_alive_enable = true;

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
