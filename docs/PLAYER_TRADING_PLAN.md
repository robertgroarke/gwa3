# Player Trading System Plan

## Goal
Enable Gemma to trade with real players in Guild Wars with enough visibility and safety to:
- read and send trade chat and whispers
- initiate player trades
- observe the live trade window state
- offer items and gold
- submit, change, accept, and cancel offers
- inspect what the partner is offering
- use Kamadan trade history to estimate fair prices

## Current Leveraged Code

### Already working in `gwa3`
- Chat send via [ChatMgr.cpp](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\gwa3\src\managers\ChatMgr.cpp)
- Chat log capture via [ChatLogMgr.cpp](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\gwa3\src\managers\ChatLogMgr.cpp)
- Merchant / trader / crafter workflows via [TradeMgr.cpp](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\gwa3\src\managers\TradeMgr.cpp)
- Bridge action execution via [ActionExecutor.cpp](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\gwa3\src\llm\ActionExecutor.cpp)
- Bridge snapshots via [GameSnapshot.cpp](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\gwa3\src\llm\GameSnapshot.cpp)

### Existing player-trade hooks we can leverage
- Player trade packet headers in [Headers.h](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\gwa3\include\gwa3\packets\Headers.h)
- Player trade wrappers in [TradeMgr.h](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\gwa3\include\gwa3\managers\TradeMgr.h)
- Trade function offsets in [Offsets.cpp](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\gwa3\src\core\Offsets.cpp)

### External reference points
- AutoIt player trade flow in [GWA2.au3](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWA%20Censured\lib\botshub\GWA2.au3)
- GWCA trade context in [TradeContext.h](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWA%20Censured\GWCA-master\Include\GWCA\Context\TradeContext.h)
- GWCA trade manager in [TradeMgr.cpp](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWA%20Censured\GWCA-master\Source\TradeMgr.cpp)
- GWToolbox Kamadan trade window in [TradeWindow.cpp](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWA%20Censured\GWToolboxpp\GWToolboxdll\Windows\TradeWindow.cpp)

## What is implemented now

### Done
- `trade` snapshot object added to Tier 2 and Tier 3 snapshots
- whisper action added to bridge
- initial player-trade bridge actions added:
  - `initiate_trade`
  - `offer_trade_item`
  - `submit_trade_offer`
  - `accept_trade`
  - `cancel_trade`
  - `change_trade_offer`
  - `remove_trade_item`
- AutoIt parity fix:
  - `SubmitOffer(gold)` now carries gold
  - `OfferItem(item_id, quantity)` now carries quantity
- bridge observation tests updated to validate `trade{}`
- bridge validation tests added for new action parameter handling

### Still missing
- live partner identity in `trade{}`
- gold offer confirmation semantics
- guaranteed correct remove-item semantics if slot vs item-id differs
- trade acceptance / submit status separation for both sides
- dedicated end-to-end player trade tests
- Kamadan history ingestion and price lookup tool
- trade-specific safety policy and anti-scam checks

## Implementation Plan

### Phase 1: Trade State Observation
Expose a reliable `trade{}` snapshot for the bridge and Gemma.

Fields to target:
- `is_open`
- `flags`
- `is_initiated`
- `offer_sent`
- `is_accepted`
- `player.gold`
- `player.items[]`
- `partner.gold`
- `partner.items[]`
- later:
  - partner identity if discoverable
  - distinct submitted/accepted bits for each side if discoverable

Acceptance:
- Tier 2 and Tier 3 always include `trade`
- closed trade reports `is_open = false`
- open trade reports both sides with sane gold and item arrays

### Phase 2: Player Trade Actions
Expose all safe player-trade actions to the bridge.

Actions:
- `initiate_trade`
- `offer_trade_item`
- `submit_trade_offer`
- `accept_trade`
- `cancel_trade`
- `change_trade_offer`
- `remove_trade_item`
- later:
  - `offer_trade_gold` if a separate action becomes necessary

Acceptance:
- parameter validation is strict
- actions return deterministic `action_result`
- no client crash from malformed or missing params

### Phase 3: Chat and Whisper Support
Support player-to-player negotiation.

Actions:
- `send_chat` for trade channel
- `send_whisper`

Observation:
- `chat[]` must include trade and whisper messages with sender/channel/message

Acceptance:
- bridge can send trade chat
- bridge can send whispers in GWCA-compatible format
- chat log captures incoming trade and whisper lines

### Phase 4: Real Trade Flow
Make Gemma able to complete a real player trade.

Flow:
1. find target player
2. initiate trade
3. observe trade window opened
4. offer item and/or gold
5. observe own offer reflected in `trade.player`
6. observe partner offer reflected in `trade.partner`
7. submit offer
8. observe submit state
9. accept trade
10. verify inventory and gold deltas

Acceptance:
- every pass is backed by before/after observation
- no phase passes just because an action returned success

### Phase 5: Kamadan History and Price Tooling
Add a Python-side trade-history source for Gemma.

Planned pieces:
- Kamadan scrape / websocket client
- cached trade messages with timestamps
- normalization and query helpers
- bridge tool:
  - `lookup_trade_prices`

Acceptance:
- can query historical listings by item keyword
- returns sample prices, counts, recency
- does not require DLL changes

### Phase 6: Safety and Logging
Prevent obvious scammy or dangerous trades.

Guardrails:
- max gold/value threshold
- allowlist / denylist item models
- confirmation-required trades above threshold
- full structured trade log

Acceptance:
- suspicious high-value trades are blocked or require confirmation mode
- trade transcript and offered assets are logged

## Test Plan

### Observation tests
- `trade` exists in Tier 2 and Tier 3
- closed state is sane
- open state fields are typed and plausible
- item counts match item arrays

### Action validation tests
- missing params fail correctly
- bad agent IDs fail correctly
- bad item IDs fail correctly
- bad quantities fail correctly

### Controlled live trade tests
Use two launcher-based clients with exact PID targeting.

Scenarios:
1. open and cancel trade
2. offer one item
3. offer gold
4. partner offers item
5. submit both sides
6. accept both sides
7. verify inventory and gold changed correctly

### Chat tests
- send trade chat
- send whisper
- verify chat log capture of sent and received lines

### Kamadan tests
- fetch history for a known item
- parse price points
- handle network failures cleanly

## Kanban

### Todo
- `GWA3-160` Add partner identity to `trade{}` if discoverable
- `GWA3-161` Confirm remove-item semantics against live player trade
- `GWA3-162` Add dedicated bridge tests for `trade{}` open-state observation
- `GWA3-163` Add live two-client trade harness for Disco Panic + helper account
- `GWA3-164` Validate gold offering semantics end to end
- `GWA3-165` Add whisper/send-trade-chat observation assertions
- `GWA3-166` Implement `lookup_trade_prices` Python tool
- `GWA3-167` Add Kamadan cache and search layer
- `GWA3-168` Add trade safety thresholds and logging

### In Progress
- `GWA3-159` Player-trade bridge foundation
  - `trade{}` snapshot
  - initial trade actions
  - whisper action

### Done
- Merchant bridge workflow end to end on Disco Panic
- Launcher-based multi-client discipline and watchdog coverage
- Merchant buy/sell round-trip bridge proof
- Full Disco Panic orchestrated bridge path

## Recommended Next Steps
1. Run the new `trade{}` observation tests in a real bridge session.
2. Add a two-client live trade harness so we can prove item/gold deltas.
3. Confirm whether `remove_trade_item` expects a slot or an item identifier.
4. Add partner identity to the snapshot if the trade partner pointer/path is available.
5. Start the Python Kamadan price history module once the live trade loop is trustworthy.
