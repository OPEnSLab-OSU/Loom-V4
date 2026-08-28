/*
 * Copyright (c) 2015 Arduino LLC. All rights reserved.
 * Original FlashStorage implementation written by Cristian Maglie.
 * SAMD21-only fixed-record adaptation for Loom.
 *
 * This library is free software; you can redistribute it and/or modify it under the terms of the
 * GNU Lesser General Public License as published by the Free Software Foundation; either version
 * 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
 * the GNU Lesser General Public License for more details.
 *
 * This file intentionally implements only Loom's fixed-record operations and does not instantiate
 * the FlashAsEEPROM translation unit or its 1 KB SRAM buffer.
 */
#include "Loom_WifiFlashStorage.h"

namespace {
constexpr uint32_t SAMD21_PAGE_SIZE = 64;
constexpr uint32_t SAMD21_ROW_SIZE = 256;
constexpr uint32_t NVM_READY_TIMEOUT_MS = 100;

bool waitForNvmReady() {
    const uint32_t started = millis();
    while (NVMCTRL->INTFLAG.bit.READY == 0) {
        if (static_cast<uint32_t>(millis() - started) >= NVM_READY_TIMEOUT_MS) {
            return false;
        }
    }
    return true;
}

uint32_t paddedFlashWord(const uint8_t *data, const uint8_t byteCount) {
    union {
        uint32_t value;
        uint8_t bytes[4];
    } result = {0xFFFFFFFFUL};
    for (uint8_t index = 0; index < byteCount; ++index) {
        result.bytes[index] = data[index];
    }
    return result.value;
}

} // namespace

Loom_WifiFlash::Loom_WifiFlash(const void *address, uint32_t size)
    : flashAddress(address), flashSize(size) {}

bool Loom_WifiFlash::write(const volatile void *flashPointer, const void *data, uint32_t size) {
    uint32_t bytesRemaining = size;
    volatile uint32_t *destination =
        static_cast<volatile uint32_t *>(const_cast<volatile void *>(flashPointer));
    const uint8_t *source = static_cast<const uint8_t *>(data);

    NVMCTRL->CTRLB.bit.MANW = 1;

    while (bytesRemaining > 0) {
        NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_PBC;
        if (!waitForNvmReady()) {
            return false;
        }

        for (uint32_t index = 0; index < SAMD21_PAGE_SIZE / 4 && bytesRemaining > 0; ++index) {
            const uint8_t byteCount = min(bytesRemaining, static_cast<uint32_t>(4));
            *destination++ = paddedFlashWord(source, byteCount);
            source += byteCount;
            bytesRemaining -= byteCount;
        }

        NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_WP;
        if (!waitForNvmReady()) {
            return false;
        }
    }
    return true;
}

bool Loom_WifiFlash::erase(const volatile void *flashPointer, uint32_t size) {
    const uint8_t *address = static_cast<const uint8_t *>(const_cast<const void *>(flashPointer));
    while (size > SAMD21_ROW_SIZE) {
        if (!eraseRow(address)) {
            return false;
        }
        address += SAMD21_ROW_SIZE;
        size -= SAMD21_ROW_SIZE;
    }
    return eraseRow(address);
}

bool Loom_WifiFlash::eraseRow(const volatile void *flashPointer) {
    NVMCTRL->ADDR.reg = reinterpret_cast<uint32_t>(flashPointer) / 2;
    NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_ER;
    return waitForNvmReady();
}
