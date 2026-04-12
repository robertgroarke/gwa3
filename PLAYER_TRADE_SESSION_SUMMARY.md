# Player Trade Session Summary

Date: 2026-04-11 to 2026-04-12

## What Was Accomplished

### Stackable Quantity Prompt Fix (COMPLETE, COMMITTED)

The core task was fixing the stackable item quantity dialog for player-to-player trades. Three prompt paths needed to work:
- **Max** (offer full stack) - was already working
- **Default** (offer quantity 1) - was failing
- **Exact** (offer arbitrary quantity N) - was failing

Both failing paths now pass. Two bugs were found and fixed:

#### Bug 1: Premature mode consumption in popup callback

**File:** [gwa3/src/managers/TradeMgr.cpp](gwa3/src/managers/TradeMgr.cpp)

The `OnChooseQuantityPopupUIMessage` hook fires on multiple UIMessages during the popup lifecycle (msg=0x4, 0x5, 0x31, 0x9). The code was consuming the pending automation mode on ANY message where `FindTradeQuantityPromptFrame()` returned a valid frame.

On the first popup open of a GW session, the frame wasn't findable until msg=0x9 (`kInitFrame`), so MaxOffer worked by luck. On subsequent opens, the frame was findable earlier (msg=0x5), so the mode was consumed before `kInitFrame`. Clicks queued at pre-init time are silently ignored by the game.

**Evidence:** DLL logs showed the Max path callback firing on msg=0x9 with `promptFrame` found, while the Default path callback fired on msg=0x5 with `promptFrame` found prematurely.

**Fix:** Gate the callback on `kInitFrame` (msg=0x9) only, matching GWToolbox's pattern. Earlier messages log and return without consuming the mode.

#### Bug 2: Popup's backing count not updated by UI manipulation

**File:** [gwa3/src/managers/TradeMgr.cpp](gwa3/src/managers/TradeMgr.cpp)

The GmItemSplit popup has an internal data structure with `count` at offset +0x04 and `maxCount` at offset +0x08. This data lives in the `uictl_context` pointer of the popup frame's `FrameInteractionCallback` entry (at `frame + 0xA8`).

All UI-level approaches (SetEditableTextValue, SetNumericFrameValue, KeyPress typing, spinner clicks, VK_RETURN) only modify the displayed UI text, NOT the backing `count` field. The OK button handler reads the backing field, not the displayed text.

**Evidence:** Data structure probing during kInitFrame revealed `ctx+0x04 = 1` (count) and `ctx+0x08 = 85` (maxCount, matching the known stack size).

**Fix:** Added `ValueOffer` mode (enum value 3) that directly writes the desired quantity to `uictl_context + 0x04` during kInitFrame, then queues OK. The callback finds its own entry in the `frame_callbacks` array by matching the hook function address against `s_chooseQuantityPopupHookAddr`.

**Commits:**
- `6ae92a0` - Fix stackable quantity prompt: gate callback on kInitFrame, direct count write
- `1c90f93` - Add round-trip trade tests with helper item offering and partner visibility
- `cef5bb3` - Fix round-trip trade timing: add submission processing delay before accept

### Approaches That Were Tried And Failed

| Approach | Result | Why |
|---|---|---|
| Direct `OfferItem(itemId, quantity)` with non-zero quantity for stackable items | Not viable | User confirmed this only works for non-stackable items; stackable items require the popup path |
| `SetEditableTextValue` on root[1] during kInitFrame | **CRASH** | Frame internals not ready for UI message dispatch during popup init |
| `SetNumericFrameValue` on root[1] during kInitFrame | **CRASH** | Same as above |
| `SetEditableTextValue` on root[1] after popup fully created | No effect | Updates display text but not backing `count` field |
| `SetNumericFrameValue` on root[1] after popup fully created | No effect | Same |
| `KeyPress` typing digits into root[1] | No effect | Same |
| VK_RETURN on value control or popup root | No effect | Game doesn't process Enter as submit |
| Spinner clicks on root[2] | No effect / previously crashy | Doesn't update backing count |
| `root[2]` numeric frame treatment | **CRASH** | Documented as unsafe in status doc |
| Clicking OK without Max on pre-init message | Silently ignored | Popup handlers not wired up before kInitFrame |
| UI button click for SubmitOffer (childOffset 123) | Wrong button | That's the party window trade-initiation button, not the trade window Submit button |
| UI button click for AcceptTrade (childOffset 122) | Wrong button | Same - party window, not trade window |

### Helper Infrastructure Extensions (COMMITTED)

**Files:**
- [gwa3/src/tests/IntegrationTestSession.cpp](gwa3/src/tests/IntegrationTestSession.cpp)
- [gwa3/bridge/tests/trade_harness.py](gwa3/bridge/tests/trade_harness.py)
- [gwa3/bridge/tests/test_f_player_trade.py](gwa3/bridge/tests/test_f_player_trade.py)

Added to support round-trip trade tests:
- Helper status now reports `partner_items` array with `item_id`, `model_id` (0 for cross-session), `quantity`
- Helper config supports `offer_item_model_id` to auto-offer items back during return trades
- `FindHelperInventoryItemByModel` for helper inventory lookups
- `ReadTradeHelperOfferItemModelConfig` config reader
- Three round-trip test functions written (prompt_max, prompt_default, prompt_exact)

**Note:** `model_id` in `partner_items` is always 0 because `ItemMgr::GetItemById` cannot resolve cross-session item IDs. Tests verify by quantity instead.

## What Still Needs Debugging

### 1. SubmitOffer / AcceptTrade Don't Update The Game UI

**Status:** Root cause identified, fix not yet implemented

**Root cause:** GWCA's own source code documents this at line 70-73 of their TradeMgr.cpp:
```
A lot of these function signatures need to update the trade window UI...
things like submitting an offer currently don't update the UI which is a problem.
TODO: Move the RVA's for submitting etc further up the UI tree
```

Our `TradeSendOffer` and `TradeAcceptOffer` offsets point to the raw internal functions that send server packets but bypass the UI update chain. The game's server state updates but the local UI doesn't reflect it, leading to:
- The trade window not visually showing "submitted" state
- Accept potentially being ignored because the game UI thinks nothing was submitted
- Trade getting stuck at flags=7 (open + submitted + accepted) without completing

**What we tried:**
- Clicking the party window trade buttons (childOffset 123/122) - wrong buttons entirely
- Need to find the actual Submit/Accept buttons inside the trade window frame (hash `3198579276`)

**What still needs to happen:**
- Add frame dump code to `SubmitOffer` that dumps the trade window's full child hierarchy
- Run a test that reaches the submit point to capture the dump
- Identify the correct child offset IDs for Submit and Accept buttons in the trade window
- Implement `ButtonClickImmediateFull` on those buttons instead of calling the raw native functions

**Alternative approach:** Call the UI-level handler function directly. The scan pattern at GWCA line 79 finds a UI callback handler. The internal functions are at offsets +0xa6 (CancelOffer) and +0x101 (SendOffer) within that handler. Calling the handler itself with the appropriate message ID would trigger both the server packet AND the UI update.

### 2. Trade Completion Never Finalizes

**Status:** Blocked by #1

When both sides submit and accept (both via raw native calls), the trade stays at flags=7 forever. The helper keeps re-accepting but the trade never completes. This is likely a consequence of the UI not being updated - the game's trade completion logic may require the UI to be in the correct state.

### 3. GW Client Hangs ("Not Responding") After DLL Injection

**Status:** Intermittent, not fully diagnosed

The DISCO PANIC GW client sometimes enters "Not Responding" state after DLL injection. The LLM mode watchdog does NOT detect hung windows (the integration test watchdog does via `SendMessageTimeoutA` with `SMTO_ABORTIFHUNG`, but this isn't active in LLM mode).

**Observations:**
- The hang occurs after GameThread hook installation and initial drain completion
- The StoC handler table hook completes successfully
- No crash dialog appears - the game just stops processing messages
- MARVIN agent's lane (different character, same DLL code) does NOT exhibit this issue
- The hang started appearing consistently late in the session, possibly related to repeated inject/kill cycles

**Possible causes:**
- Stale mode flag files confusing the DLL's mode detection (PID-specific vs non-PID-specific flag naming inconsistency between injector and DLL)
- Character-specific game state (DISCO PANIC at Longeye's Ledge with 8/8 party) triggering an edge case in hook initialization
- The StoC handler table hook blocking the game's message pump

**What the integration test watchdog has that LLM mode doesn't:**
- `SendMessageTimeoutA(gwHwnd, WM_NULL, ..., SMTO_ABORTIFHUNG, 2000)` to detect hung windows
- Crash dialog detection via `FindWindowExA(nullptr, ..., "#32770", "Gw.exe")`
- Automatic `TerminateProcess` on detection
- Render heartbeat stall detection (3-second threshold)

### 4. Build Compatibility With MARVIN Agent's Uncommitted Changes

**Status:** Workaround identified

MARVIN agent has extensive uncommitted changes to:
- `gwa3/src/bot/FroggyHM.cpp` - references DungeonInventory, DungeonLoot, DungeonInteractions, DungeonCombatRoutine
- `gwa3/src/llm/ActionExecutor.cpp` - references `OpenMerchantContext`
- `gwa3/src/tests/IntegrationTestEpic14.cpp` - references `DebugAggroMoveTo`

These files don't compile without adding missing includes and fixing signature mismatches:
- FroggyHM.cpp needs: `#include <gwa3/bot/DungeonInventory.h>`, `DungeonInteractions.h`, `DungeonLoot.h`, `DungeonCombatRoutine.h`
- `WaitMs(DWORD ms)` needs to be `WaitMs(uint32_t ms)` to match DungeonLoot's `WaitFn` typedef
- `DungeonItemPolicy.cpp` needs `#include <gwa3/game/Item.h>`
- `CtoS.cpp` needs stub implementations of `GetPacketTapSnapshot()` and `ResetPacketTap()`

When the full working tree is compiled (with MARVIN's changes), the DLL sometimes crashes on `move_to` - the first bridge action. This crash does NOT happen with the committed code alone.

## Key File References

### Our Trade Files
- [gwa3/src/managers/TradeMgr.cpp](gwa3/src/managers/TradeMgr.cpp) - quantity prompt fix, submit/accept, frame dump code
- [gwa3/include/gwa3/managers/TradeMgr.h](gwa3/include/gwa3/managers/TradeMgr.h) - trade function declarations
- [gwa3/src/managers/UIMgr.cpp](gwa3/src/managers/UIMgr.cpp) - ButtonClick, SetEditableTextValue, SetNumericFrameValue
- [gwa3/include/gwa3/managers/UIMgr.h](gwa3/include/gwa3/managers/UIMgr.h) - frame constants, message IDs
- [gwa3/src/tests/IntegrationTestSession.cpp](gwa3/src/tests/IntegrationTestSession.cpp) - helper trade mode, status writer
- [gwa3/bridge/tests/test_f_player_trade.py](gwa3/bridge/tests/test_f_player_trade.py) - all trade tests
- [gwa3/bridge/tests/trade_harness.py](gwa3/bridge/tests/trade_harness.py) - two-client launcher harness

### Reference Sources Used
- [toolbox/GWToolboxpp-master/GWToolboxdll/Modules/InventoryManager.cpp](toolbox/GWToolboxpp-master/GWToolboxdll/Modules/InventoryManager.cpp) - GmItemSplit hook pattern, queued button press mechanism
- [toolbox/GWToolboxpp-master/Dependencies/GWCA/include/GWCA/Managers/UIMgr.h](toolbox/GWToolboxpp-master/Dependencies/GWCA/include/GWCA/Managers/UIMgr.h) - Frame struct (0x1C8 bytes), FrameInteractionCallback layout
- [GWA Censured/GWCA-master/Source/TradeMgr.cpp](GWA%20Censured/GWCA-master/Source/TradeMgr.cpp) - OfferTradeItem native call, SubmitOffer TODO comment, scan patterns
- [GWA Censured/GWCA-master/Include/GWCA/Managers/TradeMgr.h](GWA%20Censured/GWCA-master/Include/GWCA/Managers/TradeMgr.h) - `OfferItem(item_id, quantity=0)` documentation

### Status Documents
- [PLAYER_TRADE_DEBUG_STATUS.md](PLAYER_TRADE_DEBUG_STATUS.md) - detailed technical status of all trade paths
- [AGENT_WORK_REGISTRY.md](AGENT_WORK_REGISTRY.md) - work area ownership (player-trade-validation owned by CODEX)
- [AGENT_ACCOUNT_REGISTRY.md](AGENT_ACCOUNT_REGISTRY.md) - account/lane assignments

### Build Configuration
- Trade lane preset: `cmake --preset trade`
- Build dir: `gwa3/build_trade`
- DLL: `gwa3_trade.dll`
- Pipe: `\\.\pipe\gwa3_llm` (hardcoded in committed IpcServer.cpp, CMake preset value `gwa3_llm_trade` is unused)
- Characters: DISCO PANIC (main, index 1), BLUMPKINS (helper, index 2)

## GmItemSplit Data Structure (Discovered)

Located via `FrameInteractionCallback.uictl_context` at the popup frame's callback matching our hook address:

```
uictl_context + 0x00: unknown
uictl_context + 0x04: count (uint32, current selection, defaults to 1)
uictl_context + 0x08: maxCount (uint32, full stack size)
uictl_context + 0x24: unknown (value 2)
uictl_context + 0x28: unknown (value 31)
uictl_context + 0x64: unknown (value 1, possibly a flag)
```

The Max button internally sets `count = maxCount`. The OK button reads `count` and commits. Writing directly to `count` at +0x04 during kInitFrame, then clicking OK, successfully offers the specified quantity.

## Test Status (as of last successful run)

| Test | Status | Notes |
|---|---|---|
| `*prompt_max*` | PASS | Control case, always worked |
| `*prompt_default_quantity*` | PASS | Fixed by kInitFrame gate |
| `*prompt_exact_quantity*` | PASS | Fixed by ValueOffer direct count write |
| `*roundtrip*prompt_max*` | NOT TESTED | Blocked by submit/accept and hang issues |
| `*roundtrip*prompt_default*` | NOT TESTED | Same |
| `*roundtrip*prompt_exact*` | NOT TESTED | Same |

## 2026-04-12 Update: Bridge/Helper Recovery And Current Boundary

This section captures the work done after the quantity-dialog fix, with emphasis on why live trade testing stalled and where the current blocker actually is.

### 1. Helper mode was broken and is now fixed

**Files:**
- [gwa3/src/dllmain.cpp](gwa3/src/dllmain.cpp)
- [gwa3/src/tests/IntegrationTestInternal.h](gwa3/src/tests/IntegrationTestInternal.h)

The BLUMPKINS helper client was not actually entering trade-helper mode in the current committed DLL. `IntegrationTestSession.cpp` still had `RunTradeHelperMode()`, but `dllmain.cpp` was no longer reading `gwa3_test_trade_helper.flag`, so the helper client fell through into normal runtime instead of the passive trade helper loop.

**Fix applied:**
- restored `tradeHelperTest` flag handling in `dllmain.cpp`
- included it in `anyTest`
- restored the `RunTradeHelperMode()` dispatch
- added the missing declarations in `IntegrationTestInternal.h`

**Result:**
- BLUMPKINS helper mode is active again
- `trade_helper_status.json` is written again
- helper heartbeat data is trustworthy again

### 2. DISCO startup issue was not a generic GW crash

**Files:**
- [gwa3/src/tests/IntegrationTest.cpp](gwa3/src/tests/IntegrationTest.cpp)
- [gwa3/src/tests/IntegrationTestInternal.h](gwa3/src/tests/IntegrationTestInternal.h)
- [gwa3/src/dllmain.cpp](gwa3/src/dllmain.cpp)

DISCO PANIC looked like it was "hanging" after injection, but the primary issue was not the Guild Wars client itself. The old LLM path was being killed by the watchdog's hung-window branch, and the bridge had an IPC write/read deadlock on first snapshot.

Two separate fixes were applied:

#### 2a. LLM watchdog hung-window kill disabled

The integration watchdog's `SendMessageTimeoutA(... SMTO_ABORTIFHUNG ...)` branch was appropriate for full integration tests, but too aggressive for `llmMode` / `llmAdvisory`. In LLM mode, it was killing DISCO while the bridge was blocked.

**Fix:**
- added `SetWatchdogHungWindowKillEnabled(bool enabled)`
- for `llmMode` and `llmAdvisory`, start watchdog but disable only the hung-window kill branch
- explicit crash-dialog and disconnect detection remain available

#### 2b. First Tier1 snapshot deadlock fixed

**File:**
- [gwa3/src/llm/IpcServer.cpp](gwa3/src/llm/IpcServer.cpp)

The duplex pipe thread was blocking in `ReadFile` while the bridge thread tried to `WriteFile` the first Tier 1 snapshot. That made DISCO appear frozen very early in LLM mode.

**Fix:**
- added `PeekNamedPipe`-based `PipeHasBytesAvailable()`
- only call `ReadMessage()` when bytes are actually available

**Evidence after fix:**
- DISCO log shows:
  - `Tier1 begin`
  - `Tier1 serialized len=2100`
  - `Tier1 send begin`
  - `Tier1 send end`
  - `Tier1 end`

So the first-snapshot transport deadlock is resolved.

### 3. Launcher/test harness stale-PID bug fixed

**File:**
- [gwa3/bridge/tests/trade_harness.py](gwa3/bridge/tests/trade_harness.py)

The launcher log parser used `re.search`, which returned the **first** `GWLAUNCHER_PID=` in an appended log file. That caused stale PIDs and false launch failures.

**Fix:**
- `_parse_pid_from_log()` now returns the **last** PID match
- launcher log files are truncated before each launch

This separated real trade failures from stale-launch failures.

### 4. Current trade-open regression: wrong `kInitiateTrade` constant

**Files:**
- [gwa3/include/gwa3/managers/UIMgr.h](gwa3/include/gwa3/managers/UIMgr.h)
- [gwa3/src/managers/TradeMgr.cpp](gwa3/src/managers/TradeMgr.cpp)

The active source had regressed `UIMgr::MSG_INITIATE_TRADE` to:
- `0x10000033`

The previously working value, consistent with local disassembly research, was:
- `0x100001A0`

This regression is visible in live logs:
- older working trade builds logged:
  - `Trade UI tap registered (initiate=0x100001A0 ...)`
- newer regressed builds logged:
  - `Trade UI tap registered (initiate=0x10000033 ...)`

The source has now been corrected back to:
- `MSG_INITIATE_TRADE = 0x100001A0`

and `build_trade` was rebuilt successfully afterward.

### 5. Where the live boundary is now

The current valid state is:

- helper mode is working again
- bridge transport is working again
- DISCO is no longer blocked on first Tier 1 snapshot send
- both DISCO and BLUMPKINS can remain alive and responsive during the test harness run

But the trade helper still reports no incoming trade state during the focused open/cancel test:

- `trade_flags = 0`
- `trade_open_count = 0`
- `trade_partner_hook_hits = 0`
- all helper trade UI counters remain `0`

This means the active blocker is back where it should be:
- **player-trade initiation semantics**
- not bridge startup
- not helper mode
- not the Tier1 IPC deadlock

### 6. Practical interpretation of the DISCO vs BLUMPKINS asymmetry

BLUMPKINS only runs the passive helper loop. DISCO runs the full LLM bridge and the active trade-open action path:
- target selection
- `InteractPlayer`
- `CallTarget`
- trade-button click path
- IPC snapshot/action servicing

So if DISCO appears stalled while BLUMPKINS looks fine, that asymmetry is expected. They are not executing the same workload.

At the time of the latest inspection:
- Windows reported **both** live clients as responding
- the "Not Responding" DISCO screenshot was therefore either transient or from an earlier moment than the latest healthy bridge state

### 7. Current remaining steps

The remaining steps are:

1. Re-run the smallest trade-open test on the corrected `0x100001A0` build:
   - `python -m bridge.tests --filter "test_player_trade_open_idle_cancel_helper"`

2. Verify helper-side state changes off the corrected build:
   - `trade_flags`
   - `trade_open_count`
   - `trade_partner_hook_hits`
   - helper `trade_ui_*` counters

3. Only if trade-open is reestablished, move back to:
   - submit/accept completion
   - locating the real trade-window Submit / Accept buttons
   - validating the round-trip tests

### 8. Current short summary

What is solved:
- stackable quantity prompt paths
- helper mode bootstrap
- LLM-mode watchdog false-kill
- first Tier1 snapshot IPC deadlock
- stale launcher PID parsing
- **IPC pipe name mismatch** (see section 9 below)
- **client disconnect detection regression** (see section 9 below)
- **log file exclusive lock** (see section 9 below)
- **LLM mode abort on GameThread failure** (see section 9 below)

What is not solved:
- trade request still does not visibly land on the helper in the current focused run
- submit/accept UI completion path is still blocked behind that

### 9. 2026-04-12 Update: DISCO startup hang fully diagnosed and fixed

The "DISCO not responding after injection" blocker has been fully diagnosed. It was NOT a real hang — DISCO was always running and responsive. Four bugs combined to make it appear broken:

#### 9a. IPC pipe name mismatch (ROOT CAUSE of harness failures)

**File:** [gwa3/src/llm/IpcServer.cpp](gwa3/src/llm/IpcServer.cpp)

IpcServer.cpp hardcoded `\\.\pipe\gwa3_llm` instead of using the `GWA3_PIPE_NAME` compile definition from CMake. The trade lane preset defines `gwa3_llm_trade`. The Python harness checked for `\\.\pipe\gwa3_llm_trade`, but the DLL created `\\.\pipe\gwa3_llm`. Result: the harness timed out waiting 45s for a pipe that would never appear under that name.

**Fix:** Use `#ifdef GWA3_PIPE_NAME` to pick up the CMake compile definition, with fallback to the old hardcoded value. Also exposed `GetPipeName()` for the log message in LlmBridge.cpp.

**Evidence:** After fix, the DLL log shows `[LLM-IPC] Waiting for client on \\.\pipe\gwa3_llm_trade` and PowerShell NamedPipeClientStream connects successfully.

#### 9b. Client disconnect not detected (post-PeekNamedPipe regression)

**File:** [gwa3/src/llm/IpcServer.cpp](gwa3/src/llm/IpcServer.cpp)

The `PeekNamedPipe` gating fix for the Tier1 deadlock introduced a regression. After a client disconnects, `PeekNamedPipe` returns FALSE with `ERROR_BROKEN_PIPE`. The old `PipeHasBytesAvailable()` treated this as "no bytes" and looped forever — it never called `ReadMessage()` which was the only disconnect detection path. The pipe handle was never cleaned up, blocking all future connections.

**Fix:** Replaced `PipeHasBytesAvailable()` with `PipeCheckState()` returning tri-state: 1 (bytes available), 0 (no data yet), -1 (pipe broken/client disconnected). The read loop now breaks on -1.

**Evidence:** After fix, the log shows:
```
[LLM-IPC] Client connected
[LLM-IPC] PipeCheckState: pipe broken, ending session
[LLM-IPC] Client disconnected
[LLM-IPC] Waiting for client on \\.\pipe\gwa3_llm_trade
[LLM-IPC] Client connected    <-- successful reconnection
```

#### 9c. Log file exclusive lock

**File:** [gwa3/src/core/Log.cpp](gwa3/src/core/Log.cpp)

`fopen_s` with mode `"a"` defaults to `_SH_DENYWR` on Windows, which prevents any other process from opening the file — even for reading. This made the DLL log completely unreadable during live debugging.

**Fix:** Switched to `_fsopen(dllPath, "a", _SH_DENYNO)` which allows concurrent reads.

#### 9d. LLM mode abort on GameThread failure

**File:** [gwa3/src/dllmain.cpp](gwa3/src/dllmain.cpp)

The `!anyTest` guard at the GameThread failure check didn't include `llmMode` or `llmAdvisory`. If GameThread::Initialize returned false for any reason, the InitThread would abort before reaching the LLM bridge initialization — silently, with only a log message that couldn't be read (due to 9c).

**Fix:** Added `&& !llmMode && !llmAdvisory` to the abort condition.

#### 9e. Verified healthy startup sequence

With all four fixes applied, the full DISCO LLM startup sequence completes in ~4 seconds:

```
13:22:31 gwa3.dll loaded, llm=1
13:22:31 Scanner + Offsets resolved
13:22:31 All managers initialized
13:22:31 RenderHook installed, Bootstrap: clicking Play
13:22:35 Map 650 loaded, Player hydrated
13:22:35 GameThread hook installed
13:22:35 CtoS, TraderHook, TargetLogHook installed
13:22:35 StoC, DialogMgr, ChatLogMgr, StringEncoding initialized
13:22:35 === LLM AGENT MODE ===
13:22:35 IPC pipe created: \\.\pipe\gwa3_llm_trade
13:22:35 Bridge thread started
13:22:35 GameThread draining, StoC handlers hooked (487)
```

DISCO window is **Responding=True, Mem=391MB**. Connect/disconnect/reconnect cycle works. Bridge sends Tier1 snapshots on connection.

#### 9f. Additional context: same-tick deadlock insight

A parallel investigation found that calling two native functions (Move-stop + ChangeTarget) in the same game engine tick deadlocks GW. The GameThread code already has a comment about this at `DrainPostQueuesOnGameThread` (line 119-120): "PacketSend has internal locks that prevent multiple calls per frame." This is not related to the startup hang but is relevant for later trade action sequencing.

#### 9g. Note about concurrent agent interference

UIMgr.h was modified by another agent after the last trade-lane build, causing the DLL to be stale (built with old `MSG_INITIATE_TRADE` value). The stale build was part of why earlier debugging was inconclusive.
