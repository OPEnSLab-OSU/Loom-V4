# LTEMongoDBBatch

Logs sensor data in batches to MongoDB over MQTT using an LTE connection.

## Required Credentials

**`arduino_secrets.h`** — defines LTE and MQTT credentials:

```c
#define NETWORK_NAME "your-network-name"
#define NETWORK_USER "your-username"
#define NETWORK_PASS "your-password"

#define SECRET_BROKER "mqtt.example.com"
#define SECRET_PORT 1883
#define DATABASE "your-database"
#define BROKER_USER "your-username"
#define BROKER_PASS "your-password"
#define PROJECT "your-project"
```
