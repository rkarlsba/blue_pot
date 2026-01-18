idf_component_register(
    SRCS "pots_module.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer freertos esp_dsp
)
