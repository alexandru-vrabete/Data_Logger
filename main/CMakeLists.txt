idf_component_register(
    SRCS
        "main.c"
        "sensor_driver.c"
        "logger_task.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_task_wdt
)
