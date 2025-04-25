#ifndef YOLO_MODEL_H
#define YOLO_MODEL_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    int x;
    int y;
    int width;
    int height;
} yolo_result_t;

#define MAX_YOLO_RESULTS 10

void yolo_init();
int run_yolo(const uint8_t *image_rgb888, yolo_result_t *results);

#endif