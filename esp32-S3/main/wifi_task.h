#ifndef WIFI_TASK_H
#define WIFI_TASK_H
#include <stdint.h>
#include "esp_event.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "camera_task.h"
#include "esp_http_client.h"
#include "esp_log.h"

void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void wifi_init(void);
void start_stream_server(void);
static esp_err_t stream_handler(httpd_req_t *req);

#endif