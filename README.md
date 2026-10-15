# ESP32 Data Logger

Temperature and humidity logger for the ESP32, built with ESP-IDF and FreeRTOS.
An SHT31 sensor is read over I2C, the samples are validated and streamed over
UART as JSON, and a small Python script stores them to CSV on the PC.

## Architecture

```
          +-------------------+
          |      SHT31        |
          |   (I2C @ 0x44)    |
          +---------+---------+
                    | SDA=GPIO21 / SCL=GPIO22
                    v
   +------------------------------------------------+
   |                    ESP32                        |
   |                                                 |
   |  sensor_task (prio 2, 1000 ms)                  |
   |    read -> validate -> xQueueSend               |
   |                 |                               |
   |                 v  sensor_queue                 |
   |  uart_logger_task (prio 1) --[uart_mutex]--+    |
   |                                            |    |
   |  heartbeat_task (prio 1, 500 ms)           |    |
   +--------------------------------------------|----+
              |                                 |
              v                                 v
        LED (GPIO2)                 UART1 TX=GPIO17 --> PC
                                                       |
                                                 tools/logger.py
                                                       |
                                                 sensor_data.csv
```

`sensor_task` and `uart_logger_task` are registered with the Task Watchdog
Timer (TWDT) and reset their timer on every loop.

## Hardware requirements

| Part            | Detail                                              |
|-----------------|-----------------------------------------------------|
| ESP32 board     | Any ESP32 dev board                                 |
| SHT31 sensor    | I2C address `0x44`, SDA = GPIO21, SCL = GPIO22      |
| Status LED      | On-board LED on GPIO2 (heartbeat)                   |
| USB-UART adapter| Connect adapter RX to ESP32 GPIO17 (UART1 TX), GND to GND |

The console/logs stay on UART0 (the programming port). Sensor data is sent on a
separate UART1 so the two streams do not mix. To use a single port instead,
change `LOG_UART_PORT` to `UART_NUM_0` in `main/logger_task.c`.

## Build and flash (ESP-IDF CLI)

```bash
idf.py set-target esp32
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with your serial port (e.g. `COM3` on Windows, `/dev/ttyUSB0`
on Linux). `monitor` shows the UART0 console logs; exit with `Ctrl+]`.

## Logging to CSV on the PC

```bash
pip install -r tools/requirements.txt
python tools/logger.py COM3
```

The script reads the data UART, parses each JSON line and appends a row to
`sensor_data.csv`. Lines that are not valid samples (boot messages, etc.) are
ignored.

## Data format

**UART output** — one JSON object per line:

```
{"temp": 23.50, "hum": 45.20}
```

**CSV file** — written by `tools/logger.py`:

```
timestamp,temperature_c,humidity_pct
2026-10-15T12:00:01,23.50,45.20
```

Readings outside the sensor range (temperature below -40 °C or above 85 °C,
humidity below 0 % or above 100 %) and samples that fail the CRC check are
discarded and never sent.
