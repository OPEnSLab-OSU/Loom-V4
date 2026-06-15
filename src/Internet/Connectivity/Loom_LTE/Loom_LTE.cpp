#include "Loom_LTE.h"
#include "Logger.h"

/*
 * SARA-R5 boot strategy
 *
 * The intermittent R5 failure looks like a bad modem state at boot, not a normal
 * APN failure. The important rule is to avoid blindly pulsing PWR_ON. On SARA-R5,
 * PWR_ON is already pulled up inside the modem and the host requests actions by
 * pulling it low through the board MOSFET. A pulse that is too long or a pulse
 * sent while the modem is already awake can look like a power toggle instead of
 * a clean boot.
 *
 * The flow below is deliberately staged:
 * 1. Put PWR_ON in its idle state.
 * 2. For OPEnS/Jolteon R5 compatibility, use the same power pulse shape as
 *    the old working Loom code: A5 HIGH for about 1 second, then A5 LOW.
 * 3. Wait for the modem OS before asking for AT.
 * 4. Keep the SARA UART fixed at 115200 unless fallback probing is explicitly enabled.
 * 5. Leave RESET_N alone by default because the old working path never drove A4.
 * 6. Print plain-language logs and raw AT snapshots when any stage fails.
 */

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::copyCredential(char* dst, const char* src, size_t dstSize){
    // All LTE credentials live in fixed-size buffers so field deployments do not
    // depend on heap-backed strings staying valid after setup.
    if(dstSize == 0)
        return;

    if(src == nullptr)
        src = "";

    strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_LTE::Loom_LTE(Manager& man, const char* apn, const char* user, const char* pass, const int pin, LTE_VERSION version, const int rstPin) : NetworkComponent("LTE"), manInst(&man), modem(SerialAT), client(modem){
    copyCredential(this->APN, apn, sizeof(this->APN));
    copyCredential(this->gprsUser, user, sizeof(this->gprsUser));
    copyCredential(this->gprsPass, pass, sizeof(this->gprsPass));
    this->powerPin = pin;
    this->resetPin = rstPin;

    lteBoardVersion = version;
    idlePowerPin();
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_LTE::Loom_LTE(Manager& man) : NetworkComponent("LTE"), manInst(&man), modem(SerialAT), client(modem){
    memset(APN, '\0', sizeof(APN));
    memset(gprsUser, '\0', sizeof(gprsUser));
    memset(gprsPass, '\0', sizeof(gprsPass));

#if defined(TINY_GSM_MODEM_SARAR5)
    lteBoardVersion = OPENS;
#endif

    idlePowerPin();
    manInst->registerModule(this);
    moduleInitialized = false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::driveControlPinIdle(int pin){
    // Idle means the modem input is released. On OPEnS/Jolteon hardware the
    // Feather drives a MOSFET gate, so the Feather level is the inverse of the
    // actual SARA pin level.
    if(pin < 0)
        return;

    if(lteBoardVersion == OPENS){
#if defined(TINY_GSM_MODEM_SARAR5)
        const uint8_t idleLevel = LOOM_LTE_R5_OPENS_CONTROL_ACTIVE_HIGH ? LOW : HIGH;
#else
        const uint8_t idleLevel = LOW;
#endif
        digitalWrite(pin, idleLevel);
        pinMode(pin, OUTPUT);
    }
    else{
        digitalWrite(pin, LOW);
        pinMode(pin, INPUT);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::driveControlPinActive(int pin){
    // Active means the SARA input is being pulled low through the board control
    // circuit. This is used for PWR_ON and RESET_N pulses.
    if(pin < 0)
        return;

    if(lteBoardVersion == OPENS){
#if defined(TINY_GSM_MODEM_SARAR5)
        const uint8_t activeLevel = LOOM_LTE_R5_OPENS_CONTROL_ACTIVE_HIGH ? HIGH : LOW;
#else
        const uint8_t activeLevel = HIGH;
#endif
        digitalWrite(pin, activeLevel);
        pinMode(pin, OUTPUT);
    }
    else{
        digitalWrite(pin, LOW);
        pinMode(pin, OUTPUT);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::pulseControlPin(int pin, uint32_t pulseMs, const __FlashStringHelper* label){
    // Keep the pulse helper boring and explicit. All pulse timing is configured
    // in Loom_LTE_Config.h so board bring-up can be tuned without editing logic.
    if(pin < 0)
        return;

    Serial.print(label);
    Serial.print(F(" pulse for "));
    Serial.print(pulseMs);
    Serial.println(F(" ms"));

    driveControlPinActive(pin);
    delay(pulseMs);
    driveControlPinIdle(pin);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::idlePowerPin(){
    driveControlPinIdle(powerPin);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::idleResetPin(){
#if defined(TINY_GSM_MODEM_SARAR5)
    if(!LOOM_LTE_R5_ENABLE_RESET_RECOVERY)
        return;
#endif
    driveControlPinIdle(resetPin);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::prepareOptionalPowerRails(){
    // Most Loom sketches let the manager own Hypnos rail control. This optional
    // path exists for bare LTE debug sketches where the LTE library needs to
    // enable the 3.3V and 5V rails itself.
#if defined(TINY_GSM_MODEM_SARAR5) && LOOM_LTE_R5_ENABLE_POWER_RAIL_PINS
    pinMode(LOOM_LTE_R5_3V3_RAIL_PIN, OUTPUT);
    pinMode(LOOM_LTE_R5_5V_RAIL_PIN, OUTPUT);
    digitalWrite(LOOM_LTE_R5_3V3_RAIL_PIN, LOOM_LTE_R5_3V3_RAIL_ON_LEVEL);
    digitalWrite(LOOM_LTE_R5_5V_RAIL_PIN, LOOM_LTE_R5_5V_RAIL_ON_LEVEL);
    delay(250);
#endif
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::powerBoardOn(){
    // The OPEnS/Jolteon R5 board powers reliably with the original Loom pulse:
    // A5 HIGH for about 1 second, then A5 LOW, then a fixed boot settle delay.
    // Keep this explicit because it is the known-good hardware behavior.
#if defined(TINY_GSM_MODEM_SARAR5)
    if(lteBoardVersion == OPENS){
        Serial.print(F("LTE PWR_ON OPENS pulse HIGH for "));
        Serial.print(LOOM_LTE_R5_PWR_PULSE_MS);
        Serial.println(F(" ms"));

        pinMode(powerPin, OUTPUT);
        digitalWrite(powerPin, HIGH);
        delay(LOOM_LTE_R5_PWR_PULSE_MS);
        digitalWrite(powerPin, LOW);

        Serial.print(F("LTE waiting for modem OS for "));
        Serial.print(LOOM_LTE_R5_POST_PWR_SETTLE_MS);
        Serial.println(F(" ms"));
        delay(LOOM_LTE_R5_POST_PWR_SETTLE_MS);
        return;
    }

    pulseControlPin(powerPin, LOOM_LTE_R5_PWR_PULSE_MS, F("LTE PWR_ON"));
    delay(LOOM_LTE_R5_POST_PWR_SETTLE_MS);
#else
    const uint32_t powerPulseMs = (lteBoardVersion == OPENS) ? 1000UL : 3000UL;
    pulseControlPin(powerPin, powerPulseMs, F("LTE PWR_ON"));
#endif
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::powerBoardOff(){
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::logPlainFailure(const __FlashStringHelper* message){
    ERROR(message);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LTE::waitForModemAT(uint32_t timeoutMs){
    // AT is the lowest-level proof that power, UART routing, baud, and level
    // shifting are all working. This intentionally runs before SIM/APN/network.
    uint32_t start = millis();

    while(SerialAT.available())
        SerialAT.read();

    while((uint32_t)(millis() - start) < timeoutMs){
        if(modem.testAT(1000L))
            return true;

        delay(300);
    }

    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LTE::selectWorkingBaud(uint32_t timeoutMs){
    // SARA-R5 should be contacted at 115200 first. Print that first attempt so
    // the UART log matches the actual order and does not make it look like the
    // code skipped the modem's expected baud.
    selectedBaud = 115200UL;
    SerialAT.end();
    delay(100);
    SerialAT.begin(selectedBaud);
    delay(250);

    Serial.print(F("LTE UART baud probe: "));
    Serial.println(selectedBaud);

    if(waitForModemAT(timeoutMs)){
        Serial.print(F("LTE UART baud selected: "));
        Serial.println(selectedBaud);
        return true;
    }

#if defined(TINY_GSM_MODEM_SARAR5) && LOOM_LTE_R5_SCAN_BAUDS_ON_FAILURE
    // Fallback baud probing is for desperate recovery only. Normal R5 operation
    // should stay fixed at 115200 so passthrough debugging is predictable.
    const uint32_t fallbackBauds[] = {9600UL, 19200UL, 38400UL, 57600UL};

    for(uint8_t i = 0; i < (sizeof(fallbackBauds) / sizeof(fallbackBauds[0])); i++){
        selectedBaud = fallbackBauds[i];

        SerialAT.end();
        delay(100);
        SerialAT.begin(selectedBaud);
        delay(250);

        Serial.print(F("LTE UART baud probe: "));
        Serial.println(selectedBaud);

        if(waitForModemAT(3000L)){
            Serial.print(F("LTE UART baud selected: "));
            Serial.println(selectedBaud);
            return true;
        }
    }

    selectedBaud = 115200UL;
    SerialAT.end();
    delay(100);
    SerialAT.begin(selectedBaud);
    delay(250);
#endif

    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LTE::initializeModemFromAT(){
    // TinyGSM initialization is separated from raw AT detection so the logs can
    // tell apart hardware/UART silence from a library-profile mismatch.
    if(modem.init()){
        modem.sendAT(GF("+CMEE=2"));
        modem.waitResponse(5000L);
        modem.sendAT(GF("+CEREG=2"));
        modem.waitResponse(5000L);
        return true;
    }

    logPlainFailure(F("INFO: the modem answered AT, but TinyGSM could not initialize it. Check that the selected SARA-R4/R5 modem define matches the hardware and the installed TinyGSM version supports that modem."));
    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LTE::sendATExpectOK(const char* command, uint32_t timeoutMs){
    // Small raw-AT helper for setup commands that TinyGSM does not wrap cleanly.
    while(SerialAT.available())
        SerialAT.read();

    SerialAT.print(command);
    SerialAT.print("\r\n");

    uint32_t start = millis();
    String response;

    while((uint32_t)(millis() - start) < timeoutMs){
        while(SerialAT.available()){
            char c = SerialAT.read();
            response += c;
            if(response.indexOf("OK") >= 0)
                return true;
            if(response.indexOf("ERROR") >= 0)
                return false;
        }
        delay(1);
    }

    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::applyR5NetworkHints(){
    // These are non-destructive hints. They make the modem verbose, ensure full
    // functionality, and optionally skip broad carrier search when a numeric
    // operator code is configured.
#if defined(TINY_GSM_MODEM_SARAR5)
    modem.sendAT(GF("+CFUN=1"));
    modem.waitResponse(10000L);

    modem.sendAT(GF("+CMEE=2"));
    modem.waitResponse(5000L);

    if(strlen(LOOM_LTE_R5_FORCE_OPERATOR_NUMERIC) > 0){
        Serial.print(F("LTE forcing operator numeric profile: "));
        Serial.println(LOOM_LTE_R5_FORCE_OPERATOR_NUMERIC);
        Serial.println(F("INFO: this skips broad operator scanning and asks the modem to register on the configured carrier code."));

        char copsCommand[48];
        snprintf(copsCommand, sizeof(copsCommand), "AT+COPS=1,2,\"%s\",%i", LOOM_LTE_R5_FORCE_OPERATOR_NUMERIC, LOOM_LTE_R5_FORCE_OPERATOR_ACT);
        if(!sendATExpectOK(copsCommand, 180000L))
            WARNING(F("INFO: forced operator registration did not return OK. Falling back to automatic network behavior."));
    }
#endif
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::logRawAT(const char* command, uint32_t timeoutMs){
    // Raw AT snapshots are printed only in failure paths so normal logs stay
    // readable, but failed field boots leave something useful on the UART.
    Serial.print(F("LTE raw AT "));
    Serial.print(command);
    Serial.print(F(": "));

    while(SerialAT.available())
        SerialAT.read();

    SerialAT.print(command);
    SerialAT.print("\r\n");

    uint32_t start = millis();
    bool sawResponse = false;

    while((uint32_t)(millis() - start) < timeoutMs){
        while(SerialAT.available()){
            char c = SerialAT.read();
            Serial.write(c);
            sawResponse = true;
            start = millis();
        }
        delay(1);
    }

    if(!sawResponse)
        Serial.print(F("(no response)"));

    Serial.println();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::logBootChecklist(){
    WARNING(F("Boot checklist:"));
    WARNING(F("1. If there is no AT response, measure the SARA 1.8V V_INT/ref rail. Missing 1.8V means the modem is not actually on."));
    WARNING(F("2. If 1.8V is present but AT is silent, check UART TX/RX crossover, the 1.8V level shifter, and that Serial1 is on the LTE pins."));
    WARNING(F("3. If AT works but SIM/network fails, this is no longer a boot problem. Check SIM, antenna, APN, carrier profile, and coverage."));
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LTE::bootModemWithRetries(){
    // Boot is treated as a state machine instead of a fixed delay. This prevents
    // the one-in-several-boots case where the modem was already awake and the
    // firmware accidentally sends a new power pulse into a live modem.
#if defined(TINY_GSM_MODEM_SARAR5)
#if !LOOM_LTE_R5_COMPAT_POWER_FIRST
    LOG(F("Checking if SARA-R5 is already awake before touching PWR_ON."));
    if(selectWorkingBaud(8000L)){
        LOG(F("Modem already answered AT. Skipping power pulse so we do not accidentally toggle it off."));
        if(initializeModemFromAT())
            return true;
    }
#else
    LOG(F("Using OPEnS-compatible power-first boot path."));
    LOG(F("INFO: this mirrors the old working Loom sequence before AT probing."));
#endif
#endif

    for(uint8_t attempt = 1; attempt <= 3; attempt++){
        char output[OUTPUT_SIZE];
        snprintf(output, OUTPUT_SIZE, "LTE modem boot attempt %u / 3", attempt);
        LOG(output);

        powerBoardOn();

        if(selectWorkingBaud(LOOM_LTE_R5_BOOT_AT_TIMEOUT_MS)){
            LOG(F("Modem answered AT."));
            if(initializeModemFromAT())
                return true;
        }
        else{
            WARNING(F("Modem did not answer AT during the boot window."));
            WARNING(F("INFO: the host can not talk to the modem yet. This is before SIM/APN/network. Check PWR_ON timing, 5V/3.8V power stability, the 1.8V V_INT/ref voltage, UART TX/RX, and the level shifter."));
            logRawAT("AT", 2000L);
            logBootChecklist();
        }

#if defined(TINY_GSM_MODEM_SARAR5) && LOOM_LTE_R5_ENABLE_RESET_RECOVERY
        if(attempt == 1 && resetPin >= 0){
            WARNING(F("INFO: trying a short RESET_N pulse because the R5 may have powered but failed to reach a clean AT-ready state."));
            pulseControlPin(resetPin, LOOM_LTE_R5_RESET_PULSE_MS, F("LTE RESET_N"));
            delay(8000);

            if(selectWorkingBaud(30000L)){
                LOG(F("Modem answered AT after reset."));
                if(initializeModemFromAT())
                    return true;
            }
        }
#endif

        if(attempt < 3)
            delay(3000);
    }

    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::initialize(){
    // Manager initialization enters here. At the end of this function,
    // moduleInitialized means both boot and the data session succeeded.
    FUNCTION_START;
    char output[OUTPUT_SIZE];
    char ip[16];

    idlePowerPin();
    idleResetPin();
    power_up();

    if(!powered){
        ERROR(F("LTE shield not detected or modem did not finish booting."));
        ERROR(F("INFO: the modem never reached the basic AT-command-ready state, so this is a boot/power/UART problem before carrier registration."));
        moduleInitialized = false;
        firstInit = false;
        FUNCTION_END;
        return;
    }

    String modemInfo = modem.getModemInfo();
    modemInfo.trim();

    if(modemInfo.length() == 0){
        ERROR(F("LTE modem info was empty."));
        ERROR(F("INFO: UART is alive, but the modem did not return identity text. This usually means it is not fully initialized yet or the wrong TinyGSM modem profile was selected."));
        moduleInitialized = false;
        firstInit = false;
        FUNCTION_END;
        return;
    }

    snprintf(output, OUTPUT_SIZE, "Modem Information: %s", modemInfo.c_str());
    LOG(output);

    moduleInitialized = connect();

    if(moduleInitialized){
        LOG(F("Connected!"));

        snprintf(output, OUTPUT_SIZE, "APN: %s", APN);
        LOG(output);

        snprintf(output, OUTPUT_SIZE, "Signal State: %i", modem.getSignalQuality());
        LOG(output);

        ipToString(modem.localIP(), ip);
        snprintf(output, OUTPUT_SIZE, "Device IP Address: %s", ip);
        LOG(output);

        LOG(F("Module successfully initialized!"));
    }
    else{
        ERROR(F("Module failed to initialize."));
        ERROR(F("INFO: the modem booted, but cellular registration or data-session activation failed."));
    }

    firstInit = false;
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::power_up(){
    // Power-up only proves that the modem can answer AT and be initialized.
    // Cellular registration and APN/PDP activation happen in connect().
    FUNCTION_START;

    if(batch_sd != nullptr && !firstInit){
        if(batch_sd->getCurrentBatch() != batch_sd->getBatchSize() - 1){
            powerUp = false;
            FUNCTION_END;
            return;
        }
        else{
            powerUp = true;
        }
    }

    LOG(F("Powering up LTE modem."));
    TIMER_DISABLE;

    prepareOptionalPowerRails();
    idlePowerPin();
    idleResetPin();
    SerialAT.end();
    delay(100);
    selectedBaud = 115200UL;
    SerialAT.begin(selectedBaud);
    delay(250);

    if(!bootModemWithRetries()){
        ERROR(F("Power-up failed: modem never reached a stable AT command state."));
        ERROR(F("INFO: this failure happened before SIM/APN/network checks. If 1.8V V_INT/ref is missing, the modem is not actually on. If 1.8V is present, check UART wiring, level shifting, and TinyGSM modem selection."));
        powered = false;
        moduleInitialized = false;
        TIMER_ENABLE;
        FUNCTION_END;
        return;
    }

    LOG(F("Powering up complete!"));
    powered = true;
    TIMER_ENABLE;

    if(!firstInit && moduleInitialized)
        moduleInitialized = connect();

    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::power_down(){
    FUNCTION_START;
    if(moduleInitialized && powerUp){
        LOG(F("Powering down LTE modem."));
        TIMER_DISABLE;
        modem.poweroff();
        delay(5000);
        powered = false;
        TIMER_ENABLE;
        LOG(F("Powering down complete!"));
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::package(){
    FUNCTION_START;
    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        json["RSSI"] = modem.getSignalQuality();
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::logSignalDiagnostic(){
    char output[OUTPUT_SIZE];
    int signal = modem.getSignalQuality();

    snprintf(output, OUTPUT_SIZE, "LTE diagnostic: signal quality value = %i", signal);
    LOG(output);

    if(signal == 99){
        WARNING(F("INFO: the modem does not know signal quality yet. It may not be registered or the antenna path is not usable."));
    }
    else if(signal <= 5){
        WARNING(F("INFO: signal is very weak. Registration and socket setup may be inconsistent."));
    }
    else if(signal <= 10){
        WARNING(F("INFO: signal is marginal. Expect slow or variable registration."));
    }
    else{
        LOG(F("INFO: signal is probably good enough for registration."));
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::logSimDiagnostic(){
    int simStatus = modem.getSimStatus();

    switch(simStatus){
        case 1:
            LOG(F("INFO: SIM is ready."));
            break;
        case 2:
            WARNING(F("INFO: SIM is locked. Check SIM PIN/PUK requirements."));
            break;
        default:
            WARNING(F("INFO: SIM is not ready or the modem could not read SIM status."));
            break;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::logRegistrationDiagnostic(){
    int regStatus = modem.getRegistrationStatus();

    switch(regStatus){
        case 1:
            LOG(F("INFO: modem is registered on the home network."));
            break;
        case 5:
            LOG(F("INFO: modem is registered while roaming."));
            break;
        case 2:
            WARNING(F("INFO: modem is still searching for a network."));
            break;
        case 3:
            WARNING(F("INFO: network registration was denied. Check SIM activation, carrier support, bands, and APN/account provisioning."));
            break;
        case 0:
            WARNING(F("INFO: modem is not registered and is not actively searching."));
            break;
        default:
            WARNING(F("INFO: network registration state is unknown."));
            break;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::logNetworkDiagnostics(){
    logRawAT("AT+CPIN?", 3000L);
    logRawAT("AT+CSQ", 3000L);
    logRawAT("AT+CEREG?", 3000L);
    logRawAT("AT+COPS?", 3000L);
    logRawAT("AT+CGDCONT?", 3000L);
    logRawAT("AT+CGATT?", 3000L);
    logRawAT("AT+CGACT?", 3000L);
    logRawAT("AT+CGPADDR=1", 3000L);
    logRawAT("AT+CEER", 3000L);
    logSimDiagnostic();
    logRegistrationDiagnostic();
    logSignalDiagnostic();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LTE::connect(){
    // Connection is intentionally retried independently from boot. A clean AT
    // boot can still fail later because of SIM, antenna, APN, carrier account,
    // tower coverage, or marginal power during transmit bursts.
    FUNCTION_START;
    char output[OUTPUT_SIZE];
    const uint8_t maxAttempts = 5;

    if(strlen(APN) == 0){
        WARNING(F("INFO: APN is empty. The modem may register, but the data session will probably fail."));
    }

    if(!powered){
        ERROR(F("Cannot connect LTE modem before power-up completes."));
        ERROR(F("INFO: connection was requested before the modem was ready for AT commands."));
        FUNCTION_END;
        return false;
    }

    TIMER_DISABLE;

    if(isConnected()){
        TIMER_ENABLE;
        FUNCTION_END;
        return true;
    }

    applyR5NetworkHints();

    for(uint8_t attempt = 1; attempt <= maxAttempts; attempt++){
        snprintf(output, OUTPUT_SIZE, "LTE connect attempt %u / %u", attempt, maxAttempts);
        LOG(output);

        modem.sendAT(GF("+CFUN=1"));
        modem.waitResponse(10000L);

        LOG(F("Injecting APN profile..."));
        modem.sendAT(GF("+CGDCONT=1,\"IP\",\""), APN, GF("\""));
        modem.waitResponse(10000L);

        LOG(F("Waiting for network..."));
        if(!modem.waitForNetwork(600000L)){
            WARNING(F("No response from network."));
            WARNING(F("INFO: the modem did not finish cellular registration in time. This is usually SIM, antenna, carrier coverage, band support, or power stability."));
            logNetworkDiagnostics();
        }
        else if(!modem.isNetworkConnected()){
            WARNING(F("Modem did not report network registration."));
            WARNING(F("INFO: the modem answered, but it is not registered to the cellular network yet."));
            logNetworkDiagnostics();
        }
        else{
            LOG(F("Connected to network!"));

            snprintf(output, OUTPUT_SIZE, "Attempting to connect to LTE Network: %s", APN);
            LOG(output);

            modem.gprsDisconnect();
            delay(500);

            if(modem.gprsConnect(APN, gprsUser, gprsPass)){
                LOG(F("Successfully Connected!"));
                delay(6000);
                TIMER_ENABLE;
                FUNCTION_END;
                return true;
            }

            snprintf(output, OUTPUT_SIZE, "PDP context connection failed on attempt %u / %u", attempt, maxAttempts);
            WARNING(output);
            WARNING(F("INFO: cellular registration worked, but the data session did not open. Check APN, SIM data plan, carrier provisioning, and IPv4/IPv6 expectations."));
            logNetworkDiagnostics();
        }

        if(attempt < maxAttempts)
            delay(10000);
    }

    ERROR(F("Connection reattempts exceeded. Connection failed."));
    ERROR(F("INFO: boot succeeded, but the modem could not reach a usable internet session after all retries."));
    TIMER_ENABLE;
    FUNCTION_END;
    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::disconnect(){
    FUNCTION_START;
    if(moduleInitialized){
        modem.gprsDisconnect();
        delay(200);
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LTE::verifyConnection(){
    // This checks a real socket after TinyGSM reports a data session. It helps
    // separate "registered and has PDP" from "can actually route traffic".
    FUNCTION_START;
    bool returnStatus = false;
    LOG(F("Attempting to verify internet connection..."));

    if(!moduleInitialized || !powered){
        ERROR(F("LTE modem is not initialized."));
        ERROR(F("INFO: the code tried to open an internet socket before LTE initialization finished."));
        FUNCTION_END;
        return false;
    }

    if(!client.connect("vsh.pp.ua", 80)){
        ERROR(F("Failed to contact TinyGSM example."));
        ERROR(F("INFO: the modem claims it has a data session, but a TCP socket could not be opened. This usually means PDP/DNS/routing/carrier data path trouble."));
        logNetworkDiagnostics();
        client.stop();
        FUNCTION_END;
        return false;
    }
    else{
        client.print("GET /TinyGSM/logo.txt HTTP/1.1\r\n");
        client.print("Host: vsh.pp.ua\r\n");
        client.print("Connection: close\r\n\r\n");
        client.println();

        uint32_t timeout = millis();
        while(client.connected() && millis() - timeout < 10000L){
            while(client.available() && millis() - timeout < 10000L){
                char c = client.read();
                Serial.print(c);
                timeout = millis();
                returnStatus = true;
            }
        }

        Serial.println();
        client.stop();
    }

    if(!returnStatus)
        WARNING(F("INFO: TCP connected, but no response data arrived before timeout."));

    TIMER_RESET;
    FUNCTION_END;
    return returnStatus;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::debugPassthrough(){
    // Direct USB-to-LTE bridge. Use Both NL & CR in the Serial Monitor and type
    // commands like AT, AT+CPIN?, AT+CSQ, AT+CEREG?, AT+CGATT?, and AT+CEER.
    while(SerialAT.available())
        Serial.write(SerialAT.read());

    while(Serial.available())
        SerialAT.write(Serial.read());
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::loadConfigFromJSON(char* json){
    FUNCTION_START;
    char output[OUTPUT_SIZE];
    StaticJsonDocument<300> doc;
    DeserializationError deserialError = deserializeJson(doc, json);

    if(deserialError != DeserializationError::Ok){
        snprintf(output, OUTPUT_SIZE, "There was an error reading the LTE credentials from SD: %s", deserialError.c_str());
        ERROR(output);
        ERROR(F("INFO: LTE config JSON could not be parsed, so APN/user/pass may be missing."));
        moduleInitialized = false;
        free(json);
        FUNCTION_END;
        return;
    }

    if(!doc["apn"].isNull()){
        copyCredential(APN, doc["apn"] | "", sizeof(APN));
        copyCredential(gprsUser, doc["user"] | "", sizeof(gprsUser));
        copyCredential(gprsPass, doc["pass"] | "", sizeof(gprsPass));
    }

    if(doc.containsKey("pin"))
        powerPin = doc["pin"].as<int>();

    if(doc.containsKey("reset_pin"))
        resetPin = doc["reset_pin"].as<int>();

    moduleInitialized = true;
    free(json);
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Client* Loom_LTE::getClient() { return (Client*)&client; }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LTE::getNetworkTime(int* year, int* month, int* day, int* hour, int* minute, int* second, float* tz) {
    if(modem.getNetworkTime(year, month, day, hour, minute, second, tz)){
        DateTime time = DateTime(*year, *month, *day, *hour, *minute, *second) + TimeSpan(0, ((int)(*tz)) * (-1), 0, 0);
        *year = time.year();
        *month = time.month();
        *day = time.day();
        *hour = time.hour();
        *minute = time.minute();
        *second = time.second();
        return true;
    }
    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
