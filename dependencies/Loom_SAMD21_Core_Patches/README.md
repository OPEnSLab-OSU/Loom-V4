# Experimental SAMD21 I²C timeouts

These patches preserve the original Feather M0 timeout investigation against the official Loom 4.9 core. They are research inputs for an isolated test copy of the board platform. The release uses the official core files.

- `SERCOM-timeout.patch` adds bus-state and error checks and caps several address, data and command waits at 100 ms.
- `Wire-timeout.patch` limits the requested-byte wait and returns the number of bytes received.

Both patches reconstruct the old investigation snapshots when applied from the root of a copy of the official core. The original SHA-256 hashes are in the [dependency verification script](../../tests/verify_patched_dependencies.ps1). Use fault injection on hardware before treating either patch as a release change.
