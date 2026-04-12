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
