# Cumulative Chimes Code 2025

Uses SDI12, TSL2591, MS5803, SHT31, Teros10, and a tipping-bucket rain gauge to measure environmental conditions.  
Logs data to the SD card and optionally publishes to MongoDB over LTE.

## Required Credentials

**`mqtt_creds.json`** — defines MQTT/MongoDB connection settings:

```json
{
    "broker": "mqtt.example.com",
    "port": 1883,
    "username": "your-user",
    "password": "your-pass",
    "database": "your-database",
    "project": "your-project"
}
```

**`mqtt_creds.json`** — defines MQTT/MongoDB connection settings:

```json
{
    "SSID": "your-ssid",
    "password": "your-pass"
}
```
