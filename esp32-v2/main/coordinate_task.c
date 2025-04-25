#include "coordinate_task.h"
#include "esp_http_client.h"
#include "esp_log.h"
#define TAG "COORD"

void send_coordinates(float x, float y, float width, float height) {
    char post_data[100];
    snprintf(post_data, sizeof(post_data),
             "{\"x\": %.2f, \"y\": %.2f, \"width\": %.2f, \"height\": %.2f}",
             x, y, width, height);

    esp_http_client_config_t config = {
        .url = "http://<YOUR_WEB_SERVER_IP>/api/coordinates",
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}

void coordinate_task(void *pvParameters) {
    BBoxMsg bbox;
    while (1) {
        if (xQueueReceive(bbox_queue, &bbox, portMAX_DELAY)) {
            send_coordinates(bbox.x, bbox.y, bbox.width, bbox.height);
        }
    }
}