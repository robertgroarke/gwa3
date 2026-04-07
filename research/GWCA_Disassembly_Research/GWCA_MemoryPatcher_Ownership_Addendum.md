# GWCA MemoryPatcher Ownership Addendum

## Scope

This pass follows the next logical question from the live patch dump:

- who actually creates the live `MemoryPatcher` objects
- whether the three live patch records can be tied back to a real GWCA subsystem
- what the staged patch bytes actually are in the live client

The result is not full ownership for all three records yet, but it does materially narrow the picture.

## `SetRedirect` is just a thin patch builder

The compiled export:

- [SetRedirect](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_1001df20_deep.log)

decompiles to a very small helper:

1. assert the `MemoryPatcher` object is not already valid
2. verify the target byte is a `CALL rel32` or `JMP rel32`
3. rebuild the 5-byte opcode plus new relative displacement
4. forward the final patch to `SetPatch(...)`

So `SetRedirect(...)` is not its own instrumentation system.
It is just a convenience wrapper over `SetPatch(...)`.

## `SetPatch` has one real code caller in this build

The caller scan for:

- [FindCallers 0x1001de70](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\findcallers_1001de70_deep.log)

shows six references total, but only two are meaningful code-level ones:

- `SetRedirect @ 0x1001df20`
- `FUN_100035d0 @ 0x100035d0`

The rest are data / entry references rather than additional discovered functions.

That means the compiled `gwca.dll` build does **not** appear to have a wide scatter of direct `SetPatch(...)` call sites.
Instead, the memory patcher surface is much narrower than the detour-hook surface.

## `FUN_100035d0` is a chat-side bootstrap routine

The key decompile is:

- [FUN_100035d0](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_100035d0_deep.log)

This function:

- scans for chat-related targets
- installs a cluster of hook detours
- builds callback maps / containers
- stages one direct 2-byte memory patch

The chat-side identity is strong because the same compiled routine resolves strings and assertions around:

- `CtChatEdit.cpp`
- `GmChatLog.cpp`
- `readOnly`

and its hook RVAs line up with the chat detour cluster we already see live:

- `0x4720`
- `0x4760`
- `0x47C0`
- `0x4810`
- `0x4860`
- `0x48C0`
- `0x4900`
- `0x4940`
- `0x4970`
- `0x49B0`

Those are exactly the same detour RVAs visible in the live hook table.

## Source cross-check: this corresponds to `ChatMgr`

The checked-in source crosswalk is:

- [ChatMgr.cpp](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWA%20Censured\GWCA-master\Source\ChatMgr.cpp)

That file has the same broad structure:

- scanner-driven chat target discovery
- chat callback registries
- hook creation for send/local/whisper/print/log/editable-text behavior

There is source/binary drift here:

- the current checked-in source does **not** show the extra `SetPatch(...)` call present in the compiled binary routine
- the compiled binary therefore appears to contain an older or divergent chat bootstrap path with one additional direct byte patch

But the module identity itself is still strong enough to call this a `ChatMgr`-side bootstrap in practice.

## Live patch bytes: the three objects are now semantically clearer

I extended:

- [live_gwca_runtime_dump.ps1](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\scripts\live_gwca_runtime_dump.ps1)

so the live report now includes:

- patched bytes
- original bytes
- live bytes at the target

The latest raw dump is in:

- [live_gwca_runtime_report.txt](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWA%20Censured\tests\live_gwca_runtime_report.txt)

## Live patch classification

### Patch 0

```text
target=0x008AFD06
size=0x2
patched_bytes=EB 0F
original_bytes=8B 45
live_bytes=8B 45
enabled=0
```

This is a dormant 2-byte branch-style patch.

It replaces ordinary instructions with a short jump when active, but in the current client state it is **not** applied live.

### Patch 1

```text
target=0x009F9362
size=0x1
patched_bytes=EB
original_bytes=74
live_bytes=EB
enabled=1
```

This is the only currently active patch object.

Semantically it flips a conditional short jump:

- original `0x74` = `JZ`
- patched `0xEB` = unconditional short `JMP`

So this record is an active condition-bypass patch.

### Patch 2

```text
target=0x008F898C
size=0x2
patched_bytes=90 90
original_bytes=75 0C
live_bytes=75 0C
enabled=0
```

This is a dormant 2-byte NOP patch.

Semantically it converts:

- original `JNZ +0x0C`

into:

- `NOP NOP`

So this is a classic branch-removal / assertion-bypass style patch when enabled.

## Strongest ownership inference so far

The compiled `FUN_100035d0` chat bootstrap stages one 2-byte patch and also installs the chat detour cluster.

One live patch object is:

- 2 bytes long
- dormant
- located near the chat-side function region already visible in the live hook table

The strongest current inference is therefore:

- at least one of the three live `MemoryPatcher` records belongs to the chat subsystem
- specifically, the best candidate is the dormant 2-byte branch-removal patch at `0x008F898C`

I am treating that as **high-confidence inferred**, not fully binary-proven, because the compiled routine uses a scanner result rather than a fixed literal target and the runtime report only shows the resolved address, not the originating object symbol.

## What this changes

This pass sharpens the overall replacement picture again:

- the memory patcher side is still small
- one of its records now plausibly belongs to `ChatMgr`
- the binary uses direct byte patches mainly as tiny condition/flow edits, not as a primary interception plane

So the engineering priority remains:

1. detour-backed subsystem replacement first
2. small memory-patch ownership cleanup second

## Best next step

The next logical reverse pass is:

1. identify the owner of the active `0x74 -> 0xEB` 1-byte patch at `0x009F9362`
2. identify the owner of the dormant `EB 0F` short-jump patch at `0x008AFD06`
3. build a final table:
   - patch target
   - bytes before/after
   - live state
   - most likely subsystem
   - confidence

At this point, though, the live memory patch set is no longer just three unexplained pointers.
