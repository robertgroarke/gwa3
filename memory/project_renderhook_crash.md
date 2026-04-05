---
name: RenderHook crash investigation
description: Mid-function render hook crashed in-game after ~2300 frames — FIXED by removing patch on map load
type: project
---

## Resolution (2026-04-05)

**Fixed**: `SetMapLoaded(true)` now calls `Shutdown()` unconditionally, restoring original bytes
and freeing the trampoline before the crash window (~2300 frames / ~37-43s after map load).

The hook is only needed for pre-game char select ButtonClick dispatch. Once in-game,
GameThread's MinHook at the function prologue handles all dispatch.

## Root Cause (confirmed via bisection 2026-04-03)

The naked render detour at `Offsets::Render` (mid-function 5-byte JMP) crashes GW Reforged
approximately 2200-2600 heartbeats (~37-43 seconds) after entering a map. Stable indefinitely
at character select screen.

The mere EXISTENCE of the 5-byte JMP + trampoline replay at the Render hook site crashes
the in-game render path. Even the minimal detour (`inc heartbeat; jmp trampoline`) crashes
because the trampoline relocates original bytes (`83 C4 04 83 3D XX XX XX XX 00`) to
VirtualAlloc'd memory — something about execution context matters in-game but not at char select.

### What was ruled out:
- Register corruption (pushad/popad alone = stable, fxsave/fxrstor didn't help)
- Shellcode dispatch (disabling all `call [s_savedCommand]` after map load = still crashes)
- FPU/SSE/MXCSR corruption (full fxsave/fxrstor = still crashes)
- Integrity check (E9 JMP byte confirmed intact at crash time)

### Suspected contributing factors:
1. **Trampoline relocation**: original bytes execute from wrong address context
2. **MinHook conflict**: GameThread hooks same function at prologue, RenderHook patches mid-function
3. **Game-internal CRC**: possible code integrity check during in-game rendering only

**How to apply:** The mid-function JMP+trampoline pattern must never persist into in-game rendering.
If future work needs a render-thread hook in-game, use MinHook at the function entry (like GameThread).
