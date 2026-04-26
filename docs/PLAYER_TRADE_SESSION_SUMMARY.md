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

## 2026-04-17 Update: Passive Strong-Seam Probe And Revised Boundary

This session took over the player-trade lane specifically to test the current blocker hypothesis: whether the stronger `GmItemSplit` seam (`(*data)->maxCount > 1`) was crashing because the detour ABI was wrong.

### What changed in code

**File:**
- [gwa3/src/managers/TradeMgr.cpp](gwa3/src/managers/TradeMgr.cpp)

Changes made:
- kept the stronger `ChooseQuantityPopup` seam selected first
- changed that stronger seam to install a **passive trampoline-only detour**
- added raw entry-state logging for:
  - `eax`, `ecx`, `edx`, `ebx`, `esi`, `edi`, `ebp`, `esp`
  - `[esp]`, `[esp+4]`, `[esp+8]`, `[esp+12]`
- disabled prompt-click automation on the stronger seam so the next run would isolate:
  - hook installation
  - whether the seam is actually hit
  - whether the crash still occurs without prompt-button interaction

### Build/run performed

Build:
- `cmake --build --preset trade --target gwa3 injector`

Focused live run:
- `python -m bridge.tests --filter 'test_player_trade_open_offer_stackable_prompt_max_cancel_helper'`

Result:
- test failed waiting for `action_result`
- DISCO still produced an explicit Guild Wars crash dialog

### New evidence from the live run

Primary log:
- `gwa3/build_trade/bin/Release/gwa3_log_149052.txt`

Relevant lines:
- `TradeMgr: ChooseQuantityPopup hook installed at 0x01001170 seam=maxCount-deref mode=passive`
- `TradeMgr: OfferItemPromptQuantity arming deferred offer item=747 quantity=0`
- `TradeMgr: OfferItemPromptMax passive seam active; observing prompt open only item=747 seam=maxCount-deref`
- `TradeMgr: OfferItemPromptMax passive seam observed prompt item=747 seam=maxCount-deref frame=0x26E6BC58 frameId=224 childCount=5 context=0x26E71128`
- watchdog crash dialog immediately afterward

Crash screenshot:
- `gwa3/build_trade/bin/Release/screenshots/watchdog_crash_dialog_20260417_075516.bmp`

### The most important negative evidence

There were **no** passive detour-entry logs before the crash.

That means:
- the stronger seam was patched and installed
- but the run did **not** prove that the `0x01001170` seam was actually executed on the crash path

This materially changes the boundary. The earlier theory, "the remaining blocker is the stronger seam detour ABI," is now too narrow.

### Additional source-level finding

The current lane has drifted from the 2026-04-16 logs. In the current source:
- `OfferItemPromptQuantity(...)` only arms `s_pendingOffer`
- `EnableTradeWindowCaptureForPlayerTrade()` logs `UpdateTradeCart hook SKIPPED (trampoline crashes GW)`

So the current lane does **not** have a confirmed prompt-open executor on this path. That makes the observed `FindTradeQuantityPromptFrame()` match suspect; it may not represent a real `GmItemSplit` popup.

### Current best boundary

What is now established:
- prompt-click automation is **not** required to reproduce the crash
- merely installing the stronger seam passively does not by itself prove the seam is executing
- the current source/lane needs a verified prompt-open path before the stronger seam ABI can be judged conclusively

What should happen next:
1. Re-establish a confirmed prompt-open execution path in the current trade lane.
2. Tighten `FindTradeQuantityPromptFrame()` so it cannot false-positive on a generic trade-window child frame.
3. Re-run the passive stronger seam only after 1 and 2 are in place.

## 2026-04-17 Follow-Up: Live Quantity Dialog Fixed Again

The next session completed steps 1 and 2 far enough to restore the actual player-trade quantity flows, without trying to force the stronger seam active again.

### What changed in code

**Files:**
- [gwa3/src/managers/TradeMgr.cpp](gwa3/src/managers/TradeMgr.cpp)

Changes made:
- restored a real prompt-open execution path
- added conservative prompt callback/context logging plus Enter/fallback instrumentation while investigating the passive strong seam
- most importantly, changed `EnsureChooseQuantityPopupHook()` to prefer the historically working `inventorySlot` hook as the **active** automation seam whenever the stronger `maxCount-*` seams are still passive-only
- kept the stronger seams available for passive investigation instead of using them as the live automation path

### Build and focused live validation

Build:
- `cmake --build --preset trade --target gwa3 injector`

Focused live runs:
- `python -m bridge.tests --filter 'test_player_trade_open_offer_stackable_prompt_exact_quantity_cancel_helper'`
- `python -m bridge.tests --filter 'test_player_trade_open_offer_stackable_prompt_default_quantity_cancel_helper'`
- `python -m bridge.tests --filter 'test_player_trade_open_offer_stackable_prompt_max_cancel_helper'`

Result:
- all three focused stackable prompt tests passed on fresh DISCO/BLUMPKINS clients

### Live evidence

Primary logs:
- `gwa3/build_trade/bin/Release/gwa3_log_150416.txt` (exact quantity)
- `gwa3/build_trade/bin/Release/gwa3_log_157240.txt` (default quantity)
- `gwa3/build_trade/bin/Release/gwa3_log_160480.txt` (max)

Key lines:
- exact quantity:
  - `TradeMgr: ChooseQuantityPopup hook installed at 0x01000FB0 seam=inventorySlot mode=active`
  - `TradeMgr: ValueOffer wrote count=2 (was 1, max=250) at ctx=0x199714A8+0x04`
  - `TradeMgr: OnChooseQuantityPopupUIMessage mode=3 msg=0x9 ... queuedClicks=1 okFrameId=129 maxFrameId=130`
- default quantity:
  - `TradeMgr: ChooseQuantityPopup hook installed at 0x01000FB0 seam=inventorySlot mode=active`
  - `TradeMgr: OnChooseQuantityPopupUIMessage mode=1 msg=0x9 ... queuedClicks=1 okFrameId=127 maxFrameId=128`
- max:
  - `TradeMgr: ChooseQuantityPopup hook installed at 0x01000FB0 seam=inventorySlot mode=active`
  - `TradeMgr: OnChooseQuantityPopupUIMessage mode=2 msg=0x9 ... queuedClicks=2 okFrameId=130 maxFrameId=131`
  - `TradeMgr: DrainQueuedPromptClicks clicked frameId=131 ...`
  - `TradeMgr: DrainQueuedPromptClicks clicked frameId=130 ...`

### What this means

This is the practical fix:
- the live trade lane is healthy again for stackable prompt max/default/exact quantity offers
- the original main-client blocker is no longer the active lane blocker

This is the remaining boundary:
- the stronger `maxCount-deref` seam is still not a proven active automation seam
- passive live evidence showed that the prompt frame's callback function can differ from the installed stronger seam address
- the stronger seam should stay a debugging/investigation path until its callback/ABI correspondence is re-proven directly

### Residual risk / cleanup debt

There is still some debug noise after the active callback path succeeds:
- a stale 5-child frame can still be picked up by the manual fallback probe
- that can produce warnings like:
  - `ConfirmTradeQuantityPromptValue no candidate closed the prompt for quantity 'N'`

The focused live tests above still pass, so this is not a blocking behavior issue. It is cleanup work for later, not part of the current blocker anymore.

## 2026-04-17 Follow-Up: Prompt Noise Removed And One-Way Completion Restored

The next pass finished the remaining cleanup and then moved directly into completion validation.

### Prompt cleanup

File touched:
- [gwa3/src/managers/TradeMgr.cpp](gwa3/src/managers/TradeMgr.cpp)

Change:
- after a successful active `inventorySlot` callback click, the code now recognizes the known residual 5-child quantity tree and stops there instead of driving the old manual fallback through it

Focused live confirmation:
- `python -m bridge.tests --filter 'test_player_trade_open_offer_stackable_prompt_exact_quantity_cancel_helper'`

Primary evidence:
- `gwa3/build_trade/bin/Release/gwa3_log_157772.txt`
  - `TradeMgr: OfferItemPromptValue callback path reached residual 5-child prompt after inventorySlot automation item=1241 quantity=2 frame=0x268795B8; skipping manual fallback`

Meaning:
- the false warning about no candidate closing the prompt is no longer emitted on the active exact-quantity path
- the log now matches the real state of the lane: the callback path already succeeded, and the 5-child tree is just residual UI noise

### Completion-path blocker

New source finding:
- [gwa3/src/llm/ActionExecutor.cpp](gwa3/src/llm/ActionExecutor.cpp)
- `HandleOfferTradeItem(...)` was missing `return MakeOk();`
- that meant `offer_trade_item` could fall off the end of the handler without returning an `ActionResult`

Why this mattered:
- the focused completion run was timing out on `offer_trade_item`
- the same run also produced a DISCO crash immediately after queuing the offer, which is consistent with undefined behavior from the missing return

Fix:
- added `return MakeOk();` to `HandleOfferTradeItem(...)`

### Live completion validation

Focused live test:
- `python -m bridge.tests --filter 'test_player_trade_zz_open_offer_submit_accept_complete_helper'`

The completion test itself was tightened first:
- if no salvage kit is available, it now falls back to any singleton non-equipped inventory item
- it now reuses `_complete_forward_trade_and_verify_helper(tc, 1)` so the live path explicitly proves the helper sees the offered partner item before submit/accept completes
- final assertion verifies that the sacrificial item leaves DISCO inventory after the completed trade

Result:
- the focused one-way completion test passed on fresh DISCO/BLUMPKINS clients

Primary main-log evidence:
- `gwa3/build_trade/bin/Release/gwa3_log_165680.txt`
  - `Handler returned: offer_trade_item success=1 error=(none)`
  - `SendResult done: offer_trade_item`
  - `TradeMgr: OfferItem post-dispatch calling native item=192 qty=1 ctx=0x1C429750`
  - `TradeMgr: OfferItem post-dispatch native returned`
  - `TradeMgr: SubmitOffer native call fn=0x01316E10 gold=0`
  - `TradeMgr: AcceptTrade native call fn=0x01316A80`
  - `[LLM-TradeSnapshot] flags=0x3 ... player_items=1 partner_items=0`
  - `[LLM-TradeSnapshot] flags=0x7 ... player_items=1 partner_items=0`
  - `[LLM-TradeSnapshot] flags=0x0 ... player_items=0 partner_items=0`

Primary helper-side evidence:
- `gwa3/build_trade/bin/Release/trade_helper_status.json`
  - `submit_attempt_count = 1`
  - `accept_attempt_count = 3`
  - `partner_item_count = 1`
  - `partner_items = [{"item_id":192,"model_id":0,"quantity":1}]`

What is now true in the live lane:
- stackable prompt flows are healthy again
- stale prompt-noise warnings are cleaned up on the active `inventorySlot` path
- one-way trade completion is healthy again:
  - DISCO offers an item
  - BLUMPKINS observes the partner item
  - BLUMPKINS submits and accepts
  - DISCO accepts
  - the trade closes
  - the offered item leaves DISCO inventory

What remains intentionally out of scope:
- the stronger `maxCount-deref` seam is still not back on the active path
- broader submit/accept UI discovery work is still unnecessary for the live trade lane

## 2026-04-17 Follow-Up: Full Receiver Inventory Now Falls Back To Reverse Direction

New live observation:
- BLUMPKINS was full (`inventory_free_slots_total = 0`)
- that explained the “almost completes but then cancels” behavior on forward trades into the helper

Work completed:
- extended helper heartbeat/status to expose:
  - `inventory_free_slots_total`
  - `helper_stackable_offer_model_id`
  - `helper_stackable_offer_quantity`
  - `helper_safe_singleton_offer_model_id`
- made `test_player_trade_zz_open_offer_stackable_submit_accept_complete_helper` capacity-aware
  - if BLUMPKINS has receiver space, it still runs the original forward stackable completion
  - if BLUMPKINS is full, it cancels the exploratory forward open and reopens in reverse mode
- narrowed the reverse helper offer to a safe singleton model and switched the helper-side offer transport to `TradeMgr::OfferItemPacket(...)`

Why the reverse helper offer needed its own transport:
- helper stackable reverse offers failed on `OfferItemPromptMax(...)`
  - live helper logs showed prompt-open never got a usable trade-window context on the helper lane
- helper singleton reverse offers also failed on `OfferItem(...)`
  - helper logs showed `TradeMgr: OfferItem post-dispatch — context unavailable ctx=0x00000000`
- the packet/direct singleton offer path removed the helper-side trade-window-context dependency

Focused live run:
- `python -m bridge.tests --filter 'test_player_trade_zz_open_offer_stackable_submit_accept_complete_helper'`
- final result: PASS

Primary evidence from the passing run:
- helper: `gwa3/build_trade/bin/Release/gwa3_log_160084.txt`
  - `Helper observed player trade open (flags=1 offerModel=2992)`
  - `Helper auto-offering item=1093 model=2992 qty=1 mode=packet_offer (flags=1)`
  - `TradeMgr: OfferItemPacket item=1093 qty=1 laneAvailable=1`
  - `CtoS: TradeOfferItemBotshub item=1093 qty=1 queued=1`
  - `Helper observed trade flags change -> 7`
  - `Helper observed trade flags change -> 0`
- main: `gwa3/build_trade/bin/Release/gwa3_log_93956.txt`
  - `[LLM-TradeSnapshot] flags=0x3 ... player_items=0 partner_items=1`
  - `[LLM-Action] Executing: accept_trade`
  - `[LLM-TradeSnapshot] flags=0x7 ... player_items=0 partner_items=1`
  - `[LLM-TradeSnapshot] flags=0x0 ... player_items=0 partner_items=0`
- helper status:
  - `inventory_free_slots_total = 0`
  - `helper_safe_singleton_offer_model_id = 2992`
  - `ctos_add_item = 1`
  - `ctos_submit = 1`
  - `ctos_cancel = 0`

What this means now:
- the live player-trade lane no longer assumes BLUMPKINS can receive
- when the helper is full, the completion validator can still finish a real trade by flipping direction
- helper reverse offers should stay on the packet/direct singleton path unless helper-side trade-window context capture is intentionally restored later

## 2026-04-17 Follow-Up: Helper-Originated Reverse Stackable Completion Now Passes

The remaining helper follow-up was to prove BLUMPKINS could send a partial stack back to DISCO without depending on helper-side quantity UI. That is now validated.

Implementation:
- added `offer_item_quantity` to the helper config plumbing
- helper trade-open now latches both `offer_item_model_id` and `offer_item_quantity`
- helper auto-offer always uses `TradeMgr::OfferItemPacket(...)`, clamped against the actual available stack size
- added `test_player_trade_zz_reverse_helper_stackable_submit_accept_complete_helper` to exercise the helper-originated reverse stackable path directly

Important boundary discovered while wiring the test:
- helper offer config is latched on the trade-open edge
- writing `offer_item_model_id` / `offer_item_quantity` after the trade was already open produced `offerModel=0 offerQuantity=0`, empty helper submit, and a stale cancel
- once the test set helper config before opening the trade, the reverse stackable path worked

Focused live run:
- `python -m bridge.tests --filter 'test_player_trade_zz_reverse_helper_stackable_submit_accept_complete_helper'`
- final result: PASS

Primary evidence from the passing run:
- helper: `gwa3/build_trade/bin/Release/gwa3_log_33896.txt`
  - `Helper observed player trade open (flags=1 offerModel=935 offerQuantity=2)`
  - `Helper auto-offering item=434 model=935 qty=2 requested=2 available=85 mode=packet_offer (flags=1)`
  - `TradeMgr: OfferItemPacket item=434 qty=2 laneAvailable=1`
  - `CtoS: TradeOfferItemBotshub item=434 qty=2 queued=1`
  - helper flags progressed `1 -> 3 -> 7 -> 0`
- main: `gwa3/build_trade/bin/Release/gwa3_log_146388.txt`
  - `[LLM-TradeSnapshot] flags=0x1 ... player_items=0 partner_items=1`
  - `[LLM-Action] Executing: submit_trade_offer`
  - `[LLM-Action] Executing: accept_trade`
  - `[LLM-TradeSnapshot] flags=0x7 ... player_items=0 partner_items=1`
  - `[LLM-TradeSnapshot] flags=0x0 ... player_items=0 partner_items=0`
- helper status:
  - `helper_stackable_offer_model_id = 935`
  - `helper_stackable_offer_quantity = 83`
  - `ctos_add_item = 1`
  - `ctos_submit = 1`
  - `ctos_accept = 1`

What this means now:
- reverse helper-originated stackable completion is working on the live trade lane
- helper-side prompt/context capture is no longer required for that path
- the remaining trade work is no longer in reverse stackable offering; it has moved to any broader production hardening or cleanup the lane still needs

## 2026-04-17 Final-Stage Follow-Up: Two-Item Round-Trip Now Passes

The requested final-stage scenario is now green on the live trade lane:
- first completed trade carries one helper singleton/non-stackable item plus one helper stackable partial quantity in the same trade
- second completed trade returns those two items in one trade
- both players finish with the same exposed model totals they started with

Important debugging outcome:
- the first failing version of this test looked like a DISCO return-leg inventory bug
- that was misleading
- the real problem was in the Python harness refresh path:
  - `_query_fresh_tier3_snapshot(...)` could accept an older buffered tier-3 snapshot after `query_state`
  - the final round-trip assertion could therefore read stale post-forward inventory instead of a fresh post-return inventory
- fix applied in `gwa3/bridge/tests/test_f_player_trade.py`:
  - tier-3 refresh now requires a strictly newer `tick`
  - the round-trip test now waits for actual restoration with `_wait_for_fresh_tier3_state_change(...)`
  - failure diagnostics were expanded to include per-model item refs and the selected return item ids

Focused live run:
- `python -m bridge.tests --filter 'test_player_trade_zz_roundtrip_singleton_and_stackable_complete_helper'`
- final result: PASS

Primary evidence from the passing run:
- helper log: `gwa3/build_trade/bin/Release/gwa3_log_47004.txt`
  - `Helper observed player trade open (flags=1 offerModel=2992 offerQuantity=1 offerModel2=935 offerQuantity2=2)`
  - `Helper auto-offering slot=1 item=1603 model=2992 qty=1 requested=1 available=1 mode=packet_offer (flags=1)`
  - `Helper auto-offering slot=2 item=1244 model=935 qty=2 requested=2 available=83 mode=packet_offer (flags=1)`
  - `CtoS: TradeOfferItemBotshub item=1603 qty=1 queued=1`
  - `CtoS: TradeOfferItemBotshub item=1244 qty=2 queued=1`
- main log: `gwa3/build_trade/bin/Release/gwa3_log_115540.txt`
  - first trade reached `flags=0x3 ... partner_items=2`, then `flags=0x7`, then closed
  - return trade executed two `offer_trade_item` actions, then reached `flags=0x3 ... player_items=2`, then `flags=0x7`, then closed
- helper status: `gwa3/build_trade/bin/Release/trade_helper_status.json`
  - `trade_open_count = 2`
  - `submit_attempt_count = 2`
  - `accept_attempt_count = 2`
  - `ctos_add_item = 2`
  - `ctos_submit = 2`
  - `ctos_accept = 2`
  - final helper totals restored:
    - `helper_stackable_offer_total_quantity = 83`
    - `helper_safe_singleton_offer_total_quantity = 11`

Resulting state:
- the live lane now has:
  - working stackable prompt flows
  - working completion
  - working reverse helper stackable packet-offer
  - working two-item round-trip completion
- the remaining work, if any, is no longer core player-trade functionality; it is production hardening, broader scenario coverage, or eventual cleanup around non-critical helper/UI-context paths

## 2026-04-17 Follow-Up: Trade Chat And Two-Way Whispers Validate With Neutral Payloads

The chat/whisper lane is now covered without sending obviously bot-marked text to live players.

What changed:
- added a real `send_whisper` action path through `ChatMgr::SendWhisper(...)`
- extended helper config/status so BLUMPKINS can:
  - send requested trade chat
  - send requested whispers
  - publish `recent_chat`
  - publish helper chat/whisper attempt counters and last processed sequence ids
- added a `WriteWhisper` hook in `ChatLogMgr`
  - the old StoC chat packet taps were enough for trade chat
  - they were not enough to observe whispers on this Reforged lane
- updated the live integration test payloads to neutral phrases:
  - trade chat from DISCO: `price check <token>`
  - trade chat from BLUMPKINS: `one sec please <token>`
  - whisper to helper: `hey, are you free <token>`
  - whisper to main: `yes, ready now <token>`

Focused live run:
- `python -m bridge.tests --filter 'test_player_trade_zzz_chat_trade_and_whisper_helper'`
- final result: PASS

Primary evidence from the latest passing run:
- main log: `gwa3/build_trade/bin/Release/gwa3_log_33120.txt`
  - `[LLM-Action] Executing: send_chat`
  - `[LLM-Action] Handler returned: send_chat success=1 error=(none)`
  - `[LLM-Action] Executing: send_whisper`
  - `[LLM-Action] Handler returned: send_whisper success=1 error=(none)`
- helper log: `gwa3/build_trade/bin/Release/gwa3_log_77460.txt`
  - helper processed the requested neutral trade chat and whisper sends during the same focused run
- helper status: `gwa3/build_trade/bin/Release/trade_helper_status.json`
  - `chat_send_attempt_count = 1`
  - `whisper_send_attempt_count = 1`
  - `last_chat_send_seq = 1`
  - `last_whisper_send_seq = 1`

Resulting state:
- the lane now validates:
  - trade chat send/read
  - two-way whisper send/read
- the live validation strings are now innocuous and no longer contain `botshub`
