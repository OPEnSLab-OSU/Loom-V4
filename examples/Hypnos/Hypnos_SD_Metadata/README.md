# Hypnos_SD_Metadata

Logs sensor data to the SD card and publishes it to MongoDB over MQTT.

## Required Credentials

**`mqtt_creds.json`** — authenticates with the MQTT broker:

```json
{
    "broker": "mqtt.example.com",
    "port": 1883,
    "username": "your-username",
    "password": "your-password",
    "database": "your-database",
    "project": "your-project"
}
```
