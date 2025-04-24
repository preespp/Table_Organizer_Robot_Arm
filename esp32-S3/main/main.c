#include "camera_task.h"
#include "yolo_task.h"
#include "coordinate_task.h"
#include "wifi_task.h"
#include "esp_http_server.h"

void stream_task(void *pvParameters) {
    start_stream_server();  // This blocks forever (you could isolate it too)
    vTaskDelete(NULL);
}

void app_main(void) {
    // Initialize Wi-Fi
    wifi_init();

    // Initialize Camera
    if(ESP_OK != init_camera()) {
        return;
    }

    // Create Queues
    //frame_queue = xQueueCreate(2, sizeof(FrameMsg));
    //bbox_queue = xQueueCreate(2, sizeof(BBoxMsg));

    // Create RTOS Tasks
    xTaskCreate(stream_task, "Stream Task", 8192, NULL, 5 , NULL);
    //xTaskCreate(yolo_task, "YOLO Task", 8192, NULL, 5, NULL);
    //xTaskCreate(coordinate_task, "Coordinate Task", 4096, NULL, 5, NULL);
}
