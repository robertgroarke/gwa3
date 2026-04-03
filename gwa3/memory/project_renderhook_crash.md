---
name: RenderHook crash investigation
description: Intermittent crash in render detour — original bytes are add esp,4 + cmp, trampoline approach now used
type: project
---

The RenderHook naked detour crashes GW intermittently (~30-90s after install).

**Current approach (2026-04-03):**
- Trampoline copies original 10 bytes from hook site + JMP back to hookAddr+0xA
- Naked detour: saves ESP, pushad/pushfd, processes queue, popfd/popad, restores ESP, jmp trampoline
- Original bytes at hook: `83 C4 04` (add esp,4) + `83 3D 54748C00 00` (cmp [0x8C7454], 0)
- These ARE the real game instructions, NOT a stack leak — confirmed by dumping bytes and matching AutoIt

**Symptoms:**
- Crash is intermittent: sometimes full 114-test suite passes, sometimes crashes at map load
- Move shellcode works for 200-unit test but may crash later
- Watchdog (heartbeat-based) successfully detects render freeze and auto-kills GW

**Key finding (2026-04-03):**
- Removing `add esp,4` causes immediate crash before Play click (confirms it's required)
- The `cmp` compares against game address 0x008C7454, not our s_disableRendering
- Crash root cause still unknown — may be in shellcode dispatch or game state corruption

**How to apply:** The `add esp,4` and `cmp` MUST be replayed. Do not remove them.
The crash is NOT caused by stack leak. Investigate shellcode calling convention,
ring buffer race conditions, or game state corruption from CtoS packets.
