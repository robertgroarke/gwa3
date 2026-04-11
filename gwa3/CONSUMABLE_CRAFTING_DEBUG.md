# Consumable Crafting Debug Harness

This harness is for the `BISCUIT` lane only.

It targets Embark Beach consumable crafters and is meant to debug:
- travel to Embark Beach
- crafter window opening
- crafter inventory contents
- `TradeMgr::TransactItems(3, ...)` crafting behavior

It does not use the player-trade harness and does not require a helper account.

The harness now prefers:
- region `4` (`Asia Japan`)
- district `99`
- fallback district `1` in the same region

Do not run this harness in America English districts unless the task explicitly requires that.

## Build

```powershell
cd C:\Users\Robert\Documents\GWA Censured X BotsHub\gwa3
cmake --build --preset biscuit --target gwa3 injector
```

## Launch BISCUIT

Use:

```powershell
"C:\Program Files (x86)\AutoIt3\AutoIt3.exe" "C:\Users\Robert\Documents\GWA Censured X BotsHub\GWA Censured\debug_scripts\launch_biscuit_via_gwlauncher.au3"
```

Then inject from:

```powershell
cd C:\Users\Robert\Documents\GWA Censured X BotsHub\gwa3\build_biscuit\bin\Release
```

## Harness Commands

List-only probe for Grail crafter:

```powershell
.\injector.exe --dll gwa3_biscuit.dll --test-consumables --consumable-stage list-only --consumable-target grail
```

Open-only probe for Essence crafter:

```powershell
.\injector.exe --dll gwa3_biscuit.dll --test-consumables --consumable-stage open-only --consumable-target essence
```

Full craft probe for Armor crafter:

```powershell
.\injector.exe --dll gwa3_biscuit.dll --test-consumables --consumable-stage full --consumable-target armor
```

Probe all three crafters:

```powershell
.\injector.exe --dll gwa3_biscuit.dll --test-consumables --consumable-stage list-only --consumable-target all
```

## Stages

- `travel-only`: verify Embark Beach travel only
- `open-only`: travel and open crafter context
- `list-only`: open crafter context and dump crafter inventory
- `craft-only`: open crafter context, dump inventory, and craft one item
- `full`: same as `craft-only`

## Targets

- `grail`: Eyja / Grail of Might
- `essence`: Kwat / Essence of Celerity
- `armor`: Alcus / Armor of Salvation
- `all`: iterate all three crafters

## Expected Output

The harness logs:
- target map and world readiness
- actual region and district reached after travel
- nearby NPC candidates near the crafter
- merchant/crafter trade context pointers
- StoC traffic seen during crafter interaction
- crafter inventory rows: item id, model id, type, value, quantity
- inventory delta before and after one craft transact
- `consumable_harness_status.json` with the latest stage/result snapshot

## Froggy Fix Included

`FroggyHM.cpp` no longer sends `QuestMgr::Dialog(npcId)` during consumable crafting.

The crafter open path now uses the same proven merchant interaction path as the Gadd's merchant flow:
- target NPC
- raw/native merchant open path
- wait for merchant context
- locate crafter item by model id
- transact craft

## GWA2 Findings

The old AutoIt maintenance code gives two important signals:

- The final intended crafting path was not "prompt clicking all the way down". It had a dedicated crafter quote and crafter execute flow in [GWA2_Crafting.au3](</C:/Users/Robert/Documents/GWA%20Censured%20X%20BotsHub/GWA%20Censured/lib/custom/GWA2_Crafting.au3>):
  - `CommandRequestCraftQuote`
  - `CommandCraftExecute`
  - `CommandCraftItemEx2`
- That native path was built around `TransactionFunction` opcode `3` (`CrafterBuy`) with explicit:
  - merchant item id / pointer
  - amount to craft
  - total gold cost
  - material id array
  - material quantity array
  - TradeID / merchant index state

The old UI layer in [GWA2_FrameUI.au3](</C:/Users/Robert/Documents/GWA%20Censured%20X%20BotsHub/GWA%20Censured/lib/custom/GWA2_FrameUI.au3>) did have a working `CraftConsumableByUI(...)`, but it was a narrow fixed-path implementation:

- item path: `0,0,index`
- craft button path: `0,1,1`
- click primitive: `SendFrameUIMsg(..., 0x31 /* kMouseClick2 */, MouseUp=0x7, ...)`

That matters because the current live BISCUIT crafter UI does not match those fixed paths. The current merchant tree has already shown that the old `[0,0,index]` assumption is wrong for this client, so the GWA2 UI method is useful for click semantics but not as a stable row/button map.

## Upstream Py4GW Findings

Checked upstream Py4GW on GitHub:

- [Py4GWCoreLib/UIManager.py](https://github.com/apoguita/Py4GW/blob/main/Py4GWCoreLib/UIManager.py)
- [Legacy code and tests/Deprecated but working/cupcake_mantainer.py](https://github.com/apoguita/Py4GW/blob/main/Legacy%20code%20and%20tests/Deprecated%20but%20working/cupcake_mantainer.py)

The current upstream does not provide a better crafter UI-message solution than the local copy:

- `UIManager.ConfirmMaxAmountDialog()` is still a generic helper with hardcoded hashes:
  - `4008686776`
  - `4014954629`
- Those hashes do not match the live BISCUIT crafter prompt hashes we are seeing.

More importantly, the upstream Py4GW crafting path is native, not UI-driven:

- `cupcake_mantainer.py` resolves the offered crafter item by output model id
- then calls `GLOBAL_CACHE.Trading.Crafter.CraftItem(...)`
- it does not drive the consumable craft through the quantity prompt with frame clicks
- the older `test_craft_transact.au3` probe also shaped the native `CrafterBuy` receive payload with the merchant item `item_id`, not a merchant item pointer

That lines up with the old GWA2 maintenance code and is strong evidence that:

1. UI is useful for opening/selecting the crafter context.
2. The quantity prompt is not the reliable long-term transaction path.
3. The real fix is still in the native crafter quote/execute path, not more child-offset guessing.

### Current Conclusion

The old AutoIt code suggests the right long-term GWA3 fix is:

1. Keep UI only for opening the crafter and selecting the desired merchant entry.
2. Stop relying on the quantity prompt as the main transaction path.
3. Port a real crafter-native quote/execute path into `TradeMgr` for opcode `3`, modeled on the old `CommandRequestCraftQuote` / `CommandCraftExecute` / `CommandCraftItemEx2` flow.

The current BISCUIT harness is still useful because it proves:

- travel is working
- crafter open is working
- merchant item resolution is mostly working
- the remaining failure is in the craft completion path

But the GWA2 maintenance code strongly implies the prompt-remap work should now be treated as diagnostic only, not the final architecture.

## Current Findings

Status as of `2026-04-11` on the `BISCUIT` lane:

- The harness reliably reaches `Embark Beach (857)` in `Asia/Japan`, preferred district `99`, with fallback to district `1`.
- The harness reliably finds Eyja and can open the crafter window, but the most reliable open path is still:
  - native `InteractNPC` attempts first
  - raw `GoNPC` fallback when native interact does not expose merchant context
- The crafter inventory is real and stable once open. Eyja consistently exposes Grail of Might in merchant slot `1`.

### UI Row / Prompt State

What is proven:

- Row selection can succeed on some runs and fail on others.
- When row selection succeeds, the current client shows a stable post-action frame after `action126`.
- The prompt subtree remains stable across runs:
  - root child `2`: hash `600507066`, `childCount=1`
  - root child `4`: hash `1852904459`, `childCount=4`
  - root child `5`: hash `846394075`, `childCount=3`
- Direct clicks on the obvious prompt candidates are real clicks but do not complete crafting:
  - `child2[0]`
  - `bar[5][2]`
  - `bar[5][1]`
  - `root[3]`
- Editable and numeric writes into the nested quantity subtree also do not complete crafting.

What is not happening:

- the prompt never closes from the current confirm logic
- no materials move
- no gold moves
- finished consumable count does not increase

Conclusion:

- The visible quantity prompt is diagnostic, but it is not yet a working transaction path.

### Native Crafter UI-Message Path

The current `TradeMgr` native crafter path uses merchant `UIMessage` traffic first and only falls back to older direct invokers when `UIMessage` is unavailable.

Validated live result:

- `native_quote_queued=1`
- `native_craft_queued=1`
- `native_quote_complete` always shows:
  - `observed=0`
  - `quoteAfter=0`
  - `costItem=0`
  - `costValue=0`
  - `ctoS=none`
- `native_craft_complete` always shows:
  - `observed=0`
  - no gold delta
  - no item delta
  - `ctoS=none`

Meaning:

- the merchant `UIMessage` crafting path is currently a no-op on this client
- it does not even translate into outbound `PacketSend` traffic

This is the strongest current finding.

### PacketSend Tap Findings

A `PacketSend` tap was added in `CtoS` and surfaced into the consumable harness status.

Validated result:

- Normal crafter `UIMessage` quote/craft attempts produce `ctoS=none`.
- So the failure is not server rejection after a packet leaves.
- The failure happens before any outbound packet is emitted.

### Explicit Packet Experiments

Two packet experiments were run after row selection / merchant open.

#### Experiment 1: `SendPacketViaGameCommand`

Validated on PID `37560`:

- `REQUEST_QUOTE (0x4C)` was sent through `SendPacketViaGameCommand`.
- The client immediately produced a real `Gw.exe` crash dialog.
- The integration watchdog detected and killed the process.
- Relevant log evidence:
  - `CtoS: SendPacketViaGameCommand header=0x4C size=2`
  - followed by explicit watchdog crash-dialog detection

Conclusion:

- `0x4C` through the engine-command transport is not safe in the current crafter state.

#### Experiment 2: normal `SendPacket`

Source was changed to switch the packet wrappers from `SendPacketViaGameCommand(...)` to normal `SendPacket(...)`.

Stable result so far:

- this removed the immediate crash behavior seen with the game-command lane
- however, the latest validated live run (`PID 17972`) hit `row_click_failed`, so the packet path was not exercised in that run

Important note:

- there is a newer source edit to also try packet fallback after `row_click_failed`
- that edit was made locally, but the rebuild was interrupted before validation
- so it is in source, but not yet live-validated

### Row Resolution State

Row resolution is better than before but still nondeterministic.

Validated examples:

- Good run `PID 35192`: row resolved and clicked successfully
- Bad run `PID 17972`: row and rowClick both resolved to `0x00000000` even though merchant context and item position were correct

Current implication:

- we have two separate issues:
  - row resolution is not stable enough across all runs
  - even on good row-selected runs, native crafting still does nothing

## Current Crafting Test State (updated 2026-04-11)

### What Works

1. Travel to quiet Asian Embark Beach — reliable
2. Crafter approach and merchant opening via raw GoNPC — reliable
3. Merchant inventory detection (8 items, Grail at position 1) — reliable
4. UI row selection via `NavigateSortedChildPath` — **works on some runs** but fails when merchant frame reports `childCount=0`

### What Does NOT Work

5. **Raw packet sends (0x4C / 0x4D) crash the client** — both `SendPacketViaGameCommand` AND normal `SendPacket` crash Gw.exe. Disabled in harness.
6. **UIMessage crafter path is a no-op** — `kSendMerchantRequestQuote` / `kSendMerchantTransactItem` produce `ctoS=none`, zero server response.
7. **Direct function call `RequestQuoteFunction`** — emits a real `0x4C` CtoS packet but server ignores it (no quote response). This is expected: **consumable crafters have no quote step**.
8. **Direct function call `TransactionFunction`** — emits no outbound packet at all.
9. **`ButtonClick` on the Craft button (action125)** — frame is often hidden after row selection, so click is skipped. When `pathActionFrame` (`{0,1,1}`) is clicked instead, it has the wrong context and produces `ctoS=none`.
10. **There is no quantity prompt** for single-craft (only enough materials for 1 craft). The "quantity prompt" the harness was trying to interact with is misidentified.

### Key Architectural Findings

- **No quote step at consumable crafter.** User confirmed: the UI is "select item → click Craft". No `RequestQuote` needed.
- **Merchant frame children use context-based linking**, not parent-relation child arrays. `GetChildFrameCount(merchantRoot)` returns 0, but `GetFrameByContextAndChildOffset(merchantContext, 125/126, ...)` finds real frames.
- **action125 = Craft button, action126 = Goodbye button** (based on childOffset in merchant context).
- **action125 is marked `hidden=1`** even after a successful row click. The Craft button may require additional internal state to become visible/active.
- **`pathActionFrame` (child path `{0,1,1}`) has a DIFFERENT context** from the merchant context. Clicking it dispatches to the wrong context and produces no effect.

### Root Cause Summary

The crafting UI path is blocked by two interacting problems:

1. **Row selection is nondeterministic** — `NavigateSortedChildPath` fails when the merchant frame has no parent-relation children. The frame tree is context-linked, not hierarchically linked.
2. **Craft button (action125) is hidden** — even when the row click succeeds, the Craft button's frame state has the hidden flag set. `ButtonClick` skips hidden frames.

### Next Steps

1. **Investigate why action125 is hidden** after row selection. Possible causes:
   - Row click via `ButtonClick` doesn't produce the same internal selection state as a real mouse click
   - The frame state needs a full mouse-down + mouse-up sequence (current code only sends mouse-up)
   - The crafter UI requires the row to be clicked with a specific frame message or wParam
2. **Try `ButtonClickImmediateFull` on action125** even when hidden — bypass the hidden check
3. **Check if the GWA2 FrameUI click primitive differs** — GWA2 uses `SendFrameUIMsg(context, 0x31, &action, 0)` with `action.action_state = 0x7` (MouseUp), same as gwa3. But GWA2 also has `kMouseClick2 = 0x31` with `MouseUp = 0x7`, matching exactly.
4. **Consider using `SendUIMessage` (global) instead of `SendFrameUIMsg`** for the craft button — GWCA's merchant path uses `UI::SendUIMessage` not `SendFrameUIMsg`
