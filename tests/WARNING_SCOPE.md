# Temporary warning-scope instrumentation

This structure exists only to make the compile audit distinguish warnings from
Loom code from warnings emitted by the Arduino core or third-party libraries.
Keep it in a separate, instrumentation-only commit so it can be reverted before
shipping a Loom release.

The structure consists of:

- `src/Loom_WarningGuards.h`, which defines the diagnostic push/pop macros.
- A `Loom_WarningGuards.h` include in each instrumented Loom source file.
- `LOOM_EXTERNAL_INCLUDE_BEGIN` and `LOOM_EXTERNAL_INCLUDE_END` around external
  include groups.
- The batch and PowerShell files in `tests`, which compile examples and split
  warning reports into Loom-owned and external warnings.

The macros affect compiler diagnostics only. They do not add runtime behavior or
change the library ABI.

## Adding the scope to a source file

Add the guard header before the first external include. Wrap only Arduino core,
board-package, vendor, or other third-party headers:

```cpp
#include "Loom_WarningGuards.h"

LOOM_EXTERNAL_INCLUDE_BEGIN
#include <Arduino.h>
#include <ThirdPartySensor.h>
LOOM_EXTERNAL_INCLUDE_END

#include "Loom_InternalHeader.h"
```

Use separate begin/end pairs when preprocessor conditions or Loom-owned includes
split external include groups. Every begin must have a matching end in the same
file.

Do not wrap:

- Loom-owned headers.
- Sketch code or complete source files.
- Function bodies or declarations.
- Standard C and C++ library headers such as `<stdint.h>`, `<array>`, or
  `<algorithm>`.

When a new dependency is added, first determine whether its header belongs to
Loom itself or to the Arduino core/package/another library. Only the latter goes
inside an external scope.

Useful checks from the Loom library root:

```powershell
rg -n "Loom_WarningGuards|LOOM_EXTERNAL_INCLUDE" src examples
rg -l "LOOM_EXTERNAL_INCLUDE_BEGIN" src
```

Then run one of the audit entry points documented in `tests/README.md`.

## Removing the structure

### Reversible one-click removal

From Windows Explorer, double-click:

```text
tests\warning_scope_strip.cmd
```

The script removes only these items from `src`:

- `#include "Loom_WarningGuards.h"` lines.
- `LOOM_EXTERNAL_INCLUDE_BEGIN` and `LOOM_EXTERNAL_INCLUDE_END` lines.
- `src/Loom_WarningGuards.h`.

Before changing the live files, it builds a context-aware Git patch containing
the exact removed lines and guard-header contents. The patch and a readable
manifest are saved under `tests/.warning_scope_state`. Whole source files are
not used as the restore mechanism, so restoring cannot silently overwrite later
4.9 code changes. If the surrounding code changes enough that the patch is no
longer safe, restoration refuses to modify the source and retains the patch.
The state directory is intentionally not ignored by Git; commit it with the
stripped source when the ability to restore that exact instrumentation must be
preserved.

To reinsert the instrumentation, double-click:

```text
tests\warning_scope_restore.cmd
```

After a successful restore, the consumed state directory is deleted. This
allows the strip/restore cycle to be repeated. The audit harness itself remains
in `tests` while the source instrumentation is stripped.

### Permanent/manual removal

The preferred removal is to revert the dedicated instrumentation commit:

```text
git revert <instrumentation-commit>
```

If it must be removed manually:

1. Remove every `LOOM_EXTERNAL_INCLUDE_BEGIN` and
   `LOOM_EXTERNAL_INCLUDE_END` line from `src`.
2. Remove every `#include "Loom_WarningGuards.h"` line.
3. Delete `src/Loom_WarningGuards.h`.
4. Delete the audit batch/PowerShell files and this documentation if the test
   harness is not being shipped.
5. Delete generated `tests/loom_compile_audit_*` report directories and any
   retained temporary build folders.
6. Run the search below. It should return no matches:

```powershell
rg -n "Loom_WarningGuards|LOOM_EXTERNAL_INCLUDE" src examples
```

Removing this instrumentation must not replace source files with files from the
older SARA R5 debug branch. Remove only the guard include, scope macros, test
harness, and generated audit output so the underlying 4.9 code and later fixes
remain intact.
