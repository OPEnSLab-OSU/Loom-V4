# ThingSpeak

Publishes sensor data to ThingSpeak over a Wi-Fi or LTE connection.

## Required Credentials

**`arduino_secrets.h`** — defines network and ThingSpeak credentials:

```c
// LTE
#define NETWORK_NAME "your-network-name"
#define NETWORK_USER "your-username"
#define NETWORK_PASS "your-password"

// Wi-Fi
#define SECRET_SSID "your-network-name"
#define SECRET_PASS "your-network-password"

// ThingSpeak
#define CHANNEL_ID 0
#define CLIENT_ID "your-client-id"
#define BROKER_USER "your-username"
#define BROKER_PASS "your-password"
```
