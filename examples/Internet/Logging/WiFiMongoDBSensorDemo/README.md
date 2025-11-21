# WiFiMongoDB w/ Sensor Demo

Publishes sensor data to MongoDB over MQTT using a Wi-Fi connection.

[Demo video](https://media.oregonstate.edu/media/t/1_yj70pccf) from 2025 Loom Capstone team's walking skeleton.

## Required Credentials

**`arduino_secrets.h`** — defines Wi-Fi and MQTT credentials:

```c
// Wi-Fi
#define SECRET_SSID "your-network-name"
#define SECRET_PASS "your-network-password"

// MQTT
#define SECRET_BROKER "mqtt.example.com"
#define SECRET_PORT 1883
#define DB_NAME "your-db"
#define BROKER_USER "your-user"
#define BROKER_PASS "your-pass"
```