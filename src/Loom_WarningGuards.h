#pragma once

/*
 * Warning scope for third-party and Arduino core headers.
 *
 * Why this exists:
 * - The compile audit runs with `--warnings all`.
 * - Several vendor/core headers intentionally contain no-op compatibility
 *   hooks or documentation text that GCC reports repeatedly from every Loom
 *   translation unit that includes them.
 * - The repeated examples are `SERCOM.h`/`SPI.h` unused-parameter stubs and
 *   RadioHead comment text. Those are not Loom defects, but they bury Loom
 *   warnings in hundreds of duplicate lines.
 *
 * Portability:
 * - This is not tied to SAMD21 or any board macro. It only scopes compiler
 *   diagnostics for GCC-compatible Arduino toolchains.
 * - On non-GCC-compatible compilers the macros expand to nothing, so the
 *   includes keep their normal behavior.
 *
 * Usage rule:
 * - Wrap only external includes.
 * - Do not wrap Loom headers, user sketches, or function bodies. Loom code
 *   should still be compiled with the full warning set.
 */
#if defined(__GNUC__)
    #define LOOM_EXTERNAL_INCLUDE_BEGIN \
        _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Wunused-parameter\"") \
        _Pragma("GCC diagnostic ignored \"-Wcomment\"")
    #define LOOM_EXTERNAL_INCLUDE_END \
        _Pragma("GCC diagnostic pop")
#else
    #define LOOM_EXTERNAL_INCLUDE_BEGIN
    #define LOOM_EXTERNAL_INCLUDE_END
#endif
