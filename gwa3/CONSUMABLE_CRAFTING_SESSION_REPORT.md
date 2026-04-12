# Consumable Crafting Debug Session Report

Date: 2026-04-11 through 2026-04-12
Lane: BISCUIT (`L I L B I S C U I T`, account index 4)
Build: `gwa3/build_biscuit`

## Goal

Implement a full conset crafting cycle: withdraw gold from bank, buy materials from material trader, craft 1 Grail of Might + 1 Essence of Celerity + 1 Armor of Salvation.

## What Works

### Frame-Click Crafting (PROVEN WORKING)

The crafter UI flow is fully working:

1. **Item selection**: `NavigateSortedChildPath(merchantFrame, {0, 0, itemIndex})` — matches AutoIt's `NavigateFramePath("0,0,index")`
2. **Craft button click**: `UIMgr::ButtonClick(action125)` via `GameThread::EnqueueRaw` — action125 is childOffset 125 in the merchant context
3. **Validated**: PID 3268 crafted a Grail of Might (`before=2 after=3`), PID 30188 crafted Armor of Salvation

The craft button click produces CtoS packet `0x049` (crafter interaction) which completes the craft.

### Movement (PROVEN WORKING)

`MovePlayerNear(x, y, threshold, timeout)` — dispatches `AgentMgr::Move` via `GameThread::EnqueuePost` every 500ms. Successfully walks character from any Embark Beach spawn point (distances up to 6000+ units) to any NPC.

### NPC Merchant Opening (PROVEN WORKING)

`AgentMgr::InteractNPC(npc)` opens merchant windows for crafters and material traders. For some NPCs, a GoNPC fallback via `GameThread::EnqueueRaw` dispatching `CtoS::SendPacket(3, 0x39, npcId, 0)` is needed.

### Crash Watchdog (PROVEN WORKING)

`StartWatchdog()` in `IntegrationTest.cpp` detects `#32770` class crash dialog windows belonging to the current process and terminates with exit code `0xCDA1`.

### Global Item Array Scan (PROVEN WORKING)

`FindTraderVirtualItemId(modelId)` scans `[BasePtr]+0x18+0x40+0xB8` for items with `bag==nullptr` and `agent_id==0` — matching AutoIt's `TraderRequest` approach. Reliably finds Iron (model 948), Dust (model 929), etc.

## What Doesn't Work

### Material Trader Buying (ALL APPROACHES FAILED)

Five different approaches were tested for buying materials from the material trader. None work:

#### 1. Native RequestQuote Function via GameThread/EngineHook

```cpp
// TraderQuoteInvoker calls:
push &itemId    // recv.item_ids
push 1          // recv.item_count
push 0, 0, 0, 0, 0  // give fields + unknown
push 0xC        // type = TraderBuy
xor ecx, ecx; mov edx, 2
call [Offsets::RequestQuote]
```

**Result**: Function executes and returns without crash. TraderHook detour fires (`quoteId` increments from 0 to 1). BUT the detour reads garbage from `[ebx+0x28]`:
- `costItemId = 0x05E40020` (heap address, not an item ID)
- `costValue = 0x003EA864` (code section address, not gold)

**Why it fails**: The TraderHook detour was designed for the rendering hook execution context where `ebx` points to the trader response struct. When called from the Engine hook or GameThread context, `ebx` has a completely different value. The `[ebp+0Ch]` alternative (the function parameter assigned to `ebx` by the original instruction) also reads garbage.

#### 2. UIMessage kSendMerchantRequestQuote (0x30000006)

```cpp
UIMgr::SendUIMessageAsm(0x30000006, &requestQuoteStruct, nullptr);
```

**Result**: Complete no-op. `quoteId=0, costItem=0, costValue=0, ctoS_total=0`.

**Why it fails**: `0x30000006` and `0x30000007` are GWCA-invented UIMessage IDs. Without GWCA's hooks installed (which register callbacks for these IDs), the game's UIMessage dispatcher has no handler and silently drops them.

#### 3. Raw SendPacket(0x4C) REQUEST_QUOTE

```cpp
CtoS::SendPacket(2, 0x4C, itemId);  // via TradeMgr::RequestQuote
```

**Result**: Immediate GW client crash (crash dialog detected by watchdog).

**Why it fails**: Raw packet sends via the CtoS sender thread crash the client for merchant-context packets. The game expects these to go through internal game functions, not raw packet injection.

#### 4. Direct TransactItems(0xC) via GameThread (skip quote)

```cpp
// Dispatched via GameThread::EnqueueRaw:
TradeMgr::TransactItems(0xC, 1, itemId);
// Which calls: CtoS::SendPacket(4, 0x4D, 0xC, 1, itemId)
```

**Result**: GW client crash after 2 dispatches.

**Why it fails**: Same as #3 — `CtoS::SendPacket` for `TRANSACT_ITEMS` goes through the sender thread which crashes for merchant transactions. (Note: for crafting, the UI frame-click approach avoids this entirely.)

#### 5. RenderHook Shellcode (RequestTraderQuoteByItemId)

```cpp
TradeMgr::RequestTraderQuoteByItemId(itemId);
// Uses RenderHook::EnqueueCommand with dynamic shellcode
```

**Result**: Returns false immediately because `RenderHook::IsInitialized()` returns false.

**Why it fails**: RenderHook is installed during bootstrap for char select, then shut down after map load to avoid conflicts with GameThread. After bootstrap, RenderHook is unavailable.

### Other Crashes Found

| What | Why | Fix |
|---|---|---|
| `CtoS::MoveToCoord` from test thread | Sender thread packet crashes for MOVE_TO_COORD | Use `MovePlayerNear` (GameThread::EnqueuePost) |
| `CtoS::SendPacket(INTERACT_LIVING)` from test thread | Sender thread crash | Use `AgentMgr::InteractNPC` (GameThread) |
| `CtoS::Dialog(agentId)` | Dialog expects button ID, not NPC agent ID | Removed |
| `UIMgr::ButtonClickImmediate` from DLL thread | SendFrameUIMsg not thread-safe | Use `ButtonClick` (GameThread::EnqueueRaw) |
| `AgentMgr::Move` via `GameThread::EnqueueRaw` (repeated calls to unreachable coords) | Native move function crash when stuck | Use `MovePlayerNear` which uses EnqueuePost |

## Key Architectural Findings

### Thread Safety Rule

ALL game function calls must go through GameThread dispatch:
- `GameThread::EnqueueRaw` — pre-dispatch (before game frame processing)
- `GameThread::EnqueuePost` — post-dispatch (after game frame processing)
- Direct calls from the DLL init thread crash the game

The one exception: `UIMgr::ButtonClick` via `GameThread::EnqueueRaw` works for UI clicks.

### FrameArray Capacity vs Size Bug (FIXED)

`FrameArrayData` was `{buffer, size}` but the game uses `GW::Array<T>` which is `{buffer, capacity, size, param}`. The code was reading `capacity` (128) as `size`, only scanning the first 128 of 1000+ frames. Fixed by adding the `capacity` field before `size`.

### InlineTask Storage Size Bug (FIXED)

`GameThread::InlineTask::storage` was 64 bytes but `CrafterTransactionTask` is 80 bytes. `EnqueueRaw` silently rejected the payload with no log. Fixed by increasing to 96 bytes and adding a warning log.

### Merchant Frame Children

The merchant frame (`hash=3613855137`) uses context-based child linking, not parent-relation child arrays. `GetChildFrameCount(merchantRoot)` now returns the correct count after the FrameArray fix. Item rows are at path `{0, 0, index}` and the Craft button is at childOffset 125 in the merchant context.

### UIMessage IDs Are GWCA-Invented

`kSendMerchantRequestQuote = 0x30000006` and `kSendMerchantTransactItem = 0x30000007` are NOT native game UIMessage IDs. They only work when GWCA hooks are installed (which register callbacks that call the original game functions). Without GWCA, these messages are dropped.

## NPC Coordinates (Embark Beach)

| NPC | X | Y | Purpose |
|---|---|---|---|
| Eyja | 3336 | 627 | Grail of Might crafter |
| Kwat | 3596 | 107 | Essence of Celerity crafter |
| Alcus | 3704 | -163 | Armor of Salvation crafter |
| Material Trader | 2933 | -2236 | Basic material trader |
| Xunlai Chest | 2283 | -2134 | Storage access |

## Material IDs

| Material | Model ID | Needed Per Conset |
|---|---|---|
| Iron Ingot | 948 | 100 (50 Grail + 50 Armor) |
| Pile of Glittering Dust | 929 | 100 (50 Grail + 50 Essence) |
| Bone | 921 | 50 (Armor) |
| Feather | 933 | 50 (Essence) |

| Consumable | Model ID | Recipe |
|---|---|---|
| Grail of Might | 24861 | 50 Iron + 50 Dust + 250g |
| Essence of Celerity | 24859 | 50 Feather + 50 Dust + 250g |
| Armor of Salvation | 24860 | 50 Iron + 50 Bone + 250g |

## Files Modified

### Core Fixes
- `gwa3/src/managers/UIMgr.cpp` — FrameArray capacity/size fix, forward declarations, KeyPress/TestMouseAction stubs, SendUIMessageAsm
- `gwa3/src/core/GameThread.cpp` — InlineTask storage 96 bytes, EnqueueRaw rejection warning
- `gwa3/src/managers/TradeMgr.cpp` — CraftMerchantItemInvoker UIMessage logging, CraftMerchantItem logging
- `gwa3/src/core/TraderHook.cpp` — Debug ebx capture, [ebp+0Ch] alternative path
- `gwa3/src/tests/IntegrationTest.cpp` — #32770 crash dialog watchdog detection

### Test Harness
- `gwa3/src/tests/IntegrationTestSession.cpp` — Full conset cycle test, ConsetMoveToNPC, ConsetCraftOneItem, ConsetBuyMaterial, FindTraderVirtualItemId, RequestTraderQuoteViaGameThread, TraderQuoteInvoker

### Build/Config
- `gwa3/CMakeLists.txt` — Added DungeonInventory.cpp, DungeonItemActions.cpp, DungeonItemPolicy.cpp
- `gwa3/include/gwa3/managers/UIMgr.h` — Added 20+ missing declarations
- `gwa3/include/gwa3/managers/TradeMgr.h` — Added 15+ missing declarations
- `gwa3/include/gwa3/packets/CtoS.h` — PacketTapSnapshot struct
- `gwa3/include/gwa3/core/Scanner.h` — ToFunctionStart declaration
- `gwa3/include/gwa3/core/SmokeTest.h` — RunConsumableCraftingTest declaration
- `gwa3/src/dllmain.cpp` — consumableCraftingTest flag and dispatch
- `gwa3/tools/injector.cpp` — --test-consumables, --consumable-stage, --consumable-target flags

## Next Step: Material Trader UI Frame-Click Buy

The only approach that works for game transactions is UI frame-clicking. The material trader UI should work the same way as the crafter:

1. Open material trader (already working via InteractNPC + GoNPC)
2. Find merchant frame by hash (`kMerchantRootHash = 3613855137`)
3. Find the material item in the trader's list by model ID → get its position
4. Click item row `{0, 0, position}` to select it
5. Find and click the Buy button (likely action125 or similar childOffset in merchant context)
6. Wait for gold decrease / inventory increase
7. Repeat for each pack needed

This mirrors the proven crafter approach. The material trader's Buy button may be at a different childOffset than the crafter's Craft button (125). A frame dump of the material trader's merchant context children would identify the correct button.
