# WiFiMongoDBBatch

Logs sensor data in batches to MongoDB over MQTT using a Wi-Fi connection.

## Required Credentials

**`arduino_secrets.h`** — defines Wi-Fi and MQTT credentials:

```c
// Wi-Fi
#define SECRET_SSID "your-network-name"
#define SECRET_PASS "your-network-password"

// MQTT
#define SECRET_BROKER "mqtt.example.com"
#define SECRET_PORT 1883
#define DATABASE "your-database"
#define BROKER_USER "your-username"
#define BROKER_PASS "your-password"
#define PROJECT "your-project"
```
