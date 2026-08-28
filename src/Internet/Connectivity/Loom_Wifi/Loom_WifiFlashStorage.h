/*
 * Focused flash-record storage for Loom WiFi credentials on the ATSAMD21G18A.
 *
 * The NVM operations are derived from Arduino's FlashStorage library, originally written by
 * Cristian Maglie and distributed under the GNU Lesser General Public License v2.1 or later.
 * Keeping only the record API prevents the separate FlashAsEEPROM translation unit from reserving
 * 1 KB of SRAM in every Loom binary.
 */
#pragma once

#include <Arduino.h>

class Loom_WifiFlash {
  public:
    Loom_WifiFlash(const void *flashAddress, uint32_t size);

    bool write(const void *data) { return write(flashAddress, data, flashSize); }
    bool erase() { return erase(flashAddress, flashSize); }
    void read(void *data) { memcpy(data, const_cast<const void *>(flashAddress), flashSize); }

  private:
    bool write(const volatile void *flashPointer, const void *data, uint32_t size);
    bool erase(const volatile void *flashPointer, uint32_t size);
    bool eraseRow(const volatile void *flashPointer);

    const volatile void *flashAddress;
    const uint32_t flashSize;
};

template <class T> class Loom_WifiFlashStorage {
  public:
    explicit Loom_WifiFlashStorage(const void *flashAddress) : flash(flashAddress, sizeof(T)) {}

    bool write(const T &data) { return flash.erase() && flash.write(&data); }

    T read() {
        T data = {};
        flash.read(&data);
        return data;
    }

  private:
    Loom_WifiFlash flash;
};
