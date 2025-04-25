#include "cv_task.h"
#include "esp_dl.h"
#include "esp_log.h"

static const char *TAG = "YOLO_MODEL";
#define YOLO_INPUT_WIDTH 128
#define YOLO_INPUT_HEIGHT 128

extern const uint8_t yolo_model_bin_start[] asm("_binary_yolo_model_bin_start");
extern const uint8_t yolo_model_bin_end[] asm("_binary_yolo_model_bin_end");

static void *yolo_handle = NULL;

void yolo_init() {
    yolo_handle = esp_dl_model_init(yolo_model_bin_start, yolo_model_bin_end - yolo_model_bin_start);
    if (!yolo_handle) {
        ESP_LOGE(TAG, "Failed to load YOLO model.");
    } else {
        ESP_LOGI(TAG, "YOLO model loaded successfully.");
    }
}

int run_yolo(const uint8_t *image_rgb888, yolo_result_t *results) {
    if (!yolo_handle) {
        ESP_LOGE(TAG, "YOLO model not initialized.");
        return 0;
    }

    esp_dl_detect_output_t output;
    esp_dl_detect_run(yolo_handle, image_rgb888, &output);

    int num_objects = output.num_box;
    if (num_objects > MAX_YOLO_RESULTS) num_objects = MAX_YOLO_RESULTS;

    for (int i = 0; i < num_objects; i++) {
        results[i].x = output.box[i].box[0];
        results[i].y = output.box[i].box[1];
        results[i].width = output.box[i].box[2] - output.box[i].box[0];
        results[i].height = output.box[i].box[3] - output.box[i].box[1];
    }

    ESP_LOGI(TAG, "YOLO detected %d objects.", num_objects);
    return num_objects;
}
