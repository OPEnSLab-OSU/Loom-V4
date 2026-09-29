# Following one Loom measurement

Loom lets an Arduino sketch run several sensors together and put their readings in one packet.
A **module** is one part of the device: a sensor, storage, a display, or a network connection.
The **Manager** keeps a list of modules and calls them in registration order.

The usual cycle is:

1. `initialize()` sets up the modules before the first reading.
2. `measure()` reads the hardware. Each sensor keeps its latest values in its own fields.
3. `package()` starts a fresh packet and asks the modules to add those values.
4. The sketch calls its storage or network module to save or send the packet.
5. For a sleeping device, the sketch prepares the modules for sleep and restores them after wake.

Reading, packaging, and sending are separate so the sketch can choose when to do each one.
For example, it can read often, save every reading to SD, and upload a batch less often.
Manager does not automatically send a packet or put the board to sleep.

## Where to start in the source

- `Module.h` defines the operations that modules share.
- `Loom_Manager.cpp` shows the measurement and packaging cycle.
- A sensor's `measure()` reads its hardware; `package()` shows its packet field names.
- `Logger.h` defines the debug calls and switches; `Logger.cpp` implements their output.
- `Loom_Multiplexer.cpp` discovers sensors behind a mux and selects their ports before reading.

Use names to explain an operation. Use comments to explain something the name cannot tell you,
such as a hardware delay, an ownership rule, a timeout, or the meaning of an invalid reading.
Keep hardware units visible in names or API documentation.

## Who keeps each object alive

Manager **borrows** the modules registered with it. It does not delete them. Modules created as
global sketch objects normally live for the entire run. A module created inside a function must
not be registered if it will disappear when that function returns.

The mux **owns** the sensors it creates during discovery. It deletes them when refreshing its
sensor list or when the mux is destroyed. Max retains its existing ownership of the actuator
pointers passed to it; those pointers must refer to objects that Max can safely delete.

Manager holds one JSON document. `getDocument()` returns a reference to it, and
`get_data_object()` returns a small view into it. These do not copy the whole packet. The next
`package()` clears the old packet, so finish using its views before that call. ArduinoJson can
retain pointers to constant C strings: names and string values passed to packet APIs must stay
valid until the packet has been saved or sent.

## Keeping RAM predictable

The Feather M0 has 32 KB of RAM shared by data, heap allocations, the call stack, and drivers.
The heap stores objects whose size or lifetime is decided while the program runs; the stack
holds local variables and active function calls. Both consume that same limited RAM.

Loom allocates Manager's JSON pool once and reuses it for each packet. Its module pointer list
reserves initial room during construction. The mux reuses its sensor-list capacity, although
refreshing sensors still deletes and creates driver objects. Register modules during setup and
avoid repeatedly building strings or growing containers in the measurement cycle.

Use a caller's buffer or stream a large output when possible. A large local array still costs
RAM even though it does not use the heap. Removing an include may improve dependencies without
changing the final program size. Confirm actual RAM, stack, and flash costs with the exact board
build and repeated-run measurements before claiming savings.

`isPacketValid()` checks whether the shared document overflowed. Check output limits separately:
a JSON packet's serialized text can be larger than its document pool. An incomplete record must
not be treated as a successful save or upload.

## Debugging without changing the measurement

The Wisp examples have quiet sketches and matching `_debug` sketches. Debug versions add
checkpoints and memory/logger output. Watchdog protection and recovery remain in both versions.

`FUNCTION_START` starts the existing function-summary object. C++ automatically records its
summary when the function exits, including early returns. `FUNCTION_END` marks the normal exit
in source; it does not emit a second summary.

`Loom_WarningGuards.h` marks external includes for the compiler-warning audit. It suppresses
only the selected warnings inside those include scopes. Loom implementations remain outside
them. See [the coding profile](SAMD21_CODING_PROFILE.md) for the detailed project rules and
[the design review](EMBEDDED_DESIGN_REVIEW.md) for changes and validation still needed.
