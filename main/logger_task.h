/**
 * @file logger_task.h
 * @brief UART output and task that streams the samples as JSON lines.
 */
#ifndef LOGGER_TASK_H
#define LOGGER_TASK_H

#include "esp_err.h"

/**
 * @brief Configure the UART used to stream the samples out.
 *
 * @return ESP_OK on success, or an error code from the UART driver.
 */
esp_err_t logger_init(void);

/**
 * @brief FreeRTOS task that writes samples to the UART.
 *
 * Waits for samples on the queue and writes each one as a single JSON
 * line, guarding the UART with a mutex. Registered with the task watchdog.
 *
 * @param arg Queue handle (QueueHandle_t) the samples are received from.
 */
void uart_logger_task(void *arg);

#endif /* LOGGER_TASK_H */
