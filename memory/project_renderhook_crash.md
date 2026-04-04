---
name: RenderHook crash investigation
description: Mid-function render hook crashes in-game after ~2300 frames, stable at char select — trampoline or MinHook conflict suspected
type: project
---

## Definitive Bisection (2026-04-03)

The naked render detour at `Offsets::Render` (mid-function 5-byte JMP) crashes GW Reforged
approximately 2200-2600 heartbeats (~37-43 seconds) after entering a map. Stable indefinitely
at character select screen.

### What was ruled out:
- **Register corruption**: pushad/popad alone = stable. fxsave/fxrstor didn't help.
- **Shellcode dispatch**: disabling all `call [s_savedCommand]` after map load = still crashes
- **FPU/SSE/MXCSR corruption**: full fxsave/fxrstor = still crashes  
- **Integrity check/patch overwrite**: E9 JMP byte confirmed intact at crash time
- **Move/ChangeTarget specifics**: routing through GameThread = still crashes

### What causes the crash:
The mere EXISTENCE of the 5-byte JMP + trampoline replay at the Render hook site crashes
the in-game render path. Even with the absolute minimal detour (`inc heartbeat; jmp trampoline`),
the in-game render loop freezes after ~2300 frames. Char select is unaffected.

### Suspected root causes:
1. **Trampoline relocation issue**: original 10 bytes (`83 C4 04 83 3D XX XX XX XX 00`)
   execute from a VirtualAlloc'd address instead of the original location. Although the
   instructions use absolute addressing, something about the execution context matters in-game.
2. **MinHook conflict**: GameThread hooks the SAME FUNCTION (0x303D10) at the prologue,
   while RenderHook patches mid-function at 0x303D9E (142 bytes in). Both hooks in the
   same function might interfere.
3. **Game-internal watchdog/CRC**: the game may check code integrity specifically during
   in-game rendering (not at char select).

### Current workaround:
Move and ChangeTarget route through `GameThread::Enqueue` (MinHook at function entry).
The render hook is still needed for pre-game ButtonClick (Play button at char select).

**How to apply:** The render hook should be REMOVED or REPLACED with a MinHook-based
approach. Do not spend time on register save/restore or FPU/SSE — the issue is the
mid-function JMP+trampoline pattern itself in the in-game render context.
