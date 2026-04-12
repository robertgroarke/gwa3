# Froggy Sparkfly Movement/Casting Debug Notes

Date: 2026-04-12

Scope:
- MARVIN lane only
- Account index `3`
- Character `Starvin M A R V I N`
- Build preset `marvin`
- Build dir `gwa3/build_marvin`
- DLL `gwa3_marvin.dll`

## Problem Statement

We have a Sparkfly-specific runtime problem in the C++ Froggy port:

- In town, movement presentation is normal.
- Inside the dungeon, movement presentation is normal.
- In Sparkfly, movement has repeatedly presented as floating/gliding instead of normal walking.
- In Sparkfly, skill execution has been logically happening in some runs, but cast presentation/animation has been wrong or absent.
- Some skill-dispatch experiments in Sparkfly caused a deterministic `Gw.exe` crash on the first live cast.

This strongly suggests the issue is not Froggy’s high-level combat policy by itself. The deeper problem is the runtime context/lane used to dispatch move/change-target/use-skill engine calls.

## Current High-Level Understanding

The important lesson from this investigation is:

- The move/change-target/use-skill anchors do not currently look stale relative to upstream BotsHub.
- The primary issue looks like command-dispatch semantics and hook context, not obviously bad scan patterns.
- GWA2/BotsHub did not rely on the current `GameThread::EnqueueSerialPre(...)` style we tested for `UseSkill`.
- GWA2/BotsHub’s command execution model is centered on a command queue drained from the engine hook path.
- Our current `CtoS.cpp` engine queue is only partially similar to upstream.
- Our `CtoSHook.cpp` render-hook experiment is not the same thing as upstream GWA2/BotsHub `MainProc`.

## What Upstream GWA2/BotsHub Does

### 1. Upstream command structs

From `BotsHub-latest/lib/GWA2.au3` and `BotsHub-latest/lib/GWA2_Assembly.au3`:

- `MOVE_STRUCT` is a 16-byte command blob:
  - entry pointer
  - `float x`
  - `float y`
  - `dword`
- `CHANGE_TARGET_STRUCT` is an 8-byte command blob:
  - entry pointer
  - `dword targetID`
- `USE_SKILL_STRUCT` is a 20-byte command blob:
  - entry pointer
  - `dword myId`
  - `dword zeroBasedSlot`
  - `dword targetID`
  - `dword callTarget`

Important detail:
- Upstream writes `skillSlot - 1` into the queued skill command.
- So the underlying native `UseSkill` function is still expected to receive a zero-based slot.

### 2. Upstream command stubs

From `BotsHub-latest/lib/GWA2_Assembly.au3`:

- `CommandMove`:
  - loads `eax + 4`
  - calls `Move`
  - jumps to `CommandReturn`
- `CommandChangeTarget`:
  - pushes `0`
  - pushes `[eax+4]`
  - calls `ChangeTarget`
  - jumps to `CommandReturn`
- `CommandUseSkill`:
  - pushes `[eax+10]`, `[eax+0C]`, `[eax+08]`, `[eax+04]`
  - calls `UseSkill`
  - cleans 16 bytes
  - jumps to `CommandReturn`

This means upstream skill execution is not a packet path and not a UI keypress path. It is a queued native engine-call path.

### 3. Upstream scan patterns/anchors

From `BotsHub-latest/lib/GWA2_Assembly.au3`:

- `UseSkill` pattern: `85F6745B83FE1174`, AutoIt offset `-0x127`
- `ChangeTarget` pattern: `3BDF0F95`, AutoIt offset `-0x89`
- `Move` pattern: `558BEC83EC208D45F0`, AutoIt offset `0x1`

Our current `gwa3/src/core/Offsets.cpp` conversion logic is:

- C++ offset = AutoIt offset - 1

So our current values line up with upstream:

- `UseSkill`: `-0x128`
- `ChangeTarget`: `-0x8A`, plus the later local adjustment logic already in our code
- `Move`: `0x0`

Conclusion:
- Based on upstream BotsHub-latest, the current offsets do not look obviously stale.

### 4. Upstream queue execution model

This is the most important architectural point.

In upstream `BotsHub-latest/lib/GWA2_Assembly.au3`, `AssemblerCreateMain()` builds `MainProc`, which is installed from the `Engine` hook site, not the `Render` hook site.

That upstream `MainProc`:

- uses a queue counter and fixed queue base
- has a `SavedIndex`
- has a `RegularFlow`
- has a `HandleCase`
- only jumps into the queued command when the engine state allows it
- advances the queue differently in the `HandleCase` path

The critical upstream logic we had dropped is this environment/state gate:

- traverse `BasePointer`
- inspect world/context pointers
- inspect `+0x198` and `+0x19C`
- if `env_index == 0`, go to `HandleCase`
- otherwise look up `Environment + env_index * 0x7C`
- inspect `flags = [env_entry + 0x10]`
- if `flags & 0x40001`, go to `HandleCase`

That means upstream does **not** always execute queued commands immediately. It conditionally skips/advances queue state depending on engine/world state.

## What We Tried

## A. Direct native move on game thread / post-dispatch

Files involved:
- `gwa3/src/managers/AgentMgr.cpp`
- `gwa3/src/core/GameThread.cpp`

What we tried:
- direct native movement calls
- `GameThread::EnqueuePost(...)`
- special-case Sparkfly movement behavior

Observed result:
- This repeatedly produced the “floating/gliding” Sparkfly presentation the user reported.

Conclusion:
- Direct native move dispatch was not matching the presentation behavior we need in Sparkfly.

## B. Engine command-slot lane for move/change-target

Files involved:
- `gwa3/src/managers/AgentMgr.cpp`
- `gwa3/src/packets/CtoS.cpp`

What we tried:
- enqueue `Move` and `ChangeTarget` through the `CtoS.cpp` engine command-slot lane
- use command blobs with entry pointers and native stubs

Observed result:
- This was materially better than direct native movement.
- Several live runs showed Sparkfly route progress through segmented waypoints using the engine command lane.
- This was the best direction for movement compared to direct off-thread movement.

Important logs:
- `gwa3/build_marvin/bin/Release/gwa3_log_4592.txt`
  - earlier segmented Sparkfly route progressed through waypoints 1-9
- `gwa3/build_marvin/bin/Release/gwa3_log_41660.txt`
  - engine command lane used for movement and skill traces

Conclusion:
- The command-slot lane is closer to the correct movement behavior than direct game-thread native move.

## C. Sparkfly UI/control-action skill experiments

Files involved:
- `gwa3/src/managers/UIMgr.cpp`
- earlier `SkillMgr.cpp` experiments

What we tried:
- Sparkfly-only UI/control-action based skill dispatch
- action/context based keypress/control message routing

Observed result:
- This did not produce a robust fix.
- It did not end up being the right model compared to upstream BotsHub.

Conclusion:
- This is not how upstream GWA2/BotsHub dispatches Froggy combat skills.

## D. Direct `UseSkill` call via `GameThread::EnqueueSerialPre(...)`

Files involved:
- `gwa3/src/managers/SkillMgr.cpp`

What we tried:
- call the scanned native `UseSkill` directly from a `GameThread::EnqueueSerialPre(...)` lambda
- pass `myId`, `slot - 1`, `target`, `callTarget`

Observed result:
- This still produced live instability in Sparkfly.
- It did not solve cast presentation.
- In crash-focused live runs it still led to `Gw.exe` crashing after the first cast.

Conclusion:
- The correct solution is not just “call the right function from the game thread”.
- The dispatch context/queue model matters.

## E. Engine command lane for `UseSkill`

Files involved:
- `gwa3/src/managers/SkillMgr.cpp`
- `gwa3/src/packets/CtoS.cpp`

What we tried:
- queue 20-byte `UseSkill` command blobs on the engine command lane
- use native stub pushing `myId`, zero-based slot, target, callTarget

Observed result:
- In some runs, Froggy’s logical combat behavior improved a lot.
- We captured successful spirit-chain proof with slots `2`, `3`, `4`, and `5`.

Important logs:
- `gwa3/build_marvin/bin/Release/gwa3_log_41648.txt`
  - successful spirit-chain proof
  - trace shows:
    - slot 2 used
    - slot 3 used
    - slot 4 used
    - slot 5 used
- `gwa3/build_marvin/bin/Release/gwa3_log_41660.txt`
  - successful spirit-chain proof on the engine command lane

Example from those successful traces:
- Froggy used `Signet of Spirits` in slot `2`
- then used follow-up spirits in slots `3`, `4`, `5`

Observed problem:
- Even when internal combat proof looked correct, the user still reported that Sparkfly cast presentation/animation looked wrong at the client level.

Conclusion:
- The engine command lane can achieve logical combat progress.
- But our engine queue implementation is still not presentation-faithful to upstream GWA2/BotsHub.

## F. Render-hook `CtoSHook` lane for move/change-target/use-skill

Files involved:
- `gwa3/src/packets/CtoSHook.cpp`
- `gwa3/src/managers/AgentMgr.cpp`
- `gwa3/src/managers/SkillMgr.cpp`

Why we tried it:
- It looked “more AutoIt-like” at first glance because it is an AutoIt-style command blob queue.

What we learned:
- This was a wrong assumption.
- `CtoSHook.cpp` is a render-hook experiment.
- Upstream GWA2/BotsHub `MainProc` command queue is built from the `Engine` hook, not the `Render` hook.

Observed result:
- Render-lane movement looked promising.
- The first live render-lane `UseSkill` in Sparkfly caused a deterministic crash.

Important log:
- `gwa3/build_marvin/bin/Release/gwa3_log_7184.txt`

Crash artifact:
- `gwa3/build_marvin/bin/Release/screenshots/froggy_end_20260412_112104.png`

Conclusion:
- The render-lane skill path is not safe in the current implementation.
- It should not be the active hot path for Froggy casting.

## G. Compare `CtoSHook.cpp` against upstream GWA2/BotsHub

What we learned:
- `CtoSHook.cpp` does not reproduce upstream `MainProc`.
- It patches a different hook site (`Offsets::Render`) and uses a simplified return/replay model.
- Upstream `MainProc` has much more state handling before queue execution.

This was the critical correction:
- The upstream queue logic we need to match belongs in `CtoS.cpp` engine-hook behavior, not in `CtoSHook.cpp`.

## H. Patch `CtoS.cpp` engine detour with upstream-style handle-case gate

Files involved:
- `gwa3/src/packets/CtoS.cpp`

What we changed:
- added `ShouldDeferBotshubCommands()`
- mirrored the upstream GWA2/BotsHub state test:
  - `BasePointer`
  - nested world/context pointers
  - `+0x198`
  - `+0x19C`
  - `Environment + env_index * 0x7C`
  - `flags & 0x40001`
- if the gate says “unsafe”, the engine detour now advances/drops the queued botshub command instead of executing it immediately

Observed result:
- This removed the immediate render-skill crash because the render lane was removed from the hot path.
- Live Sparkfly test no longer crashed immediately.
- But it regressed target-change success and route progress.

Important log:
- `gwa3/build_marvin/bin/Release/gwa3_log_41744.txt`

Observed in that run:
- `ChangeTarget` did not actually stick
- target remained `0`
- Sparkfly waypoint 1 failed
- run ended with:
  - `Passed: 20 / Failed: 4 / Skipped: 2`

Conclusion:
- The newly added handle-case gate is directionally correct from upstream analysis.
- But our engine queue still does not yet match upstream well enough.
- The current engine queue likely still differs in queue semantics/return behavior beyond just this one gate.

## Best Evidence Collected So Far

## 1. Offsets are probably not the primary bug

We checked local upstream BotsHub sources and our own `Offsets.cpp`.

Current understanding:
- `UseSkill`, `Move`, and `ChangeTarget` anchors line up with upstream BotsHub-latest.

Therefore:
- the bug is probably not “we scanned the wrong function” in the simple sense
- the bug is more likely in dispatch context, queue semantics, or hook behavior

## 2. Sparkfly is exposing a dispatch/context problem

User-observed behavior:
- town movement okay
- dungeon movement okay
- Sparkfly movement/presentation bad
- Sparkfly cast presentation bad

This pattern points away from simple Froggy policy bugs and toward:
- hook context
- queue timing
- movement/cast dispatch interaction
- engine state gating

## 3. Logical combat can work even when presentation is wrong

We have direct proof in logs that Froggy can logically execute the spirit chain:

- slots `2`, `3`, `4`, `5` were all used in successful traces

So the remaining problem is not purely:
- “Froggy never decides to use these skills”

The remaining problem is:
- making engine-command execution happen in the same safe presentation context as upstream GWA2/BotsHub

## Current Working Theory

The most likely root cause is:

1. The active move/change-target/use-skill dispatch path still does not faithfully match upstream GWA2/BotsHub engine queue semantics.
2. Our current `CtoS.cpp` engine command queue still differs from upstream in important ways:
   - queue bookkeeping model
   - saved index / return behavior
   - when commands are executed vs skipped
   - possibly how multiple queued commands interact when movement and combat overlap
3. `CtoSHook.cpp` is not the right place to solve Froggy Sparkfly casting because it is not upstream `MainProc`.
4. The remaining work should focus on making `CtoS.cpp`’s engine-hook command queue much closer to upstream GWA2/BotsHub.

## Files Touched During This Investigation

Primary files:
- `gwa3/src/managers/AgentMgr.cpp`
- `gwa3/src/managers/SkillMgr.cpp`
- `gwa3/src/packets/CtoS.cpp`
- `gwa3/src/packets/CtoSHook.cpp`
- `gwa3/include/gwa3/packets/CtoSHook.h`
- `gwa3/src/managers/UIMgr.cpp`
- `gwa3/src/tests/IntegrationTestEpic14.cpp`
- `gwa3/tools/run_froggy_test.ps1`

## Most Useful Logs / Artifacts

Successful logical spirit-chain evidence:
- `gwa3/build_marvin/bin/Release/gwa3_log_41648.txt`
- `gwa3/build_marvin/bin/Release/gwa3_log_41660.txt`

Segmented Sparkfly route evidence:
- `gwa3/build_marvin/bin/Release/gwa3_log_4592.txt`

Render-lane skill crash:
- `gwa3/build_marvin/bin/Release/gwa3_log_7184.txt`
- `gwa3/build_marvin/bin/Release/screenshots/froggy_end_20260412_112104.png`

Engine-lane with new handle-case gate:
- `gwa3/build_marvin/bin/Release/gwa3_log_41744.txt`
- `gwa3/build_marvin/bin/Release/screenshots/froggy_end_20260412_112556.png`

## I. Line-by-line upstream comparison and HandleCase fall-through fix

Date: 2026-04-12 (continued)

### Upstream model fully understood

Performed a complete line-by-line comparison of upstream `AssemblerCreateMain()` in `BotsHub-latest/lib/GWA2_Assembly.au3` against our `CtoS.cpp` engine detour.

**Verified matches (no changes needed):**
- `UseSkill` struct layout: upstream field names are misleading (`skillSlot/targetID/callTarget/bool`) but actual data written by `GWA2.au3:203-207` is `(myID, slot-1, targetID, callTarget)` — matches our `UseSkillCommand` exactly
- `BotshubMoveCommandStub`: `lea eax,[eax+4]; push eax; call Move; add esp,4` — matches upstream `CommandMove`
- `BotshubChangeTargetCommandStub`: `xor edx,edx; push edx; mov eax,[eax+4]; push eax; call ChangeTarget; add esp,8` — matches upstream `CommandChangeTarget`
- `BotshubUseSkillCommandStub`: pushes `[eax+16],[eax+12],[eax+8],[eax+4]` → `UseSkill(myId,slot,target,callTarget)` — matches upstream `CommandUseSkill`
- `ShouldDeferBotshubCommands()` correctly mirrors upstream environment gate (BasePointer chain → +0x198/+0x19C → Environment+index*0x7C+0x10 → flags & 0x40001)
- Return thunk (`GWA3BotshubCommandReturnThunk`) is equivalent to upstream `CommandReturn` using stack-saved tail index instead of global SavedIndex

**Upstream queue model:**
- Two counters: AutoIt `$queue_counter` (write head) and assembly `QueueCounter` (read tail) — functionally identical to our `s_botshubCmdHead`/`s_botshubCmdTail`
- 64 slots × 256 bytes, circular addressing
- `RegularFlow`: save index, clear slot, JMP to command stub (execute)
- `HandleCase`: clear slot, advance counter, JMP to MainExit (drop without executing)

### Critical bug found: HandleCase fall-through

The previous `EngineDetourNaked` had three `je check_botshub_command` exits from the HandleCase block and a fall-through after the drop logic. All paths led to `check_botshub_command`, which immediately tried to execute the NEXT queued command.

Failure scenario with two queued commands (Move at slot N, ChangeTarget at slot N+1):
1. `ShouldDeferBotshubCommands()` returns 1 (HandleCase)
2. HandleCase block: clears Move at slot N, advances tail to N+1
3. **Falls through** to `check_botshub_command`
4. `check_botshub_command`: tail (N+1) != head (N+2), executes ChangeTarget
5. ChangeTarget runs in exactly the state where execution should be deferred

This explains why experiment H regressed target-change: commands were being executed during HandleCase state.

### Fix applied: defer instead of drop

Changed HandleCase to **defer** (leave command in queue, retry next tick) instead of upstream's **drop** semantics. Rationale:
- Upstream BotsHub AutoIt loops continuously re-issue Move/UseSkill, so drops are harmless
- Our C++ bot issues commands once and waits for completion, so drops = silently lost commands
- Deferral means: when HandleCase fires, skip this tick; when RegularFlow fires next, execute

Code change in `EngineDetourNaked`:
```asm
// OLD: HandleCase → clear + advance + fall through to execute (BUG)
// NEW: HandleCase → increment diagnostic counter + JMP no_botshub_command (skip)
```

Added diagnostic counters: `s_deferCount`, `s_deferWithCmd`, `s_regularFlowExec` — logged each time a botshub command is enqueued so we can monitor the defer/flow ratio.

### What this should fix
- ChangeTarget should no longer be silently dropped during HandleCase
- Move commands should no longer be silently dropped during HandleCase
- UseSkill commands should no longer be executed during unsafe HandleCase state
- No more double-execution of commands when multiple are queued during HandleCase

### Build
- Built with marvin preset: `gwa3_marvin.dll` (2026-04-12 11:40)
- Awaiting live Sparkfly validation

## J. Live validation results

### Environment deref fix validation (PID 2016)

- `defer=0 deferCmd=0 exec=18` — HandleCase fires 0% of the time after the Environment deref fix
- **Sparkfly waypoint 1: PASSED** — 7 consecutive Move commands executed cleanly
- **ChangeTarget via engine lane (stationary): PASSED** — target changed to foe 53
- **GW crashed on first UseSkill** via engine command lane at waypoint 2

### UseSkill packet fallback (PID 9832)

- UseSkill moved to packet path (`CtoS::UseSkill` sends header 0x46)
- Movement still passing, ChangeTarget still on engine lane
- **GW crashed on ChangeTarget during active movement** at waypoint 2

### ChangeTarget on packet path (PID 32988)

- Discovered: `TARGET_AGENT` (0xC1) is NOT a valid target-change packet
- Upstream ChangeTarget is native-only (command queue), never uses packets
- Packet-based ChangeTarget: target stayed 0 (FAILED) and GW crashed

### ChangeTarget on GameThread (PID 48792)

- ChangeTarget via `GameThread::Enqueue` + native `InvokeChangeTargetRaw`
- **Combat probe (stationary): target changed to foe 56 — PASSED**
- **All 20+ combat probe checks: PASSED**
- **GW crashed when GameThread dispatched ChangeTarget while Move was active**

### Pattern identified

The native `ChangeTarget` function crashes when called during active character movement, regardless of dispatch context (engine hook or GameThread). The native `Move` function is safe because it replaces the current movement state rather than reading from it.

Upstream BotsHub doesn't crash because the AutoIt bot WAITS for movement to complete before issuing ChangeTarget. The `CommandMove` → sleep → `ChangeTarget` pattern ensures the game's movement state machine is idle before target changes.

### Current dispatch model

| Command | Path | Status |
|---|---|---|
| Move | Engine command lane (botshub queue) | **WORKING** — fixes floating/gliding |
| ChangeTarget | GameThread::Enqueue + native call | **Works when stationary, crashes during movement** |
| UseSkill | Packet path (CtoS::UseSkill 0x46) | **Not yet validated** (deferred pending ChangeTarget fix) |

## Recommended Next Steps

1. Fix the movement+combat timing in FroggyHM.cpp or the test harness:
   - Ensure ChangeTarget is NOT called while the character is actively walking
   - Add a brief movement-settle check before issuing combat commands
   - Or: cancel current action before ChangeTarget
2. Once ChangeTarget is stable during route traversal, re-validate UseSkill on the packet path
3. If packet-based UseSkill doesn't produce proper animations, revisit the native UseSkill on the engine command lane with a similar movement-settle guard
4. Keep the render-lane experiment disabled for live Froggy runs

## Bottom Line

**Major progress:** Sparkfly movement is now fully fixed. The floating/gliding problem is solved by using the engine command lane for Move, matching upstream GWA2/BotsHub semantics.

Three bugs were found and fixed:
1. HandleCase fall-through in the engine detour (commands executed during defer state)
2. `Offsets::Environment` not dereferenced (environment gate reading garbage code addresses)
3. `TARGET_AGENT` (0xC1) is not a valid ChangeTarget packet (upstream uses native calls only)

The remaining issue is a timing/state conflict: native ChangeTarget crashes during active movement. This needs bot-level timing coordination (wait for movement to settle before combat commands), matching how upstream BotsHub sequences its Move → ChangeTarget → UseSkill commands.
