#pragma once
#ifndef LOOM_TRACE
#define LOOM_TRACE 0
#endif
#if LOOM_TRACE
#define LOOM_TRACE_CHECKPOINT(phase) do { ++traceCalls; tracePhase = phase; } while (false)
#else
#define LOOM_TRACE_CHECKPOINT(phase) do {} while (false)
#endif
