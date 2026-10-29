# WiFiMongoDBClientID

Publishes sensor data to MongoDB over MQTT using a Wi-Fi connection, with a custom MQTT client ID.

`Loom_MongoDB` sets the client ID to the device name and instance number automatically (`Manager manager("Device", 1)` gives `Device1`), so the device is identifiable in the broker log. Call `mqtt.setClientID("...")` before the first publish only to use a different ID.

The ID must be unique per device: the broker disconnects an existing client when another connects with the same ID.

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
