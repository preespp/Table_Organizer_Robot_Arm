#ifndef YOLO_TASK_H
#define YOLO_TASK_H
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "camera_task.h"

extern QueueHandle_t bbox_queue;

typedef struct {
    float x, y, width, height;
} BBoxMsg;

void yolo_task(void *pvParameters);

#endif