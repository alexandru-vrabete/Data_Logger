/**
 * @file logger_task.c
 * @brief UART setup and the task that streams samples as JSON lines.
 */
#include "logger_task.h"
#include "sensor_driver.h"

#include <stdio.h>

#include "driver/uart.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// Dedicated data UART, kept separate from the console on UART0.
#define LOG_UART_PORT   UART_NUM_1
#define LOG_UART_TX     17
#define LOG_UART_RX     16
#define LOG_UART_BAUD   115200
#define LOG_UART_BUF    512

// Block up to 2 s waiting for a sample so we can still feed the watchdog.
#define QUEUE_WAIT_MS   2000

static const char *TAG = "logger";

// Protects the UART from concurrent writes.
static SemaphoreHandle_t uart_mutex;

esp_err_t logger_init(void)
{
    uart_config_t cfg = {
        .baud_rate  = LOG_UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_driver_install(LOG_UART_PORT, LOG_UART_BUF, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        return err;
    }
    err = uart_param_config(LOG_UART_PORT, &cfg);
    if (err != ESP_OK) {
        return err;
    }
    err = uart_set_pin(LOG_UART_PORT, LOG_UART_TX, LOG_UART_RX,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        return err;
    }

    uart_mutex = xSemaphoreCreateMutex();
    if (uart_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void uart_logger_task(void *arg)
{
    QueueHandle_t queue = (QueueHandle_t)arg;
    sensor_data_t sample;
    char line[64];

    esp_task_wdt_add(NULL);

    while (1) {
        esp_task_wdt_reset();

        if (xQueueReceive(queue, &sample, pdMS_TO_TICKS(QUEUE_WAIT_MS)) == pdTRUE) {
            int len = snprintf(line, sizeof(line),
                               "{\"temp\": %.2f, \"hum\": %.2f}\r\n",
                               sample.temperature_c, sample.humidity_pct);

            // Only one writer on the UART at a time.
            if (xSemaphoreTake(uart_mutex, portMAX_DELAY) == pdTRUE) {
                uart_write_bytes(LOG_UART_PORT, line, len);
                xSemaphoreGive(uart_mutex);
            }

            ESP_LOGI(TAG, "%.2f C, %.2f %%", sample.temperature_c, sample.humidity_pct);
        }
    }
}
