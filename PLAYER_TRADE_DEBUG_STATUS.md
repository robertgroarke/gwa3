# Player Trade Debug Status

Last updated: 2026-04-11

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
