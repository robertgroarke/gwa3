---
name: RenderHook crash investigation
description: Intermittent crash in naked render detour — add esp,4 stack leak suspected
type: project
---

The RenderHook naked detour crashes GW intermittently (~30-90s after install).

**Symptoms:**
- Crash during ChangeTarget or Move shellcode execution
- Sometimes survives full 114-test suite, sometimes crashes at char select
- Watchdog detected heartbeat freeze at ~2800 frames

**Root cause investigation:**
- `add esp, 4` replays original game code at hook site
- AutoIt's RenderingModProc also replays `add esp,4` + `cmp [DisableRendering],1`
- BUT: the JMP detour enters mid-function, the `add esp,4` may consume stack
  that wasn't set up because we bypassed the code before the hook point
- Each frame leaks 4 bytes of stack → overflow after ~2800 frames (~47s)

**Why:** The 5-byte JMP overwrites the first instruction(s) at the hook site.
The code BEFORE the hook site pushes something that `add esp,4` was meant to
clean up. Since we JMP past that push, the `add esp,4` removes 4 bytes of
the calling function's stack frame instead.

**How to apply:** Need to examine the ACTUAL original bytes at the Render hook
site to understand the stack discipline. The fix may be to save/restore ESP
around the detour body, or to not replay `add esp,4` but instead adjust the
return address to skip the original `add esp,4` instruction.
