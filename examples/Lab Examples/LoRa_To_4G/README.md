# LoRa_to_4G

Uses LoRa to connect to a MQTT broker on a 4G network for sending data to MongoDB

## Required Credentials

**`arduino_secrets.h`** — defines Wi-Fi network credentials:

```c
#define NETWORK_NAME "4G-name"
#define NETWORK_USER "4G-user"
#define NETWORK_PASS "4G-pass"

#define SECRET_BROKER "mqtt-secret"
#define SECRET_PORT 1883
#define DATABASE "your-database"
#define BROKER_USER "mqtt-user"
#define BROKER_PASS "mqtt-pass"
```
