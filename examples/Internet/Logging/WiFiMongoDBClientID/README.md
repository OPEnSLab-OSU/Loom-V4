# WiFiMongoDBClientID

Publishes sensor data to MongoDB over MQTT using a Wi-Fi connection, with a fixed MQTT client ID.

By default the MQTT client ID is random (`Arduino-xxxxxxxx`), so you can't tell which device is connecting in the broker log. Calling `mqtt.setClientID("Device1_Lab")` before the first publish gives it a fixed name.

The ID must be unique per device: the broker disconnects an existing client when another connects with the same ID.

## Example Mosquitto Log

Default ID:

```
1793223625: New client connected from 203.0.113.10:33176 as Arduino-1bd668f1 (p4, c1, k60, u'username').
1793223974: Client Arduino-1bd668f1 [203.0.113.10:33176] disconnected: exceeded timeout.
```

With `setClientID("Device1_Lab")`:

```
1793224093: New client connected from 203.0.113.11:36259 as Device1_Lab (p4, c1, k60, u'username').
1793224185: Client Device1_Lab [203.0.113.11:36259] disconnected: exceeded timeout.
```

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
