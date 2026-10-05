#pragma once

#include <cstdint>

class Manager;
class SDManager;

/** Attach the one optional library-owned recorder to a static/sketch-lifetime
 * Manager and SDManager. Call before initialize(), or after it to start immediately.
 * Main-loop/setup calls only. Duplicate attachment with identical settings is harmless; conflicting sessions
 * are rejected. No allocations or SD writes occur merely to configure startup.
 * A failed start/append stops this capture attempt for the boot; no hidden retries. */
bool loomTraceAttach(Manager &manager, SDManager *sd, bool heapHooks, uint8_t heapWindowEvents);
