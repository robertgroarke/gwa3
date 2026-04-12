# Sparkfly Movement/Cast Debug Handoff

## Your Task

Continue validating and fixing the Sparkfly movement + combat dispatch in the C++ Froggy bot. Three architectural fixes were just committed but NOT yet tested together. Run the Sparkfly validator, observe the results, and fix whatever remains.

## Critical Context Files (Read These First)

1. `AGENTS.md` — agent rules, engine hook constraints, account registry
2. `AGENT_ACCOUNT_REGISTRY.md` — you are MARVIN (account index 3)
3. `gwa3/FROGGY_SPARKFLY_MOVE_CAST_DEBUG.md` — full investigation history
4. `gwa3/MOVEMENT_SKILL_DISPATCH_COMPARISON.md` — how GWA2/GWCA/Py4GW/gwa3 dispatch Move/UseSkill/ChangeTarget
5. `MULTI_AGENT_BUILD_ARCHITECTURE.md` — build lane isolation

## Your Lane

- Account index: 3, Character: `Starvin M A R V I N`
- Build preset: `marvin`, Build dir: `gwa3/build_marvin`
- DLL: `gwa3_marvin.dll`, Pipe: `\\.\pipe\gwa3_llm_marvin`
- Do NOT touch other agent lanes/build dirs/accounts.

## What Was Done This Session

### Three architectural fixes committed (untested together):

**Fix #1: PacketSend through GameThread** (commit 5dc84d7)
- `gwa3/src/packets/CtoS.cpp` — `SendPacket()` now uses `GameThread::Enqueue` if off game thread, or calls directly if on game thread. Matches GWCA's `CtoS::SendPacket` exactly.
- The old `PacketSenderThread` background thread was the proven crash cause for USE_SKILL (0x46) packets — `PacketSend` is not thread-safe when called concurrently with game combat processing.

**Fix #2: ChangeTarget via UIMessage** (commit 79974e3)
- `gwa3/src/managers/AgentMgr.cpp` — `ChangeTarget()` now sends `UIMessage 0x10000020` (kChangeTarget) with a `ChangeTargetUIMsg` struct, matching Py4GW's approach.
- The native `ChangeTarget` function deadlocks from engine hook, crashes from GameThread during movement. UIMessage goes through the game's own UI dispatch which handles state safety internally.

**Fix #3: Hardcoded engine hook exit** (commit 0315e7c)
- `gwa3/src/packets/CtoS.cpp` — Both `EngineDetourNaked` and `GWA3BotshubCommandReturnThunk` now exit with hardcoded `mov ebp,esp; fld dword [ebp+8]; jmp [s_engineReturnAddr]` instead of jumping through a VirtualAlloc'd trampoline.
- Upstream GWA2/BotsHub hardcodes these original bytes in MainProc. The trampoline indirection may have been causing hero floating and rendering corruption.

### Earlier fixes (already validated):

- **HandleCase fall-through bug** — engine detour's HandleCase path used to fall through to command execution. Fixed: defers instead.
- **Offsets::Environment deref** — was using code address as data pointer. Fixed: dereferences the embedded immediate.
- **Movement guards on UseSkill** — checks `move_x/move_y == 0` and `IsBotshubQueueIdle()` before dispatching native UseSkill. Upstream BotsHub stops before casting.

### Key findings:

- **Sparkfly movement is FIXED** — engine command lane for Move works, waypoints 1-3 pass consistently
- **UseSkill via packet from sender thread = crash** — proven by suppressing UseSkill (no crash) then re-enabling (crash)
- **UseSkill via native function from engine hook OR GameThread = delayed crash** — corrupts action state, crashes 2-3 seconds later when movement resumes
- **Hero floating** was observed even with minimal engine detour (just heartbeat + pushad/popad + trampoline). Fix #3 addresses this but is UNTESTED.
- UseSkill is currently dispatched via `GameThread::EnqueuePost` with movement+queue guards in `SkillMgr.cpp`

## What Needs To Happen Now

### 1. Build and run the Sparkfly validator with all three fixes:

```powershell
cd C:\Users\Robert\Documents\GWA Censured X BotsHub\gwa3
cmake --build --preset marvin --target gwa3 injector
powershell -NoProfile -ExecutionPolicy Bypass -File "C:\Users\Robert\Documents\GWA Censured X BotsHub\gwa3\tools\run_froggy_test.ps1" -AccountIndex 3 -BuildDir "C:\Users\Robert\Documents\GWA Censured X BotsHub\gwa3\build_marvin" -DllName "gwa3_marvin.dll" -SparkflyOnly -RunTimeoutSeconds 480
```

### 2. Observe IN-GAME:

- Do heroes float? (Fix #3 should address this)
- Do hero health bars update when they take damage?
- Does player movement look normal (walking animation, not gliding)?
- Does ChangeTarget work? (Fix #2 — UIMessage path)
- Does UseSkill fire without crashing? (Fix #1 — GameThread packets + movement guard)

### 3. Check the log for:

- `defer=` counters (should be low/zero in Sparkfly)
- `ChangeTarget using UIMessage` log line
- `UseSkill using GameThread post-dispatch` log line
- Any `SEH`, `CRASH`, `EXCEPTION` lines
- Waypoint pass/fail status

### 4. If heroes still float:

The engine hook site itself (`Offsets::Engine` at `0x00653C91`) may be fundamentally unsuitable. Consider:
- Moving Move dispatch to `GameThread::EnqueuePost` instead of the engine command lane (same as GWCA's direct native call but on game thread)
- Or removing the engine hook entirely and using GameThread for everything

### 5. If UseSkill still crashes:

UseSkill is currently on `GameThread::EnqueuePost` with movement guards. If it still crashes:
- Try UIMessage for UseSkill too (research `kSendUseSkill` or equivalent in GWCA UIMgr.h)
- Or suppress UseSkill and rely on auto-attack + hero AI for combat (the suppressed test survived the entire route)

### 6. Success criteria:

- No GW crash dialog
- No "Not Responding" hang
- Sparkfly waypoints 1-4 all pass
- No hero floating
- ChangeTarget works during route
- UseSkill fires at least once without crash

## Key Files

| File | What |
|---|---|
| `gwa3/src/packets/CtoS.cpp` | Engine hook, packet dispatch, command queue |
| `gwa3/src/managers/AgentMgr.cpp` | Move (engine lane), ChangeTarget (UIMessage) |
| `gwa3/src/managers/SkillMgr.cpp` | UseSkill (GameThread::EnqueuePost + movement guard) |
| `gwa3/src/bot/FroggyHM.cpp` | AggroMoveToEx combat loop (calls ChangeTarget + FightTarget) |
| `gwa3/src/tests/IntegrationTestEpic14.cpp` | RunFroggySparkflyRouteTest (line 3320) |
| `gwa3/src/core/Offsets.cpp` | Pattern scanning, offset resolution |
| `gwa3/FROGGY_SPARKFLY_MOVE_CAST_DEBUG.md` | Full debug history |
| `gwa3/MOVEMENT_SKILL_DISPATCH_COMPARISON.md` | Cross-framework comparison |
