#include "Loom_LTE.h"
#include "Logger.h"

/*
 * SARA-R5 startup sequence
 *
 * LTE startup is split from network registration. A modem that never answers AT
 * is in a power, control-pin, UART, level-shifter, or modem-profile failure
 * state. SIM, APN, carrier profile, antenna, and coverage checks are only useful
 * after AT is stable.
 *
 * OPEnS/Jolteon R5 boards drive the SARA PWR_ON input through a MOSFET. The
 * board-level startup sequence asserts A5, waits for LOOM_LTE_R5_PWR_PULSE_MS,
 * releases A5, then gives the modem OS LOOM_LTE_R5_POST_PWR_SETTLE_MS before
 * sending AT.
 *
 * The SARA-R5 UART uses LOOM_LTE_R5_UART_BAUD for normal operation. Fallback
 * autobaud probing is a bench diagnostic and remains disabled unless explicitly
 * enabled in Loom_LTE_Config.h.
 *
 * RESET_N recovery is disabled by default. Enable it only after confirming the
 * reset-pin polarity and timing on the target board.
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
// Construct a configured LTE module from sketch-supplied APN credentials.
// The board version determines which PWR_ON sequence is used during power-up.
Loom_LTE::Loom_LTE(Manager& man, const char* apn, const char* user, const char* pass,
                   const int pin, LTE_VERSION version, const int rstPin,
                   LTE_MODEM modemType)
    : NetworkComponent("LTE"), modemType(modemType), manInst(&man),
      modem(createLteModem(modemType == LTE_MODEM::SARA_R5, SerialAT)){
    copyCredential(this->APN, apn, sizeof(this->APN));
    copyCredential(this->gprsUser, user, sizeof(this->gprsUser));
    copyCredential(this->gprsPass, pass, sizeof(this->gprsPass));
    this->powerPin = pin;
    this->resetPin = rstPin;

    lteBoardVersion = version;
    selectedBaud = isSaraR5() ? LOOM_LTE_R5_UART_BAUD : 9600UL;
    idlePowerPin();
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Construct an LTE module whose APN credentials will be supplied later from SD JSON.
Loom_LTE::Loom_LTE(Manager& man, LTE_MODEM modemType)
    : NetworkComponent("LTE"), modemType(modemType), manInst(&man),
      modem(createLteModem(modemType == LTE_MODEM::SARA_R5, SerialAT)){
    memset(APN, '\0', sizeof(APN));
    memset(gprsUser, '\0', sizeof(gprsUser));
    memset(gprsPass, '\0', sizeof(gprsPass));

    if(isSaraR5())
        lteBoardVersion = OPENS;
    selectedBaud = isSaraR5() ? LOOM_LTE_R5_UART_BAUD : 9600UL;

    idlePowerPin();
    manInst->registerModule(this);
    moduleInitialized = false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_LTE::~Loom_LTE(){ delete modem; }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Put a board-level control pin into its inactive state.
// For OPEnS R5 hardware this releases the SARA input through the MOSFET circuit.
void Loom_LTE::driveControlPinIdle(int pin){
    // Idle means the modem input is released. On OPEnS/Jolteon hardware the
    // Feather drives a MOSFET gate, so the Feather level is the inverse of the
    // actual SARA pin level.
    if(pin < 0)
        return;

    if(lteBoardVersion == OPENS){
        const uint8_t idleLevel = isSaraR5()
            ? (LOOM_LTE_R5_OPENS_CONTROL_ACTIVE_HIGH ? LOW : HIGH)
            : LOW;
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
// Put a board-level control pin into its active state.
// Active means the SARA-side input is being asserted by the board control circuit.
void Loom_LTE::driveControlPinActive(int pin){
    // Active means the SARA input is being pulled low through the board control
    // circuit. This is used for PWR_ON and RESET_N pulses.
    if(pin < 0)
        return;

    if(lteBoardVersion == OPENS){
        const uint8_t activeLevel = isSaraR5()
            ? (LOOM_LTE_R5_OPENS_CONTROL_ACTIVE_HIGH ? HIGH : LOW)
            : HIGH;
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
    // in Loom_LTE_Config.h so board startup can be tuned without editing logic.
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
    if(isSaraR5() && !LOOM_LTE_R5_ENABLE_RESET_RECOVERY)
        return;
    driveControlPinIdle(resetPin);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::prepareOptionalPowerRails(){
    // Most Loom sketches let the manager own Hypnos rail control. This optional
    // path exists for bare LTE debug sketches where the LTE library needs to
    // enable the 3.3V and 5V rails itself.
    if(isSaraR5() && LOOM_LTE_R5_ENABLE_POWER_RAIL_PINS){
        pinMode(LOOM_LTE_R5_3V3_RAIL_PIN, OUTPUT);
        pinMode(LOOM_LTE_R5_5V_RAIL_PIN, OUTPUT);
        digitalWrite(LOOM_LTE_R5_3V3_RAIL_PIN, LOOM_LTE_R5_3V3_RAIL_ON_LEVEL);
        digitalWrite(LOOM_LTE_R5_5V_RAIL_PIN, LOOM_LTE_R5_5V_RAIL_ON_LEVEL);
        delay(250);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Apply the board-specific power-on sequence.
// R5 OPEnS/Jolteon boards use the explicit startup path configured above.
void Loom_LTE::powerBoardOn(){
    // The OPEnS/Jolteon R5 board uses a OPEnS/Jolteon R5 startup sequence here:
    // A5 HIGH requests power-on through the control MOSFET, A5 LOW releases it,
    // and the following settle delay gives the modem OS time to expose AT.
    if(isSaraR5()){
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
        return;
    }

    // Preserve the proven 4.9 SARA-R4 board sequences exactly.
    pinMode(powerPin, OUTPUT);
    if(lteBoardVersion == OPENS){
        digitalWrite(powerPin, HIGH);
        delay(5000);
    }
    else{
        digitalWrite(powerPin, LOW);
        delay(3000);
    }
    pinMode(powerPin, INPUT);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::powerBoardOff(){
    // NOTE: We don't need to power off the sparkfun LTE board we can just use the power off command
    // Handle powering off the parkfun board
    // if(lteBoardVersion == OPENS){
    //      pinMode(powerPin, OUTPUT);
    //     digitalWrite(powerPin, LOW);
    //     delay(2500);
    //     pinMode(powerPin, INPUT);
    // }
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
        if(modem->testAT(1000L))
            return true;

        delay(300);
    }

    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Open the LTE UART at the configured primary baud and verify AT.
// Optional fallback probing is only for bench diagnostics.
bool Loom_LTE::selectWorkingBaud(uint32_t timeoutMs){
    // The host UART still needs one concrete baud before it can send the first
    // AT command. Keep that primary rate in Loom_LTE_Config.h so board startup
    // does not require editing this file.
    // Restore the configured primary baud before returning failure.
    const uint32_t primaryBaud = isSaraR5() ? LOOM_LTE_R5_UART_BAUD : 9600UL;
    selectedBaud = primaryBaud;
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

    if(isSaraR5() && LOOM_LTE_R5_SCAN_BAUDS_ON_FAILURE){
        // Fallback probing is for diagnostics only. The supported one-shot
        // autobaud rates are listed here, but the configured primary baud is always
        // tried first and skipped in this fallback loop.
        static const uint32_t supportedBauds[] = {
            9600UL,
            19200UL,
            38400UL,
            57600UL,
            115200UL,
            230400UL,
            460800UL,
            921600UL
        };

        for(uint8_t i = 0; i < (sizeof(supportedBauds) / sizeof(supportedBauds[0])); i++){
            // Skip the configured primary baud because it was already tested first.
            if(supportedBauds[i] == primaryBaud)
                continue;

            // Try the next supported one-shot autobaud rate.
            selectedBaud = supportedBauds[i];

            // Reopen the UART at the candidate baud.
            SerialAT.end();
            delay(100);
            SerialAT.begin(selectedBaud);
            delay(250);

            // Print each fallback attempt for debug traces.
            Serial.print(F("LTE UART baud probe: "));
            Serial.println(selectedBaud);

            // Accept the candidate baud only if the modem answers AT.
            if(waitForModemAT(3000L)){
                Serial.print(F("LTE UART baud selected: "));
                Serial.println(selectedBaud);
                return true;
            }
        }

        // Restore the configured primary baud before returning failure.
        selectedBaud = primaryBaud;
        SerialAT.end();
        delay(100);
        SerialAT.begin(selectedBaud);
        delay(250);
    }

    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LTE::initializeModemFromAT(){
    // TinyGSM initialization is separated from raw AT detection so the logs can
    // tell apart hardware/UART silence from a library-profile mismatch.
    if(modem->init()){
        modem->sendAT(F("+CMEE=2"));
        modem->waitResponse(5000L);
        modem->sendAT(F("+CEREG=2"));
        modem->waitResponse(5000L);
        return true;
    }

    logPlainFailure(F("INFO: the modem answered AT, but TinyGSM could not initialize it. Check that the selected SARA-R4/R5 constructor profile matches the hardware and the installed TinyGSM version supports that modem."));
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
    if(!isSaraR5())
        return;

    modem->sendAT(F("+CMEE=2"));
    modem->waitResponse(5000L);

    if(strlen(LOOM_LTE_R5_FORCE_OPERATOR_NUMERIC) > 0){
        Serial.print(F("LTE forcing operator numeric profile: "));
        Serial.println(LOOM_LTE_R5_FORCE_OPERATOR_NUMERIC);
        Serial.println(F("INFO: this skips broad operator scanning and asks the modem to register on the configured carrier code."));

        char copsCommand[48];
        snprintf(copsCommand, sizeof(copsCommand), "AT+COPS=1,2,\"%s\",%i", LOOM_LTE_R5_FORCE_OPERATOR_NUMERIC, LOOM_LTE_R5_FORCE_OPERATOR_ACT);
        if(!sendATExpectOK(copsCommand, 180000L))
            WARNING(F("INFO: forced operator registration did not return OK. Falling back to automatic network behavior."));
    }
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
    // Boot is handled in stages so the logs show exactly where startup fails.
    // In R5 startup sequence, the power pulse is sent first, then AT
    // detection proves that power, UART, and TinyGSM selection are usable.
    if(isSaraR5()){
#if !LOOM_LTE_R5_COMPAT_POWER_FIRST
        LOG(F("Checking if SARA-R5 is already awake before touching PWR_ON."));
        if(selectWorkingBaud(8000L)){
            LOG(F("Modem already answered AT. Skipping power pulse so we do not accidentally toggle it off."));
            if(initializeModemFromAT())
                return true;
        }
#else
        LOG(F("Using OPEnS/Jolteon R5 startup sequence."));
        LOG(F("INFO: this uses the OPEnS/Jolteon R5 startup sequence before AT probing."));
#endif
    }

    for(uint8_t attempt = 1; attempt <= 3; attempt++){
        char output[OUTPUT_SIZE];
        snprintf(output, OUTPUT_SIZE, "LTE modem boot attempt %u / 3", attempt);
        LOG(output);

        // PWR_ON is a stateful hardware control, not an idempotent reset. Pulse
        // it once, then retry UART/TinyGSM initialization without risking a
        // second pulse switching an already-running modem back off.
        if(attempt == 1){
            powerBoardOn();
        }
        else{
            LOG(F("Retrying modem initialization without another PWR_ON pulse."));
        }

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

        if(isSaraR5() && LOOM_LTE_R5_ENABLE_RESET_RECOVERY && attempt == 1 && resetPin >= 0){
            WARNING(F("INFO: trying a short RESET_N pulse because the R5 may have powered but failed to reach a clean AT-ready state."));
            pulseControlPin(resetPin, LOOM_LTE_R5_RESET_PULSE_MS, F("LTE RESET_N"));
            delay(8000);

            if(selectWorkingBaud(30000L)){
                LOG(F("Modem answered AT after reset."));
                if(initializeModemFromAT())
                    return true;
            }
        }

        if(attempt < 3)
            delay(3000);
    }

    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Full manager initialization: boot the modem, read identity, then try to open a data session.
// moduleInitialized tracks modem/AT readiness so a failed data session can be retried later.
void Loom_LTE::initialize(){
    // Manager initialization enters here. At the end of this function,
    // moduleInitialized means the modem booted and TinyGSM can issue AT commands.
    FUNCTION_START;
    char output[OUTPUT_SIZE];
    char ip[16];

    // Put the board-level control pins into their released states before boot.
    idlePowerPin();

    // RESET_N is only touched when reset recovery is explicitly enabled.
    idleResetPin();

    // Start up the modem and prove it can answer AT before network setup.
    power_up();

    // If power_up() could not reach AT/TinyGSM readiness, stop before SIM/APN checks.
    if(!powered){
        ERROR(F("LTE shield not detected or modem did not finish booting."));
        ERROR(F("INFO: the modem never reached the basic AT-command-ready state, so this is a boot/power/UART problem before carrier registration."));
        moduleInitialized = false;
        firstInit = false;
        FUNCTION_END;
        return;
    }

    // Get the modem identity after TinyGSM initialization has succeeded.
    String modemInfo = modem->getModemInfo();

    // Remove stray CR/LF characters before testing whether the response is usable.
    modemInfo.trim();

    // If no identity text came back, UART is alive but modem initialization is incomplete.
    if(modemInfo.length() == 0){
        ERROR(F("LTE modem info was empty."));
        ERROR(F("INFO: UART is alive, but the modem did not return identity text. This usually means it is not fully initialized yet or the wrong TinyGSM modem profile was selected."));
        moduleInitialized = false;
        firstInit = false;
        FUNCTION_END;
        return;
    }

    // Print the modem identity for field logs.
    snprintf(output, OUTPUT_SIZE, "Modem Information: %s", modemInfo.c_str());
    LOG(output);

    // Connect to the LTE network and open the APN/PDP data session.
    moduleInitialized = true;
    const bool connected = connect();

    // If we successfully connected to the LTE network print out some information.
    if(connected){
        LOG(F("Connected!"));

        // Print APN.
        snprintf(output, OUTPUT_SIZE, "APN: %s", APN);
        LOG(output);

        // Print signal quality as reported by the modem.
        snprintf(output, OUTPUT_SIZE, "Signal State: %i", modem->getSignalQuality());
        LOG(output);

        // Log IP address.
        ipToString(modem->localIP(), ip);
        snprintf(output, OUTPUT_SIZE, "Device IP Address: %s", ip);
        LOG(output);

        // verifyConnection() is intentionally left for the main loop or user sketch.
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

    if(powered){
        moduleInitialized = true;
        if(!firstInit)
            (void)connect();
        FUNCTION_END;
        return;
    }

    LOG(F("Powering up LTE modem."));
    TIMER_DISABLE;

    // Enable optional Hypnos rails only when this library owns rail control.
    prepareOptionalPowerRails();

    // Release PWR_ON before applying the OPEnS-compatible power pulse.
    idlePowerPin();

    // Leave RESET_N alone unless reset recovery has been explicitly enabled.
    idleResetPin();

    // Restart the host UART cleanly before the boot sequence.
    SerialAT.end();
    delay(100);

    // Preserve the 4.9 R4 UART while allowing an R5 object to select 115200.
    selectedBaud = isSaraR5() ? LOOM_LTE_R5_UART_BAUD : 9600UL;
    SerialAT.begin(selectedBaud);
    delay(250);

    // Power on the LTE board and verify the modem reaches a stable AT state.
    if(!bootModemWithRetries()){
        ERROR(F("Power-up failed: modem never reached a stable AT command state."));
        ERROR(F("INFO: this failure happened before SIM/APN/network checks. If 1.8V V_INT/ref is missing, the modem is not actually on. If 1.8V is present, check UART wiring, level shifting, and TinyGSM modem selection."));
        powered = false;
        moduleInitialized = false;
        TIMER_ENABLE;
        FUNCTION_END;
        return;
    }

    // Mark the modem as powered only after AT and TinyGSM initialization succeed.
    LOG(F("Powering up complete!"));
    powered = true;
    moduleInitialized = true;
    TIMER_ENABLE;

    // Connect to the network if we are powering up after the first initialization pass.
    if(!firstInit)
        (void)connect();

    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::power_down(){
    FUNCTION_START;
    if(powered && powerUp){
        LOG(F("Powering down LTE modem."));
        TIMER_DISABLE;
        modem->poweroff();
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
        json["RSSI"] = modem->getSignalQuality();
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_LTE::logSignalDiagnostic(){
    char output[OUTPUT_SIZE];
    int signal = modem->getSignalQuality();

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
    int simStatus = modem->getSimStatus();

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
    int regStatus = modem->getRegistrationStatus();

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
// Register on the cellular network and activate the APN/PDP data session.
// This stage is only entered after boot has already proven AT/TinyGSM readiness.
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

        modem->sendAT(F("+CFUN=1"));
        modem->waitResponse(10000L);

        // Inject the APN profile into PDP context 1.
        LOG(F("Injecting APN profile..."));
        modem->setPdpContext(APN);
        modem->waitResponse(10000L);

        // Wait for the modem to register on the cellular network.
        LOG(F("Waiting for network..."));
        if(!modem->waitForNetwork(600000L)){
            WARNING(F("No response from network."));
            WARNING(F("INFO: the modem did not finish cellular registration in time. This is usually SIM, antenna, carrier coverage, band support, or power stability."));
            logNetworkDiagnostics();
        }
        else if(!modem->isNetworkConnected()){
            WARNING(F("Modem did not report network registration."));
            WARNING(F("INFO: the modem answered, but it is not registered to the cellular network yet."));
            logNetworkDiagnostics();
        }
        else{
            LOG(F("Connected to network!"));

            snprintf(output, OUTPUT_SIZE, "Attempting to connect to LTE Network: %s", APN);
            LOG(output);

            // Clear any stale PDP state before opening the data session.
            modem->gprsDisconnect();
            delay(500);

            // Open the APN/PDP data session.
            if(modem->gprsConnect(APN, gprsUser, gprsPass)){
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
        modem->gprsDisconnect();
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

    // Connect to TinyGSM's example endpoint to prove socket routing works.
    Client* client = modem->getClient();
    if(!client->connect("vsh.pp.ua", 80)){
        ERROR(F("Failed to contact TinyGSM example."));
        ERROR(F("INFO: the modem claims it has a data session, but a TCP socket could not be opened. This usually means PDP/DNS/routing/carrier data path trouble."));
        logNetworkDiagnostics();
        client->stop();
        FUNCTION_END;
        return false;
    }
    else{
        // Request the logo.txt to display.
        client->print("GET /TinyGSM/logo.txt HTTP/1.1\r\n");
        client->print("Host: vsh.pp.ua\r\n");
        client->print("Connection: close\r\n\r\n");
        client->println();

        // Print response data to the serial monitor while the socket remains open.
        uint32_t timeout = millis();
        while(client->connected() && millis() - timeout < 10000L){
            // Print available data.
            while(client->available() && millis() - timeout < 10000L){
                char c = client->read();
                Serial.print(c);
                timeout = millis();
                returnStatus = true;
            }
        }

        Serial.println();
        client->stop();
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

    // Load cellular credentials when present.
    if(!doc["apn"].isNull()){
        copyCredential(APN, doc["apn"] | "", sizeof(APN));
        copyCredential(gprsUser, doc["user"] | "", sizeof(gprsUser));
        copyCredential(gprsPass, doc["pass"] | "", sizeof(gprsPass));
    }

    // Override the LTE PWR_ON pin when the SD config supplies one.
    if(doc.containsKey("pin"))
        powerPin = doc["pin"].as<int>();

    // Override the LTE RESET_N pin when the SD config supplies one.
    if(doc.containsKey("reset_pin"))
        resetPin = doc["reset_pin"].as<int>();

    moduleInitialized = true;
    free(json);
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Client* Loom_LTE::getClient() { return modem->getClient(); }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_LTE::getNetworkTime(int* year, int* month, int* day, int* hour, int* minute, int* second, float* tz) {
    // TinyGSM overwrites tz with the modem's CCLK suffix. On the SARA-R410,
    // the returned date/time fields are already UTC; applying that suffix here
    // shifts UTC a second time (for example, 23:28 becomes 07:28 the next day
    // in PST). Hypnos owns UTC-to-local conversion, so preserve its configured
    // timezone and pass the modem clock fields through unchanged.
    const float configuredTimezone = (tz != nullptr) ? *tz : 0.0f;
    float modemTimezone = configuredTimezone;
    const bool updated = modem->getNetworkTime(
        year, month, day, hour, minute, second, &modemTimezone
    );

    if(tz != nullptr)
        *tz = configuredTimezone;

    return updated;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
