idf_component_register(
    SRCS "bt_module.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer freertos uart
)
