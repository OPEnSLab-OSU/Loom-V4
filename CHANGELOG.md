# Loom 4.9.1

## 4.9-joshfixes

- Hypnos uses checked UTC alarm handling, preserves sampling cadence across overruns, and suspends
  the SAMD21 watchdog in standby. Chimes tipping-bucket wakes leave the RTC alarm armed.
- SD logging checks writes and rotates CSVs when their schema changes. Confirmed SD batch
  records remain available across reboots and failed MQTT publishes. Serial and optional SD
  text logs remain available for reporting operational faults.
- LTE supports SARA-R4, SARA-R5 and the SIM7080/7090 family. Startup, registration, PDP and
  MQTT failures have separate recovery paths. The SIM7090G Click example uses external
  power-key control.
- Scheduled LoRa SD journals retry stored readings until the hub confirms its SD append.
  Receipt exchanges in the benchmark and journal stop retrying as soon as the hub confirms
  storage, even if the link ACK is lost. Retries must fit the remaining slot.
- Multiplexer startup checks the control register, supports selected ports and drivers, and
  closes channels around rail transitions. Sensor updates cover AS5311, ADS1115, SEN55/SEN66
  and signed, multi-page SDI-12 TEROS measurements.

See [patched dependencies](dependencies/README.md) and [validation commands](tests/README.md).
