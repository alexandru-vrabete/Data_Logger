#ifndef LOGGER_TASK_H
#define LOGGER_TASK_H

#include "esp_err.h"

// Configure the UART used to stream the samples out.
esp_err_t logger_init(void);

// Task: waits for samples on the queue (passed as the task argument) and
// writes each one to the UART as a single JSON line.
void uart_logger_task(void *arg);

#endif // LOGGER_TASK_H
