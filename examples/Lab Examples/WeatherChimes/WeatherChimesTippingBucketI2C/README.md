# WeatherChimesI2C with TippingBucket

Uses SDI12, TSL2591 and an SHT31 sensor to log environment data and logs it to both the SD card and also MQTT/MongoDB

## Required Credentials

**`mqtt_creds.json`** — defines network credentials:

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
**`wifi_creds.json`** — defines network credentials:
```json
{
    "SSID": "your-ssid",
    "password": "your-pass"
}
```
