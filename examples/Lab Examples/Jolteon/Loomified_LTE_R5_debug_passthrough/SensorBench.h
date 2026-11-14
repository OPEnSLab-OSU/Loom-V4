#pragma once

static bool branchStart(uint8_t channel) {
  const uint8_t mask = uint8_t(1u << channel);
  uint8_t selected = 0;
  Serial.print(F("Mux channel ")); Serial.println(channel);
  return muxSelect(mask) && muxRead(selected) && selected == mask;
}

static bool branchEnd() {
  if (muxSelect(0)) return true;
  Serial.println(F("Mux deselect failed. Isolate the branch or power-cycle if it holds the bus LOW."));
  return false;
}

static void scanBranch(uint8_t channel) {
  if (!rootBus(MUX)) return;
  bool upstream[128] = {};
  for (uint8_t address=8; address<=0x77; ++address) {
    upstream[address] = ping(address);
    if (bus.fault) { Serial.println(F("Root bus timeout; scan stopped.")); return; }
  }
  if (branchStart(channel)) scan(nullptr,upstream);
  else Serial.println(F("Channel select failed."));
  branchEnd();
}

static bool seesawRead(uint8_t addr, uint8_t module, uint8_t reg,
                       uint8_t *bytes, size_t count, unsigned delayUs) {
  const uint8_t command[] = {module, reg};
  if (!bus.transfer(addr, command, sizeof(command), nullptr, 0)) return false;
  delayMicroseconds(delayUs);
  return bus.transfer(addr, nullptr, 0, bytes, count);
}

static bool readSoil(uint8_t addr) {
  uint8_t id = 0, temperature[4], touch[2];
  if (!seesawRead(addr, 0x00, 0x01, &id, 1, 1000) || (id != 0x55 && id != 0x87)) {
    Serial.println(F("No supported seesaw hardware ID."));
    return false;
  }
  if (!seesawRead(addr, 0x00, 0x04, temperature, sizeof(temperature), 1000) ||
      !seesawRead(addr, 0x0F, 0x10, touch, sizeof(touch), 3000)) return false;
  const uint32_t encoded = (uint32_t(temperature[0]) << 24) | (uint32_t(temperature[1]) << 16) |
                           (uint32_t(temperature[2]) << 8) | temperature[3];
  const uint16_t capacitance = uint16_t(touch[0] << 8) | touch[1];
  if (capacitance == 0xffff) return false;
  Serial.print(F("STEMMA soil temperature C=")); Serial.print(int32_t(encoded) / 65536.0f, 3);
  Serial.print(F(" capacitance counts=")); Serial.println(capacitance);
  return true;
}

static uint8_t gasChecksum(const uint8_t *bytes) {
  // Match the bundled DFRobot driver: six bytes after the 0xFF header.
  uint8_t sum = 0;
  for (uint8_t i=1; i<=6; ++i) sum += bytes[i];
  return uint8_t(0u - sum);
}

static bool gasExchange(uint8_t address, uint8_t command, uint8_t argument, uint8_t (&reply)[9]) {
  uint8_t request[10] = {0, 0xff, 1, command, argument};
  request[9] = gasChecksum(request + 1);
  if (!bus.transfer(address, request, sizeof(request), nullptr, 0)) return false;
  delay(10);
  return bus.transfer(address, request, 1, nullptr, 0) &&
         bus.transfer(address, nullptr, 0, reply, sizeof(reply)) &&
         reply[0] == 0xff && reply[1] == command && reply[8] == gasChecksum(reply);
}

static bool readGasCo(uint8_t addr) {
  uint8_t response[9];
  // Select passive acquisition, then fetch and validate a fresh response.
  if (!gasExchange(addr, 0x78, 0x04, response) || response[2] != 1) {
    Serial.println(F("DFGas passive acquisition setup failed."));
    return false;
  }
  delay(1000);
  if (!gasExchange(addr, 0x86, 0, response)) {
    Serial.println(F("DFGas concentration response missing or checksum invalid."));
    return false;
  }
  if (response[4] != 0x04 || response[5] > 2) {
    Serial.println(F("DFGas response is not a recognized CO reading."));
    return false;
  }
  const float scale[] = {1.0f, 0.1f, 0.01f};
  const float ppm = ((uint16_t(response[2]) << 8) | response[3]) * scale[response[5]];
  Serial.print(F("DFGas CO ppm=")); Serial.println(ppm, response[5]);
  return true;
}

static bool sensorAddress(const char *arg, uint32_t &address) {
  if (!arg) return false;
  if (arg[0]=='0' && (arg[1]=='x' || arg[1]=='X')) {
    if (!isxdigit(uint8_t(arg[2])) || strlen(arg)>4) return false;
    char *end = nullptr;
    address = strtoul(arg+2, &end, 16);
    if (*end) return false;
  } else if (!unsignedNumber(arg,address,0x77)) return false;
  return address>=8 && address<=0x77 && address!=muxAddr;
}

static void muxSensor(bool gas, const char *channelArg, const char *addressArg) {
  uint32_t channel = 0, address = gas ? 0x74 : 0x36;
  if (!unsignedNumber(channelArg, channel, 7) || (addressArg && !sensorAddress(addressArg,address))) {
    Serial.println(F("Use dfgas read <0..7> [0x74|0x75] or soil read <0..7> [0x36]."));
    return;
  }
  if (!rootBus(MUX)) return;
  const bool autoGas = gas && !addressArg;
  if (address == muxAddr || ping(address) || bus.fault ||
      (autoGas && (muxAddr == 0x75 || ping(0x75) || bus.fault))) {
    Serial.println(F("Address conflicts with the upstream bus, or the bus timed out."));
    return;
  }
  bool ok = branchStart(channel);
  if (ok && autoGas && !ping(address) && !bus.fault) address = 0x75;
  if (ok && !bus.fault && ping(address)) {
    Serial.print(F("Sensor ACK at 0x")); hex(address); Serial.println();
    ok = gas ? readGasCo(address) : readSoil(address);
  } else {
    Serial.print(F("Sensor branch/address unavailable at 0x")); hex(address); Serial.println();
    ok = false;
  }
  ok = branchEnd() && ok;
  Serial.println(ok ? F("[PASS] Sensor measurement received.") : F("[FAIL] Sensor measurement or mux cleanup failed."));
}
