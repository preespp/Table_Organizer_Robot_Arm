#include "coordinate_task.h"
#include "yolo_model.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "dl_image_proc.h"
#include "cJSON.h"

static const char *TAG = "COORDINATE_TASK";
#define YOLO_INPUT_WIDTH 128
#define YOLO_INPUT_HEIGHT 128

static char json_buffer[256];  // Stores the JSON string

void coordinate_task(void *pvParameters) {
    yolo_init();

    while (1) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed.");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // Convert JPEG to RGB888
        dl_matrix3du_t *image_rgb = dl_matrix3du_alloc(1, fb->width, fb->height, 3);
        if (!fmt2rgb888(fb->buf, fb->len, fb->format, image_rgb->item)) {
            ESP_LOGE(TAG, "JPEG to RGB888 conversion failed.");
            esp_camera_fb_return(fb);
            continue;
        }
        esp_camera_fb_return(fb);

        // Resize to YOLO input
        dl_matrix3du_t *input_resized = dl_matrix3du_alloc(1, YOLO_INPUT_WIDTH, YOLO_INPUT_HEIGHT, 3);
        image_resize_linear(input_resized->item, YOLO_INPUT_WIDTH, YOLO_INPUT_HEIGHT,
                            image_rgb->item, fb->width, fb->height, 3);
        dl_matrix3du_free(image_rgb);

        // Run YOLO
        yolo_result_t results[MAX_YOLO_RESULTS];
        int num_objects = run_yolo(input_resized->item, results);
        dl_matrix3du_free(input_resized);

        // Generate JSON
        cJSON *root = cJSON_CreateArray();
        for (int i = 0; i < num_objects; i++) {
            int x_center = results[i].x + results[i].width / 2;
            int y_center = results[i].y + results[i].height / 2;

            cJSON *obj = cJSON_CreateObject();
            cJSON_AddNumberToObject(obj, "x", x_center);
            cJSON_AddNumberToObject(obj, "y", y_center);
            cJSON_AddItemToArray(root, obj);
        }

        char *json_str = cJSON_PrintUnformatted(root);
        strncpy(json_buffer, json_str, sizeof(json_buffer));
        free(json_str);
        cJSON_Delete(root);

        ESP_LOGI(TAG, "Detection JSON: %s", json_buffer);

        vTaskDelay(pdMS_TO_TICKS(500));  // Adjust as needed
    }
}

const char* get_detection_json() {
    return json_buffer;
}
