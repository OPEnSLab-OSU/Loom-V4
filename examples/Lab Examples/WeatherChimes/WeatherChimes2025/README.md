# WeatherChimes 2025

Logs environmental data from SDI12, TSL2591, MS5803, SHT31, Teros10, and a tipping-bucket rain gauge.  
Stores data locally on the SD card and publishes to MongoDB over LTE.

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

**`wifi_creds.json`** — defines Wi-Fi network credentials:

```json
{
    "SSID": "your-ssid",
    "password": "your-pass"
}
```
