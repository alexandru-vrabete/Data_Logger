/**
 * @file main.c
 * @brief Application entry point: peripherals, queue, watchdog and tasks.
 */
#include <stdbool.h>

#include "sensor_driver.h"
#include "logger_task.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

/* Heartbeat LED */
#define LED_PIN         2
#define LED_PERIOD_MS   500

/* Shared queue holding sensor samples. */
#define QUEUE_LENGTH    10

/* Watchdog timeout for the registered tasks. */
#define WDT_TIMEOUT_MS  5000

static const char *TAG = "main";

static QueueHandle_t sensor_queue;

/**
 * @brief Task that toggles the status LED to show the system is alive.
 *
 * @param arg Unused.
 */
static void heartbeat_task(void *arg)
{
    bool level = false;
    while (1) {
        level = !level;
        gpio_set_level(LED_PIN, level);
        vTaskDelay(pdMS_TO_TICKS(LED_PERIOD_MS));
    }
}

/**
 * @brief Initialise the task watchdog, reconfiguring it if already running.
 */
static void wdt_setup(void)
{
    esp_task_wdt_config_t cfg = {
        .timeout_ms = WDT_TIMEOUT_MS,
        .idle_core_mask = 0,
        .trigger_panic = true,
    };

    /* The watchdog may already be running from the default config. */
    esp_err_t err = esp_task_wdt_init(&cfg);
    if (err == ESP_ERR_INVALID_STATE) {
        esp_task_wdt_reconfigure(&cfg);
    }
}

/**
 * @brief Application entry point.
 *
 * Sets up the LED, peripherals, the shared queue and the watchdog, then
 * starts the sensor, logger and heartbeat tasks.
 */
void app_main(void)
{
    /* Status LED. */
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    /* Peripherals. */
    ESP_ERROR_CHECK(sensor_init());
    ESP_ERROR_CHECK(logger_init());

    /* Queue between the sensor task and the logger task. */
    sensor_queue = xQueueCreate(QUEUE_LENGTH, sizeof(sensor_data_t));
    if (sensor_queue == NULL) {
        ESP_LOGE(TAG, "failed to create sensor queue");
        return;
    }

    wdt_setup();

    xTaskCreate(sensor_task,      "sensor",    4096, sensor_queue, 2, NULL);
    xTaskCreate(uart_logger_task, "logger",    4096, sensor_queue, 1, NULL);
    xTaskCreate(heartbeat_task,   "heartbeat", 2048, NULL,         1, NULL);

    ESP_LOGI(TAG, "data logger started");
}
