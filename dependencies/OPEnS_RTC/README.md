# Hardened OPEnS RTC for Loom

This dependency starts from the supplied JeeLabs/OPEnS RTC implementation and targets the DS3231
on Feather M0/Hypnos deployments. It intentionally preserves the field-proven direct-`Wire` alarm
sequence while adding checks needed for a stable re-release.

## Preserved behavior

- STOP-separated register selection and read transactions.
- Alarm registers are written before the interrupt is armed.
- Control and status are written contiguously while arming.
- Both alarm flags are cleared before relying on the active-low `INT/SQW` wake line.
- The old `setAlarm(...)` API remains available.

## Hardened behavior

- No dynamic allocation or Arduino `String` use.
- Six-byte `DateTime`; `text(char *, size_t)` supports caller-owned formatting storage.
- DS3231 presence, I2C return values, byte counts, dates, alarms, and arm readback are checked.
- Alarm flag clearing preserves oscillator-stop and 32 kHz status bits.
- Correct weekday-alarm encoding and signed temperature decoding.
- RTClib-compatible Alarm 1/2 method names for Loom 4.9 source compatibility.
- One-second bound around temperature-conversion polling.

`lastOperationSucceeded()` must be checked after value-returning calls such as `now()`,
`lostPower()`, and `getAlarm1()`. Boolean mutating calls return their status directly.
`lastI2CError()` returns the underlying `Wire.endTransmission()` code where available; internal
codes 5 and 6 mean a short read and invalid data respectively.

## Packaging and limitation

Copy this complete `OPEnS_RTC` folder into the Arduino package's `libraries` directory when
assembling the release dependency bundle. Repository CLI builds can point `--libraries` at the
top-level `dependencies` directory.

The loom4 SAMD `Wire`/SERCOM implementation itself has unbounded hardware polling. Consequently a
physically stuck SDA/SCL line can still block inside the core before this library can return an
error. Core timeout and bus-recovery changes need separate hardware validation.
