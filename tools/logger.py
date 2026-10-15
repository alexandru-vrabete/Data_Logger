#!/usr/bin/env python3
"""Read JSON lines from the ESP32 over serial and append them to a CSV file.

Usage:
    python logger.py [PORT]

PORT defaults to the value below. Examples: COM3 (Windows), /dev/ttyUSB0 (Linux).
"""

import csv
import json
import sys
from datetime import datetime

import serial

PORT = "COM3"
BAUD = 115200
OUTPUT = "sensor_data.csv"


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else PORT

    with serial.Serial(port, BAUD, timeout=2) as ser, \
         open(OUTPUT, "a", newline="") as f:
        writer = csv.writer(f)
        if f.tell() == 0:
            writer.writerow(["timestamp", "temperature_c", "humidity_pct"])

        print(f"logging from {port} to {OUTPUT} (Ctrl+C to stop)")
        while True:
            line = ser.readline().decode("utf-8", errors="ignore").strip()
            if not line:
                continue

            try:
                data = json.loads(line)
                temp = data["temp"]
                hum = data["hum"]
            except (json.JSONDecodeError, KeyError):
                # Skip boot messages and anything that is not a sample.
                continue

            timestamp = datetime.now().isoformat(timespec="seconds")
            writer.writerow([timestamp, temp, hum])
            f.flush()
            print(timestamp, temp, hum)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nstopped")
