/**
 * @file sensor_driver.c
 * @brief SHT31 I2C driver and the sensor sampling task.
 */
#include "sensor_driver.h"

#include <stdint.h>

#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* I2C bus */
#define I2C_PORT        I2C_NUM_0
#define I2C_SDA_PIN     21
#define I2C_SCL_PIN     22
#define I2C_FREQ_HZ     100000

/* SHT31 sensor */
#define SHT31_ADDR      0x44
#define SHT31_CMD_MSB   0x24    /* single shot, high repeatability, no clock stretch */
#define SHT31_CMD_LSB   0x00
#define SHT31_MEAS_MS   20      /* conversion time for high repeatability */

/* Accepted measurement range (sensor operating range). */
#define TEMP_MIN_C      (-40.0f)
#define TEMP_MAX_C      (85.0f)
#define HUM_MIN_PCT     (0.0f)
#define HUM_MAX_PCT     (100.0f)

#define SENSOR_PERIOD_MS 1000
#define I2C_TIMEOUT_MS   100

static const char *TAG = "sensor";

/**
 * @brief Compute the SHT31 CRC-8 (polynomial 0x31, init 0xFF).
 *
 * @param data Bytes to checksum.
 * @param len  Number of bytes.
 * @return The computed CRC-8.
 */
static uint8_t sht31_crc8(const uint8_t *data, int len)
{
    uint8_t crc = 0xFF;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}

esp_err_t sensor_init(void)
{
    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };

    esp_err_t err = i2c_param_config(I2C_PORT, &cfg);
    if (err != ESP_OK) {
        return err;
    }
    return i2c_driver_install(I2C_PORT, cfg.mode, 0, 0, 0);
}

esp_err_t sensor_read(sensor_data_t *out)
{
    const uint8_t cmd[2] = { SHT31_CMD_MSB, SHT31_CMD_LSB };
    uint8_t raw[6] = {0};
    esp_err_t err;

    /* Trigger a measurement. */
    err = i2c_master_write_to_device(I2C_PORT, SHT31_ADDR, cmd, sizeof(cmd),
                                     pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    if (err != ESP_OK) {
        return err;
    }

    /* Wait for the conversion to finish. */
    vTaskDelay(pdMS_TO_TICKS(SHT31_MEAS_MS));

    /* Read temperature (2 bytes + CRC) and humidity (2 bytes + CRC). */
    err = i2c_master_read_from_device(I2C_PORT, SHT31_ADDR, raw, sizeof(raw),
                                      pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    if (err != ESP_OK) {
        return err;
    }

    /* Verify both CRCs before trusting the bytes. */
    if (sht31_crc8(&raw[0], 2) != raw[2] ||
        sht31_crc8(&raw[3], 2) != raw[5]) {
        return ESP_ERR_INVALID_CRC;
    }

    uint16_t raw_temp = (raw[0] << 8) | raw[1];
    uint16_t raw_hum  = (raw[3] << 8) | raw[4];

    /* Datasheet conversion formulas. */
    out->temperature_c = -45.0f + 175.0f * ((float)raw_temp / 65535.0f);
    out->humidity_pct  = 100.0f * ((float)raw_hum / 65535.0f);

    return ESP_OK;
}

void sensor_task(void *arg)
{
    QueueHandle_t queue = (QueueHandle_t)arg;
    sensor_data_t sample;

    /* Let the watchdog watch this task. */
    esp_task_wdt_add(NULL);

    while (1) {
        esp_task_wdt_reset();

        esp_err_t err = sensor_read(&sample);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "read failed: %s", esp_err_to_name(err));
        } else if (sample.temperature_c < TEMP_MIN_C || sample.temperature_c > TEMP_MAX_C ||
                   sample.humidity_pct  < HUM_MIN_PCT || sample.humidity_pct  > HUM_MAX_PCT) {
            /* Drop readings outside the valid range. */
            ESP_LOGW(TAG, "out of range: %.2f C, %.2f %%",
                     sample.temperature_c, sample.humidity_pct);
        } else {
            /* Non-blocking send; skip the sample if the queue is full. */
            if (xQueueSend(queue, &sample, 0) != pdTRUE) {
                ESP_LOGW(TAG, "queue full, sample dropped");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(SENSOR_PERIOD_MS));
    }
}
