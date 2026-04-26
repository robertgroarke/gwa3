# Player Trade Debug Status

Last updated: 2026-04-17

## Scope

This document captures the current state of `gwa3` player-to-player trade work, with emphasis on the stackable-item quantity dialog path.

Primary live test lane:
- build: `gwa3/build_trade`
- dll: `gwa3_trade.dll`
- pipe: `\\\\.\\pipe\\gwa3_llm_trade`

Primary live characters:
- main: `D I S C O P A N I C`
- helper: `B L U M P K I N S`

Launcher/test discipline:
- launch only via `GWLauncher`
- inject only the launcher-returned PID
- use fresh clients per run
- clean up only same-character clients
- run in quiet districts:
  - preferred: Asia/Japan district `99`
  - fallback: Asia/Japan district `1`

## Current Working State

The following player-trade behaviors are working:

1. Trade open
- `initiate_trade` is working.
- The stable path is:
  - target the player
  - use native interact/player UI path
  - click the real trade button

2. Trade cancel
- open/cancel helper test passes.

3. Helper-side trade acceptance harness
- helper mode can passively observe or actively accept/submit based on config.

4. Non-stack item offer
- native player trade window context capture is working well enough for non-stack item offering.

5. Stackable `Max` prompt path
- stackable offer through the quantity prompt works when using `Max`.
- focused test passes:
  - `test_player_trade_open_offer_stackable_prompt_max_cancel_helper`

6. Stackable default quantity prompt path (NEWLY PASSING)
- offering a stackable item and confirming the prompt at default quantity (`1`) now works.
- focused test passes:
  - `test_player_trade_open_offer_stackable_prompt_default_quantity_cancel_helper`

7. Stackable exact quantity prompt path (NEWLY PASSING)
- entering an arbitrary quantity through the prompt now works.
- focused test passes:
  - `test_player_trade_open_offer_stackable_prompt_exact_quantity_cancel_helper`

## Root Causes Found

### Bug 1: Premature mode consumption in popup callback

The `OnChooseQuantityPopupUIMessage` hook fires on multiple UIMessages during popup lifecycle (msg=0x4, 0x5, 0x31, 0x9). The code was queuing clicks and consuming the pending mode on any message where `FindTradeQuantityPromptFrame()` returned a valid frame.

On the **first** popup open of a GW session, the frame wasn't findable until msg=0x9 (kInitFrame), so MaxOffer worked by luck. On subsequent popup opens (or when the trade window had already been set up), the frame was findable earlier (msg=0x5), so the mode was consumed before kInitFrame. Clicks queued at pre-init time are silently ignored by the game because the popup's event handlers aren't wired up yet.

**Fix:** Gate the callback on `kInitFrame` (msg=0x9) only, matching GWToolbox's pattern. Earlier messages log and return without consuming the mode.

### Bug 2: Popup's backing count not updated by UI manipulation

The GmItemSplit popup has an internal data structure with:
- `+0x04`: count (current quantity selection, defaults to 1)
- `+0x08`: maxCount (full stack size)

This data structure lives in the `uictl_context` pointer of the popup's `FrameInteractionCallback` entry. The Max button internally sets `count = maxCount`. The OK button reads `count` and commits.

All of our UI-level approaches (SetEditableTextValue, SetNumericFrameValue, KeyPress typing, spinner clicks) only modify the displayed UI text, NOT the backing `count` field. The OK handler always reads the backing field.

**Fix:** For `ValueOffer` mode, directly write the desired quantity to `uictl_context + 0x04` during kInitFrame, then queue OK. The callback finds its own entry in the frame_callbacks array by matching the hook function address.

## Implementation Details

### QuantityPromptAutomationMode

```
None = 0        // no automation
DefaultOffer = 1 // queue just OK on kInitFrame (count defaults to 1)
MaxOffer = 2     // queue Max then OK on kInitFrame
ValueOffer = 3   // write count to uictl_context+0x04, then queue OK
```

### Data structure layout (GmItemSplit context)

Found via live probing the `FrameInteractionCallback` array at `frame + 0xA8`:
- `uictl_context + 0x00`: unknown
- `uictl_context + 0x04`: count (uint32, current selection)
- `uictl_context + 0x08`: maxCount (uint32, full stack size)
- Confirmed by comparing maxCount against known item stack sizes

### Key files changed

- [TradeMgr.cpp](./gwa3/src/managers/TradeMgr.cpp):
  - `OnChooseQuantityPopupUIMessage`: gated on kInitFrame, ValueOffer mode with direct count write
  - `OfferItemPromptValue`: now uses callback-based ValueOffer mode
  - `OfferItemPromptDefault`: already used callback-based DefaultOffer mode

## Proven Unsafe / Rejected Paths

1. Raw low-level player-trade initiation packet guessing
- stale/raw trade-initiate paths caused crashes or inert behavior.
- stable trade-open now uses the UI button path instead.

2. `root[2]` numeric-frame treatment
- treating `root[2]` like a numeric value frame caused an explicit `Gw.exe` crash dialog in live testing.

3. `SetNumericFrameValue` / `SetEditableTextValue` during kInitFrame callback
- calling these on root[1] during the popup's kInitFrame crashed the game.
- the frame internals are not ready for UI message dispatch at that point.

4. Helper auto-submit during quantity prompt tests
- this polluted prompt tests.
- helper prompt tests now run with `auto_submit=False`.

5. Reusing old GW clients between trade test runs
- invalid because a client can only be injected once.

6. UI-level value manipulation for popup commit
- SetEditableTextValue, SetNumericFrameValue, KeyPress typing, spinner clicks
- all of these modify the displayed UI text but NOT the popup's backing count
- the OK handler reads the backing field, not the displayed text

## Current Test Suite Status

| Test | Status |
|---|---|
| open/cancel | PASS |
| non-stack offer | PASS |
| stackable prompt max | PASS |
| stackable prompt default quantity | PASS |
| stackable prompt exact quantity | PASS |

## Suggested Minimal Repro For Future Debugging

Use these focused tests:

1. Control case: `python -m bridge.tests --filter '*prompt_max*'`
2. Default quantity: `python -m bridge.tests --filter '*prompt_default_quantity*'`
3. Exact quantity: `python -m bridge.tests --filter '*prompt_exact_quantity*'`

## 2026-04-17 Update: Passive Strong-Seam Validation Changed The Boundary

Trade lane rebuilt:
- `cmake --build --preset trade --target gwa3 injector`

Focused live run:
- `python -m bridge.tests --filter 'test_player_trade_open_offer_stackable_prompt_max_cancel_helper'`

Code changes made before the run:
- the stronger `ChooseQuantityPopup` seam now installs a **passive trampoline-only detour**
- that passive detour logs raw entry registers and stack words
- no prompt clicks are queued when the stronger seam is selected

Important current-source reality:
- `TradeMgr::OfferItemPromptQuantity(...)` in the current lane no longer performs the older native prompt-open call
- it only arms `s_pendingOffer`
- `EnableTradeWindowCaptureForPlayerTrade()` currently logs:
  - `TradeMgr: UpdateTradeCart hook SKIPPED (trampoline crashes GW)`
- that means the current source does **not** have a confirmed prompt-open executor on this path

Live evidence from `gwa3/build_trade/bin/Release/gwa3_log_149052.txt`:
- `TradeMgr: ChooseQuantityPopup hook installed at 0x01001170 seam=maxCount-deref mode=passive`
- `TradeMgr: OfferItemPromptQuantity arming deferred offer item=747 quantity=0`
- `TradeMgr: OfferItemPromptMax passive seam active; observing prompt open only item=747 seam=maxCount-deref`
- `TradeMgr: OfferItemPromptMax passive seam observed prompt item=747 seam=maxCount-deref frame=0x26E6BC58 frameId=224 childCount=5 context=0x26E71128`
- crash dialog still appeared immediately afterward
- watchdog screenshot:
  - `gwa3/build_trade/bin/Release/screenshots/watchdog_crash_dialog_20260417_075516.bmp`

Most important negative evidence:
- there were **no** `ChooseQuantityPopup passive entry[...]` logs before the crash
- the stronger seam was installed, but it was not observed executing on this path before Guild Wars faulted

Current interpretation:
- the earlier theory that the remaining blocker is only the stronger seam detour ABI is now too narrow
- the stronger seam may still be the wrong ABI or wrong start, but the passive run shows the crash can still happen without that detour ever being observed
- the current `FindTradeQuantityPromptFrame()` match is not sufficient proof that a real `GmItemSplit` popup was opened, because the current lane did not execute a confirmed prompt-open path

Current tighter boundary:
1. Re-establish a real prompt-open execution path in the current source/lane.
2. Tighten prompt-frame validation so a generic trade-window child cannot be mistaken for the quantity popup.
3. Only after steps 1 and 2 should the stronger seam ABI/start be judged from live evidence.

## 2026-04-17 Update: Prompt Paths Restored By Preferring InventorySlot Active Hook

Trade lane rebuilt:
- `cmake --build --preset trade --target gwa3 injector`

Focused live runs:
- `python -m bridge.tests --filter 'test_player_trade_open_offer_stackable_prompt_exact_quantity_cancel_helper'`
- `python -m bridge.tests --filter 'test_player_trade_open_offer_stackable_prompt_default_quantity_cancel_helper'`
- `python -m bridge.tests --filter 'test_player_trade_open_offer_stackable_prompt_max_cancel_helper'`

Result:
- all three focused stackable prompt tests now pass again on fresh DISCO/BLUMPKINS clients

What changed in code:
- `TradeMgr::EnsureChooseQuantityPopupHook()` now prefers the historically working `inventorySlot` hook as an **active** automation hook whenever the stronger `maxCount-*` seams are still marked passive-only
- the stronger seams remain available for passive investigation, but they no longer block live quantity-dialog automation
- the conservative manual prompt-confirm code remains in place as fallback/instrumentation, but the live success path is back on the callback-driven `inventorySlot` route

Live evidence:
- `gwa3/build_trade/bin/Release/gwa3_log_150416.txt`
  - `TradeMgr: ChooseQuantityPopup hook installed at 0x01000FB0 seam=inventorySlot mode=active`
  - `TradeMgr: ValueOffer wrote count=2 (was 1, max=250) at ctx=0x199714A8+0x04`
  - `TradeMgr: OnChooseQuantityPopupUIMessage mode=3 msg=0x9 ... queuedClicks=1 okFrameId=129 maxFrameId=130`
- `gwa3/build_trade/bin/Release/gwa3_log_157240.txt`
  - `TradeMgr: ChooseQuantityPopup hook installed at 0x01000FB0 seam=inventorySlot mode=active`
  - `TradeMgr: OnChooseQuantityPopupUIMessage mode=1 msg=0x9 ... queuedClicks=1 okFrameId=127 maxFrameId=128`
- `gwa3/build_trade/bin/Release/gwa3_log_160480.txt`
  - `TradeMgr: ChooseQuantityPopup hook installed at 0x01000FB0 seam=inventorySlot mode=active`
  - `TradeMgr: OnChooseQuantityPopupUIMessage mode=2 msg=0x9 ... queuedClicks=2 okFrameId=130 maxFrameId=131`
  - `TradeMgr: DrainQueuedPromptClicks clicked frameId=131 ...`
  - `TradeMgr: DrainQueuedPromptClicks clicked frameId=130 ...`

Current interpretation:
- the operational blocker is fixed
- the live trade lane is healthy again for stackable quantity dialog automation
- the strongest remaining unresolved item is **not** user-facing quantity behavior; it is the exact ABI/callback correspondence of the stronger `maxCount-deref` seam

Residual cleanup debt:
- after the active 6-child prompt callback path completes, the manual fallback can still find a stale 5-child frame and log:
  - `ConfirmTradeQuantityPromptValue no candidate closed the prompt for quantity 'N'`
- this warning is currently noisy but non-blocking; the focused tests above still pass

## 2026-04-17 Update: Stale Prompt Noise Cleaned Up, Completion Path Unblocked

Trade lane rebuild:
- `cmake --build --preset trade --target gwa3 injector`

Focused live runs:
- `python -m bridge.tests --filter 'test_player_trade_open_offer_stackable_prompt_exact_quantity_cancel_helper'`
- `python -m bridge.tests --filter 'test_player_trade_zz_open_offer_submit_accept_complete_helper'`

Code changes made before the runs:
- `TradeMgr.cpp`
  - after an active `inventorySlot` callback click, the prompt flow now treats the known residual 5-child quantity tree as a terminal post-success state instead of driving the old manual fallback through it
  - `OfferItemPromptValue(...)` now logs:
    - `callback path reached residual 5-child prompt after inventorySlot automation ...; skipping manual fallback`
- `ActionExecutor.cpp`
  - fixed `HandleOfferTradeItem(...)` to return `MakeOk()`
  - before this fix, the handler fell off the end without returning an `ActionResult`, which left `offer_trade_item` in undefined-behavior territory

Live evidence for the prompt-noise cleanup:
- `gwa3/build_trade/bin/Release/gwa3_log_157772.txt`
  - `TradeMgr: OfferItemPromptValue callback path reached residual 5-child prompt after inventorySlot automation item=1241 quantity=2 frame=0x268795B8; skipping manual fallback`
- the earlier false warning:
  - `ConfirmTradeQuantityPromptValue no candidate closed the prompt for quantity '2'`
  is absent from the new exact-quantity run

Live evidence for the completion-path unblocker:
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
- helper-side status after the completion run:
  - `gwa3/build_trade/bin/Release/trade_helper_status.json`
  - `submit_attempt_count = 1`
  - `accept_attempt_count = 3`
  - `partner_item_count = 1`
  - `partner_items = [{"item_id":192,"model_id":0,"quantity":1}]`

Result:
- stale manual-fallback prompt noise is cleaned up on the active `inventorySlot` path
- the one-way completion path now passes live again:
  - DISCO offers one item
  - BLUMPKINS sees the partner item
  - BLUMPKINS submits/accepts
  - DISCO accepts
  - the trade closes and the sacrificial item leaves DISCO inventory

Current boundary:
- stackable prompt flows are healthy again
- one-way submit/accept completion is healthy again
- the stronger `maxCount-deref` seam remains intentionally off the critical path and still needs separate ABI-level investigation later

## 2026-04-17 Update: Receiver-Full Reverse Trade Path Validated

Root cause observed live:
- BLUMPKINS can sit at `inventory_free_slots_total = 0`
- in that state, forward DISCO -> BLUMPKINS completion is the wrong validation direction even though trade transport is healthy

What changed:
- helper status now reports:
  - `inventory_free_slots_total`
  - `helper_stackable_offer_model_id`
  - `helper_stackable_offer_quantity`
  - `helper_safe_singleton_offer_model_id`
- the stackable completion test now flips direction when BLUMPKINS has no receiver slot
- helper reverse-direction offering was narrowed to a safe singleton and sent via `TradeMgr::OfferItemPacket(...)`

Why packet-offer mattered:
- helper stackable `PromptMax` stalled because helper-side prompt-open never captured trade-window context
- helper singleton `OfferItem(...)` also stalled because helper-side post-dispatch trade context was `0x00000000`
- `OfferItemPacket(...)` removed the helper-side UI-context dependency for the reverse fallback

Focused live validation:
- `python -m bridge.tests --filter 'test_player_trade_zz_open_offer_stackable_submit_accept_complete_helper'`
- result: PASS on fresh clients with BLUMPKINS still full

Primary evidence:
- `gwa3/build_trade/bin/Release/gwa3_log_160084.txt`
  - `Helper observed player trade open (flags=1 offerModel=2992)`
  - `Helper auto-offering item=1093 model=2992 qty=1 mode=packet_offer (flags=1)`
  - `TradeMgr: OfferItemPacket item=1093 qty=1 laneAvailable=1`
  - `CtoS: TradeOfferItemBotshub item=1093 qty=1 queued=1`
  - `Helper observed trade flags change -> 7`
  - `Helper observed trade flags change -> 0`
- `gwa3/build_trade/bin/Release/gwa3_log_93956.txt`
  - `[LLM-TradeSnapshot] flags=0x3 ... player_items=0 partner_items=1`
  - `[LLM-Action] Executing: accept_trade`
  - `[LLM-TradeSnapshot] flags=0x7 ... player_items=0 partner_items=1`
  - `[LLM-TradeSnapshot] flags=0x0 ... player_items=0 partner_items=0`
- `gwa3/build_trade/bin/Release/trade_helper_status.json`
  - `inventory_free_slots_total = 0`
  - `helper_safe_singleton_offer_model_id = 2992`
  - `ctos_add_item = 1`
  - `ctos_submit = 1`
  - `ctos_cancel = 0`

Current boundary:
- player-trade completion now has a validated fallback when the receiver inventory is full:
  - do not force DISCO -> BLUMPKINS
  - reverse direction and let BLUMPKINS send a safe singleton item to DISCO
- helper-side reverse offers should stay on packet/direct-offer transport unless helper trade-window context capture is explicitly restored later

## 2026-04-17 Update: Helper Reverse Stackable Packet Offer Validated

What changed:
- helper auto-offer now latches `offer_item_quantity` at trade-open and always uses `TradeMgr::OfferItemPacket(...)`
- added a dedicated reverse completion test:
  - `test_player_trade_zz_reverse_helper_stackable_submit_accept_complete_helper`
- the first attempt failed because the new test wrote helper config after the trade was already open
  - helper log showed `offerModel=0 offerQuantity=0`, then BLUMPKINS auto-submitted empty and eventually stale-cancelled
  - that confirmed the helper-offer config is latched only on the trade-open edge
- the test was then corrected to set helper config before opening the reverse trade

Focused live validation:
- `python -m bridge.tests --filter 'test_player_trade_zz_reverse_helper_stackable_submit_accept_complete_helper'`
- result: PASS on fresh clients

Primary evidence:
- `gwa3/build_trade/bin/Release/gwa3_log_33896.txt`
  - `Helper observed player trade open (flags=1 offerModel=935 offerQuantity=2)`
  - `Helper auto-offering item=434 model=935 qty=2 requested=2 available=85 mode=packet_offer (flags=1)`
  - `TradeMgr: OfferItemPacket item=434 qty=2 laneAvailable=1`
  - `CtoS: TradeOfferItemBotshub item=434 qty=2 queued=1`
  - helper trade flags advanced `1 -> 3 -> 7 -> 0`
- `gwa3/build_trade/bin/Release/gwa3_log_146388.txt`
  - `[LLM-TradeSnapshot] flags=0x1 ... player_items=0 partner_items=1`
  - `[LLM-Action] Executing: submit_trade_offer`
  - `[LLM-Action] Executing: accept_trade`
  - `[LLM-TradeSnapshot] flags=0x7 ... player_items=0 partner_items=1`
  - `[LLM-TradeSnapshot] flags=0x0 ... player_items=0 partner_items=0`
- `gwa3/build_trade/bin/Release/trade_helper_status.json`
  - `helper_stackable_offer_model_id = 935`
  - `helper_stackable_offer_quantity = 83`
  - `ctos_add_item = 1`
  - `ctos_submit = 1`
  - `ctos_accept = 1`

Current boundary after this run:
- helper-originated reverse stackable trades are now validated on packet/direct-offer transport
- helper-side prompt/UI context is no longer required for the reverse stackable completion path
- the stronger `maxCount-deref` quantity seam remains out of the critical path

## 2026-04-17 Update: Two-Item Round-Trip Completion Was Failing On A Stale Tier-3 Read, Not A Live Trade Bug

What changed:
- added a final-stage live test:
  - `test_player_trade_zz_roundtrip_singleton_and_stackable_complete_helper`
- this test performs one completed trade carrying both:
  - one safe singleton/non-stackable helper item
  - one helper stackable partial quantity
- then DISCO opens a second trade and returns both items in one trade
- final validation now asserts that both players return to their starting model totals

What the false failure actually was:
- the earlier failing run was not a proven trade-logic regression
- `_query_fresh_tier3_snapshot(...)` in `gwa3/bridge/tests/test_f_player_trade.py` could consume an already-buffered tier-3 snapshot after `query_state`
- that meant the final assertion could read a stale post-forward inventory image even though the return trade had already closed successfully
- the fix was to require a strictly newer tier-3 `tick` than the last known snapshot before accepting the `query_state` result
- the round-trip test was also tightened to wait for actual inventory restoration via `_wait_for_fresh_tier3_state_change(...)` instead of asserting on one post-close query

Focused live validation:
- `python -m bridge.tests --filter 'test_player_trade_zz_roundtrip_singleton_and_stackable_complete_helper'`
- result: PASS on fresh clients

Primary evidence from the passing run:
- helper: `gwa3/build_trade/bin/Release/gwa3_log_47004.txt`
  - `Helper observed player trade open (flags=1 offerModel=2992 offerQuantity=1 offerModel2=935 offerQuantity2=2)`
  - `Helper auto-offering slot=1 item=1603 model=2992 qty=1 requested=1 available=1 mode=packet_offer (flags=1)`
  - `Helper auto-offering slot=2 item=1244 model=935 qty=2 requested=2 available=83 mode=packet_offer (flags=1)`
  - `CtoS: TradeOfferItemBotshub item=1603 qty=1 queued=1`
  - `CtoS: TradeOfferItemBotshub item=1244 qty=2 queued=1`
- main: `gwa3/build_trade/bin/Release/gwa3_log_115540.txt`
  - first trade reached `flags=0x3` with `partner_items=2`, then `flags=0x7`, then closed
  - second trade showed two outbound `offer_trade_item` calls, then `flags=0x3` with `player_items=2`, then `flags=0x7`, then closed
- helper status: `gwa3/build_trade/bin/Release/trade_helper_status.json`
  - `trade_open_count = 2`
  - `submit_attempt_count = 2`
  - `accept_attempt_count = 2`
  - `ctos_add_item = 2`
  - `ctos_submit = 2`
  - `ctos_accept = 2`
  - final exposed totals returned to baseline:
    - `helper_stackable_offer_total_quantity = 83`
    - `helper_safe_singleton_offer_total_quantity = 11`

Current boundary after this run:
- the final two-item round-trip live scenario now passes
- current validation is helper-originated first leg, then main-originated return leg:
  - BLUMPKINS -> DISCO: singleton + stackable in one trade
  - DISCO -> BLUMPKINS: same model/quantity pair returned in one trade
- the previous failure signature on this scenario should be treated as a stale-snapshot harness bug unless reproduced after the tick-gated refresh change

## 2026-04-17 Update: Trade Chat And Two-Way Whispers Now Validate On The Live Two-Client Lane

What changed:
- the bridge already exposed `send_chat`, but `send_whisper` was schema-only and had no handler
- added a real `send_whisper` action path through `ChatMgr::SendWhisper(...)`
- extended helper config/status plumbing so BLUMPKINS can:
  - send trade chat on request
  - send whispers on request
  - publish `recent_chat`
  - publish helper chat/whisper send attempt counters and last processed sequence ids
- `ChatLogMgr` previously only captured StoC chat packets `0x5D` through `0x61`
  - that was enough for trade chat
  - it was not enough for whispers on this Reforged lane
- added a GWCA-aligned `WriteWhisper` hook into `ChatLogMgr`
  - scan pattern used from GWCA:
    - `\x83\xC4\x04\x8D\x58\x2E`
    - offset `-0x18`
  - captured whispers are now pushed into the same chat ring buffer used by snapshots/helper status

Focused live validation:
- `python -m bridge.tests --filter 'test_player_trade_zzz_chat_trade_and_whisper_helper'`
- result: PASS on fresh clients

What the focused test now covers:
- DISCO sends a trade chat message and both sides can read it
- BLUMPKINS sends a trade chat message and DISCO can read it
- DISCO whispers BLUMPKINS and BLUMPKINS can read it
- BLUMPKINS whispers DISCO and DISCO can read it

Primary evidence:
- main: `gwa3/build_trade/bin/Release/gwa3_log_110196.txt`
  - `[ChatLogMgr] Whisper hook enabled at 0x00F6BC80`
  - `[ChatLogMgr] Initialized — capturing chat on 5 packet types plus whisper hook`
  - `[LLM-Action] Executing: send_chat`
  - `[LLM-Action] Executing: send_whisper`
- helper: `gwa3/build_trade/bin/Release/gwa3_log_95428.txt`
  - `[ChatLogMgr] Whisper hook enabled at 0x0024BC80`
- `Helper sending chat seq=1 channel=trade message=one sec please ...`
- `Helper sending whisper seq=1 recipient=D I S C O P A N I C message=yes, ready now ...`
- helper status: `gwa3/build_trade/bin/Release/trade_helper_status.json`
  - `chat_send_attempt_count = 1`
  - `whisper_send_attempt_count = 1`
  - `last_chat_send_seq = 1`
  - `last_whisper_send_seq = 1`
  - `recent_chat` includes:
    - helper-observed DISCO trade chat
    - helper-observed BLUMPKINS trade chat
    - helper-observed DISCO whisper with `channel = "whisper"` and sender `D I S C O P A N I C`

Current boundary after this run:
- player-trade lane validation now includes two-client chat coverage in the same district/lane
- direct-message observability depended on the added `WriteWhisper` hook, not just the old StoC chat packet taps
- the lane now validates:
  - trade open/cancel
  - stackable quantity prompt flows
  - trade completion
  - two-item round-trip return
  - trade chat send/read
  - two-way whisper send/read
