#!/usr/bin/env python3
"""
Read NMEA sentences from one serial port and forward them to another,
dropping any PHTG sentences.
"""

import serial
import time

# Serial configuration
SERIAL_IN = "/dev/ttyUSB0"
SERIAL_OUT = "/dev/ttyUSB1"
SERIAL_IN_BAUD = 115200
SERIAL_OUT_BAUD = 57600

# PHTG marker used to filter authentication messages
DROP_TOKEN = "PHTG"


def main():
    print(f"Opening input port {SERIAL_IN} @ {SERIAL_IN_BAUD} baud...")
    ser_in = serial.Serial(SERIAL_IN, SERIAL_IN_BAUD, timeout=1)

    print(f"Opening output port {SERIAL_OUT} @ {SERIAL_OUT_BAUD} baud...")
    ser_out = serial.Serial(SERIAL_OUT, SERIAL_OUT_BAUD, timeout=1)

    print("Forwarding NMEA sentences; dropping PHTG messages. CTRL+C to stop.")

    while True:
        try:
            line_bytes = ser_in.readline()
            if not line_bytes:
                continue

            # Decode for filtering and sanitize; enforce ASCII NMEA with CRLF
            text = line_bytes.decode("ascii", errors="ignore").strip()
            print(f"RX: {text!r}")
            if not text:
                continue

            if not text.startswith("$"):
                print(f"SKIP (no $): {text}")
                continue

            if DROP_TOKEN in text:
                print(f"DROP: {text}")
                continue

            # Re-encode with a clean CRLF terminator for the consumer
            out_bytes = (text + "\r\n").encode("ascii", errors="ignore")
            ser_out.write(out_bytes)
            ser_out.flush()
            print(f"TX: {text}")

        except KeyboardInterrupt:
            print("Stopping bridge.")
            break
        except Exception as exc:
            print("Error:", exc)
            time.sleep(0.2)


if __name__ == "__main__":
    main()
