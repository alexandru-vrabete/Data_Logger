/**
 * @file sensor_driver.h
 * @brief I2C driver and task for the SHT31 temperature/humidity sensor.
 */
#ifndef SENSOR_DRIVER_H
#define SENSOR_DRIVER_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
 * @brief One temperature/humidity sample passed through the queue.
 */
typedef struct {
    float temperature_c;  /**< Temperature in degrees Celsius. */
    float humidity_pct;   /**< Relative humidity in percent. */
} sensor_data_t;

/**
 * @brief Configure the I2C bus and the SHT31 sensor.
 *
 * @return ESP_OK on success, or an error code from the I2C driver.
 */
esp_err_t sensor_init(void);

/**
 * @brief Read one sample from the sensor.
 *
 * @param[out] out Filled with the temperature and humidity on success.
 * @return ESP_OK on success, ESP_ERR_INVALID_CRC on a CRC mismatch,
 *         or an error code from the I2C driver.
 */
esp_err_t sensor_read(sensor_data_t *out);

/**
 * @brief FreeRTOS task that reads the sensor once a second.
 *
 * Reads the sensor, drops out-of-range readings and pushes the valid
 * samples to the queue. Registered with the task watchdog.
 *
 * @param arg Queue handle (QueueHandle_t) the samples are sent to.
 */
void sensor_task(void *arg);

#endif /* SENSOR_DRIVER_H */
