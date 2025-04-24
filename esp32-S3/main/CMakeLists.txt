set(srcs "main.c"
         "camera_task.c"
         "yolo_task.c"
         "coordinate_task.c"
         "wifi_task.c")
idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES console esp_timer esp_http_client esp_http_server esp32-camera esp_wifi nvs_flash)
