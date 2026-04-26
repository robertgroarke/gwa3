# Player Trading Remaining Work

## Purpose

This file captures the remaining follow-up work for Gemma-driven player-to-player trading after the current live lane validation.

It is intentionally narrower than [PLAYER_TRADING_PLAN.md](C:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\PLAYER_TRADING_PLAN.md): it focuses on what is still worth doing after the core trade and chat mechanics were already implemented and tested.

## What Is Already Working

These mechanics are implemented through the LLM bridge and have been validated on the live DISCO/BLUMPKINS trade lane:

- trade open and cancel
- stackable quantity prompt flows
- trade completion
- reverse-direction completion when the receiver is full
- helper-originated reverse stackable packet offers
- two-item round-trip:
  - one non-stackable plus one stackable item traded over
  - both items traded back
  - both sides restored to starting totals
- trade chat send/read both ways
- whisper send/read both ways

Primary bridge/tool surface:

- [gwa3/bridge/tool_schema.py](C:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\gwa3\bridge\tool_schema.py)
- [gwa3/src/llm/ActionExecutor.cpp](C:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\gwa3\src\llm\ActionExecutor.cpp)
- [gwa3/src/managers/TradeMgr.cpp](C:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\gwa3\src\managers\TradeMgr.cpp)
- [gwa3/src/managers/ChatMgr.cpp](C:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\gwa3\src\managers\ChatMgr.cpp)
- [gwa3/src/managers/ChatLogMgr.cpp](C:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\gwa3\src\managers\ChatLogMgr.cpp)

Primary live tests:

- [test_player_trade_zz_roundtrip_singleton_and_stackable_complete_helper](C:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\gwa3\bridge\tests\test_f_player_trade.py:2539)
- [test_player_trade_zzz_chat_trade_and_whisper_helper](C:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\gwa3\bridge\tests\test_f_player_trade.py:2799)

Supporting notes and evidence:

- [PLAYER_TRADE_DEBUG_STATUS.md](C:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\PLAYER_TRADE_DEBUG_STATUS.md)
- [PLAYER_TRADE_SESSION_SUMMARY.md](C:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\PLAYER_TRADE_SESSION_SUMMARY.md)

## Remaining Work

### 1. Add One Fully Consolidated End-To-End Scenario

Status:
- useful follow-up
- not required for the currently proven mechanics

Why it still matters:
- the trade round-trip and the chat/whisper exchange are both passing today, but they are covered by two separate focused tests
- one final combined scenario would give a stronger regression check for the exact user-facing workflow Gemma will run

Suggested scope:
- DISCO and BLUMPKINS exchange trade chat
- DISCO and BLUMPKINS exchange whispers
- one side trades a non-stackable plus a stackable item
- both sides submit and accept
- the receiving side returns both items
- final inventory totals match the starting state on both clients

Suggested file:
- [gwa3/bridge/tests/test_f_player_trade.py](C:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\gwa3\bridge\tests\test_f_player_trade.py)

### 2. Expose Better Partner Identity In `trade{}`

Status:
- still desirable for real-player autonomy

Why it still matters:
- the current helper lane is enough for controlled validation, but Gemma will be safer if the bridge can tell her exactly which player is in the trade window without inference

Desired outcome:
- stable partner character identity in `trade{}`
- enough information to cross-check that the trade partner matches the person who was messaged or targeted

Likely files:
- [gwa3/src/llm/GameSnapshot.cpp](C:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\gwa3\src\llm\GameSnapshot.cpp)
- [gwa3/src/managers/TradeMgr.cpp](C:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\gwa3\src\managers\TradeMgr.cpp)

### 3. Separate Per-Side Submit And Accept State More Cleanly

Status:
- still useful for safer autonomous acceptance

Why it still matters:
- Gemma should not accept based on ambiguous aggregate trade flags if we can expose cleaner state for:
  - my side submitted
  - partner side submitted
  - my side accepted
  - partner side accepted

Desired outcome:
- explicit per-side fields in `trade{}`
- fewer assumptions in the Gemma decision loop about when it is safe to accept

Likely files:
- [gwa3/src/llm/GameSnapshot.cpp](C:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\gwa3\src\llm\GameSnapshot.cpp)
- [gwa3/src/managers/TradeMgr.cpp](C:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\gwa3\src\managers\TradeMgr.cpp)

### 4. Validate Gold Offers End-To-End

Status:
- not proven by the current focused live suite

Why it still matters:
- item trading is now proven
- trading with gold is a separate risk surface and should not be assumed correct without live evidence

Suggested scope:
- offer gold only
- offer gold plus item
- confirm reflected values in `trade.player` and `trade.partner`
- complete the trade and verify post-trade gold totals

Likely files:
- [gwa3/bridge/tests/test_f_player_trade.py](C:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\gwa3\bridge\tests\test_f_player_trade.py)
- [gwa3/src/managers/TradeMgr.cpp](C:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\gwa3\src\managers\TradeMgr.cpp)

### 5. Confirm `remove_trade_item` Semantics Live

Status:
- still worth validating

Why it still matters:
- the bridge exposes `remove_trade_item`
- if the underlying operation expects a slot index in some cases and an item identifier in others, Gemma could remove the wrong offer entry

Desired outcome:
- one proven semantic contract
- one dedicated live test for remove-and-reoffer behavior

Likely files:
- [gwa3/bridge/tool_schema.py](C:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\gwa3\bridge\tool_schema.py)
- [gwa3/bridge/tests/test_f_player_trade.py](C:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\gwa3\bridge\tests\test_f_player_trade.py)
- [gwa3/src/managers/TradeMgr.cpp](C:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\gwa3\src\managers\TradeMgr.cpp)

### 6. Add Kamadan Price Lookup To The Gemma Trade Flow

Status:
- planned
- not part of the current live trade-mechanics proof

Why it still matters:
- Gemma can mechanically trade now
- she still needs a better pricing input if the goal is autonomous negotiation in real trading districts

Desired outcome:
- one bridge-accessible price lookup tool
- recent buy/sell context available before Gemma commits to a price

Likely files:
- [gwa3/bridge/kamadan_client.py](C:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\gwa3\bridge\kamadan_client.py)
- [gwa3/bridge/agent_loop.py](C:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\gwa3\bridge\agent_loop.py)
- [PLAYER_TRADING_PLAN.md](C:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\PLAYER_TRADING_PLAN.md)

### 7. Add Trade Safety And Anti-Scam Guardrails

Status:
- still open

Why it still matters:
- the bridge can now trade
- that is not the same thing as having a production-safe autonomous trader

Suggested safeguards:
- high-value trade thresholds that require explicit confirmation mode
- cross-checks between negotiated price and offered assets
- rejection of sudden last-second offer changes
- structured trade transcript logging
- optional “do not trade” lists or trust lists

Likely files:
- [gwa3/bridge/agent_loop.py](C:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\gwa3\bridge\agent_loop.py)
- [PLAYER_TRADING_PLAN.md](C:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\PLAYER_TRADING_PLAN.md)

### 8. Write A Short Gemma-Facing Player Trade Runbook

Status:
- recommended documentation follow-up

Why it still matters:
- the current evidence is spread across debug notes, the session summary, and the broader trading plan
- that is enough for engineering handoff, but not yet a clean “what Gemma can do, what Gemma should not do, and how Gemma should decide” document

Suggested contents:
- bridge tools Gemma may use for player trades
- expected observation loop using `query_state`
- safe ordering:
  - chat
  - verify partner
  - open trade
  - inspect offers
  - submit
  - accept
- conditions that should make Gemma cancel instead of accepting

Suggested file:
- `gwa3/bridge/README.md` or a dedicated `GEMMA_PLAYER_TRADING_GUIDE.md`

### 9. Low-Priority Cleanup: Rename Internal `Botshub` Labels

Status:
- low priority
- not player-visible in current verified behavior

Why it still matters:
- the live player-facing chat payloads were already cleaned up
- however, some internal debug strings still contain `Botshub`, for example `TradeOfferItemBotshub`

Suggested scope:
- rename internal packet/debug labels only
- do not change packet behavior

Likely files:
- [gwa3/src/packets/CtoS.cpp](C:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\gwa3\src\packets\CtoS.cpp)
- any docs that quote those internal log lines

## Recommended Resume Order

If this work is resumed later, the recommended order is:

1. add the single consolidated end-to-end scenario
2. validate gold offers
3. confirm `remove_trade_item` semantics
4. improve partner identity and per-side trade state in `trade{}`
5. add Kamadan pricing and anti-scam policy
6. write the short Gemma-facing runbook
7. clean up internal `Botshub` labels

## Current Bottom Line

Gemma already has the bridge-level mechanics needed to:

- read trade and whisper chat from the bridge-visible state
- send trade chat and whispers
- complete item-based player trades
- complete a tested two-item round-trip trade

The remaining work is mostly about:

- broader regression coverage
- safer autonomous decision-making
- pricing and anti-scam policy
- cleaner operator-facing documentation
