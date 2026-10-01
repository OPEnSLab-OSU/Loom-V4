# Sketch-selected heap capture

Install this folder as a separate Arduino library beside Loom, named `Loom_TraceHeap`.
The Loom board package must ship it at its top-level `libraries/` directory; Arduino does
not discover dependencies nested inside Loom. No board recipe changes are required.

With `LOOM_TRACE=1` and `LOOM_TRACE_HEAP=1` before `Loom_TraceSketch.h`, ordinary IDE
Verify/Upload discovers this library and applies its allocator linker flags. Turning either
flag off removes the include, library and hooks from the firmware. Explicit CLI heap
builds continue to use Loom's own hook translation unit and skip this companion.

The startup message reports actual allocation-capture availability, and the saved session
records `heap_hooks:true` when the hooks are active. If the IDE reports a missing
`Loom_TraceHeapLink.h`, install this companion beside Loom or disable heap capture.
If the IDE was open during manual installation and cannot discover the new library, restart it once.

The `src/cortex-m0plus/libLoomTraceHeapLink.a` file is an **empty GNU archive** (8-byte
`!<arch>\n` header). Arduino applies library `ldflags` when a matching precompiled target
directory exists. `precompiled=true` also compiles the C++ source, so there is no opaque
prebuilt allocator code: the hook definitions come from Loom's shared `.inc` source.
Keep this archive in release packages. This setup is verified for SAMD21 Cortex M0+;
additional architectures/CPUs need their own validated allocator/linker configuration.

Capture begins when the sketch starts its trace recorder. Existing boot-time allocations
remain baseline objects; new allocator activity is intercepted thereafter. Existing bounded
buffer, allocator recursion guard and lost-event reporting are unchanged.
