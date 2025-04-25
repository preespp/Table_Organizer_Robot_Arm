#ifndef CAMERA_HTTPH_H
#define CAMERA_HTTPH_H

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_camera.h"

void tcp_send_frame(camera_fb_t *fb);
void capture_and_send_task(void *pvParameters);
void start_camera_server();

#endif