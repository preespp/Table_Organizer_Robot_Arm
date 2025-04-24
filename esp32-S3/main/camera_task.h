#ifndef CAMERA_TASK_H
#define CAMERA_TASK_H
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_camera.h"

extern QueueHandle_t frame_queue;

typedef struct {
    camera_fb_t *frame;
} FrameMsg;

esp_err_t init_camera(void);
//void camera_task(void *pvParameters);
camera_fb_t* camera_capture(void);

#endif