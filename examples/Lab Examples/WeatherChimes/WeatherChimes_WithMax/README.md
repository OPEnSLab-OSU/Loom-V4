# WeatherChimes with Max

Uses SDI12, TSL2591 and an SHT31 sensor to log environment data and logs it to both the SD card and also MQTT/MongoDB

## Required Credentials

**`arduino_secrets.h`** — defines network credentials:

```c
#define NETWORK_APN "hologram"
#define NETWORK_USER "your-user"
#define NETWORK_PASS "your-pass"

#define SECRET_BROKER "mqtt-secret"
#define SECRET_PORT 1883
#define DATABASE "your-database"
#define BROKER_USER "mqtt-user"
#define BROKER_PASS "mqtt-pass"
```
