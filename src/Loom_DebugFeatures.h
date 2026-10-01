#pragma once

// Library-wide compile switches: pass with compiler.cpp.extra_flags so the
// sketch and every Loom translation unit use the same settings. Defaults keep
// existing runtime setter APIs working. The debug build helper applies these
// automatically when -Diagnostics off is selected.
#ifndef LOOM_COMPILE_MUX_DEBUG
#define LOOM_COMPILE_MUX_DEBUG 1
#endif
#ifndef LOOM_COMPILE_SD_WRITE_DEBUG
#define LOOM_COMPILE_SD_WRITE_DEBUG 1
#endif

// Comparison override: keep a sketch's scan list, but use the complete mux loader.
#ifndef LOOM_MUX_FORCE_ALL_DRIVERS
#define LOOM_MUX_FORCE_ALL_DRIVERS 0
#endif
