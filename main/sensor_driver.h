#ifndef SENSOR_DRIVER_H
#define SENSOR_DRIVER_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// One temperature/humidity sample, passed through the queue.
typedef struct {
    float temperature_c;
    float humidity_pct;
} sensor_data_t;

// Configure the I2C bus and the SHT31 sensor.
esp_err_t sensor_init(void);

// Read one sample from the sensor. Returns ESP_OK and fills *out on success.
esp_err_t sensor_read(sensor_data_t *out);

// Task: reads the sensor once a second, validates the values and pushes
// the valid ones to the queue passed as the task argument.
void sensor_task(void *arg);

#endif // SENSOR_DRIVER_H
