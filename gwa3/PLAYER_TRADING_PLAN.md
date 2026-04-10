# Player Trading System — Implementation Plan

## Overview

Enable Gemma (LLM agent) to trade with real players in Guild Wars: read/send chat messages, initiate trades, negotiate offers, and complete transactions. Also integrate Kamadan trade chat history for price discovery.

---

## Existing Infrastructure Audit

### Already Working (gwa3)
| Component | Status | Location |
|-----------|--------|----------|
| `ChatMgr::SendChat()` | C++ done, tool exposed | `send_chat` tool, channels: all/team/guild/trade |
| `ChatLogMgr` | C++ done, snapshot exposed | `chat[]` in Tier2 snapshot with channel/sender/message |
| `TradeMgr` player trade C++ | **Headers declared, needs verification** | `InitiateTrade`, `OfferItem`, `SubmitOffer`, `AcceptTrade`, `CancelTrade`, `ChangeOffer`, `RemoveItem` |
| `TradeMgr` NPC merchant | C++ done, tools exposed | `open_merchant`, `buy_materials`, `request_quote`, `transact_items`, `craft_item` |
| `TradeContext` (GWCA) | Reference available | Flags: CLOSED/INITIATED/OFFER_SEND/ACCEPTED, player/partner items+gold |
| Chat channel constants | Done | Trade = channel 12, prefix `$` |
| Whisper send | **Not exposed as tool** | `SendChat` with `/whisper receiver,msg` format works in AutoIt |

### Needs Implementation
| Component | Gap | Effort |
|-----------|-----|--------|
| Player trade tools in `tool_schema.py` | Not exposed to LLM | Small |
| Player trade action handlers in `ActionExecutor.cpp` | Not wired up | Medium |
| Trade window state in `GameSnapshot.cpp` | No `BuildTradeJson()` | Medium |
| `SendWhisper` dedicated tool | Only generic send_chat exists | Small |
| Trade context memory reading in C++ | Need to find/hook TradeContext pointer | Medium |
| Kamadan trade history integration | No WebSocket or HTTP client in gwa3 | Large — do in Python bridge |
| Price lookup tool for LLM | Not started | Medium |
| Trade negotiation prompting | LLM needs trade-aware system prompt | Medium |

---

## Architecture

```
                    Kamadan WS/HTTP
                         |
                   Python Bridge
                   /            \
          tool_schema.py    kamadan_client.py (NEW)
                |                    |
          ActionExecutor.cpp    price history cache
                |
           TradeMgr / ChatMgr / TradeContext
                |
           Guild Wars Client
```

**Key design decisions:**
1. **Kamadan scraping stays in Python** — The bridge already has HTTP/async capabilities; no need to add WebSocket to the C++ DLL.
2. **Trade state observation goes in GameSnapshot** — Same pattern as merchant state; LLM sees trade window contents each tick.
3. **Player trade actions go through ActionExecutor** — Same IPC pattern as all other tools.
4. **Whisper is a separate tool** — Cleaner than overloading send_chat with slash-command syntax.

---

## Phase 1: Trade State Observation (C++ / Snapshot)

### 1a. Hook TradeContext pointer
- Find TradeContext in game memory (GWCA pattern: `TradeContext` via `GameContext`)
- Expose in gwa3: `GetTradeFlags()`, `GetPlayerOffer()`, `GetPartnerOffer()`
- Read: flags (closed/initiated/offer_sent/accepted), player items+gold, partner items+gold

### 1b. BuildTradeJson() in GameSnapshot
Add to Tier2 snapshot:
```json
{
  "trade": {
    "state": "closed|initiated|offer_sent|accepted",
    "player_offer": {
      "gold": 0,
      "items": [{"item_id": 123, "quantity": 1, "name": "..."}]
    },
    "partner_offer": {
      "gold": 5000,
      "items": [{"item_id": 456, "quantity": 1, "name": "..."}]
    }
  }
}
```

### 1c. Incoming trade request detection
- StoC packet `GAME_SMSG_TRADE_REQUEST` (0x0000) signals someone wants to trade
- Log to chat or add to snapshot as `trade.pending_request: {agent_id, player_name}`

---

## Phase 2: Player Trade Actions (C++ ActionExecutor + Python tools)

### 2a. C++ Action Handlers
Add to `ActionExecutor.cpp`:
- `HandleInitiateTrade(params)` — calls `TradeMgr::InitiateTrade(agentId)`
- `HandleOfferItem(params)` — calls `TradeMgr::OfferItem(itemId)`
- `HandleOfferGold(params)` — calls `TradeMgr::SubmitOffer()` with gold amount
- `HandleAcceptTrade(params)` — calls `TradeMgr::AcceptTrade()`
- `HandleCancelTrade(params)` — calls `TradeMgr::CancelTrade()`
- `HandleChangeOffer(params)` — calls `TradeMgr::ChangeOffer()` (retract offer to modify)
- `HandleRemoveTradeItem(params)` — calls `TradeMgr::RemoveItem(itemId)`

### 2b. Python Tool Schema
Add to `tool_schema.py`:
```python
INITIATE_TRADE = {
    "name": "initiate_trade",
    "description": "Open a trade window with a nearby player",
    "parameters": {"agent_id": {"type": "integer", "description": "Agent ID of player to trade with"}}
}

OFFER_TRADE_ITEM = {
    "name": "offer_trade_item",
    "description": "Add an item from inventory to the trade offer",
    "parameters": {
        "item_id": {"type": "integer"},
        "quantity": {"type": "integer", "default": 1}
    }
}

REMOVE_TRADE_ITEM = {
    "name": "remove_trade_item",
    "description": "Remove an item from the trade offer",
    "parameters": {"item_id": {"type": "integer"}}
}

SUBMIT_TRADE_OFFER = {
    "name": "submit_trade_offer",
    "description": "Submit current trade offer (items + gold). Both players must submit before accepting.",
    "parameters": {"gold": {"type": "integer", "default": 0}}
}

ACCEPT_TRADE = {
    "name": "accept_trade",
    "description": "Accept the current trade. Both players must accept to complete.",
    "parameters": {}
}

CANCEL_TRADE = {
    "name": "cancel_trade",
    "description": "Cancel the current trade and close the trade window",
    "parameters": {}
}

CHANGE_TRADE_OFFER = {
    "name": "change_trade_offer",
    "description": "Retract submitted offer to modify items/gold before resubmitting",
    "parameters": {}
}
```

### 2c. SendWhisper tool
```python
SEND_WHISPER = {
    "name": "send_whisper",
    "description": "Send a private whisper message to a player",
    "parameters": {
        "recipient": {"type": "string"},
        "message": {"type": "string"}
    }
}
```
Implementation: `ChatMgr::SendChat(L"/whisper recipient,message", L'/')` or dedicated whisper function.

---

## Phase 3: Kamadan Trade Chat History (Python Bridge)

### 3a. KamadanClient class
New file: `gwa3/bridge/kamadan_client.py`
- WebSocket connection to `wss://kamadan.gwtoolbox.com` for live trade chat
- HTTP GET to `https://kamadan.gwtoolbox.com` for historical search
- Message format: `{"s": "player_name", "m": "message", "t": timestamp_ms}`
- Search query format: `{"query": "search_string"}` → `{"results": [...], "num_results": N}`

### 3b. Price lookup tool
```python
LOOKUP_TRADE_PRICES = {
    "name": "lookup_trade_prices",
    "description": "Search Kamadan trade chat history for recent listings of an item. Returns seller names, messages, and timestamps.",
    "parameters": {
        "item_name": {"type": "string", "description": "Item name to search for (e.g. 'Ecto', 'Glob of Ectoplasm')"},
        "max_results": {"type": "integer", "default": 10}
    }
}
```
- Handled entirely in Python bridge (no C++ needed)
- Returns recent trade messages mentioning the item
- LLM interprets prices from message text (WTS/WTB patterns)

### 3c. Live trade chat feed
- Option A: Merge Kamadan WS messages into the chat snapshot (add `"source": "kamadan"` field)
- Option B: Separate `kamadan_trade_chat` field in snapshot
- Prefer Option B to avoid confusion with in-game trade chat

---

## Phase 4: Trade-Aware LLM Prompting

### 4a. System prompt additions
- Trade protocol: initiate → offer items/gold → submit → accept (both sides)
- Safety rules: never trade away items worth more than X without user confirmation
- Price awareness: use `lookup_trade_prices` before agreeing to prices
- Scam detection: watch for empty offers, last-second item swaps, gold amount changes

### 4b. Trade flow state machine guidance
```
CLOSED → (initiate_trade or incoming request) → INITIATED
INITIATED → (offer items/gold) → OFFERING
OFFERING → (submit_trade_offer) → OFFER_SENT
OFFER_SENT → (partner also submits) → BOTH_SUBMITTED
BOTH_SUBMITTED → (accept_trade) → ACCEPTED → CLOSED (trade complete)
Any state → (cancel_trade) → CLOSED
OFFER_SENT → (change_trade_offer) → OFFERING (retract to modify)
```

---

## Phase 5: Safety & Polish

### 5a. Trade value guards
- Inventory value tracking: LLM must know item values before trading
- Configurable max trade value without user confirmation
- Log all completed trades to file

### 5b. Anti-scam measures
- Snapshot the partner's offer at submit time, re-verify at accept time
- Alert if partner changes offer after initial submit
- Never accept empty trades (giving away items for nothing)

### 5c. Trade logging
- Log every trade action + timestamp + partner name to `gwa3/logs/trades.jsonl`
- Include items exchanged, gold amounts, Kamadan price at time of trade

---

## Test Plan

### Unit Tests (Python bridge)

| Test | Description |
|------|-------------|
| `test_kamadan_search` | Mock HTTP response, verify parsing of search results |
| `test_kamadan_ws_message` | Mock WS message, verify JSON parsing |
| `test_trade_tool_schema` | Validate all new tool definitions have required fields |
| `test_whisper_format` | Verify whisper message formatting |
| `test_price_extraction` | Parse WTS/WTB prices from sample trade messages |

### Integration Tests (C++ DLL + Python)

| Test | Description |
|------|-------------|
| `test_trade_state_snapshot` | Verify trade state appears in GameSnapshot when trade window is open |
| `test_send_trade_chat` | Send message on trade channel via `send_chat`, verify in chat log |
| `test_initiate_trade_roundtrip` | IPC call → ActionExecutor → TradeMgr → verify trade state changes |
| `test_offer_item_roundtrip` | Offer item via IPC, verify it appears in trade snapshot player_offer |
| `test_submit_accept_flow` | Full trade lifecycle: initiate → offer → submit → accept |
| `test_cancel_trade` | Initiate trade then cancel, verify state returns to CLOSED |
| `test_whisper_roundtrip` | Send whisper via IPC, verify it appears in outgoing chat |

### Manual Tests (In-Game)

| Test | Description |
|------|-------------|
| `manual_trade_with_alt` | Trade between BEASTRIT (bot) and DISCOPANIC (manual). Verify items transfer. |
| `manual_kamadan_price_check` | Go to Kamadan AE1, run price lookup for Ectos, verify results match trade chat |
| `manual_incoming_trade` | Have alt initiate trade with bot, verify bot detects incoming trade request |
| `manual_scam_detection` | Have alt modify offer after submit, verify bot detects the change |
| `manual_chat_read_write` | Send trade chat messages, verify bot reads them; bot sends message, verify visible |

---

## Kanban Board

### Backlog
| ID | Task | Phase | Est |
|----|------|-------|-----|
| T-01 | Find TradeContext pointer in gwa3 memory (GWCA GameContext pattern) | P1 | M |
| T-02 | Implement `GetTradeFlags()`, `GetPlayerOffer()`, `GetPartnerOffer()` in TradeMgr | P1 | M |
| T-03 | Add `BuildTradeJson()` to GameSnapshot.cpp | P1 | M |
| T-04 | Hook StoC `TRADE_REQUEST` packet for incoming trade detection | P1 | M |
| T-05 | Wire `HandleInitiateTrade` in ActionExecutor.cpp | P2 | S |
| T-06 | Wire `HandleOfferItem` in ActionExecutor.cpp | P2 | S |
| T-07 | Wire `HandleSubmitOffer` (with gold param) in ActionExecutor.cpp | P2 | S |
| T-08 | Wire `HandleAcceptTrade` in ActionExecutor.cpp | P2 | S |
| T-09 | Wire `HandleCancelTrade` in ActionExecutor.cpp | P2 | S |
| T-10 | Wire `HandleChangeOffer` in ActionExecutor.cpp | P2 | S |
| T-11 | Wire `HandleRemoveTradeItem` in ActionExecutor.cpp | P2 | S |
| T-12 | Add all player trade tools to tool_schema.py | P2 | S |
| T-13 | Add `send_whisper` tool to tool_schema.py + ActionExecutor | P2 | S |
| T-14 | Implement `kamadan_client.py` — WebSocket live feed | P3 | L |
| T-15 | Implement `kamadan_client.py` — HTTP search/history | P3 | M |
| T-16 | Add `lookup_trade_prices` tool to tool_schema.py | P3 | S |
| T-17 | Wire price lookup handler in Python bridge (no C++ needed) | P3 | M |
| T-18 | Add Kamadan trade feed to snapshot or dedicated field | P3 | M |
| T-19 | Update LLM system prompt with trade protocol + safety rules | P4 | M |
| T-20 | Implement trade value guard (max value without confirmation) | P5 | M |
| T-21 | Implement anti-scam offer-change detection | P5 | M |
| T-22 | Add trade logging to `trades.jsonl` | P5 | S |

### Test Tasks
| ID | Task | Phase | Type |
|----|------|-------|------|
| TT-01 | Unit test: Kamadan search response parsing | P3 | Unit |
| TT-02 | Unit test: Kamadan WS message parsing | P3 | Unit |
| TT-03 | Unit test: trade tool schema validation | P2 | Unit |
| TT-04 | Unit test: whisper message formatting | P2 | Unit |
| TT-05 | Unit test: WTS/WTB price extraction from trade messages | P3 | Unit |
| TT-06 | Integration test: trade state in GameSnapshot | P1 | Integration |
| TT-07 | Integration test: send trade chat message | P2 | Integration |
| TT-08 | Integration test: initiate trade IPC roundtrip | P2 | Integration |
| TT-09 | Integration test: offer item IPC roundtrip | P2 | Integration |
| TT-10 | Integration test: full trade lifecycle | P2 | Integration |
| TT-11 | Integration test: cancel trade | P2 | Integration |
| TT-12 | Integration test: whisper IPC roundtrip | P2 | Integration |
| TT-13 | Manual test: trade between two characters | P2 | Manual |
| TT-14 | Manual test: Kamadan price check in-game | P3 | Manual |
| TT-15 | Manual test: incoming trade request detection | P1 | Manual |
| TT-16 | Manual test: scam detection (offer swap) | P5 | Manual |
| TT-17 | Manual test: chat read/write on trade channel | P2 | Manual |

### Priority Order
1. **P1** (T-01 → T-04, TT-06, TT-15) — Trade state observation. Foundation for everything.
2. **P2** (T-05 → T-13, TT-03/04/07-12/13/17) — Trade action tools. Gemma can trade.
3. **P3** (T-14 → T-18, TT-01/02/05/14) — Kamadan price history. Gemma knows prices.
4. **P4** (T-19) — LLM prompting. Gemma understands trade protocol.
5. **P5** (T-20 → T-22, TT-16) — Safety rails. Production-ready.

### Size Legend
- **S** = Small (< 1 hour, single file change)
- **M** = Medium (1-3 hours, 2-3 files)
- **L** = Large (3+ hours, new module)
