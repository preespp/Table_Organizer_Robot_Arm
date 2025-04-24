#ifndef COORDINATE_TASK_H
#define COORDINATE_TASK_H
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "yolo_task.h"

void coordinate_task(void *pvParameters);

#endif