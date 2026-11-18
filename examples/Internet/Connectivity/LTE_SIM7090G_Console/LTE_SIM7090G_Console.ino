/**
 * Feather M0 + Hypnos + MIKROE LTE IoT 14 Click (SIM7090G).
 * VCC SEL = 3V3, USB BOOT = 0. See README.md for power and header wiring.
 * Serial Monitor: 115200 baud, Newline or Both NL & CR.
 * Use the Click's PWR button to turn on the modem, then enter init.
 */
#include <Loom_Manager.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Utilities/Loom_ATReply.h>
#include <Utilities/Loom_GnssUtils.h>
#include <cstring>
#include <cstdlib>

Manager manager("SIM7090Bench", 1);
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, false, false);
// Replace with the SIM provider's APN. APN can also be changed with the apn command.
Loom_LTE lte(manager, "hologram", "", "", -1, SPARKFUN, -1, LTE_MODEM::SIM7090);

char commandLine[160];
size_t commandLength = 0;
bool commandOverflow = false;
bool watchGPS = false;
uint32_t lastGPS = 0;

bool runAT(const char *command, uint32_t timeoutMs = 5000) {
    LoomWatchdogPause watchdogPause;
    for (size_t i = 0; i < 512 && Serial1.available(); ++i) Serial.write(Serial1.read());
    Serial.print(F("> "));
    Serial.println(command);
    Serial1.print(command);
    Serial1.print("\r\n");
    loomAt::ReplyReader reply;
    const uint32_t started = millis();
    while (static_cast<uint32_t>(millis() - started) < timeoutMs) {
        if (Serial1.available()) {
            const char c = Serial1.read();
            Serial.write(c);
            const auto result = reply.consume(c);
            if (result != loomAt::Reply::Waiting) return result == loomAt::Reply::Accepted;
        } else {
            delay(1);
        }
    }
    Serial.println(F("[AT timeout; a delayed reply may appear later]"));
    return false;
}

void printHelp() {
    Serial.println(F(
        "\nSIM7090G commands\n"
        "  help                  This command list\n"
        "  init                  Initialize UART/modem only, no cellular wait\n"
        "  info                  Identity, firmware, IMEI\n"
        "  sim                   SIM readiness and ICCID\n"
        "  status                Signal, operator, registration, data/IP\n"
        "  diag                  Extended radio, PDP, error and flow-control queries\n"
        "  apn <name>            Set APN in RAM, empty username/password\n"
        "  mode m1|nb|auto       LTE-M, NB-IoT, or both; restarts radio\n"
        "  connect               One Loom data-connect attempt, 60 s registration wait\n"
        "  disconnect            Close the cellular data session\n"
        "  tcp <host> <port>     Open and close a TinyGSM TCP socket\n"
        "  http [host] [path]    HTTP GET on port 80, default example.com /\n"
        "  gps on|off|read|raw   Control/read GNSS; raw prints CGNSINF CSV\n"
        "  gps watch            Start GNSS and print a fix every 5 s\n"
        "  gps quiet            Stop periodic printing, leave receiver running\n"
        "  rtc                   Read Hypnos RTC in UTC\n"
        "  rtc sync              Set Hypnos RTC from cellular network time\n"
        "  sample                Measure/package Loom data and print JSON\n"
        "  off                   Checked modem shutdown; PWR button + init to resume\n"
        "  AT...                 Send an arbitrary AT command, 10 s reply deadline\n"
        "Blocking: TinyGSM data/socket calls can take minutes on network failure.\n"
        "After raw AT changes to power/GNSS/data state, use init and gps on again.\n"));
}

void printFix() {
    loomGnss::Fix fix;
    if (!lte.readGNSS(fix)) {
        Serial.println(F("No valid GNSS fix. Use gps on; put the antenna outdoors."));
        return;
    }
    char utc[32];
    snprintf(utc, sizeof(utc), "%04u-%02u-%02uT%02u:%02u:%02uZ", fix.year,
             fix.month, fix.day, fix.hour, fix.minute, fix.second);
    Serial.print(F("Fix UTC: "));
    Serial.print(utc);
    Serial.print(F("  lat: "));
    Serial.print(fix.latitude, 7);
    Serial.print(F("  lon: "));
    Serial.println(fix.longitude, 7);
}

void socketTest(const char *host, uint16_t port, const char *path) {
    LoomWatchdogPause watchdogPause;
    Client *client = lte.getClient();
    if (!client || lte.getCachedPowerStatus() != Loom_LTE::PowerStatus::AtReady) {
        Serial.println(F("Initialize and connect first."));
        return;
    }
    Serial.print(F("Opening TCP: "));
    Serial.print(host);
    Serial.print(':');
    Serial.println(port);
    if (!client->connect(host, port)) {
        Serial.println(F("TCP failed. Check connect/status, DNS, APN and destination."));
        client->stop();
        return;
    }
    Serial.println(F("TCP connected."));
    if (path) {
        client->print(F("GET "));
        client->print(path);
        client->print(F(" HTTP/1.0\r\nHost: "));
        client->print(host);
        client->print(F("\r\nConnection: close\r\n\r\n"));
        const uint32_t started = millis();
        size_t received = 0;
        while (static_cast<uint32_t>(millis() - started) < 15000 && received < 1024) {
            if (client->available()) {
                const int value = client->read();
                if (value >= 0) { Serial.write(value); ++received; }
            } else if (!client->connected()) {
                break;
            } else {
                delay(1);
            }
        }
        Serial.println(F("\n[HTTP output limited to 1024 bytes / 15 s after connect]"));
    }
    client->stop();
    Serial.println(F("Socket closed."));
}

void executeCommand(char *line) {
    LoomWatchdogPause watchdogPause;
    if ((line[0] == 'A' || line[0] == 'a') && (line[1] == 'T' || line[1] == 't')) {
        runAT(line, 10000);
        return;
    }
    char *command = strtok(line, " ");
    char *arg = strtok(nullptr, " ");
    if (!command) return;
    if (!strcmp(command, "help")) printHelp();
    else if (!strcmp(command, "init")) {
        watchGPS = false;
        Serial.println(lte.initializeModemOnly() ? F("Modem ready. Cellular data is manual.")
                                                : F("No modem. Check PWR button, wiring and power."));
    } else if (!strcmp(command, "info")) {
        runAT("ATI"); runAT("AT+CGMR"); runAT("AT+GSN");
    } else if (!strcmp(command, "sim")) {
        runAT("AT+CPIN?"); runAT("AT+CCID");
    } else if (!strcmp(command, "status")) {
        runAT("AT+CSQ"); runAT("AT+COPS?"); runAT("AT+CEREG?");
        runAT("AT+CGATT?"); runAT("AT+CNACT?");
    } else if (!strcmp(command, "diag")) {
        runAT("AT+CEER"); runAT("AT+CPSI?"); runAT("AT+CNMP?");
        runAT("AT+CMNB?"); runAT("AT+CGDCONT?"); runAT("AT+CGNAPN");
        runAT("AT+CNACT?"); runAT("AT+IFC?"); runAT("AT+CGNSPWR?");
    } else if (!strcmp(command, "apn") && arg) {
        if (strlen(arg) > 99 || strpbrk(arg, "\"\\")) {
            Serial.println(F("APN must be <=99 characters, without quotes or backslashes."));
            return;
        }
        lte.disconnect();
        lte.setCredentials(arg);
        Serial.println(F("APN set in RAM. Use connect."));
    } else if (!strcmp(command, "mode") && arg) {
        const char *mode = !strcmp(arg, "m1") ? "AT+CMNB=1" :
                           !strcmp(arg, "nb") ? "AT+CMNB=2" :
                           !strcmp(arg, "auto") ? "AT+CMNB=3" : nullptr;
        if (!mode) { Serial.println(F("Use mode m1, mode nb or mode auto.")); return; }
        watchGPS = false;
        if (!runAT("AT+CFUN=0", 10000)) return;
        const bool configured = runAT("AT+CNMP=38") && runAT(mode);
        const bool restored = runAT("AT+CFUN=1", 10000);
        Serial.println(configured && restored ? F("Radio mode selected. Use init, then connect.")
                                               : F("Mode setup failed. Check AT replies and CFUN."));
    } else if (!strcmp(command, "connect")) {
        Serial.println(F("Registering and opening data. TinyGSM activation may take minutes."));
        Serial.println(lte.connect(60000UL, 1) ? F("Data connected.") : F("Data failed. Use status/diag."));
    } else if (!strcmp(command, "disconnect")) {
        lte.disconnect(); Serial.println(F("Disconnect requested. Use status to verify."));
    } else if (!strcmp(command, "tcp") && arg) {
        char *portText = strtok(nullptr, " ");
        char *tail = nullptr;
        const unsigned long port = portText ? strtoul(portText, &tail, 10) : 0;
        if (!portText || !*portText || *tail || port == 0 || port > 65535) {
            Serial.println(F("Use tcp <host> <port 1..65535>.")); return;
        }
        socketTest(arg, static_cast<uint16_t>(port), nullptr);
    } else if (!strcmp(command, "http")) {
        const char *path = strtok(nullptr, " ");
        if (path && path[0] != '/') { Serial.println(F("Path must start with /.")); return; }
        socketTest(arg ? arg : "example.com", 80, path ? path : "/");
    } else if (!strcmp(command, "gps") && arg) {
        if (!strcmp(arg, "on") || !strcmp(arg, "watch")) {
            const bool started = lte.startGNSS();
            watchGPS = started && !strcmp(arg, "watch");
            lastGPS = millis();
            Serial.println(started ? F("GNSS started. Acquisition needs sky view and time.")
                                   : F("GNSS start failed. Use init and inspect power/AT replies."));
        } else if (!strcmp(arg, "off")) {
            watchGPS = false;
            Serial.println(lte.stopGNSS() ? F("GNSS stopped.") : F("GNSS stop failed."));
        } else if (!strcmp(arg, "read")) printFix();
        else if (!strcmp(arg, "raw")) runAT("AT+CGNSINF");
        else if (!strcmp(arg, "quiet")) watchGPS = false;
        else Serial.println(F("Use gps on/off/read/raw/watch/quiet."));
    } else if (!strcmp(command, "rtc")) {
        if (arg && !strcmp(arg, "sync")) {
            Serial.println(hypnos.networkTimeUpdate() ? F("RTC synchronized from network UTC.")
                                                     : F("No valid network time. RTC unchanged."));
        } else {
            DateTime now;
            if (hypnos.tryGetCurrentTime(now)) {
                char utc[32];
                snprintf(utc, sizeof(utc), "%04u-%02u-%02uT%02u:%02u:%02uZ", now.year(),
                         now.month(), now.day(), now.hour(), now.minute(), now.second());
                Serial.print(utc); Serial.println(F(" (RTC UTC)"));
            } else Serial.println(F("RTC read failed."));
        }
    } else if (!strcmp(command, "sample")) {
        manager.measure(); manager.package();
        manager.display_data();
    } else if (!strcmp(command, "off")) {
        watchGPS = false;
        lte.power_down();
        Serial.println(lte.canRemovePower() ? F("Modem off. Press PWR, wait for STAT, then init.")
                                           : F("Shutdown unconfirmed. Keep power applied."));
    } else Serial.println(F("Unknown/incomplete command. Enter help."));
}

void setup() {
    manager.beginSerial(false);
    hypnos.setCompileTime(__DATE__, __TIME__);
    hypnos.enable();
    hypnos.setNetworkInterface(&lte);
    lte.setAutoConnect(false);
    manager.initialize();
    Serial1.begin(115200);
    Serial.println(F("\nFeather M0 / Hypnos / SIM7090G console. Press the Click PWR button if STAT is off."));
    printHelp();
}

void loop() {
    // Bounded work keeps continuous modem output from trapping USB command input.
    for (size_t i = 0; i < 128 && Serial1.available(); ++i) Serial.write(Serial1.read());
    for (size_t i = 0; i < 160 && Serial.available(); ++i) {
        const char c = Serial.read();
        if (c == '\r' || c == '\n') {
            if (commandOverflow) Serial.println(F("Command too long; discarded."));
            else if (commandLength) {
                commandLine[commandLength] = '\0';
                executeCommand(commandLine);
                Serial.println(F("\nready>"));
            }
            commandLength = 0; commandOverflow = false;
        } else if (c == '\b' || c == 127) {
            if (!commandOverflow && commandLength) --commandLength;
        } else if (c >= 32 && c <= 126 && !commandOverflow) {
            if (commandLength < sizeof(commandLine) - 1) commandLine[commandLength++] = c;
            else commandOverflow = true;
        }
    }
    if (watchGPS && static_cast<uint32_t>(millis() - lastGPS) >= 5000) {
        lastGPS = millis();
        printFix();
    }
}
