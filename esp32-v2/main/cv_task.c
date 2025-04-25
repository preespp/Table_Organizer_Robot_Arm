#include "yolo_task.h"
#include "camera_task.h"
#include "esp_log.h"
#include "dl_image_proc.h"     // For fmt2rgb888, resize
#include "esp_dl.h"            // For esp_dl_detect_run()
#include "freertos/queue.h"

#define TAG "YOLO"
#define YOLO_INPUT_WIDTH 128
#define YOLO_INPUT_HEIGHT 128

extern QueueHandle_t bbox_queue;

extern const uint8_t yolo_model_bin_start[] asm("_binary_yolo_model_bin_start");
extern const uint8_t yolo_model_bin_end[]   asm("_binary_yolo_model_bin_end");

void *yolo_handle = esp_dl_model_init(yolo_model_bin_start, yolo_model_bin_end - yolo_model_bin_start);

void yolo_task(void *pvParameters) {

    // Load YOLO model (once)
    yolo_handle = esp_dl_model_init(yolo_model_bin_start, yolo_model_bin_end - yolo_model_bin_start);

    if (!yolo_handle) {
        ESP_LOGE(TAG, "Failed to load YOLO model");
        vTaskDelete(NULL);
    }

    while (1) {
        camera_fb_t *fb = camera_capture();
        if (!fb) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // Convert JPEG to RGB888
        dl_matrix3du_t *image_rgb = dl_matrix3du_alloc(1, fb->width, fb->height, 3);
        if (!fmt2rgb888(fb->buf, fb->len, fb->format, image_rgb->item)) {
            ESP_LOGE(TAG, "JPEG to RGB888 conversion failed");
            esp_camera_fb_return(fb);
            continue;
        }
        esp_camera_fb_return(fb);

        // Resize image to YOLO input
        dl_matrix3du_t *input_resized = dl_matrix3du_alloc(1, YOLO_INPUT_WIDTH, YOLO_INPUT_HEIGHT, 3);
        image_resize_linear(input_resized->item, YOLO_INPUT_WIDTH, YOLO_INPUT_HEIGHT,
                            image_rgb->item, fb->width, fb->height, 3);
        dl_matrix3du_free(image_rgb);

        // Run YOLO (pseudo)
        esp_dl_detect_output_t output;
        esp_dl_detect_run(yolo_handle, input_resized->item, &output);

        // Send dummy bbox to next task
        BBoxMsg bbox = {100.0f, 50.0f, 200.0f, 150.0f};
        xQueueSend(bbox_queue, &bbox, portMAX_DELAY);

        dl_matrix3du_free(input_resized);

        vTaskDelay(pdMS_TO_TICKS(500));  // Adjust as needed
    }
}
