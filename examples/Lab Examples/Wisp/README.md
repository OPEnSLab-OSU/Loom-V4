# Wisp examples

- `Wisp_Batch_Logging` saves readings to SD and publishes accumulated batches over LTE.
- `Wisp_Mux_BatchLogging` discovers sensors on a TCA9548A before sampling and batching.
- `WispV2_Deploy_2026` uses five-minute standby intervals and 72-record MQTT batches.

Copy the project's `mqtt_creds.json` onto the SD card. Match the mux address, port mask and
sensor rail configuration to the deployed stack.
