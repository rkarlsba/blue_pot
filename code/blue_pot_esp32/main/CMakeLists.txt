idf_component_register(
    SRCS "main.c" "cmd_processor.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer esp_event freertos nvs_flash uart
)
