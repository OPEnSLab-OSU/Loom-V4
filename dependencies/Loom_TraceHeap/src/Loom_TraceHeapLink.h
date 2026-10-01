#pragma once

// This header activates the library through Arduino's conditional dependency scan.
// Firmware implementation lives in Loom's shared, source-built allocator hooks.
#if !defined(ARDUINO_ARCH_SAMD)
#error "Loom's IDE heap hooks currently support SAMD/newlib. Use a supported core or disable LOOM_TRACE_HEAP."
#endif
