# GWCA & UIMessage Research — Character Select Programmatic Control

## Goal
Press the Play button and dismiss the reconnect dialog at the character select screen **programmatically** — no keyboard, no mouse, no window focus dependency.

---

## Architecture: GW's UI Systems

### Global UIMessage (SendUIMessage)
- **Function**: `SendUIMessage(UIMessage msgid, void* wParam, void* lParam)`
- **Scanned via**: Pattern `B900000000E8000000005DC3894508` offset -0x14
- **Label**: `UIMessage` in our framework
- **Used for**: In-game commands (map travel, item use, dialog, skill load, etc.)
- **Dispatches to**: All registered handlers
- **Does NOT route**: Frame-level messages (0x22 kMouseClick, 0x31 kMouseClick2, 0x2F kMouseAction) to specific frames

### Frame UIMessage (SendFrameUIMessage)
- **Function**: `SendFrameUIMessage(Frame* frame_callbacks, UIMessage msgid, void* wParam, void* lParam)`
- **Calling convention**: `__thiscall` — ECX = `frame_ptr + 0xA8` (callbacks array)
- **Stack args**: msgid, wParam, lParam (callee cleans — stdcall-like)
- **Game address**: `0x007986D0` (confirmed by BOTH our scan AND gwca.dll)
- **Scan pattern**: `83 C1 DC E8` (4 bytes, unique — only 1 match in .text)
  - At offset +3, there's an E8 (CALL) instruction
  - Resolve via `FunctionFromNearCall(match_addr + 3)` to get target
- **Used for**: Clicking buttons, frame-specific UI events

### Frame System
- **Frame struct**: 0x1C8 bytes (456 bytes), defined in GWCA UIMgr.h
- **Frame array**: Scanned via assertion `P:\Code\Engine\Frame\FrMsg.cpp` + `frame`
- **Frame lookup**: By `frame_hash_id` at offset `0x134` (in FrameRelation at Frame+0x128)
- **Key offsets in Frame**:
  - `0xA8`: `frame_callbacks` (GW::Array, buffer/size/capacity)
  - `0xB8`: `child_offset_id`
  - `0xBC`: `frame_id` (index in global frame array)
  - `0x128`: `FrameRelation` (parent, siblings, hash)
  - `0x134`: `frame_hash_id` (used by GetFrameByHash)
  - `0x18C`: `frame_state` (bit 0x4=created, 0x200=hidden, 0x10=disabled)

---

## Known Frame Hashes (from py4gw frame_aliases.json)

### Character Select Screen
| Hash | Frame |
|---|---|
| **184818986** | `Character_Select_Frame.Play_Button` |
| 41327607 | `Character_Select_Frame.Play_Button_Greyed_Out` |
| **1398610279** | `did_not_cleaning_disconnect_popup_charselect_YES_BUTTON` |
| **3600335809** | `did_not_cleaning_disconnect_popup_charselect_NO_BUTTON` |
| 3372446797 | `Character_Select_Frame.Create_Button` |
| 3379687503 | `Character_Select_Frame.Delete_Button` |
| 1117342925 | `Character_Select_Frame.Log_Out_Button` |
| 1601494406 | `Character_Select_Frame.Edit_Account_Button` |
| 828467986 | `Character_Select_Frame.Character_Frame` |
| 941138463 | `Character_Select_Frame.Status_dropdown` |

### In-Game
| Hash | Frame |
|---|---|
| 3332025202 | Party Formation |
| 2874675009 | Inventory Window |
| 2315448754 | Xunlai Window |
| 1532320307 | Merchant Buy Button |
| 684387150 | Salvage Window |

Full list: `tests/frame_aliases.json` (1164 entries from py4gw)

---

## GWCA DLL Analysis

### Key Exports (from gwca.dll disassembly)
| RVA Offset | Export | Notes |
|---|---|---|
| +0x16660 | `ButtonFrame::Click()` | Calls MouseAction(0x6) then MouseAction(0x7) |
| +0x173D0 | `ButtonFrame::MouseAction(ActionState)` | Builds kMouseAction struct, calls SendFrameUIMessage |
| +0x255E0 | `ButtonClick(Frame*)` | Wrapper: GetFrameById + Click |
| +0x259A0 | `GetChildFrame(Frame*, uint32)` | Navigate frame hierarchy |
| +0x25CC0 | `GetFrameById(uint32)` | Lookup in global frame array |
| +0x25D30 | `GetFrameByLabel(wchar_t*)` | Hash label, search frame array |
| +0x25D90 | `GetFrameContext(Frame*)` | Returns frame context pointer |
| +0x25FE0 | `GetRootFrame()` | Returns root frame pointer |
| +0x274D0 | `SendFrameUIMessage(Frame*, msg, wp, lp)` | GWCA wrapper (hooks, then calls game func) |
| +0x27680 | `SendUIMessage(msg, wp, lp)` | GWCA wrapper for global dispatch |

### Data Section Addresses (relative to gwca.dll base)
| Offset | Content | Value (example) |
|---|---|---|
| +0x8A39C | Original SendFrameUIMsg game func ptr | 0x007986D0 |
| +0x8A3A0 | Hooked SendFrameUIMsg (GWCA's wrapper) | (injection region) |
| +0x8A37C | GetChildFrame game func ptr | 0x0077E2B0 |
| +0x8A410 | RootFrame game func ptr | 0x0079DC20 |
| +0x880F0 | Hash seed (ESI initial) | 0x325D1EAE |
| +0x880F4 | Hash seed (EAX initial) | 0xE2C15C9D |
| +0x880F8 | Hash seed 2 | 0x2170A28A |
| +0x8A3B0 | Frame hash table address | 0x00D25624 |

### GWCA DLL Injection
- gwca.dll loads successfully via `CreateRemoteThread(LoadLibraryW)`
- `Scanner::Initialize(gw_module_handle)` works on non-game thread
- `GW::Initialize()` works but sets up hooks (may conflict with BotsHub hooks)
- Calling `ButtonFrame::Click()` via `CreateRemoteThread` crashes (must run on game thread)

---

## Hash Function (GetFrameByLabel internals)

Located at gwca.dll +0x1A450. Algorithm:
```
hash0 = seed_eax  (from +0x880F4)
hash1 = seed_esi  (from +0x880F0)
result = 0

for each wchar ch in label:
    ch = toupper(ch)    // case-insensitive
    hash0 = (hash0 << 3) ^ ch
    hash1 += table[hash0 & 0xF]   // 4-byte entries at +0x52938
result ^= (hash0 + hash1)

---

## GWCA UIMessage Layer Deep Dive

This section focuses on GWCA itself rather than the earlier character-select/frame-click work.

The main takeaway is:

- `GW::UI::SendUIMessage` is GWCA's central interception bus for UI-level actions.
- Many `0x30000000` `kSend*` messages are not blindly forwarded to the game's native UI dispatcher.
- Instead, GWCA often uses them as internal relay messages:
  - hook a native game function,
  - translate the call into a `kSend*` UI message,
  - let callbacks inspect/block/modify it,
  - replay the original native function from a post-callback handler.

That distinction matters a lot when reasoning about botting and packet flow.

### 1. Core GWCA UI functions

Declared in `GWCA-master/Include/GWCA/Managers/UIMgr.h`:

- `GW::UI::SendUIMessage(UIMessage msgid, void* wParam = nullptr, void* lParam = nullptr)`
- `GW::UI::RegisterUIMessageCallback(HookEntry* entry, UIMessage message_id, const UIMessageCallback& callback, int altitude = -0x8000)`
- `GW::UI::RemoveUIMessageCallback(HookEntry* entry)`

Implemented in `GWCA-master/Source/UIMgr.cpp`.

Important internal globals:

- `SendUIMessage_Func`
  - scanned pointer to the game's native UI dispatcher
- `RetSendUIMessage`
  - original function pointer after GWCA hooks `SendUIMessage_Func`
- `UIMessage_callbacks`
  - `std::unordered_map<UIMessage, std::vector<CallbackEntry>>`
- `CallbackEntry`
  - stores `altitude`, `HookEntry*`, and callback functor

### 2. How GWCA finds and hooks the dispatcher

In `UIMgr.cpp`, GWCA scans the Guild Wars module for the native dispatcher:

- `SendUIMessage_Func = Scanner::Find(..., -0x1A);`

Then it hooks it:

- `HookBase::CreateHook(SendUIMessage_Func, OnSendUIMessage, (void **)&RetSendUIMessage);`

Wrapper behavior:

- the native game dispatcher calls the hooked GWCA shim `OnSendUIMessage`
- `OnSendUIMessage` re-enters GWCA's higher-level `UI::SendUIMessage`
- `UI::SendUIMessage` runs callbacks and decides whether the underlying game dispatcher should actually be called

So the public GWCA API and the hook interception path intentionally converge on the same logic.

### 3. `RawSendUIMessage` versus `SendUIMessage`

Source behavior in `UIMgr.cpp`:

```cpp
bool RawSendUIMessage(UIMessage msgid, void* wParam, void* lParam) {
    if (!RetSendUIMessage)
        return false;
    if (((uint32_t)msgid & 0x30000000) == 0x30000000)
        return true; // Internal GWCA UI Message, used for hooks
    HookBase::EnterHook();
    RetSendUIMessage(msgid, wParam, lParam);
    HookBase::LeaveHook();
    return true;
}
```

This is one of the most important design details in GWCA:

- if the message id is in the `0x30000000` family, `RawSendUIMessage` returns `true` without calling the game's native dispatcher
- GWCA explicitly labels these as internal GWCA UI messages used for hooks

That means messages like:

- `kSendDialog`
- `kSendEnterMission`
- `kSendLoadSkillbar`
- `kSendMoveItem`
- `kSendMerchantRequestQuote`
- `kSendMerchantTransactItem`
- `kSendUseItem`
- `kSendSetActiveQuest`
- `kSendAbandonQuest`

are often GWCA relay/control messages, not simple "send this directly to the game UI" wrappers.

By contrast, `0x10000000` family messages like:

- `kTravel`
- `kGuildHall`
- `kLeaveGuildHall`
- `kWriteToChatLog`
- `kInitiateTrade`

can go straight to the underlying game dispatcher through `RetSendUIMessage`.

### 4. Callback execution order and altitude

`SendUIMessage` in `UIMgr.cpp` does:

1. look up callbacks registered for that specific `UIMessage`
2. run "pre" callbacks while `altitude <= 0`
3. if not blocked, call `RawSendUIMessage`
4. run "post" callbacks for the remaining entries

The key loop split is:

- callbacks with `altitude <= 0` run before the actual send
- callbacks with `altitude > 0` run after the actual send decision

This gives GWCA a clean way to support:

- observers
- blockers
- translators
- replay handlers that should run only after the dispatch decision

The `status.blocked` flag is the control point:

- pre-callbacks can set `blocked = true`
- then the raw/native send is skipped
- post-callbacks still run

### 5. Binary confirmation from `gwca.dll`

Decompiling `GW::UI::SendUIMessage` from `gwca.dll` confirms the same architecture:

- lookup callbacks by message id
- run an initial callback pass
- if not blocked and message is not `0x30000000` family, call the native dispatcher
- then run a second callback pass

Most important binary-confirmed behavior:

- `0x30000000` messages are treated specially and are not forwarded into the game dispatcher in the raw send path

So the source and binary agree on the central relay design.

### 6. Exact message families in `UIMgr.h`

GWCA's `UIMessage` enum is a mix of:

- observed game UI events
- client UI commands
- GWCA-added relay messages

The important split is:

#### 6.1 `0x10000000` family

Examples:

- `kChangeTarget = 0x10000020`
- `kWriteToChatLog = 0x1000007E`
- `kLogout = 0x1000009B`
- `kGuildHall = 0x10000177`
- `kLeaveGuildHall = 0x10000179`
- `kTravel = 0x1000017A`
- `kMoveItem = 0x1000019E`
- `kInitiateTrade = 0x100001A0`
- `kOpenTemplate = 0x100001B9`

These are in the "game-facing UI dispatcher" family.

#### 6.2 `0x30000000` family

Examples:

- `kSendDialog = 0x30000001`
- `kSendEnterMission = 0x30000002`
- `kSendLoadSkillbar = 0x30000003`
- `kSendPingWeaponSet = 0x30000004`
- `kSendMoveItem = 0x30000005`
- `kSendMerchantRequestQuote = 0x30000006`
- `kSendMerchantTransactItem = 0x30000007`
- `kSendUseItem = 0x30000008`
- `kSendSetActiveQuest = 0x30000009`
- `kSendAbandonQuest = 0x30000010`

These are the GWCA relay/hook family.

### 7. GWCA relay pattern used by managers

A recurring pattern appears throughout GWCA manager code:

1. scan a native game function
2. hook that native function
3. hook callback translates native invocation into a `kSend*` UI message
4. register a post-callback on that `kSend*` message with altitude `0x1`
5. if the message was not blocked, the post-callback calls the original native function pointer

This gives GWCA a normalized interception surface around actions that are otherwise implemented by scattered native functions.

### 8. Concrete manager examples

#### 8.1 Agent manager: dialog

In `Source/AgentMgr.cpp`:

- `OnSendDialog(dialog_id)` calls:
  - `UI::SendUIMessage(UI::UIMessage::kSendDialog, (void*)dialog_id);`
- `OnSendDialog_UIMessage(...)` runs later and, if not blocked:
  - calls `RetSendDialog(last_dialog_id)` or `RetSendSignpostDialog(last_dialog_id)`

Meaning:

- GWCA turns the act of sending a dialog into an interceptable relay event
- the actual game-native dialog send is replayed only after callbacks have had a chance to block it

#### 8.2 Map manager: enter mission

In `Source/MapMgr.cpp`:

- `OnEnterChallengeMission_Hook(identifier)` calls:
  - `GW::UI::SendUIMessage(UI::UIMessage::kSendEnterMission, (void*)identifier);`
- `OnEnterChallengeMission_UIMessage(...)` replays:
  - `EnterChallengeMission_Ret((uint32_t)wparam);`

So `kSendEnterMission` is a relay hook message, not a direct pass-through command.

#### 8.3 Skillbar manager: load skillbar

In `Source/SkillbarMgr.cpp`:

- `OnLoadSkillbar(...)` builds:
  - `struct { uint32_t agent_id; uint32_t skill_ids[8]; }`
- then sends:
  - `UI::SendUIMessage(UI::UIMessage::kSendLoadSkillbar, &pack);`
- `OnLoadSkillbar_UIMessage(...)` replays:
  - `RetLoadSkills(pack->agent_id, 8, pack->skill_ids);`

Again this is a GWCA relay message.

#### 8.4 Item manager: use item

In `Source/ItemMgr.cpp`:

- `OnUseItem(item_id)` sends:
  - `kSendUseItem`
- `OnUseItem_UIMessage(...)` replays:
  - `UseItem_Ret((uint32_t)wparam);`

Payload:

- `wParam = item_id`

#### 8.5 Item manager: move item

In `Source/ItemMgr.cpp`:

- `OnMoveItem(item_id, quantity, bag_id, slot)` builds:
  - `{ item_id, quantity, bag_id, slot }`
- sends:
  - `kSendMoveItem`
- `OnMoveItem_UIMessage(...)` replays:
  - `MoveItem_Ret(pack[0], pack[1], pack[2], pack[3]);`

Payload layout:

- `uint32_t[4]`
- field order:
  - `item_id`
  - `quantity`
  - `bag_id`
  - `slot`

#### 8.6 Item manager: ping weapon set

In `Source/ItemMgr.cpp`:

- `OnPingWeaponSet(...)` builds:
  - `{ agent_id, weapon_item_id, offhand_item_id }`
- sends:
  - `kSendPingWeaponSet`
- post callback replays original native function

#### 8.7 Merchant manager: request quote

In `Source/MerchantMgr.cpp`:

- `OnRequestQuote(...)` builds:
  - `RequestQuoteStruct { type, unknown, give, recv }`
- sends:
  - `kSendMerchantRequestQuote`
- post callback replays:
  - `RequestQuote_Ret(packet->type, packet->unknown, packet->give, packet->recv);`

#### 8.8 Merchant manager: transact item

In `Source/MerchantMgr.cpp`:

- `OnTransactItem(...)` builds:
  - `TransactItemStruct { type, gold_give, give, gold_recv, recv }`
- sends:
  - `kSendMerchantTransactItem`
- post callback replays original transaction function

#### 8.9 Quest manager: set active quest

In `Source/QuestMgr.cpp`:

- `OnSetActiveQuest(quest_id)` sends:
  - `kSendSetActiveQuest`
- post callback replays:
  - `SetActiveQuest_Ret((uint32_t)wparam);`

Payload:

- `wParam = quest_id`

#### 8.10 Quest manager: abandon quest

In `Source/QuestMgr.cpp`:

- `OnAbandonQuest(quest_id)` sends:
  - `kSendAbandonQuest`
- post callback replays:
  - `AbandonQuest_Ret((uint32_t)wparam);`

Payload:

- `wParam = quest_id`

### 9. Direct game-dispatch UI message examples

Not everything is a relay message. Some manager calls use `SendUIMessage` as a direct UI dispatch into the game.

#### 9.1 Travel

In `Source/MapMgr.cpp`:

- `Travel(map_id, district, region, language)` builds:
  - `struct MapStruct { map_id, region, language, district }`
- sends:
  - `UI::SendUIMessage(UI::UIMessage::kTravel, &t);`

This is a `0x10000000` family message and can go through the native game dispatcher path.

#### 9.2 Guild hall travel

In `Source/GuildMgr.cpp`:

- `TravelGH(GHKey key)` sends:
  - `kGuildHall`

Payload:

- `wParam = gh key (uint32_t[4])`

#### 9.3 Leave guild hall

In `Source/GuildMgr.cpp`:

- `LeaveGH()` sends:
  - `kLeaveGuildHall`

#### 9.4 Chat log injection

In `Source/ChatMgr.cpp`:

- `UI::SendUIMessage(UI::UIMessage::kWriteToChatLog, &param);`

This writes to the in-game chat log UI rather than replaying a hooked native "send" action.

#### 9.5 Open trade window

In `Source/TradeMgr.cpp`:

- `Trade::OpenTradeWindow(agent_id)` sends:
  - `kInitiateTrade`

Payload:

- `wParam = agent_id`

### 10. Message payloads GWCA documents explicitly

`UIMgr.h` is very valuable because it comments several payload layouts directly.

Important documented payloads:

- `kChangeTarget`
  - `wparam = ChangeTargetUIMsg*`
- `kPlayerChatMessage`
  - `wparam = { uint32_t channel, wchar_t* message, uint32_t player_number }`
- `kLogout`
  - `wparam = { bool unknown, bool character_select }`
- `kGuildHall`
  - `wparam = gh key (uint32_t[4])`
- `kTravel`
  - implicit map struct visible in `MapMgr.cpp`
- `kMoveItem`
  - `wparam = { item_id, to_bag, to_slot, bool prompt }`
- `kOpenTemplate`
  - `wparam = GW::UI::ChatTemplate*`
- `kSendLoadSkillbar`
  - `wparam = { uint32_t agent_id, uint32_t* skill_ids }`
- `kSendPingWeaponSet`
  - `wparam = { uint32_t agent_id, uint32_t weapon_item_id, uint32_t offhand_item_id }`
- `kSendMoveItem`
  - `wparam = { uint32_t item_id, uint32_t quantity, uint32_t bag_id, uint32_t slot }`
- `kSendMerchantRequestQuote`
  - `wparam = { TransactionType, gold/quote fields... }`
- `kSendMerchantTransactItem`
  - `wparam = { TransactionType, quote info... }`
- `kSendUseItem`
  - `wparam = item_id`
- `kSendSetActiveQuest`
  - `wparam = quest_id`
- `kSendAbandonQuest`
  - `wparam = quest_id`

### 11. What to infer carefully

Safe conclusions:

- GWCA's UI-message layer is a true middleware layer, not just a thin export table.
- `SendUIMessage` is both:
  - a public API for callers
  - the internal hook funnel for intercepted native UI/game actions
- `0x30000000` `kSend*` ids are often GWCA-owned relay messages
- `0x10000000` ids are the main game-facing UI dispatcher family

Things to avoid oversimplifying:

- not every `kSend*` means "GWCA sends a raw game packet"
- not every action that uses `SendUIMessage` ultimately hits the game's native dispatcher
- in many cases the real execution is:
  - GW native function hook
  - GWCA relay message
  - callbacks
  - replay of original native function pointer

### 12. Practical botting relevance

For bot/reversing work, this means GWCA gives you two different powers:

- a direct command surface for real UI/game commands like `kTravel`, `kGuildHall`, `kInitiateTrade`, and `kWriteToChatLog`
- a normalized interception surface for actions that GWCA re-encodes as relay messages like `kSendUseItem`, `kSendMoveItem`, `kSendDialog`, `kSendLoadSkillbar`, and merchant/quest operations

That is why the UI-message system is so central:

- it is both a control plane and an instrumentation plane.

### 13. Best next slice to investigate

If we keep going deeper on GWCA UI messages, the next highest-value pass is:

1. enumerate every manager function that calls `UI::SendUIMessage`
2. classify each one as:
   - direct game dispatch
   - GWCA relay/hook message
3. document exact `wParam`/`lParam` layouts
4. correlate each one against `gwca.dll` decompilation where source and binary differ

### 14. Binary decompilation notes from `gwca.dll`

I decompiled several GWCA UI-message related functions directly from the compiled `gwca.dll` with Ghidra headless.

These results are useful because they show what the shipped binary actually does, not just what the source tree says.

#### 14.1 `GW::UI::SendUIMessage`

Binary-decompiled behavior matches the source-level architecture:

- look up callbacks by `UIMessage`
- run a first callback pass
- if not blocked and the message is not in the `0x30000000` family, call the native dispatcher
- run a second callback pass

Most important binary-confirmed detail:

- `0x30000000` messages are treated specially and are not forwarded to the native dispatcher in the raw path

So the "GWCA relay message" interpretation is not just a source inference; it is confirmed in the binary.

#### 14.2 `GW::UI::Keydown`

Decompiled body:

```cpp
bool __cdecl GW::UI::Keydown(ControlAction param_1,Frame *param_2)
{
  bool bVar1;
  int iVar2;
  uint uVar3;
  ControlAction local_10 [3];

  local_10[0] = param_1;
  local_10[1] = 0;
  local_10[2] = 0;
  if (param_2 == (Frame *)0x0) {
    iVar2 = FUN_10025e00();
    if ((DAT_1008a37c == (code *)0x0) || (iVar2 == 0)) {
      param_2 = (Frame *)0x0;
    }
    else {
      uVar3 = (*DAT_1008a37c)(*(undefined4 *)(iVar2 + 0xbc),6);
      param_2 = GetFrameById(uVar3);
    }
  }
  bVar1 = SendFrameUIMessage(param_2,0x20,local_10,(void *)0x0);
  return bVar1;
}
```

Meaning:

- `Keydown` is a frame-message path, not a global `SendUIMessage` path
- it builds a 3-dword control packet
- if no frame is supplied, it tries to resolve a default/current frame
- it sends frame message id `0x20`

#### 14.3 `GW::UI::Keyup`

Decompiled body:

```cpp
bool __cdecl GW::UI::Keyup(ControlAction param_1,Frame *param_2)
{
  bool bVar1;
  int iVar2;
  uint uVar3;
  ControlAction local_10 [3];

  local_10[0] = param_1;
  local_10[1] = 0;
  local_10[2] = 0;
  if (param_2 == (Frame *)0x0) {
    iVar2 = FUN_10025e00();
    if ((DAT_1008a37c == (code *)0x0) || (iVar2 == 0)) {
      param_2 = (Frame *)0x0;
    }
    else {
      uVar3 = (*DAT_1008a37c)(*(undefined4 *)(iVar2 + 0xbc),6);
      param_2 = GetFrameById(uVar3);
    }
  }
  bVar1 = SendFrameUIMessage(param_2,0x22,local_10,(void *)0x0);
  return bVar1;
}
```

Meaning:

- same packet shape as `Keydown`
- different frame message id: `0x22`

#### 14.4 `GW::UI::Keypress`

Decompiled body:

```cpp
bool __cdecl GW::UI::Keypress(ControlAction param_1,Frame *param_2)
{
  bool bVar1;
  int iVar2;
  uint uVar3;
  ControlAction local_10 [3];

  local_10[1] = 0;
  local_10[2] = 0;
  local_10[0] = param_1;
  if (param_2 == (Frame *)0x0) {
    iVar2 = FUN_10025e00();
    if ((DAT_1008a37c == (code *)0x0) || (iVar2 == 0)) {
      param_2 = (Frame *)0x0;
    }
    else {
      uVar3 = (*DAT_1008a37c)();
      param_2 = GetFrameById(uVar3);
    }
  }
  bVar1 = SendFrameUIMessage(param_2,0x20,local_10,(void *)0x0);
  if (bVar1) {
    GameThread::Enqueue();
    return true;
  }
  return false;
}
```

Meaning:

- `Keypress` is not just `Keydown` + `Keyup` in a simple inline way
- it still uses `SendFrameUIMessage`
- on success it queues additional work via `GameThread::Enqueue()`

Practical implication:

- some GWCA "input" helpers live in the frame UI plane rather than the global `SendUIMessage` plane

#### 14.5 `GW::GuildMgr::TravelGH`

Decompiled body:

```cpp
bool __cdecl GW::GuildMgr::TravelGH(void)
{
  bool bVar1;

  bVar1 = UI::SendUIMessage(0x1000017c,&stack0x00000004,(void *)0x0);
  return bVar1;
}
```

Meaning:

- guild hall travel in this binary is a thin wrapper over a `0x10000000` family UI message
- this is a direct UI-dispatch style wrapper

Note:

- the exact numeric id shown here differs from the local source enum comment, so binary/source drift exists

#### 14.6 `GW::GuildMgr::LeaveGH`

Decompiled body:

```cpp
bool __cdecl GW::GuildMgr::LeaveGH(void)
{
  bool bVar1;

  bVar1 = UI::SendUIMessage(0x1000017e,(void *)0x0,(void *)0x0);
  return bVar1;
}
```

Meaning:

- same pattern as `TravelGH`
- simple direct wrapper around a single UI message id

Again, the numeric id differs from the local source enum comments, which is another sign that this compiled DLL is not identical to the checked-in source snapshot.

#### 14.7 `GW::Trade::OpenTradeWindow`

Decompiled body:

```cpp
bool __cdecl GW::Trade::OpenTradeWindow(uint param_1)
{
  bool bVar1;

  bVar1 = UI::SendUIMessage(0x100001a8,(void *)param_1,(void *)0x0);
  return bVar1;
}
```

Meaning:

- opening trade is a direct UI-message wrapper
- payload is simply `wParam = agent_id`

### 15. What these binary results add

The binary pass gives a cleaner split:

- global UI dispatcher plane:
  - `SendUIMessage`
  - manager wrappers like `TravelGH`, `LeaveGH`, `OpenTradeWindow`
- frame UI/input plane:
  - `Keydown`
  - `Keyup`
  - `Keypress`

So when we say "GWCA UI message functions", there are really two nearby but different mechanisms:

- global UI-message dispatch
- frame-message based UI/input dispatch

That distinction is especially important for botting and automation, because a lot of "input-like" GWCA helpers do not use the same path as travel, trade, guild hall, or chat-log actions.

### 16. Frame-side decompilation: dispatch, lookup, and button actuation

To complete the UI story, I also decompiled the frame-side functions that `Keydown`, `Keyup`, `Keypress`, and `ButtonClick` rely on.

This is the other half of GWCA's UI system:

- global UI-message dispatch is for map/trade/chat/guild-style actions
- frame UI dispatch is for button and input-style interactions inside the frame tree

#### 16.1 `GW::UI::SendFrameUIMessage`

Decompiled body:

```cpp
bool __cdecl
GW::UI::SendFrameUIMessage(Frame *param_1,UIMessage param_2,void *param_3,void *param_4)
{
  if ((DAT_1008a3a0 == (code *)0x0) || (param_1 == (Frame *)0x0)) {
    return false;
  }
  ...
  if (iVar3 == DAT_1008a458) {
    Hook::EnterHook();
    (*DAT_1008a3a0)(param_2,param_3,param_4);
    Hook::LeaveHook();
    return true;
  }
  ...
}
```

What this shows:

- `SendFrameUIMessage` is the frame-plane analogue of `SendUIMessage`
- it has its own callback registry and pre/post callback flow
- if no callbacks are registered for that frame message id, it calls the underlying frame dispatcher directly
- it requires a non-null frame/context pointer and a valid underlying callback target

So GWCA duplicates the same middleware pattern on the frame plane:

- callback lookup
- pre callbacks
- underlying dispatch
- post callbacks

#### 16.2 `GW::UI::GetFrameById`

Decompiled body:

```cpp
Frame * __cdecl GW::UI::GetFrameById(uint param_1)
{
  Frame *pFVar1;
  int *piVar2;

  piVar2 = DAT_1008a3b0;
  if ((DAT_1008a3b0 == (int *)0x0) || ((uint)DAT_1008a3b0[2] <= param_1)) {
    return (Frame *)0x0;
  }
  ...
  pFVar1 = *(Frame **)(*piVar2 + param_1 * 4);
  if ((pFVar1 != (Frame *)0x0) && (pFVar1 != (Frame *)0xffffffff)) {
    return pFVar1;
  }
  return (Frame *)0x0;
}
```

What this proves:

- frame lookup by id is just indexed access into the global frame array
- GWCA rejects null and `0xFFFFFFFF` sentinel entries

This is the concrete basis for a lot of higher-level frame work.

#### 16.3 `GW::UI::GetChildFrame`

Decompiled body:

```cpp
Frame * __cdecl GW::UI::GetChildFrame(Frame *param_1,uint param_2)
{
  uint uVar1;
  Frame *pFVar2;

  if ((DAT_1008a37c != (code *)0x0) && (param_1 != (Frame *)0x0)) {
    uVar1 = (*DAT_1008a37c)(*(undefined4 *)(param_1 + 0xbc),param_2);
    pFVar2 = GetFrameById(uVar1);
    return pFVar2;
  }
  return (Frame *)0x0;
}
```

What this proves:

- child lookup is not a manual pointer walk in this helper
- GWCA asks an internal child-resolution function for a child frame id
- then resolves that id through `GetFrameById`

So the important pair is:

- child resolver returns frame id
- frame array lookup returns `Frame*`

#### 16.4 `GW::UI::GetFrameContext`

Decompiled body:

```cpp
void * __cdecl GW::UI::GetFrameContext(Frame *param_1)
{
  void *pvVar1;
  int iVar2;
  uint uVar3;

  if (((param_1 != (Frame *)0x0) && (*(int *)(param_1 + 0xb0) != 0)) &&
     (uVar3 = *(int *)(param_1 + 0xb0) - 1, -1 < (int)uVar3)) {
    iVar2 = uVar3 * 0xc;
    do {
      ...
      pvVar1 = *(void **)(*(int *)(param_1 + 0xa8) + 4 + iVar2);
      if (pvVar1 != (void *)0x0) {
        return pvVar1;
      }
      iVar2 = iVar2 + -0xc;
      uVar3 = uVar3 - 1;
    } while (-1 < (int)uVar3);
  }
  return (void *)0x0;
}
```

What this proves:

- the exported `GetFrameContext` does not simply return `frame + constant`
- it scans backward through the frame's interaction-callback array
- it returns the last non-null callback/context entry it finds

This is important because "frame context" here is a live callback-context lookup, not just a static parent-frame pointer shortcut.

#### 16.5 `GW::UI::ButtonClick`

Decompiled body:

```cpp
bool __cdecl GW::UI::ButtonClick(Frame *param_1)
{
  if (param_1 != (Frame *)0x0) {
    return ButtonFrame::Click((ButtonFrame *)param_1);
  }
  return false;
}
```

What this shows:

- `ButtonClick` itself is just a null-check wrapper
- the real click logic lives in `ButtonFrame::Click` / `ButtonFrame::MouseAction`

#### 16.6 `GW::ButtonFrame::MouseAction`

Decompiled body:

```cpp
bool __thiscall GW::ButtonFrame::MouseAction(ButtonFrame *this,ActionState param_1)
{
  bool bVar1;
  Frame *pFVar2;
  ...
  if ((*(uint *)(this + 0x18c) & 0x214) == 4) {
    pFVar2 = UI::GetParentFrame((Frame *)this);
    if ((pFVar2 != (Frame *)0x0) && ((*(uint *)(pFVar2 + 0x18c) >> 2 & 1) != 0)) {
      local_28 = *(undefined4 *)(this + 0xbc);
      local_24 = *(undefined4 *)(this + 0xb8);
      local_10 = *(undefined4 *)(this + 0x1c4);
      local_1c = &local_14;
      local_20 = param_1;
      local_18 = 0;
      local_14 = 0;
      local_c = 0;
      bVar1 = UI::SendFrameUIMessage(pFVar2,0x31,&local_28,(void *)0x0);
      return bVar1;
    }
  }
  return false;
}
```

What this proves:

- button actuation is implemented as a frame message, not a global UI message
- the send target is the parent frame, not necessarily the button frame itself
- the packet starts with fields pulled from the button:
  - `frame_id` at `+0xBC`
  - `child_offset_id` at `+0xB8`
  - value at `+0x1C4`
  - requested `ActionState`
- the frame message id used here is `0x31`

This is a strong binary confirmation of the frame-click model:

- button clicks are synthesized as structured frame messages aimed at the frame hierarchy

### 17. Updated model of GWCA UI

After combining the global and frame decompilation results, the practical model is:

1. `SendUIMessage`
   - global UI/message middleware
   - used by map/guild/trade/chat and GWCA relay hooks
2. `SendFrameUIMessage`
   - frame-local middleware
   - used by input/button interactions
3. `GetFrameById` / `GetChildFrame`
   - resolve frame pointers through the frame-id space
4. `ButtonClick`
   - thin wrapper
5. `ButtonFrame::MouseAction`
   - real button actuation path
   - builds structured frame payload
   - dispatches message `0x31` through the parent frame

### 18. Why this matters for automation

This makes the split very explicit:

- traveling, trading, guild hall travel, chat injection:
  - global UI-message plane
- keypresses, button clicks, frame interactions:
  - frame-message plane

So a bot or tool that wants complete UI control in Guild Wars through GWCA needs both:

- global UI-message understanding
- frame-dispatch understanding

### 19. Frame hierarchy helpers from the binary

I also decompiled the remaining high-value frame hierarchy helpers so the frame tree is no longer a guess.

#### 19.1 `GW::UI::GetParentFrame`

Decompiled body:

```cpp
Frame * __cdecl GW::UI::GetParentFrame(Frame *param_1)
{
  if (param_1 != (Frame *)0x0) {
    return (Frame *)(-(uint)(*(int *)(param_1 + 0x128) != 0) & *(int *)(param_1 + 0x128) - 0x128U);
  }
  return (Frame *)0x0;
}
```

What this proves:

- the parent pointer is derived from the relation pointer at `frame + 0x128`
- if that relation pointer is non-null, GWCA subtracts `0x128`
- so the relation object is embedded inside the parent frame at offset `0x128`

This is the clean binary confirmation of the pattern:

- `parent_frame = *(frame + 0x128) - 0x128`

That matches the frame-relation interpretation used elsewhere in the research.

#### 19.2 `GW::UI::GetRootFrame`

Decompiled body:

```cpp
Frame * __cdecl GW::UI::GetRootFrame(void)
{
  if (DAT_1008a410 != (code *)0x0) {
    return (Frame *)(*DAT_1008a410)();
  }
  return (Frame *)0x0;
}
```

What this proves:

- root-frame lookup is delegated to a scanned internal game function
- GWCA is not synthesizing the root itself in this helper

#### 19.3 Sibling helper note

I did not get a named `GetSiblingFrame` match from this `gwca.dll` build.

That likely means one of:

- the helper is absent from this build,
- it exists under a different export name,
- or sibling navigation is expected to happen via relation fields rather than a public helper.

This is a gap, but not a blocker for reconstructing button-click actuation.

### 20. Reconstructing the `0x31` frame packet

`ButtonFrame::MouseAction` gives the clearest binary view of the click packet passed into the frame dispatcher.

Relevant decompiled setup:

```cpp
local_28 = *(undefined4 *)(this + 0xbc);
local_24 = *(undefined4 *)(this + 0xb8);
local_10 = *(undefined4 *)(this + 0x1c4);
local_1c = &local_14;
local_20 = param_1;
local_18 = 0;
local_14 = 0;
local_c = 0;
bVar1 = UI::SendFrameUIMessage(pFVar2,0x31,&local_28,(void *)0x0);
```

From that, the packet layout passed as `wParam` is approximately:

```cpp
struct MouseActionPacket {
    uint32_t frame_id;          // this + 0xBC
    uint32_t child_offset_id;   // this + 0xB8
    ActionState action_state;   // method argument
    uint32_t unk0;              // initialized 0
    uint32_t* unk1_ptr;         // points at following local field
    uint32_t unk1;              // initialized 0
    uint32_t dialog_or_label;   // this + 0x1C4
    uint32_t unk2;              // initialized 0
};
```

What we can say with high confidence:

- first field is the button frame id
- second field is the child-offset id
- third field is the requested action state
- packet includes the button field at `+0x1C4`
- several trailing fields are zeroed
- one field is a self-referential/local pointer into the packet

What we should treat cautiously:

- exact semantic names for the trailing pointer/zero fields
- whether `this + 0x1C4` is always dialog id, label id, or a more generic button payload field

Still, this is enough to understand the actuation pattern precisely:

- GWCA does not send a naked "click button X" integer
- it sends a structured frame action packet that names both the frame and its child-offset identity

### 21. Complete click trace from `ButtonClick` to the game dispatcher

With the decompiled pieces combined, the end-to-end click path now looks like this:

#### 21.1 Entry point

User-facing helper:

- `GW::UI::ButtonClick(Frame* frame)`

Binary body:

- null-checks frame
- calls `ButtonFrame::Click((ButtonFrame*)frame)`

#### 21.2 Click expansion

`ButtonFrame::Click` is now binary-confirmed directly:

```cpp
bool __thiscall GW::ButtonFrame::Click(ButtonFrame *this)
{
  bool bVar1;

  bVar1 = MouseAction(this,6);
  if (bVar1) {
    bVar1 = MouseAction(this,7);
    if (bVar1) {
      return true;
    }
  }
  return false;
}
```

So a logical GWCA button click is not one frame message. It is a two-stage expansion:

- first `MouseAction(this, 6)`
- then `MouseAction(this, 7)`

The safest interpretation is:

- `6` = press/down-style state
- `7` = release/up-style state

That interpretation is still semantic inference, but the sequencing itself is no longer inferred.

#### 21.3 Per-action packet construction

`ButtonFrame::MouseAction(ActionState state)`:

1. checks the button frame state at `+0x18C`
2. resolves the parent frame with:
   - `GetParentFrame(button) = *(button + 0x128) - 0x128`
3. verifies the parent frame is valid/created
4. builds the `0x31` mouse-action packet:
   - frame id
   - child offset id
   - action state
   - zeroed support fields
   - button field from `+0x1C4`
5. calls:
   - `SendFrameUIMessage(parent_frame, 0x31, &packet, 0)`

Because `ButtonFrame::Click()` invokes `MouseAction` twice, the complete click emits two separate `0x31` frame-dispatches, differing primarily by the `action_state` field.

#### 21.4 Frame-plane middleware

`SendFrameUIMessage(parent_frame, 0x31, &packet, 0)` then:

1. looks up frame-message callbacks for message `0x31`
2. runs pre callbacks if any
3. if not blocked, calls the underlying frame dispatcher
4. runs post callbacks if any

Important distinction:

- unlike `SendUIMessage`, this is not the global UI-message bus
- it is a separate frame interaction bus

#### 21.5 Underlying game dispatcher

For a logical click, this handoff happens twice: once for action state `6`, then again for action state `7`.

If not blocked, GWCA ultimately invokes the scanned frame dispatcher pointer:

- `DAT_1008a3a0`

with:

- message id `0x31`
- `wParam = &MouseActionPacket`
- `lParam = 0`

That is the moment where the synthesized button interaction leaves GWCA middleware and enters the game's frame subsystem.

### 22. Unified model: global UI messages and frame messages as one system

At this point the two halves can be described as a single layered system:

#### 22.1 Global command/UI layer

Used for:

- map travel
- guild hall travel / leave
- trade open
- chat log injection
- GWCA relay `kSend*` hook messages

Entry point:

- `SendUIMessage`

#### 22.2 Frame interaction layer

Used for:

- keydown
- keyup
- keypress
- button click / mouse action

Entry point:

- `SendFrameUIMessage`

#### 22.3 Shared design pattern

Both layers implement:

- callback registry
- pre-callback interception
- optional block
- underlying dispatch
- post-callback continuation

So GWCA's UI architecture is internally consistent:

- same middleware idea
- two dispatch planes
- one for global/UI/game actions
- one for frame-local interaction actions

### 23. Strongest practical conclusion

For bots, automation, and reverse-engineering:

- if you want to trigger game actions like travel or trade, think in terms of `SendUIMessage`
- if you want to click actual UI controls, think in terms of `SendFrameUIMessage` plus structured frame packets

The most concrete click formula now supported by binary evidence is:

1. obtain button `Frame*`
2. compute parent via `GetParentFrame`
3. build mouse-action packet from:
   - `button + 0xBC`
   - `button + 0xB8`
   - `button + 0x1C4`
   - desired `ActionState`
4. dispatch message `0x31` through `SendFrameUIMessage(parent, 0x31, &packet, 0)`

That is the clearest unified explanation of how GWCA bridges from high-level "click this button" intent to the underlying game frame dispatcher.

return result
```

---

## Root Cause: Command Queue Doesn't Execute at Char Select

### The Problem
The BotsHub framework's `MainProc` hook has TWO paths:
- **HandleCase** (line 1535): Reads command, zeros it, increments counter, but **DOES NOT EXECUTE** (`jmp MainExit`)
- **RegularFlow** (line 1553): Reads command, zeros it, **and EXECUTES** (`jmp ebx`)

`HandleCase` is taken when `[BasePointer]->...->field_198 == 0`, which happens at the character select screen (no active map loaded). This means **ALL command queue operations are silently discarded at char select**.

### The Fix
Patch `HandleCase`'s `jmp MainExit` to `jmp RegularFlow` — making it execute commands regardless of game state. This is a 1-byte change in the injected code.

Or: patch the `je HandleCase` conditional jump to `je RegularFlow`.

### Assembler .5 Offset Issue
The BotsHub assembler accumulates a fractional `.5` byte in `$asm_injection_size` from upstream code. This causes ALL extension code labels to be ~4 bytes off from their actual memory position. Workaround: scan memory for instruction signatures to find the real address (calibration).

---

## Current Implementation Status

### Working
- `GetFrameByHash($hash)` — walks frame array, finds frames by hash at offset 0x134
- `IsFrameVisible($hash)` / `IsAtCharSelect()` / `IsReconnectDialogShowing()` — state detection
- `ExtendScanner_FrameUI()` — scans game binary for SendFrameUIMsg function
- Shellcode execution via VirtualAllocEx + command queue (when HandleCase is patched)
- GWCA DLL injection + initialization

### Not Working Yet
- `ClickFrameButton($hash)` — shellcode never executes because HandleCase discards it
- Need to patch HandleCase before any char select commands work

### Execution Pipeline — SOLVED (2026-03-28)
Commands now execute at char select via the rendering hook:
- **MainProc hook**: NOT called at char select (game state checks fail)
- **RenderingModProc hook**: IS called at char select (rendering active)
- Added queue processing to RenderingModProc: `call ebx` with shellcode ending in RET
- HandleCase in MainProc also patched to execute commands (was discarding them)

### Previous Blocker (RESOLVED — was _WriteLE32 bug)
The `_WriteLE32` function was broken: it used DllStructGetData element indexing on a
dword field, which only returns the full value for element 1 and 0 for elements 2-4.
This means ALL shellcode addresses and call offsets were corrupted (only low byte
written, rest zeroed). Every previous click attempt used garbage shellcode.

**Fixed 2026-03-28**: Replaced with BitShift/BitAND byte extraction. Confirmed by
reading back shellcode bytes and verifying marker writes in game memory.

### Current Blocker (2026-03-28)
With correct shellcode (verified via marker test), SendFrameUIMsg still doesn't click:
- Tested ECX = frame+0x84, frame+0xA8, cbBuf, frame_ptr
- Tested msgid = 0x2F, 0x31, 0x22, 0x2B (game uses 0x2B at actual call site)
- Tested wParam = action struct, NULL
- All return cleanly, no crash, no click
- The function at game+0x2286D0 IS SendFrameUIMsg and is NOT hooked by BotsHub

### Key Discoveries (2026-03-28)

**Rendering hook IS active at char select** on fresh clients. Marker writes from
shellcode confirm execution. Queue counter advances correctly.

**`_WriteLE32` was the root cause** of ALL previous failures. Fixed now.

**GW::Initialize breaks BotsHub rendering hook** — every time. GWCA hooks the same
game functions as BotsHub (including rendering). After GW::Initialize, the rendering
hook stops processing commands. Scanner::Initialize alone does NOT cause this.

**Scanner::Initialize alone doesn't populate GWCA data** — the actual pattern scanning
happens in GW::Initialize when each module calls Scanner::Find.

**SendFrameUIMsg = Action** — both BotsHub scan labels resolve to game+0x2286D0. This
is a general frame message dispatcher: `__thiscall(callbacks, msgid, wParam, lParam)`.
It checks msgid in sequence (9, 11, ...) and dispatches. NOT hooked (starts with
`55 8B EC` original prologue).

**Game call site uses msgid 0x2B** at the scan pattern location. The hex context:
```
6A 00       ; push 0 (lParam)
50          ; push eax (wParam = local struct ptr)
6A 2B       ; push 0x2B (msgid)
83 C1 DC    ; add ecx, -0x24 → ECX = frame+0x84
E8 ...      ; call SendFrameUIMsg
```

**callback[0] takes a STRUCT pointer** as first arg, not individual (frame, msg, wp, lp).
Its prologue: `mov esi,[ebp+8]; mov eax,[esi+4]; sub eax,1` — reads field_4 from
the first argument struct.

### SOLVED: Native ButtonClick (2026-03-28)

**Working approach — NO gwca.dll needed:**

1. `_GetFrameContext(Frame*)` = `[frame+0x128] - 0x128` = parent frame pointer
   - GWCA's MouseAction calls an INTERNAL function at `+0x25EC0` (not exported `+0x25D90`)
   - This function simply reads the `FrameRelation` pointer and subtracts its offset
   - Returns the PARENT FRAME, which is the "context" for SendFrameUIMsg

2. Build kMouseAction: `{frame_id, child_offset_id, action_state, 0, 0}`

3. Call `SendFrameUIMsg(__thiscall)`:
   - ECX = `context + 0xA8` (parent frame's callback array)
   - Stack: `msgid=0x31, wParam=&kMouseAction, lParam=0`

4. Send MouseDown (action_state=6) then MouseUp (action_state=7)

5. Execute via rendering hook on the game thread

**Why all previous attempts failed:**
- We used `frame + 0xA8` or `frame + 0x84` as ECX — WRONG
- The function needs `PARENT_FRAME + 0xA8`, not `BUTTON_FRAME + 0xA8`
- GWCA approach also failed initially because `+0x8A3A0` (hooked ptr) was NULL
- `_WriteLE32` was broken (only wrote low byte) — ALL shellcode was corrupted

**Key functions:**
- `SendFrameUIMsg`: game + 0x2286D0 (scan pattern `83 C1 DC E8`, offset 3)
- `GetFrameContext`: pure memory read — `[frame+0x128] - 0x128`
- Frame detection: walk FrameArray, compare `frame_hash_id` at offset 0x134

**Known frame hashes:**
- Play button: 184818986
- Play greyed: 41327607
- Reconnect YES: 1398610279
- Reconnect NO: 3600335809

---

## Deeper Binary Layer: Neighboring Frame Helpers

The next useful step after recovering `ButtonFrame::Click()` is to map the neighboring helpers in the same frame-control cluster. That gives a better picture of the game's frame action vocabulary instead of treating clicks as a special case.

### `GW::ButtonFrame::DoubleClick`

Binary body:

```cpp
bool __thiscall GW::ButtonFrame::DoubleClick(ButtonFrame *this)
{
  bool bVar1;

  bVar1 = MouseAction(this,9);
  return bVar1;
}
```

This matters because it shows GWCA does not model a double-click as "call `Click()` twice".

Instead:

- normal click = `MouseAction(6)` then `MouseAction(7)`
- double-click = one `MouseAction(9)`

So at the frame protocol level, double-click is its own action code, not just repeated single-click behavior.

### `GW::DropdownFrame::SelectIndex`

Binary body:

```cpp
bool __thiscall GW::DropdownFrame::SelectIndex(DropdownFrame *this,uint param_1)
{
  bool bVar1;
  void *pvVar2;
  Frame *pFVar3;
  UIMessage UVar4;
  undefined4 *puVar5;
  undefined4 local_18;
  undefined4 local_14;
  undefined4 local_10;
  uint local_c;
  undefined4 local_8;

  pFVar3 = (Frame *)(-(uint)(this != (DropdownFrame *)0x0) & (uint)(this + 4));
  pvVar2 = UI::GetFrameContext(pFVar3);
  if ((pvVar2 != (void *)0x0) && (param_1 < *(uint *)((int)pvVar2 + 8))) {
    bVar1 = UI::SendFrameUIMessage(pFVar3,0x60,(void *)param_1,(void *)0x0);
    if (bVar1) {
      local_18 = *(undefined4 *)(this + 0xc0);
      local_14 = *(undefined4 *)(this + 0xbc);
      puVar5 = &local_18;
      pvVar2 = (void *)0x0;
      UVar4 = 0x31;
      local_8 = 0;
      local_10 = 7;
      local_c = param_1;
      pFVar3 = UI::GetParentFrame(pFVar3);
      bVar1 = UI::SendFrameUIMessage(pFVar3,UVar4,puVar5,pvVar2);
      return bVar1;
    }
  }
  return false;
}
```

This is one of the strongest new findings, because it proves frame actuation is not button-only and not single-message-only.

Dropdown selection is a two-stage protocol:

1. send frame message `0x60` to the dropdown frame itself, using the selected index as `wParam`
2. if that succeeds, send frame message `0x31` to the parent frame with a structured packet

That means `0x31` is broader than "button click". It behaves more like a parent-facing frame action / commit / notification message that several controls can reuse.

The second packet appears to contain:

- `this + 0xC0`
- `this + 0xBC`
- hardcoded state/value `7`
- selected index
- trailing zero field

The exact semantic names of those fields are still inferred, but the dispatch sequence is binary-confirmed.

### `GW::DropdownFrame::SelectOption`

Binary body:

```cpp
bool __thiscall GW::DropdownFrame::SelectOption(DropdownFrame *this,uint param_1)
{
  bool bVar1;
  int *piVar2;
  int iVar3;
  int iVar4;
  uint uVar5;

  uVar5 = param_1;
  piVar2 = UI::GetFrameContext((Frame *)(-(uint)(this != (DropdownFrame *)0x0) & (uint)(this + 4)));
  if (piVar2 != (int *)0x0) {
    iVar4 = *piVar2;
    iVar3 = piVar2[2] * 0x20 + iVar4;
    if (iVar4 != iVar3) {
      while (*(int *)(iVar4 + 8) == 0) {
        iVar4 = iVar4 + 0x20;
        if (iVar4 == iVar3) {
          bVar1 = SelectIndex(this,uVar5);
          return bVar1;
        }
      }
      bVar1 = GetOptionIndex(this,uVar5,&param_1);
      uVar5 = param_1;
      if (!bVar1) {
        return false;
      }
    }
  }
  bVar1 = SelectIndex(this,uVar5);
  return bVar1;
}
```

This shows `SelectOption` is a value-to-index translation layer, not its own dispatch primitive.

It:

- inspects option storage via `GetFrameContext`
- optionally resolves a symbolic option value into a concrete list index
- then delegates to `SelectIndex`

So the real dropdown actuation path is still:

- symbolic option/value
- resolve index
- send `0x60`
- send `0x31`

### `GW::TabsFrame::GetTabButton`

Binary body:

```cpp
ButtonFrame * __thiscall GW::TabsFrame::GetTabButton(TabsFrame *this,Frame *param_1)
{
  Frame *pFVar1;

  if (param_1 != (Frame *)0x0) {
    pFVar1 = UI::GetChildFrame((Frame *)this,*(uint *)(param_1 + 0xb8));
    if (pFVar1 == param_1) {
      pFVar1 = UI::GetChildFrame((Frame *)this,~*(uint *)(param_1 + 0xb8));
      return (ButtonFrame *)pFVar1;
    }
  }
  return (ButtonFrame *)0x0;
}
```

This is a subtle structural clue about the frame tree:

- tab controls and their paired button controls are related through `child_offset_id`
- GWCA finds the companion by taking the bitwise complement of the current child id

That strongly suggests the child-id space is deliberately structured and paired, rather than being a flat bag of unrelated numbers.

### What This Changes About The UI Model

Before these neighboring decompiles, the frame story could still be compressed too much into:

- "buttons send `0x31` packets"

That is no longer precise enough.

The better model is:

- `MouseAction(state)` is a generic frame action primitive used by button controls
- `0x31` is a broader parent-facing frame action channel, not button-exclusive
- controls can perform local control-state updates first through other frame messages such as dropdown `0x60`
- some control relationships are encoded through `child_offset_id` transforms, including bitwise-complement pairing

In other words, GWCA is exposing pieces of the game's native frame protocol, not just wrapping a single generic click helper.

## Protocol Catalog Expansion: Tabs, Checkboxes, Labels, Text, Sliders

The next pass through the same binary cluster fills in several more control families. At this point the frame-side model is strong enough to describe GWCA as exposing a small protocol catalog, where each control type has:

- one or more control-local frame messages
- optional state/query validation
- and sometimes a parent-facing `0x31` finalize/notify step

### `GW::TabsFrame::ChooseTab(unsigned int)`

Binary body:

```cpp
bool __thiscall GW::TabsFrame::ChooseTab(TabsFrame *this,uint param_1)
{
  bool bVar1;
  undefined4 local_8;

  local_8 = 0;
  bVar1 = UI::SendFrameUIMessage((Frame *)this,0x59,(void *)param_1,&local_8);
  if (!bVar1) {
    return false;
  }
  bVar1 = UI::SendFrameUIMessage((Frame *)this,0x5c,(void *)param_1,(void *)0x0);
  return bVar1;
}
```

This shows tab selection by index is a two-message protocol entirely on the tab frame itself:

1. `0x59` with the requested tab index and an output/status slot in `lParam`
2. `0x5C` with the same tab index to finalize the selection

That is notable because tabs do not use the parent-facing `0x31` pattern here. Their commit path appears to stay local to the tab container.

### `GW::TabsFrame::ChooseTab(Frame*)`

Binary body:

```cpp
bool __thiscall GW::TabsFrame::ChooseTab(TabsFrame *this,Frame *param_1)
{
  void *pvVar1;
  bool bVar2;

  if (param_1 != (Frame *)0x0) {
    pvVar1 = *(void **)(param_1 + 0xb8);
    param_1 = (Frame *)0x0;
    bVar2 = UI::SendFrameUIMessage((Frame *)this,0x59,pvVar1,&param_1);
    if (bVar2) {
      bVar2 = UI::SendFrameUIMessage((Frame *)this,0x5c,pvVar1,(void *)0x0);
      if (bVar2) {
        return true;
      }
    }
  }
  return false;
}
```

This overload confirms the same protocol, but it resolves the tab by the frame's `child_offset_id` at `+0xB8` rather than a raw numeric index.

So for tabs:

- `0x59` = validate/prepare/select candidate
- `0x5C` = finalize selection
- key identity can be passed either as index or `child_offset_id`

### `GW::CheckboxFrame::SetChecked(bool)`

Binary body:

```cpp
bool __thiscall GW::CheckboxFrame::SetChecked(CheckboxFrame *this,bool param_1)
{
  bool bVar1;
  Frame *pFVar2;
  int local_8;

  local_8 = 0;
  pFVar2 = (Frame *)(-(uint)(this != (CheckboxFrame *)0x0) & (uint)(this + 4));
  UI::SendFrameUIMessage(pFVar2,0x57,(void *)0x0,&local_8);
  if ((local_8 == 1) != param_1) {
    bVar1 = UI::SendFrameUIMessage(pFVar2,0x56,(void *)(uint)param_1,(void *)0x0);
    if (!bVar1) {
      return false;
    }
  }
  return true;
}
```

Checkboxes use a read-before-write pattern:

1. `0x57` queries the current checked state into `lParam`
2. only if the desired state differs, `0x56` writes the new boolean state

That tells us two useful things:

- `0x57` acts as a local state query for at least checkbox controls
- `0x56` acts as a local state/value setter for at least checkbox controls

### `GW::CheckboxFrame::SetValue(unsigned int)`

Binary body:

```cpp
bool __thiscall GW::CheckboxFrame::SetValue(CheckboxFrame *this,uint param_1)
{
  Frame *pFVar1;
  bool bVar2;

  bVar2 = param_1 != 0;
  param_1 = 0;
  pFVar1 = (Frame *)(-(uint)(this != (CheckboxFrame *)0x0) & (uint)(this + 4));
  UI::SendFrameUIMessage(pFVar1,0x57,(void *)0x0,&param_1);
  if ((param_1 == 1) != bVar2) {
    bVar2 = UI::SendFrameUIMessage(pFVar1,0x56,(void *)(uint)bVar2,(void *)0x0);
    if (!bVar2) {
      return false;
    }
  }
  return true;
}
```

This is effectively the same checkbox protocol, but surfaced through a generic `SetValue` API:

- normalize integer -> bool
- query via `0x57`
- write via `0x56` only if the value actually changes

That reinforces the idea that GWCA's frame wrappers often avoid redundant UI writes if the current state already matches.

### `GW::ButtonFrame::SetLabel` and `GW::TextLabelFrame::SetLabel`

Binary body:

```cpp
bool __thiscall GW::ButtonFrame::SetLabel(ButtonFrame *this,wchar_t *param_1)
{
  bool bVar1;

  if (param_1 != (wchar_t *)0x0) {
    bVar1 = UI::SendFrameUIMessage((Frame *)this,0x5b,param_1,(void *)0x0);
    if (bVar1) {
      return true;
    }
  }
  return false;
}
```

This same body is shared for:

- `ButtonFrame::SetLabel`
- `TextLabelFrame::SetLabel`

So for single-line label-style controls:

- `0x5B` is the local label/text setter

### `GW::MultiLineTextLabelFrame::SetLabel`

Binary body:

```cpp
bool __thiscall
GW::MultiLineTextLabelFrame::SetLabel(MultiLineTextLabelFrame *this,wchar_t *param_1)
{
  bool bVar1;

  if (param_1 != (wchar_t *)0x0) {
    bVar1 = UI::SendFrameUIMessage((Frame *)this,0x61,param_1,(void *)0x0);
    if (bVar1) {
      return true;
    }
  }
  return false;
}
```

This matters because it shows multiline text is not just another alias of `0x5B`.

Instead:

- single-line button/text label setter = `0x5B`
- multiline label setter = `0x61`

So even closely related visible controls can have distinct frame opcodes.

### `GW::EditableTextFrame::SetValue(wchar_t const*)`

Binary body:

```cpp
bool __thiscall GW::EditableTextFrame::SetValue(EditableTextFrame *this,wchar_t *param_1)
{
  bool bVar1;
  Frame *pFVar2;
  UIMessage UVar3;
  undefined4 *puVar4;
  void *pvVar5;
  undefined4 local_18;
  undefined4 local_14;
  undefined4 local_10;
  wchar_t *local_c;
  undefined4 local_8;

  if (param_1 != (wchar_t *)0x0) {
    bVar1 = UI::SendFrameUIMessage((Frame *)this,0x5d,param_1,(void *)0x0);
    if (bVar1) {
      local_18 = *(undefined4 *)(this + 0xbc);
      local_14 = *(undefined4 *)(this + 0xb8);
      puVar4 = &local_18;
      pvVar5 = (void *)0x0;
      UVar3 = 0x31;
      local_8 = 0;
      local_10 = 7;
      local_c = param_1;
      pFVar2 = UI::GetParentFrame((Frame *)this);
      bVar1 = UI::SendFrameUIMessage(pFVar2,UVar3,puVar4,pvVar5);
      return bVar1;
    }
  }
  return false;
}
```

Editable text fields follow the same broad pattern we already saw with dropdowns:

1. local control update first via `0x5D`
2. then parent-facing `0x31` with a structured packet

The `0x31` packet here contains at least:

- `this + 0xBC`
- `this + 0xB8`
- hardcoded state `7`
- the new `wchar_t*` value
- trailing zero field

So editable text fields look much closer to dropdowns and sliders than to tabs or plain labels.

### `GW::SliderFrame::SetValue(unsigned int)`

Binary body:

```cpp
bool __thiscall GW::SliderFrame::SetValue(SliderFrame *this,uint param_1)
{
  bool bVar1;
  Frame *pFVar2;
  void *pvVar3;
  Frame *pFVar4;
  UIMessage UVar5;
  undefined4 *puVar6;
  undefined4 local_18;
  undefined4 local_14;
  undefined4 local_10;
  uint local_c;
  undefined4 local_8;

  pFVar4 = (Frame *)(this + 4);
  pFVar2 = pFVar4;
  if (this == (SliderFrame *)0x0) {
    pFVar2 = (Frame *)0x0;
  }
  pvVar3 = UI::GetFrameContext(pFVar2);
  if (((pvVar3 != (void *)0x0) && (*(uint *)((int)pvVar3 + 0xc) <= param_1)) &&
     (param_1 <= *(uint *)((int)pvVar3 + 0x10))) {
    pFVar2 = pFVar4;
    if (this == (SliderFrame *)0x0) {
      pFVar2 = (Frame *)0x0;
    }
    UI::SendFrameUIMessage(pFVar2,0x56,(void *)param_1,(void *)0x0);
    local_18 = *(undefined4 *)(this + 0xc0);
    local_14 = *(undefined4 *)(this + 0xbc);
    puVar6 = &local_18;
    pvVar3 = (void *)0x0;
    UVar5 = 0x31;
    local_8 = 0;
    local_10 = 7;
    local_c = param_1;
    pFVar4 = UI::GetParentFrame(pFVar4);
    bVar1 = UI::SendFrameUIMessage(pFVar4,UVar5,puVar6,pvVar3);
    return bVar1;
  }
  return false;
}
```

Sliders add one more nuance:

- before writing, they validate the requested value against min/max-style bounds in the frame context

Then they follow the same general two-stage pattern:

1. local update via `0x56`
2. parent-facing `0x31` finalize/notify packet

### Control Families By Observed Pattern

At this point, the binary evidence supports a more concrete grouping:

`Local-only two-step controls`
- tabs: `0x59` then `0x5C`

`Query-then-set controls`
- checkboxes: `0x57` query, `0x56` set

`Direct local text setters`
- button/text label: `0x5B`
- multiline text label: `0x61`

`Local-update plus parent `0x31` commit`
- button click: `0x31` with action-state packet
- dropdown select: `0x60` then `0x31`
- editable text: `0x5D` then `0x31`
- slider value: `0x56` then `0x31`

This is the clearest evidence so far that GWCA's frame API is mirroring a real internal UI protocol taxonomy rather than inventing arbitrary wrappers.

## Observed Frame Opcode Matrix

At this point we have enough binary-confirmed samples to build a first-pass frame-opcode matrix. This is not a full game enum, but it is now a practical catalog of message ids that GWCA actually emits for known control families.

### High-confidence observed frame opcodes

| Msg ID | Observed role | Observed users | Notes |
|---|---|---|---|
| `0x20` | key down | `UI::Keydown`, `UI::Keypress` | Frame-input plane, not global UI-message plane |
| `0x22` | key up | `UI::Keyup` | Paired with `0x20` |
| `0x31` | parent-facing action / commit / notify | `ButtonFrame::MouseAction`, `DropdownFrame::SelectIndex`, `EditableTextFrame::SetValue`, `SliderFrame::SetValue` | Not button-exclusive; reused by multiple controls |
| `0x56` | local value setter | `CheckboxFrame::SetChecked`, `CheckboxFrame::SetValue`, `SliderFrame::SetValue` | Meaning varies by control family: bool set for checkboxes, numeric set for sliders |
| `0x57` | local state query | `CheckboxFrame::SetChecked`, `CheckboxFrame::SetValue` | Used to fetch current checkbox state before writing |
| `0x59` | local prepare / candidate select / parameter set | `TabsFrame::ChooseTab`, `EditableTextFrame::SetMaxLength`, `ProgressBar::SetMax` | Shared numeric setter/prepare opcode across unrelated controls |
| `0x5A` | local read-only setter | `EditableTextFrame::SetReadOnly` | Text-entry specific in current evidence |
| `0x5B` | local single-line label setter | `ButtonFrame::SetLabel`, `TextLabelFrame::SetLabel` | Shared by button and text-label controls |
| `0x5C` | local finalize tab selection | `TabsFrame::ChooseTab` | Paired with `0x59` for tabs |
| `0x5D` | local editable-text value setter | `EditableTextFrame::SetValue` | Followed by parent `0x31` |
| `0x60` | local dropdown index set | `DropdownFrame::SelectIndex` | Followed by parent `0x31` |
| `0x61` | local multiline-label setter | `MultiLineTextLabelFrame::SetLabel` | Distinct from single-line `0x5B` |
| `0x7FFFFFF5` | scrollable page/context change | `ScrollableFrame::SetPage` | Negative-signed special-case opcode, unlike the small positive control-local ids |

### What the matrix suggests

The observed opcodes naturally fall into several behavioral buckets:

`Input opcodes`
- `0x20`, `0x22`

`Local query/set opcodes`
- `0x56`, `0x57`, `0x59`, `0x5A`, `0x5B`, `0x5C`, `0x5D`, `0x60`, `0x61`

`Parent-facing action/commit opcode`
- `0x31`

`Special-case container/navigation opcode`
- `0x7FFFFFF5`

This is a much stronger model than "every frame action is just a click":

- some controls are entirely local to their own frame
- some query current state before writing
- some update locally and then escalate a structured `0x31` packet to the parent
- some container controls use outlier opcodes that likely route through a different internal path

### Reused opcodes are not globally uniform

One subtle but important point is that the same opcode can be reused by different control families with different semantics.

Examples:

- `0x56` means "set checkbox checked state" for checkbox controls, but "set numeric value" for slider controls
- `0x59` means "choose/prepare tab candidate" for tabs, but also appears as a general numeric setter for things like progress-bar max and editable-text max length

So the correct mental model is not:

- `opcode -> one universal meaning`

It is closer to:

- `opcode + control family/context -> concrete meaning`

That explains why binary reversing has to keep following both the caller type and the message id together.

### Current best protocol taxonomy

From the recovered bodies so far, the cleanest taxonomy is:

`Local-only setters`
- `SetLabel` on single-line controls via `0x5B`
- `SetLabel` on multiline controls via `0x61`
- `SetReadOnly` via `0x5A`
- `SetMax` / `SetMaxLength` via `0x59`

`Local query-then-set`
- checkbox family via `0x57` then `0x56`

`Local-then-parent-commit`
- button click via `0x31`
- dropdown select via `0x60` then `0x31`
- editable text via `0x5D` then `0x31`
- slider value via `0x56` then `0x31`

`Local two-step finalize`
- tabs via `0x59` then `0x5C`

`Container/page control`
- scrollables via `0x7FFFFFF5`

This is the most compact accurate summary of the frame protocol we have recovered so far.

## Next Semantic Pass: Reused IDs And Special Cases

The next decompilation batch sharpened two important points:

1. reused frame ids like `0x56`, `0x57`, `0x59`, and decimal `99` / `100` are definitely family-dependent rather than globally uniform
2. some container/layout controls use outlier opcodes that do not fit the smaller positive setter/query pattern

### `GW::TabsFrame::DisableTab`

Binary body:

```cpp
bool __thiscall GW::TabsFrame::DisableTab(TabsFrame *this,uint param_1)
{
  bool bVar1;

  bVar1 = UI::SendFrameUIMessage((Frame *)this,0x56,(void *)param_1,(void *)0x0);
  return bVar1;
}
```

This is a very useful correction to the earlier mental model:

- `0x56` is not just "checkbox/slider set value"
- for tabs, `0x56` is used as a disable operation keyed by tab index

### `GW::TabsFrame::EnableTab`

Binary body:

```cpp
bool __thiscall GW::TabsFrame::EnableTab(TabsFrame *this,uint param_1)
{
  bool bVar1;

  bVar1 = UI::SendFrameUIMessage((Frame *)this,0x57,(void *)param_1,(void *)0x0);
  return bVar1;
}
```

This pairs neatly with `DisableTab`:

- `0x56` = disable tab
- `0x57` = enable tab

So even the `0x56`/`0x57` pair is not a universal query/set pair. In the checkbox family it behaves like query/set; in the tabs family it behaves like disable/enable.

### `GW::ProgressBar::SetColorId`

Binary body:

```cpp
bool __thiscall GW::ProgressBar::SetColorId(ProgressBar *this,uint param_1)
{
  bool bVar1;

  if (param_1 < 7) {
    bVar1 = UI::SendFrameUIMessage
                      ((Frame *)(-(uint)(this != (ProgressBar *)0x0) & (uint)(this + 4)),100,
                       (void *)param_1,(void *)0x0);
    if (bVar1) {
      return true;
    }
  }
  return false;
}
```

New information here:

- decimal `100` / hex `0x64` is a progress-bar color/style-family opcode
- GWCA validates the domain first: only color ids `< 7` are accepted

That suggests some higher opcodes are specialized visual/style mutators rather than generic state setters.

### `GW::ProgressBar::SetStyle`

Binary body:

```cpp
bool __thiscall GW::ProgressBar::SetStyle(ProgressBar *this,ProgressBarStyle param_1)
{
  bool bVar1;

  if ((uint)param_1 < 4) {
    bVar1 = UI::SendFrameUIMessage
                      ((Frame *)(-(uint)(this != (ProgressBar *)0x0) & (uint)(this + 4)),99,
                       (void *)param_1,(void *)0x0);
    if (bVar1) {
      return true;
    }
  }
  return false;
}
```

This gives us another concrete visual opcode:

- decimal `99` / hex `0x63` = progress-bar style setter

Combined with `SetColorId`, this makes a small visual-control subfamily:

- `0x63` style
- `0x64` color id

### `GW::ScrollableFrame::SetSortHandler`

Binary body:

```cpp
bool __thiscall
GW::ScrollableFrame::SetSortHandler(ScrollableFrame *this,_func_int_uint_uint *param_1)
{
  bool bVar1;

  bVar1 = UI::SendFrameUIMessage((Frame *)this,99,param_1,(void *)0x0);
  return bVar1;
}
```

This is one of the best examples of why the matrix must stay family-aware:

- progress bars use `99` / `0x63` for style
- scrollable frames use `99` / `0x63` for sort-handler installation

So even visual-style-looking ids are not globally semantic. They are only meaningful when combined with the control type.

### `GW::ProgressBar::SetMax`

We already saw:

```cpp
SendFrameUIMessage(progressbar_frame, 0x59, max_value, 0)
```

and now we can interpret it more carefully:

- `0x59` is not just "tab candidate prepare"
- it is also a generic numeric configuration opcode used by some controls

Current known uses of `0x59`:

- tabs: selection-prep stage
- editable text: max length
- progress bar: max value

That makes `0x59` one of the strongest examples of a context-sensitive setter family.

### `GW::EditableTextFrame::SetReadOnly`

Binary body:

```cpp
bool __thiscall GW::EditableTextFrame::SetReadOnly(EditableTextFrame *this,bool param_1)
{
  bool bVar1;

  bVar1 = UI::SendFrameUIMessage((Frame *)this,0x5a,(void *)(uint)param_1,(void *)0x0);
  return bVar1;
}
```

This confirms `0x5A` as a clean local boolean setter in the editable-text family.

Unlike text value changes:

- `SetReadOnly` stays local
- `SetValue` does local `0x5D` then parent `0x31`

So even within the same control family, some mutations are purely local while others trigger parent notification.

### `GW::ScrollableFrame::SetPage`

We already recovered:

```cpp
UI::SendFrameUIMessage((Frame *)this,0x7ffffff5,param_1,(void *)0x0);
pFVar1 = UI::GetChildFrame((Frame *)this,0);
pFVar1 = UI::GetChildFrame(pFVar1,0);
return pFVar1;
```

This is still the strongest special-case outlier.

What it suggests:

- some container/page-management operations use signed-special message ids rather than the common small positive control ids
- after sending the page-change message, GWCA immediately re-enters the frame tree and resolves the new active child page frame

So `0x7FFFFFF5` looks less like a normal field setter and more like a control-internal navigation or rebind event.

### Matrix corrections after this pass

The earlier matrix was directionally right, but this pass sharpens the caveat:

- `0x56` can mean checkbox-set, slider local-set, or tab-disable
- `0x57` can mean checkbox-query or tab-enable
- `0x59` can mean tab-prepare, text max-length set, or progress-bar max set
- `0x63` can mean progress-bar style set or scrollable sort-handler set

So the safest form of every claim is:

- `message id + owning control class + call pattern = meaning`

not:

- `message id alone = meaning`

### Best current inference

The frame protocol appears to be organized less like a global enum of unique verbs and more like a small shared dispatcher namespace where:

- control families reuse nearby ids for their own local methods
- some ids are conventionally associated with common shapes like query/set or local-then-parent-commit
- but the caller type remains essential for interpreting the payload correctly

That is a much more realistic reversing model for GW's frame system than assuming each id has one universal meaning across the whole UI.

## Family Traces: Tabs, Scrollables, And Additional Query Shapes

This pass was aimed at going deeper one family at a time instead of only adding more one-off setters. The biggest improvement is that the tab and scrollable families now each have an identifiable local opcode neighborhood.

### Tab family: a fuller local opcode neighborhood

With the newer decompiles combined, the tab-family message set now looks like this:

- `0x56` = disable tab
- `0x57` = enable tab
- `0x58` = query current tab index
- `0x59` = prepare/select candidate tab
- `0x5B` = query enabled state for a specific tab
- `0x5C` = finalize tab selection

That is the first family where we can see a reasonably dense local protocol instead of just one setter and one getter.

### `GW::TabsFrame::GetCurrentTabIndex`

Binary body:

```cpp
bool __thiscall GW::TabsFrame::GetCurrentTabIndex(TabsFrame *this,uint *param_1)
{
  bool bVar1;

  bVar1 = UI::SendFrameUIMessage((Frame *)this,0x58,(void *)0x0,param_1);
  return bVar1;
}
```

This is important because it gives `0x58` a strong local-query role in the tab family:

- `wParam = 0`
- `lParam = out_current_tab_index`

So the tab family now has a clear read path alongside the write paths.

### `GW::TabsFrame::GetIsTabEnabled`

Binary body:

```cpp
bool __thiscall GW::TabsFrame::GetIsTabEnabled(TabsFrame *this,uint param_1,uint *param_2)
{
  bool bVar1;

  bVar1 = UI::SendFrameUIMessage((Frame *)this,0x5b,(void *)param_1,param_2);
  return bVar1;
}
```

This is another strong example of why message ids cannot be interpreted globally:

- `0x5B` is a label setter in button/text-label controls
- `0x5B` is an enabled-state query in the tab family

So `0x5B` is not "label" in the abstract. It is only "label set" in one control family and "tab enabled query" in another.

### `GW::TabsFrame::RemoveTab`

Binary body:

```cpp
bool __thiscall GW::TabsFrame::RemoveTab(TabsFrame *this,uint param_1)
{
  bool bVar1;
  Frame *pFVar2;
  Frame *pFVar3;

  pFVar2 = UI::GetChildFrame((Frame *)this,param_1);
  if (pFVar2 != (Frame *)0x0) {
    pFVar3 = UI::GetChildFrame((Frame *)this,*(uint *)(pFVar2 + 0xb8));
    if (pFVar3 == pFVar2) {
      pFVar3 = UI::GetChildFrame((Frame *)this,~*(uint *)(pFVar2 + 0xb8));
      goto LAB_10017504;
    }
  }
  pFVar3 = (Frame *)0x0;
LAB_10017504:
  bVar1 = UI::DestroyUIComponent(pFVar2);
  if (bVar1) {
    bVar1 = UI::DestroyUIComponent(pFVar3);
    if (bVar1) {
      return true;
    }
  }
  return false;
}
```

This is structurally valuable even though it is not a `SendFrameUIMessage` wrapper:

- removing a tab is not a local opcode send in GWCA's wrapper layer
- it resolves the tab frame and its paired companion button
- then destroys both UI components directly

That reinforces the earlier paired-child model:

- tabs and tab buttons are companion controls
- the relationship is tracked through `child_offset_id` and its bitwise complement

### Scrollable family: first coherent opcode neighborhood

The scrollable family now has at least these locally observed operations:

- `0x55` = clear items
- `0x56` = add item
- `0x63` = set sort handler
- `0x7FFFFFF5` = set/change page context

That is enough to say scrollables have their own small control-local protocol rather than borrowing only generic shared setters.

### `GW::ScrollableFrame::AddItem`

Binary body:

```cpp
bool __thiscall
GW::ScrollableFrame::AddItem
          (ScrollableFrame *this,uint param_1,uint param_2,
          _func_void_InteractionMessage_ptr_void_ptr_void_ptr *param_3)
{
  bool bVar1;
  uint local_18;
  uint local_14;
  _func_void_InteractionMessage_ptr_void_ptr_void_ptr *local_10;
  undefined4 *local_c;
  undefined4 local_8;

  local_18 = param_1;
  local_14 = param_2;
  local_10 = param_3;
  local_c = &local_8;
  local_8 = 0;
  bVar1 = UI::SendFrameUIMessage((Frame *)this,0x56,&local_18,local_c);
  return bVar1;
}
```

This is one of the strongest payload-shape findings in the scrollable family:

- `0x56` is used with a structured local packet
- the packet includes two `uint` fields and an interaction callback pointer
- `lParam` is an out/status pointer

So scrollable `0x56` is not remotely the same payload contract as checkbox `0x56` or slider `0x56`.

That gives us a direct example where:

- same message id
- totally different payload shape
- totally different owning control family

### `GW::ScrollableFrame::ClearItems`

Binary body:

```cpp
bool __thiscall GW::ScrollableFrame::ClearItems(ScrollableFrame *this)
{
  bool bVar1;

  bVar1 = UI::SendFrameUIMessage((Frame *)this,0x55,(void *)0x0,(void *)0x0);
  return bVar1;
}
```

This makes `0x55` the first clearly identified scrollable-item-list management opcode.

Paired with `AddItem`, the scrollable control now has a visible local item-list protocol:

- `0x55` clear list
- `0x56` add item

### `GW::ScrollableFrame::GetPage`

Binary body:

```cpp
Frame * __thiscall GW::ScrollableFrame::GetPage(ScrollableFrame *this)
{
  Frame *pFVar1;

  pFVar1 = UI::GetChildFrame((Frame *)this,0);
  pFVar1 = UI::GetChildFrame(pFVar1,0);
  return pFVar1;
}
```

This clarifies the `SetPage` story:

- `SetPage(0x7FFFFFF5, context)` changes the scrollable's page/container state
- `GetPage()` then resolves the active page frame by descending through child `0`, then child `0` again

So the page model is not abstract metadata only. It corresponds to a concrete nested frame subtree.

### New implications for `0x31`

This pass did not reveal any additional `0x31` builders beyond the ones already recovered:

- button action
- dropdown selection
- editable text value set
- slider value set

That absence is informative too:

- not every control family escalates to parent `0x31`
- tabs and several scrollable operations appear to stay local
- `0x31` currently looks concentrated in controls whose local state change also needs a parent-level interaction/commit signal

So the best current interpretation is:

- `0x31` is a parent-notification/commit pattern used by some interactive/value-bearing controls
- but it is not a universal second step across the whole frame system

### Outstanding unresolved helpers

Two functions remain unresolved because of Ghidra memory pressure / project-lock issues:

- `AddTab`
- `SetFrameMargins`

They are still worth revisiting, but they are no longer blocking the broader model:

- the tab family already has a meaningful opcode neighborhood
- the scrollable family already has a meaningful opcode neighborhood
- the major remaining unknown is whether those helpers introduce new local ids or just package existing ones

### Updated family view after this pass

`Tabs`
- local read/write family centered around `0x56`, `0x57`, `0x58`, `0x59`, `0x5B`, `0x5C`
- paired tab/button structure confirmed by direct remove logic

`Scrollables`
- local item/page/sort family centered around `0x55`, `0x56`, `0x63`, `0x7FFFFFF5`
- active page resolves as a concrete nested child-frame path

`Parent-commit controls`
- still primarily button, dropdown, editable text, and slider via `0x31`

This is the strongest control-family decomposition recovered so far.

## Reader Expansion And Payload Comparisons

The next pass filled in several read/query helpers that make the family models much less speculative.

### `GW::TabsFrame::GetCurrentTab`

Binary body:

```cpp
Frame * __thiscall GW::TabsFrame::GetCurrentTab(TabsFrame *this)
{
  bool bVar1;
  Frame *pFVar2;
  uint local_c;
  void *local_8;

  local_8 = (void *)0x0;
  bVar1 = UI::SendFrameUIMessage((Frame *)this,0x58,(void *)0x0,&local_8);
  if (bVar1) {
    local_c = 0;
    bVar1 = UI::SendFrameUIMessage((Frame *)this,0x59,local_8,&local_c);
    if (bVar1) {
      pFVar2 = UI::GetFrameById(local_c);
      return pFVar2;
    }
  }
  return (Frame *)0x0;
}
```

This is a very strong clarification of the tab family:

- `0x58` does not just return a plain integer tab index
- it returns an intermediate tab identity token/pointer-like value into `local_8`
- `0x59` can then translate that identity into a frame id

So in the tab family:

- `0x58` = query current selection handle
- `0x59` = resolve selection handle or candidate into frame id / selection target

That makes `0x59` even more clearly a family-scoped "resolve/prepare" opcode rather than one universal setter.

### `GW::TabsFrame::GetTabFrameId`

Binary body:

```cpp
bool __thiscall GW::TabsFrame::GetTabFrameId(TabsFrame *this,uint param_1,uint *param_2)
{
  bool bVar1;

  bVar1 = UI::SendFrameUIMessage((Frame *)this,0x59,(void *)param_1,param_2);
  return bVar1;
}
```

This matches what `GetCurrentTab()` suggested:

- `0x59` is a tab-local resolver opcode
- given a tab identity/index-like input, it can return the frame id through `lParam`

So the tab family now has a much cleaner internal picture:

- `0x58` = ask for current tab selection token
- `0x59` = resolve token/index into frame id or candidate selection
- `0x5C` = finalize selection

### `GW::ScrollableFrame::GetItems`

Binary body:

```cpp
uint __thiscall GW::ScrollableFrame::GetItems(ScrollableFrame *this,uint *param_1,uint param_2)
{
  uint *puVar1;
  bool bVar2;
  Frame *pFVar3;
  uint *puVar4;
  uint uVar5;
  undefined4 local_1c;
  undefined4 local_18;
  uint **local_14;
  undefined4 local_10;
  ScrollableFrame *local_c;
  uint *local_8;

  local_1c = 2;
  uVar5 = 0;
  local_14 = &local_8;
  local_18 = 0;
  local_10 = 0;
  local_8 = (uint *)0x0;
  local_c = this;
  bVar2 = UI::SendFrameUIMessage((Frame *)this,0x58,&local_1c,(void *)0x0);
  puVar1 = param_1;
  if ((bVar2) && (puVar4 = local_8, local_8 != (uint *)0x0)) {
    do {
      pFVar3 = UI::GetFrameById((uint)puVar4);
      if (pFVar3 == (Frame *)0x0) {
        return uVar5;
      }
      local_18 = *(undefined4 *)(pFVar3 + 0xb8);
      local_14 = &param_1;
      local_1c = 0;
      local_10 = 0;
      param_1 = (uint *)0x0;
      bVar2 = UI::SendFrameUIMessage((Frame *)local_c,0x58,&local_1c,(void *)0x0);
      puVar4 = param_1;
      if (!bVar2) {
        puVar4 = (uint *)0x0;
      }
      if ((*(uint *)(pFVar3 + 0x18c) >> 9 & 1) == 0) {
        if ((puVar1 != (uint *)0x0) && (uVar5 < param_2)) {
          puVar1[uVar5] = *(uint *)(pFVar3 + 0xb8);
        }
        uVar5 = uVar5 + 1;
      }
    } while (puVar4 != (uint *)0x0);
    return uVar5;
  }
  return 0;
}
```

This is probably the strongest scrollable-family reader we have recovered so far.

It shows:

- scrollable `0x58` is an iterator-style enumeration opcode, not a simple scalar query
- the caller passes a small local control block into `wParam`
- repeated calls to `0x58` walk item frames one-by-one
- GWCA filters hidden items using the frame-state bits before returning child ids to the caller

So scrollable `0x58` is radically different from tab `0x58`, even though the opcode number matches.

### `GW::ScrollableFrame::GetSortHandler`

Binary body:

```cpp
_func_int_uint_uint * __thiscall GW::ScrollableFrame::GetSortHandler(ScrollableFrame *this)
{
  Frame *pFVar1;
  void *pvVar2;

  pFVar1 = UI::GetChildFrame((Frame *)this,0);
  pFVar1 = UI::GetChildFrame(pFVar1,0);
  pvVar2 = UI::GetFrameContext(pFVar1);
  if (pvVar2 != (void *)0x0) {
    return *(_func_int_uint_uint **)((int)pvVar2 + 0xc);
  }
  return (_func_int_uint_uint *)0x0;
}
```

This is useful precisely because it does **not** use `SendFrameUIMessage`.

Combined with `SetSortHandler(0x63, fn)`, it suggests:

- setting the sort handler goes through a local frame opcode
- reading the sort handler is just direct frame-context access on the active page subtree

So not every getter has a matching query opcode. Some reads bypass the message bus entirely and inspect the frame context directly.

### Side-by-side comparison: same opcode, different contracts

At this point we can make the payload mismatch story much more explicit.

#### `0x56`

Observed uses:

- checkbox family: local bool set
- slider family: local numeric value set
- tabs family: disable tab by index
- scrollable family: add item with structured payload

Observed payload shapes:

- checkbox: `wParam = bool-ish scalar`, `lParam = 0`
- slider: `wParam = numeric value`, `lParam = 0`
- tabs: `wParam = tab index`, `lParam = 0`
- scrollables: `wParam = {id_a, id_b, callback_ptr, out_ptr}`, `lParam = out/status`

So `0x56` is now conclusively a dispatcher slot reused by multiple families with incompatible packet layouts.

#### `0x57`

Observed uses:

- checkbox family: query current checked state
- tabs family: enable tab by index

Observed payload shapes:

- checkbox: `wParam = 0`, `lParam = out_current_state`
- tabs: `wParam = tab index`, `lParam = 0`

Again, same opcode number, very different contract.

#### `0x58`

Observed uses:

- tabs family: query current selected tab handle/token
- scrollable family: enumerate item frames using a structured iterator block

Observed payload shapes:

- tabs: `wParam = 0`, `lParam = out_selection_token`
- scrollables: `wParam = iterator/control block`, `lParam = 0`

This is one of the clearest pieces of evidence that family context matters as much as the numeric id.

#### `0x59`

Observed uses:

- tabs family: resolve selection/candidate into frame id
- editable text family: set max length
- progress bar family: set max value

Observed payload shapes:

- tabs: `wParam = token/index`, `lParam = out_frame_id`
- editable text: `wParam = max_length`, `lParam = 0`
- progress bar: `wParam = max_value`, `lParam = 0`

So even when an opcode looks "setter-like" in one family, it may behave as a resolver in another.

#### `0x5B`

Observed uses:

- button/text label family: set single-line label text
- tabs family: query whether a tab is enabled

Observed payload shapes:

- label setter: `wParam = wchar_t*`, `lParam = 0`
- tab query: `wParam = tab index`, `lParam = out_enabled_state`

This is probably the most intuitive proof that a flat global enum interpretation is wrong.

### Best updated model

After this pass, the most accurate description of GWCA's frame-message layer is:

- there is one shared numeric dispatch namespace
- control families reuse the same opcode numbers for different local verbs
- payload shape and caller class are both mandatory to interpret a message correctly
- some reads/writes go through `SendFrameUIMessage`
- some reads bypass the message bus and inspect frame context directly

That is a significantly stronger and more precise model than treating the recovered ids as globally unique operations.

## Completing The Remaining Gaps: `AddTab` And `SetFrameMargins`

The two persistent unresolved helpers finally decompiled cleanly once I reused the analyzed projects with `-process` rather than re-importing every run.

### `GW::TabsFrame::AddTab`

Binary body:

```cpp
Frame * __thiscall
GW::TabsFrame::AddTab
          (TabsFrame *this,wchar_t *param_1,uint param_2,uint param_3,
          _func_void_InteractionMessage_ptr_void_ptr_void_ptr *param_4,void *param_5)
{
  Frame *pFVar1;
  wchar_t *local_18;
  uint local_14;
  uint local_10;
  _func_void_InteractionMessage_ptr_void_ptr_void_ptr *local_c;
  void *local_8;

  local_18 = param_1;
  local_14 = param_2;
  local_10 = param_3;
  local_c = param_4;
  local_8 = param_5;
  param_1 = (wchar_t *)0x0;
  UI::SendFrameUIMessage((Frame *)this,0x55,&local_18,&param_1);
  pFVar1 = UI::GetFrameById((uint)param_1);
  return pFVar1;
}
```

This is a very valuable closure for the tab family:

- `0x55` is the add-tab opcode in the tab family
- it takes a structured payload containing:
  - label pointer
  - two numeric fields
  - interaction callback pointer
  - user/context pointer
- `lParam` is used as an out parameter for the new frame id

So the tab family now includes:

- `0x55` add tab
- `0x56` disable tab
- `0x57` enable tab
- `0x58` query current selection handle/index path
- `0x59` resolve tab identity / candidate into frame id
- `0x5B` query enabled state
- `0x5C` finalize selection

That is now the most complete family-local opcode neighborhood recovered so far.

### `GW::UI::SetFrameMargins`

Binary body:

```cpp
bool __cdecl
GW::UI::SetFrameMargins(Frame *param_1,uint param_2,float *param_3,float *param_4,uint param_5)
{
  if ((param_1 != (Frame *)0x0) && (DAT_1008a030 != (code *)0x0)) {
    (*DAT_1008a030)(*(undefined4 *)(param_1 + 0xbc),param_2,param_3,param_4,param_5);
    return true;
  }
  return false;
}
```

This one changes the model in a meaningful way:

- `SetFrameMargins` does **not** use `SendFrameUIMessage`
- instead it calls a separate internal function directly
- the call is keyed by `frame_id = *(frame + 0xBC)`

So not all UI-manipulation helpers in GWCA belong to the frame-message bus.

That gives us at least three distinct actuation styles inside GWCA's UI layer:

1. global `SendUIMessage(...)`
2. frame-local `SendFrameUIMessage(...)`
3. direct internal helper calls keyed by frame identity

This is an important correction because it prevents overfitting the whole UI layer to the frame-message abstraction alone.

## Normalized Recovery Table

To cap this pass, here is a normalized summary of the most useful recovered operations.

| Family | Helper | Msg / Mechanism | `wParam` shape | `lParam` shape | Follow-up behavior |
|---|---|---|---|---|---|
| Button | `Click` | `0x31` via `MouseAction(6)` then `MouseAction(7)` | structured action packet | `0` | parent-facing commit/action |
| Button | `DoubleClick` | `0x31` via `MouseAction(9)` | structured action packet | `0` | single parent-facing double-click action |
| Dropdown | `SelectIndex` | `0x60` then `0x31` | index, then structured commit packet | `0` | local update then parent notify |
| EditableText | `SetValue` | `0x5D` then `0x31` | `wchar_t*`, then structured commit packet | `0` | local update then parent notify |
| Slider | `SetValue` | `0x56` then `0x31` | numeric value, then structured commit packet | `0` | local update then parent notify |
| Checkbox | `SetChecked` | `0x57` then `0x56` | `0`, then bool-ish scalar | out current state, then `0` | query-then-set |
| Tabs | `AddTab` | `0x55` | structured add-tab packet | out frame id | local creation, returns created frame |
| Tabs | `DisableTab` | `0x56` | tab index | `0` | local tab-state change |
| Tabs | `EnableTab` | `0x57` | tab index | `0` | local tab-state change |
| Tabs | `GetCurrentTabIndex` | `0x58` | `0` | out current index/token | query |
| Tabs | `GetCurrentTab` | `0x58` then `0x59` | `0`, then selection token | out token, then out frame id | query then resolve |
| Tabs | `GetTabFrameId` | `0x59` | tab identity/index | out frame id | resolve |
| Tabs | `GetIsTabEnabled` | `0x5B` | tab index | out enabled state | query |
| Tabs | `ChooseTab` | `0x59` then `0x5C` | index or child id | out status, then `0` | prepare/resolve then finalize |
| Tabs | `RemoveTab` | direct component destruction | child id lookups | n/a | destroys tab frame plus paired button |
| Scrollable | `ClearItems` | `0x55` | `0` | `0` | local list clear |
| Scrollable | `AddItem` | `0x56` | structured item packet | out/status | local list append |
| Scrollable | `GetItems` | repeated `0x58` | iterator/control block | `0` | enumerates frames, filters hidden entries |
| Scrollable | `SetSortHandler` | `0x63` | callback pointer | `0` | local sort behavior update |
| Scrollable | `GetSortHandler` | direct frame-context read | n/a | n/a | bypasses message bus |
| Scrollable | `SetPage` | `0x7FFFFFF5` | page/context ptr | `0` | local page change, then child-tree resolution |
| Scrollable | `GetPage` | child-frame traversal | n/a | n/a | resolves active page frame directly |
| Label/Text | `SetLabel` | `0x5B` or `0x61` | `wchar_t*` | `0` | local text update |
| EditableText | `SetReadOnly` | `0x5A` | bool-ish scalar | `0` | local flag update |
| ProgressBar | `SetMax` | `0x59` | numeric max | `0` | local numeric config |
| ProgressBar | `SetStyle` | `0x63` | style enum | `0` | local style update |
| ProgressBar | `SetColorId` | `0x64` | color id | `0` | local visual update |
| Layout/UI | `SetFrameMargins` | direct internal helper call | frame id, margin args | n/a | bypasses `SendFrameUIMessage` |

### What this table resolves

This normalized view makes the architecture much harder to misunderstand:

- the same opcode can absolutely have unrelated meanings in different families
- some families are mostly message-driven
- some helpers are hybrid read/write systems
- some operations bypass the message bus entirely

So the strongest accurate claim now is:

- GWCA exposes a mixed UI control surface composed of global UI messages, frame-local dispatch, direct context reads, and direct internal helper calls

That is the best binary-backed description of how the UI layer really behaves at this point in the research.

## Field Atlas: Recurring Packet And Argument Layouts

This appendix normalizes the recurring packet shapes that keep showing up across the recovered UI helpers. The goal is not to claim perfect semantic names for every field, but to make future reversing faster by showing the stable layouts we can already recognize.

### Atlas A: Parent-facing `0x31` commit packets

The `0x31` family is the most important recurring parent-commit shape in the frame layer. So far, every recovered `0x31` sender builds a small local packet on the stack and then calls:

```cpp
SendFrameUIMessage(parent_frame, 0x31, &packet, 0);
```

What changes across controls is the packet payload.

#### A1. Button / MouseAction packet

Recovered from `ButtonFrame::MouseAction`:

```cpp
struct MouseActionPacket {
    uint32_t frame_id;          // this + 0xBC
    uint32_t child_offset_id;   // this + 0xB8
    uint32_t action_state;      // method argument: 6, 7, 9, etc.
    uint32_t unk0;              // zero
    uint32_t* unk1_ptr;         // local/self-referential pointer
    uint32_t unk1;              // zero
    uint32_t button_payload;    // this + 0x1C4
    uint32_t unk2;              // zero
};
```

Stable traits:

- names both the frame and child identity
- always carries an action-state field
- includes the `+0x1C4` button payload field
- uses a local pointer field inside the packet

#### A2. Dropdown commit packet

Recovered from `DropdownFrame::SelectIndex`:

```cpp
struct DropdownCommitPacket {
    uint32_t field_0;           // this + 0xC0
    uint32_t field_4;           // this + 0xBC
    uint32_t action_or_state;   // constant 7
    uint32_t selected_index;    // method argument
    uint32_t unk0;              // zero
};
```

Stable traits:

- no self-pointer field like the button packet
- carries a hardcoded `7`
- final payload field is the selected index
- dispatched to the parent after local `0x60`

#### A3. Editable text commit packet

Recovered from `EditableTextFrame::SetValue`:

```cpp
struct EditableTextCommitPacket {
    uint32_t frame_id;          // this + 0xBC
    uint32_t child_offset_id;   // this + 0xB8
    uint32_t action_or_state;   // constant 7
    wchar_t* text_value;        // new text
    uint32_t unk0;              // zero
};
```

Stable traits:

- same general five-field footprint as dropdown/slider commits
- field 3 is now a string pointer rather than an integer
- dispatched after local `0x5D`

#### A4. Slider commit packet

Recovered from `SliderFrame::SetValue`:

```cpp
struct SliderCommitPacket {
    uint32_t field_0;           // this + 0xC0
    uint32_t field_4;           // this + 0xBC
    uint32_t action_or_state;   // constant 7
    uint32_t slider_value;      // requested value
    uint32_t unk0;              // zero
};
```

Stable traits:

- extremely close to the dropdown packet
- field 3 is numeric value instead of selected index
- dispatched after local `0x56`

#### A5. Best shared interpretation for `0x31`

Across these four packet builders, the strongest common pattern is:

```cpp
struct ParentCommitLike {
    uint32_t identity_a;
    uint32_t identity_b;
    uint32_t action_or_state;
    payload_t payload;
    uint32_t trailing_zero_or_support;
    // optional extra support fields in button variant
};
```

So the parent-facing `0x31` channel appears to be a family of related commit packets, not one single universal struct.

### Atlas B: Local create/list-management packets

#### B1. Tab add packet for `0x55`

Recovered from `TabsFrame::AddTab`:

```cpp
struct AddTabPacket {
    wchar_t* label;
    uint32_t field_4;
    uint32_t field_8;
    UIInteractionCallback callback;
    void* user_context;
};
```

Dispatch pattern:

```cpp
SendFrameUIMessage(tabs_frame, 0x55, &AddTabPacket, &out_frame_id);
```

Stable traits:

- structured create packet
- returns new frame id via `lParam`
- strongly suggests `0x55` is a creator opcode in the tab family

#### B2. Scrollable add-item packet for `0x56`

Recovered from `ScrollableFrame::AddItem`:

```cpp
struct AddItemPacket {
    uint32_t field_0;
    uint32_t field_4;
    UIInteractionCallback callback;
    uint32_t* out_ptr;
};
```

Dispatch pattern:

```cpp
SendFrameUIMessage(scrollable_frame, 0x56, &AddItemPacket, &out_status);
```

Stable traits:

- also a structured create/append packet
- callback-bearing like `AddTab`
- same opcode number as several unrelated local setters in other families

### Atlas C: Iterator/query control blocks

#### C1. Scrollable enumeration block for `0x58`

Recovered from `ScrollableFrame::GetItems`:

First call:

```cpp
struct ScrollEnumBlock {
    uint32_t mode;              // starts as 2
    uint32_t current_child_id;  // updated per iteration
    uint32_t** out_next;        // points to local cursor storage
    uint32_t unk0;              // zero
    ScrollableFrame* owner;     // captured frame
    uint32_t* cursor;           // receives next frame id/ptr
};
```

Repeated call shape:

- first call seeds enumeration with `mode = 2`
- subsequent calls mutate fields and continue iteration
- returned cursor is converted through `GetFrameById`

Stable traits:

- this is a stateful iterator/control block, not a simple out parameter
- proves `0x58` can represent a mini protocol all by itself inside a family

#### C2. Tab current-selection query path

Recovered from `GetCurrentTab` / `GetCurrentTabIndex`:

```cpp
SendFrameUIMessage(tabs_frame, 0x58, 0, &out_selection_token);
SendFrameUIMessage(tabs_frame, 0x59, out_selection_token, &out_frame_id);
```

Stable traits:

- `0x58` returns a token/handle-like result
- `0x59` resolves it
- much simpler than the scrollable iterator block despite reusing neighboring ids

### Atlas D: Scalar setter/query contracts

These are the simplest recurring shapes and the easiest to recognize.

#### D1. Checkbox query/set

Query:

```cpp
SendFrameUIMessage(checkbox_frame, 0x57, 0, &out_checked);
```

Set:

```cpp
SendFrameUIMessage(checkbox_frame, 0x56, desired_bool, 0);
```

#### D2. Tab enable/disable

Disable:

```cpp
SendFrameUIMessage(tabs_frame, 0x56, tab_index, 0);
```

Enable:

```cpp
SendFrameUIMessage(tabs_frame, 0x57, tab_index, 0);
```

#### D3. Tab current-index query

```cpp
SendFrameUIMessage(tabs_frame, 0x58, 0, &out_index_or_token);
```

#### D4. Editable text local setters

Read-only:

```cpp
SendFrameUIMessage(edit_frame, 0x5A, read_only_bool, 0);
```

Value:

```cpp
SendFrameUIMessage(edit_frame, 0x5D, text_ptr, 0);
```

Max length:

```cpp
SendFrameUIMessage(edit_frame, 0x59, max_length, 0);
```

#### D5. Label setters

Single-line:

```cpp
SendFrameUIMessage(label_or_button_frame, 0x5B, text_ptr, 0);
```

Multiline:

```cpp
SendFrameUIMessage(multiline_label_frame, 0x61, text_ptr, 0);
```

#### D6. Progress bar setters

Max:

```cpp
SendFrameUIMessage(progress_frame, 0x59, max_value, 0);
```

Style:

```cpp
SendFrameUIMessage(progress_frame, 0x63, style_enum, 0);
```

Color:

```cpp
SendFrameUIMessage(progress_frame, 0x64, color_id, 0);
```

### Atlas E: Special container/layout operations

#### E1. Scrollable page change

Recovered from `ScrollableFrame::SetPage`:

```cpp
SendFrameUIMessage(scrollable_frame, 0x7FFFFFF5, page_context_ptr, 0);
```

Traits:

- outlier signed-special opcode
- not a plain field setter
- followed by child-tree resolution of the active page

#### E2. Direct layout helper: frame margins

Recovered from `UI::SetFrameMargins`:

```cpp
InternalSetFrameMargins(
    frame->frame_id,
    margin_mode_or_flags,
    from_vec_or_margin_a,
    to_vec_or_margin_b,
    extra_flags_or_axis
);
```

Traits:

- bypasses `SendFrameUIMessage`
- uses frame id rather than frame pointer directly
- likely belongs to a separate layout/manipulation subsystem

### Practical recognition rules

When encountering a new GWCA/UI helper in this region of the binary, the fastest way to classify it now is:

1. check whether it calls `SendUIMessage`, `SendFrameUIMessage`, or a direct helper pointer
2. if it calls `SendFrameUIMessage`, check whether `wParam` is:
   - a scalar
   - a small local packet
   - an iterator/control block
   - a string pointer
3. check whether there is a second-stage parent `0x31` dispatch
4. use the owning control class to interpret reused opcode numbers

That workflow should make future passes much faster and much less ambiguous.

---

## Next Disassembly Pass: Dropdown Option And Query Helpers

I went back into the same frame-control cluster and decompiled the next useful dropdown/list readers and writers. This pass matters because it expands the dropdown family beyond "select by index" and shows another round of family-scoped opcode reuse.

The most important takeaways are:

1. dropdown option management is not just `SelectIndex(0x60)` plus parent `0x31`
2. dropdowns have a local option/value sub-protocol involving `0x56`
3. selected-value queries introduce another local opcode, `0x66`
4. some dropdown state reads bypass `SendFrameUIMessage` entirely and come straight from frame context

### `GW::DropdownFrame::AddOption`

Binary-confirmed body:

```cpp
bool __thiscall GW::DropdownFrame::AddOption(DropdownFrame *this,wchar_t *param_1,uint param_2)
{
  wchar_t *local_c;
  uint local_8;

  local_c = param_1;
  local_8 = param_2;
  if (param_1 != (wchar_t *)0x0) {
    return UI::SendFrameUIMessage((Frame *)(this + 4),0x56,&local_c,0);
  }
  return false;
}
```

Recovered meaning:

- dropdown `0x56` is not a generic scalar setter here
- it takes a small local packet containing:
  - option label pointer
  - option value/id
- it targets the dropdown's internal frame object (`this + 4` in this build), not the outer wrapper directly

So `0x56` now has at least these family meanings:

- checkbox set
- slider local numeric set
- tabs disable
- scrollable add-item
- dropdown add-option

That is exactly why global opcode-only interpretation is unsafe.

### `GW::DropdownFrame::GetOptions`

Binary-confirmed body:

```cpp
std::vector<uint32_t> __thiscall GW::DropdownFrame::GetOptions(DropdownFrame *this)
{
    // simplified
    ctx = UI::GetFrameContext((Frame *)(this + 4));
    for (i = 0; i < ctx->size; i++) {
        if (has_value_mapping) {
            out.push_back(option_array[i].value_at_plus_8);
        } else {
            out.push_back(i);
        }
    }
}
```

Important structural findings:

- dropdown option entries are stored as `0x20`-byte records
- `GetOptions()` scans the option records and detects whether a value-mapping mode is active
- when mapping is active, the exported option value comes from `record + 0x8`
- otherwise the exported value degenerates to the positional index

This is a strong clue that GWCA is modeling dropdowns as "index plus optional user value," not just raw ordered labels.

### `GW::DropdownFrame::GetOptionIndex`

Binary-confirmed behavior:

- calls `GetOptions()` first
- if mapping is active, scans the exported option-value array for the requested value and returns the matching positional index
- if mapping is not active, treats the supplied value as an index directly if it is in range

So:

- `GetOptionIndex(value)` is really "value-to-index resolution"
- it only behaves like identity when the dropdown has no explicit value mapping

That matches the earlier `SelectOption()` result nicely:

- `SelectOption(value)` is not a separate transport
- it is higher-level value resolution that eventually funnels into `SelectIndex(index)`

### `GW::DropdownFrame::GetOptionValue`

Binary-confirmed behavior:

- calls `GetOptions()` first
- if mapping is active, returns the stored mapped value for the requested index
- otherwise returns the index itself

So `GetOptionIndex()` and `GetOptionValue()` form an inverse pair around the dropdown's optional value-mapping layer.

That makes the dropdown family look much more like a small typed control model than a bag of unrelated helpers.

### `GW::DropdownFrame::GetSelectedIndex`

Binary-confirmed body:

```cpp
bool __thiscall GW::DropdownFrame::GetSelectedIndex(DropdownFrame *this,uint *param_1)
{
  void *ctx = UI::GetFrameContext((Frame *)(this + 4));
  if (ctx != 0) {
    *param_1 = *(uint *)((int)ctx + 0x58);
    return true;
  }
  return false;
}
```

Important point:

- this does **not** use `SendFrameUIMessage`
- the selected index is read directly from dropdown frame context at offset `+0x58`

This fits the broader pattern we have already seen:

- some writes go through local frame opcodes
- some reads use direct context inspection instead of the frame bus

### `GW::ScrollableFrame::GetSelectedValue`

Binary-confirmed body:

```cpp
bool __thiscall GW::ScrollableFrame::GetSelectedValue(ScrollableFrame *this,uint *param_1)
{
  uint64_t local = 0;
  bool ok = UI::SendFrameUIMessage((Frame *)this,0x66,0,&local);
  if ((ok) && ((uint32_t)local != 0)) {
    *param_1 = (uint32_t)(local >> 32);
    return true;
  }
  return false;
}
```

This is the first recovered use of local frame opcode `0x66`.

Interpretation:

- `0x66` appears to be a scrollable-family selected-value query
- the result is returned through an 8-byte output block
- low dword acts like a success/presence flag
- high dword carries the selected value

So not all frame queries are scalar `lParam` outs; some use compact structured return blocks.

### `GW::CheckboxFrame::IsChecked`

Binary-confirmed body:

```cpp
bool __thiscall GW::CheckboxFrame::IsChecked(CheckboxFrame *this)
{
  int out = 0;
  UI::SendFrameUIMessage((Frame *)(this + 4),0x57,0,&out);
  return out == 1;
}
```

This cleanly confirms the earlier checkbox interpretation:

- checkbox `0x57` is the local query/read opcode
- checkbox `0x56` is the corresponding setter

### `GW::EditableTextFrame::IsReadOnly`

Binary-confirmed body:

```cpp
bool __thiscall GW::EditableTextFrame::IsReadOnly(EditableTextFrame *this)
{
  undefined4 scratch;
  UI::SendFrameUIMessage((Frame *)this,0x55,(void *)((int)&scratch + 3),0);
  return (bool)((uint8_t *)&scratch)[3];
}
```

This is one of the stranger helpers recovered so far.

Important points:

- editable-text `IsReadOnly()` does **not** query via `lParam`
- it passes a pointer into the high byte of a local dword as `wParam`
- the callee writes a one-byte read-only result there

That means opcode `0x55` now has still another family-specific meaning:

- scrollable clear-items
- tabs add-tab
- editable-text read-only query

Again, numeric opcode alone tells us almost nothing without control family and payload shape.

### What this pass changes in the overall model

This pass strengthens several broad conclusions:

- dropdowns have an internal option/value model, not just a selected index
- frame-bus reads are heterogeneous:
  - direct context reads
  - scalar out-params
  - structured out-blocks
  - weird byte-targeted pointer writes
- reused ids like `0x55`, `0x56`, and `0x57` are even more family-scoped than they already looked

Newly observed meanings:

- `0x55`
  - scrollable clear-items
  - tabs add-tab
  - editable-text read-only query
- `0x56`
  - checkbox set
  - slider local value set
  - tabs disable
  - scrollable add-item
  - dropdown add-option
- `0x57`
  - checkbox query
  - tabs enable
- `0x66`
  - scrollable selected-value query

### Best current reading of dropdown-family architecture

At this point the dropdown family looks like this:

- option creation: local `0x56`
- option lookup/value mapping: direct context-backed helper logic
- select by index: local `0x60`, then parent `0x31`
- select by value: helper resolution, then `SelectIndex`
- current index read: direct context read

That is much richer than "dropdown = one opcode plus click commit."

---

## Next Disassembly Pass: Traversal, Value Getters, And Label Decoding

I continued in the same address cluster and decompiled another set of getter-oriented helpers. This pass makes the read side of the frame protocol much more concrete.

The biggest additions are:

- scrollable child traversal is now clearly a `0x58` control-block family
- several `GetValue()` helpers now pin specific query semantics to specific families
- label frames store encoded and decoded strings back-to-back in context memory rather than via a fresh decode call at getter time

### `GW::DropdownFrame::GetCount`

Binary-confirmed body:

```cpp
bool __thiscall GW::DropdownFrame::GetCount(DropdownFrame *this,uint *param_1)
{
  void *ctx = UI::GetFrameContext((Frame *)(this + 4));
  if (ctx != 0) {
    *param_1 = *(uint *)((int)ctx + 8);
    return true;
  }
  return false;
}
```

This is a direct context read.

Important point:

- dropdown option count lives in frame context at `+0x8`
- no `SendFrameUIMessage` is involved

So dropdown query state is now split across:

- direct context reads for count and current selection
- helper logic for value mapping
- local frame opcodes for mutation/select flows

### `GW::ScrollableFrame::GetCount`

Binary-confirmed body:

```cpp
bool __thiscall GW::ScrollableFrame::GetCount(ScrollableFrame *this,uint *param_1)
{
  *param_1 = GetItems(this,0,0);
  return param_1 != 0;
}
```

This is subtle but useful:

- scrollable count is not stored as one obvious scalar in this helper
- GWCA computes it by reusing the enumerator path from `GetItems()`

So the scrollable family behaves more like an iterable container than a plain "count field plus items field" object.

### `GW::ScrollableFrame::GetFirstChildFrameId`
### `GW::ScrollableFrame::GetLastChildFrameId`
### `GW::ScrollableFrame::GetNextChildFrameId`
### `GW::ScrollableFrame::GetPrevChildFrameId`

These four helpers all use the same local `0x58` machinery with different mode selectors.

Recovered patterns:

```cpp
// first
mode = 2;
SendFrameUIMessage(scrollable, 0x58, &control_block, 0);

// last
mode = 3;
SendFrameUIMessage(scrollable, 0x58, &control_block, 0);

// next
mode = 0;
current = frame_id;
SendFrameUIMessage(scrollable, 0x58, &control_block, 0);

// prev
mode = 1;
current = frame_id;
SendFrameUIMessage(scrollable, 0x58, &control_block, 0);
```

This is strong confirmation that scrollable `0x58` is not a single "get items" opcode. It is a small iterator/traversal protocol with mode-driven behavior:

- `0` = next
- `1` = previous
- `2` = first
- `3` = last

Combined with the already recovered `GetItems()`, the best current reading is:

- `0x58` on scrollables is a generic child/item traversal service
- higher-level helpers are just specialized wrappers over that shared control block

### `GW::ButtonFrame::GetLabel`

Binary-confirmed body:

```cpp
bool __thiscall GW::ButtonFrame::GetLabel(ButtonFrame *this,wchar_t **param_1)
{
  void *ctx = UI::GetFrameContext((Frame *)this);
  if ((ctx != 0) && (*(int *)((int)ctx + 0xc) != 0)) {
    *param_1 = *(wchar_t **)((int)ctx + 4);
    return true;
  }
  return false;
}
```

This is another direct context read:

- string pointer at `+0x4`
- related length or span field at `+0xC`

So button labels are not fetched through a message opcode in the getter path.

### Encoded vs decoded label layout

I decompiled:

- `MultiLineTextLabelFrame::GetEncodedLabel`
- `TextLabelFrame::GetEncodedLabel`
- `MultiLineTextLabelFrame::GetDecodedLabel`
- `TextLabelFrame::GetDecodedLabel`

The pattern is consistent:

- encoded label getter returns the base string pointer from frame context
- decoded label getter walks that first null-terminated string, then returns the next string immediately after it, if still within the stored length/span

Simplified model:

```cpp
encoded = ctx->string_base;
decoded = encoded + wcslen(encoded) + 1;
```

with bounds checks against the stored span/count.

This strongly suggests the frame context stores the encoded and decoded forms contiguously in one buffer rather than allocating a separate decoded-string field.

That is a useful structural result because it tells us:

- label getters are mostly memory interpreters
- they are not actively decoding on demand
- the decode step likely happened earlier in the UI pipeline

### `GW::DropdownFrame::HasValueMapping`

Binary-confirmed body:

```cpp
bool __thiscall GW::DropdownFrame::HasValueMapping(DropdownFrame *this)
{
  ctx = UI::GetFrameContext((Frame *)(this + 4));
  for (entry in ctx->entries_0x20) {
    if (entry.value_at_plus_8 != 0) return true;
  }
  return false;
}
```

This confirms the inference from `GetOptions()`:

- value mapping is not a separate top-level flag
- GWCA infers it by scanning the option records and checking the mapped-value field at `+0x8`

So "has value mapping" is an emergent property of the option records themselves.

### `GetValue()` family decompilation

I decompiled six different `GetValue()` helpers in this cluster, and they are a great demonstration that the same method name hides very different low-level mechanisms.

#### `CheckboxFrame::GetValue()`

```cpp
SendFrameUIMessage((Frame *)(this + 4), 0x57, 0, &out);
return out == 1;
```

Meaning:

- checkbox `GetValue()` is just the checkbox query opcode wrapped as a boolean-returning convenience method

#### `DropdownFrame::GetValue()`

```cpp
ctx = UI::GetFrameContext((Frame *)(this + 4));
selected_index = *(uint *)(ctx + 0x58);
GetOptionValue(this, selected_index, &out_value);
return out_value;
```

Meaning:

- dropdown `GetValue()` is not a direct bus query
- it is a two-step composition:
  - read selected index from context
  - resolve selected index through the optional value-mapping layer

That is one of the clearest examples in the whole file of a high-level helper sitting above both raw context state and another helper protocol.

#### `EditableTextFrame::GetValue()`

```cpp
ctx = UI::GetFrameContext((Frame *)this);
return *(wchar_t **)((int)ctx + 0x48);
```

Meaning:

- editable text current value is a direct context pointer at `+0x48`

#### `ProgressBar::GetValue()`

```cpp
uint out = 0;
SendFrameUIMessage((Frame *)(this + 4), 0x55, 0, &out);
return out;
```

Meaning:

- progress-bar `0x55` is a local numeric-value query

That adds another family-specific meaning for `0x55`.

#### `SliderFrame::GetValue(uint *out)`

```cpp
return SendFrameUIMessage((Frame *)(this + 4), 0x57, out, 0);
```

#### `SliderFrame::GetValue()`

```cpp
uint out = 0;
ok = SendFrameUIMessage((Frame *)(this + 4), 0x57, &out, 0);
return ok ? out : 0;
```

Meaning:

- slider `0x57` is the local numeric-value query
- this differs from checkbox `0x57`, even though the numeric opcode is the same

So `0x57` now has at least:

- checkbox state query
- slider value query
- tabs enable

which again proves the opcode namespace is class-scoped, not globally semantic.

### What this pass changes in the overall model

This pass sharpens the getter side of the architecture:

- some families compute derived values from direct context plus helper logic
- some families query through `SendFrameUIMessage`
- some families reuse iterator/traversal control blocks instead of exposing dedicated one-off getters
- the same method names like `GetValue()` hide completely different transport patterns

Newly strengthened opcode interpretations:

- `0x55`
  - scrollable clear-items
  - tabs add-tab
  - editable-text read-only query
  - progress-bar numeric-value query
- `0x57`
  - checkbox state query
  - slider numeric-value query
  - tabs enable
- `0x58`
  - scrollable traversal/enumeration service
  - tabs current-selection query
- `0x66`
  - scrollable selected-value query

### Best current reading of the read/query side

At this point the frame-side read model looks like a mix of:

- direct context reads
- family-scoped frame query opcodes
- iterator control blocks
- wrapper helpers that compose multiple lower-level steps

That is a much more accurate picture than treating getters as a single uniform API style.

---

## Next Disassembly Pass: Creation Helpers And Scanner Bootstrapping

I pushed deeper into the same cluster again, this time focusing on the `Create(...)` helpers and the shared initializer they all depend on.

This pass is important because it shows that GWCA's frame-creation surface is not just a thin exported wrapper. It lazily discovers the underlying game-side creation callbacks by scanning the client, caches those addresses, and then routes later creation calls through `UI::CreateUIComponent(...)`.

### `GW::ButtonFrame::Create`

Binary-confirmed body:

```cpp
ButtonFrame * __cdecl
GW::ButtonFrame::Create(uint parent_id,uint style,uint child_offset,wchar_t *arg4,wchar_t *arg5)
{
  FUN_100163c0();
  if (DAT_1008a020 == 0) return 0;

  parent = UI::GetFrameById(parent_id);
  if (parent != 0) {
    while (UI::GetChildFrame(parent, child_offset) != 0) {
      child_offset++;
    }
    new_id = UI::CreateUIComponent(parent_id, style, child_offset, DAT_1008a020, arg4, arg5);
    if (new_id != 0) return (ButtonFrame *)UI::GetFrameById(new_id);
  }
  return 0;
}
```

Important findings:

- creation is lazy-initialized through `FUN_100163c0()`
- GWCA checks whether the requested child-offset slot is already occupied
- if it is occupied, GWCA linearly increments until it finds a free child offset
- actual instantiation then goes through `UI::CreateUIComponent(...)`

So frame creation is not a blind "use exactly the child offset I asked for" operation. GWCA treats the requested child offset as a starting point and auto-resolves collisions.

### `GW::CheckboxFrame::Create`

Binary-confirmed body:

```cpp
CheckboxFrame * __cdecl
GW::CheckboxFrame::Create(uint parent_id,uint style,uint child_offset,wchar_t *arg4,wchar_t *arg5)
{
  p = ButtonFrame::Create(parent_id, style | 0x8000, child_offset, arg4, arg5);
  return p ? (CheckboxFrame *)(p - 4) : 0;
}
```

This shows checkbox creation is not its own totally separate path:

- it reuses button creation
- it forces a style bit `0x8000`
- it then adjusts the returned pointer to the checkbox-specific wrapper layout

That is a strong hint that button and checkbox frames are closely related in the in-memory class layout.

### `GW::ScrollableFrame::Create`

Binary-confirmed body:

```cpp
ScrollableFrame * __cdecl
GW::ScrollableFrame::Create
          (uint parent_id,uint style,uint child_offset,ScrollablePageContext *page_ctx,wchar_t *arg5)
{
  FUN_100163c0();
  ctor = DAT_1008a028;
  default_page_factory = DAT_1008a02c;

  if (page_ctx == 0) page_ctx = &default_local_page_ctx;
  if (*(int *)(page_ctx + 4) == 0) return 0;
  if (ctor == 0) return 0;

  parent = UI::GetFrameById(parent_id);
  if (parent != 0) {
    while (UI::GetChildFrame(parent, child_offset) != 0) {
      child_offset++;
    }
    new_id = UI::CreateUIComponent(parent_id, style | 0x20000, child_offset, ctor, page_ctx, arg5);
    if (new_id != 0) return (ScrollableFrame *)UI::GetFrameById(new_id);
  }
  return 0;
}
```

Important findings:

- scrollables force style bit `0x20000`
- scrollable creation depends on a valid `ScrollablePageContext`
- if no explicit page context is supplied, GWCA falls back to a default scanned helper/context
- the same child-offset collision handling is reused here too

So scrollables are not plain components with one constructor signature; they need page-context wiring as part of creation.

### `GW::TextLabelFrame::Create`

Binary-confirmed body:

```cpp
TextLabelFrame * __cdecl
GW::TextLabelFrame::Create(uint parent_id,uint style,uint child_offset,wchar_t *arg4,wchar_t *arg5)
{
  FUN_100163c0();
  if (DAT_1008a024 == 0) return 0;

  parent = UI::GetFrameById(parent_id);
  if (parent != 0) {
    while (UI::GetChildFrame(parent, child_offset) != 0) {
      child_offset++;
    }
    new_id = UI::CreateUIComponent(parent_id, style, child_offset, DAT_1008a024, arg4, arg5);
    if (new_id != 0) return (TextLabelFrame *)UI::GetFrameById(new_id);
  }
  return 0;
}
```

This matches the general creation template:

- lazy scan/init
- find free child offset
- create component
- map returned frame id back to a typed pointer

### `GW::ScrollableFrame::GetItemFrameId`

Binary-confirmed body:

```cpp
uint __thiscall GW::ScrollableFrame::GetItemFrameId(ScrollableFrame *this,uint item_id)
{
  uint out = 0;
  UI::SendFrameUIMessage((Frame *)this,0x5A,(void *)item_id,&out);
  return out;
}
```

This adds another scrollable-family query helper:

- scrollable `0x5A` maps an item id or item-like key to a concrete frame id

That gives the scrollable family another class-scoped meaning for an opcode already used elsewhere:

- editable-text `0x5A` = set read-only
- scrollable `0x5A` = get item frame id

Again, the numeric id only becomes meaningful once you know the owning control family.

### `GW::TabsFrame::GetTabByLabel`

Binary-confirmed behavior:

- iterates candidate tab indices by repeatedly calling `SendFrameUIMessage(this, 0x59, token, &out_frame_id)`
- maps returned frame id to a real frame via `GetFrameById`
- resolves the paired button/label frame using the complemented child-offset trick
- reads that label from frame context
- compares it directly to the requested label
- stops after at most 10 iterations in this build

This is useful because it shows tab lookup-by-label is:

- not a dedicated indexed map lookup
- not a hash lookup
- but an iterative resolver layered on top of the same `0x59` tab-family machinery we already recovered

So `0x59` continues to look like a family-specific resolver primitive, not just a setter.

### The shared initializer: `FUN_100163c0`

This is the most important function in this pass.

Binary-confirmed body, simplified:

```cpp
void FUN_100163c0(void)
{
  if (!initialized) {
    initialized = true;

    DAT_1008a020 = ToFunctionStart(FindAssertion("UiCtlBtn.cpp", "!s_btnCheckImageList"));
    DAT_1008a024 = ToFunctionStart(FindAssertion("CtlText.cpp", "FrameTestStyles(hdr.frameId, CTLTEXT_STYLE_MODEL)"));
    DAT_1008a028 = *(uint *)Find(pattern_for_scrollable_ctor);
    DAT_1008a02c = ToFunctionStart(FindAssertion("CtlFrameList.cpp", "No valid case for switch variable 'msg.relation'"));
    DAT_1008a030 = ToFunctionStart(FindAssertion("FrApi.cpp", "params->inputMask < FrameMarginsParams::INPUT_ILLEGAL_BIT_FIRST"));

    if (DAT_1008a030 != 0) {
      GW::Hook::CreateHook(&DAT_1008a030, FUN_10017490, &DAT_1008a034);
      GW::Hook::EnableHooks(DAT_1008a030);
    }
  }
}
```

This tells us a lot about GWCA's UI bootstrapping:

- GWCA lazily scans the game for class-specific UI creation helpers
- some are found by assertion-string anchored scans
- at least one (`DAT_1008a028`) is found by a raw pattern scan
- the frame-margin helper is also scanned and then hooked

That means the frame-creation layer is not static import binding. It is runtime scanner bootstrapping.

### What this pass changes in the overall model

This pass sharpens the architecture on the creation side:

- GWCA frame creation is scanner-driven and lazy-initialized
- typed frame `Create(...)` helpers are mostly adapters over:
  - class-specific scanned ctor callbacks
  - `UI::CreateUIComponent(...)`
  - free-child-offset selection
- some controls inject forced style bits during creation
- tab and scrollable lookup helpers continue to reuse the family-local opcode machinery rather than bypassing it

This is one of the clearest places where GWCA stops looking like a plain wrapper and starts looking like a runtime-discovered UI abstraction layer over the game.

### Side note on two neighboring unknown functions

I also decompiled two nearby unnamed functions:

- `FUN_10016110`
- `FUN_100162a0`

They appear to be generic formatting/runtime helpers rather than UI/frame control functions, so I am not treating them as part of the GWCA UI protocol story.

---

## Next Disassembly Pass: Hook Plumbing Under The UI Layer

I went one level deeper underneath the helper surface and decompiled the callback trampoline that `FUN_100163c0()` installs on the internal frame-margins helper.

This pass is less about new opcodes and more about GWCA's mediation strategy: which internal game helpers it simply calls, which ones it hooks, and what those hooks actually do.

### `FUN_10017490`: the frame-margins hook target

Binary-confirmed body:

```cpp
void __cdecl
FUN_10017490(arg1,arg2,arg3,arg4,arg5)
{
  GW::Hook::EnterHook();
  (*DAT_1008a034)(arg1,arg2,arg3,arg4,arg5);
  GW::Hook::LeaveHook();
}
```

This is a very thin trampoline.

What it tells us:

- `DAT_1008a034` is the saved original frame-margins helper
- the hook does not transform parameters in the recovered binary
- it simply brackets the original call with `EnterHook()` / `LeaveHook()`

That means the value of this hook is not parameter rewriting in the visible wrapper body. The value is:

- callback altitude / hook-state management
- reentrancy bookkeeping
- creating a stable interception point GWCA can sit on

This matches the broader pattern we already saw in `SendUIMessage`-style mediation: some GWCA hooks are semantically rich, while others are intentionally minimal and exist mainly to bring the call into GWCA's hook lifecycle.

### What this confirms about `SetFrameMargins`

Earlier binary work already showed:

```cpp
GW::UI::SetFrameMargins(frame, mode, a, b, extra)
    -> InternalSetFrameMargins(frame_id, mode, a, b, extra)
```

This new decompile adds the missing infrastructure detail:

- the internal helper is scanner-found in `FUN_100163c0()`
- GWCA then hooks it with `FUN_10017490`
- later `UI::SetFrameMargins()` calls the hooked helper entrypoint, not a separate public bus

So the complete path is now:

`SetFrameMargins wrapper -> scanned helper pointer -> thin GWCA hook trampoline -> original game helper`

That makes `SetFrameMargins` a clean example of a fourth interaction style beyond the ones already cataloged:

- global `SendUIMessage`
- local `SendFrameUIMessage`
- direct context reads
- direct helper call through a GWCA-managed hook trampoline

### Source cross-check: `CreateUIComponent` hook path

The checked-in GWCA source in `Source/UIMgr.cpp` shows a parallel design for component creation:

```cpp
typedef uint32_t(__cdecl* CreateUIComponent_pt)(
    uint32_t frame_id,
    uint32_t component_flags,
    uint32_t tab_index,
    void* event_callback,
    wchar_t* name_enc,
    wchar_t* component_label);

uint32_t __cdecl OnCreateUIComponent(...) {
    GW::Hook::EnterHook();
    UI::CreateUIComponentPacket packet = {...};
    for (auto& it : OnCreateUIComponent_callbacks) {
        it.second(&packet);
    }
    uint32_t out = CreateUIComponent_Ret(...packet...);
    GW::Hook::LeaveHook();
    return out;
}
```

And the source scan/bootstrap path does:

```cpp
CreateUIComponent_Func = Scanner::Find(...pattern...);
HookBase::CreateHook(CreateUIComponent_Func, OnCreateUIComponent, &CreateUIComponent_Ret);
```

Important note:

- I did **not** recover a named `CreateUIComponent` wrapper symbol directly from the `gwca.dll` function table in this pass
- so this specific creation-hook story is source-proven, not binary-proven in the same direct way as `FUN_10017490`

But it lines up extremely well with the creation helpers we already decompiled:

- `ButtonFrame::Create`
- `ScrollableFrame::Create`
- `TextLabelFrame::Create`

Those helpers all route through `UI::CreateUIComponent(...)`, which is exactly the hook point the checked-in source says GWCA instruments.

### Source cross-check: `CreateUIComponentPacket`

The public header in `Include/GWCA/Managers/UIMgr.h` defines:

```cpp
struct CreateUIComponentPacket {
    uint32_t frame_id;
    uint32_t component_flags;
    uint32_t tab_index;
    void* event_callback;
    wchar_t* name_enc;
    wchar_t* component_label;
};
```

That lines up tightly with the binary `Create(...)` helpers:

- parent frame id
- style/flags
- child/tab slot
- class-specific event callback
- encoded name/label payloads

So even where we do not yet have the binary wrapper body named as `CreateUIComponent`, the source and the recovered call sites reinforce each other very strongly.

### What this pass changes in the overall model

This pass sharpens the "how GWCA mediates internal helpers" story:

- some internal helpers are exposed through rich message-level mediation
- some are wrapped in very thin hook trampolines
- creation and layout manipulation are both scanner-bootstrapped
- GWCA's hook system is not only for gameplay/network-like actions; it also sits underneath UI construction and layout operations

That moves the model one level deeper than "GWCA sends messages."

At this point, the UI stack is better understood as:

- discovered helper addresses
- GWCA hook/trampoline layer
- exported helper wrappers
- control-family protocol helpers
- high-level bot/UI abstractions

---

## Next Disassembly Pass: Exported Creation/Destruction And Callback Registries

I kept pushing deeper and recovered the late-stage exported helpers that sit underneath the source-level hook API:

- `GW::UI::CreateUIComponent`
- `GW::UI::DestroyUIComponent`
- `GW::UI::RegisterCreateUIComponentCallback`
- `GW::UI::RemoveCreateUIComponentCallback`
- `GW::UI::RegisterUIMessageCallback`
- `GW::UI::RemoveUIMessageCallback`
- top-level `GW::EnableHooks`
- top-level `GW::DisableHooks`

This pass is important because it closes the loop between:

- source-declared callback registration APIs
- binary-confirmed exported entrypoints
- the internal scanner/hook bootstrapping we already recovered

### `GW::UI::CreateUIComponent`

Binary-confirmed body:

```cpp
uint __cdecl
GW::UI::CreateUIComponent(
    uint frame_id,
    uint component_flags,
    uint tab_index,
    UIInteractionCallback event_callback,
    void *name_or_ctor_arg,
    wchar_t *component_label)
{
    if (DAT_1008a384 != 0) {
        return (*DAT_1008a384)();
    }
    return 0;
}
```

Even though Ghidra could not fully reconstruct the indirect-call prototype in this build, the important part is clear:

- the exported GWCA wrapper exists in the compiled binary
- it dispatches indirectly through a cached function pointer
- it is not an inline direct call to a fixed address

This lines up very well with the source model where `CreateUIComponent_Func` is scanner-found and then hooked.

So we can now say more confidently:

- source says GWCA hooks `CreateUIComponent_Func`
- binary confirms GWCA exports a `CreateUIComponent` wrapper that forwards through an indirect cached pointer

### `GW::UI::DestroyUIComponent`

Binary-confirmed body:

```cpp
bool __cdecl GW::UI::DestroyUIComponent(Frame *frame)
{
  if ((frame != 0) && (DAT_1008a390 != 0)) {
    (*DAT_1008a390)(*(uint *)(frame + 0xBC));
    return true;
  }
  return false;
}
```

Important findings:

- teardown is frame-id driven, not frame-pointer driven
- the wrapper extracts `frame->frame_id` from `+0xBC`
- actual destruction is delegated through another cached internal helper pointer

This mirrors the `SetFrameMargins` pattern:

- exported GWCA wrapper
- frame-id extraction when needed
- indirect call through a scanner-populated helper pointer

So creation and destruction are both mediated, but in slightly different styles:

- creation forwards multiple constructor-style arguments
- destruction reduces to a frame id and calls the internal helper

### `GW::UI::RegisterCreateUIComponentCallback`

Binary-confirmed behavior:

- first calls `RemoveCreateUIComponentCallback(entry)` to avoid duplicate registration
- walks the existing callback storage until it finds the insertion point by altitude
- copies/clones the incoming `std::function`
- inserts a record containing:
  - altitude
  - hook entry
  - callback functor
- calls a helper at the end (`FUN_100251d0((uint)param_1)`)

This is important because it means create-component callbacks are not just stored in an unordered bag in the compiled build. The binary registration logic preserves ordering by altitude, just like the UI-message callback system.

That is a stronger result than the current checked-in source snippet alone suggests.

### `GW::UI::RemoveCreateUIComponentCallback`

Binary-confirmed behavior:

- linearly scans the callback storage for a matching `HookEntry*`
- if found, shifts later callback records down over it
- destroys/releases any `std::function` state as needed
- shrinks the logical end pointer by one callback slot (`0xC` dwords per entry in this build)

So the create-component callback list is:

- ordered
- compacted in-place on removal
- holding callable state that must be explicitly released

### `GW::UI::RegisterUIMessageCallback`

Binary-confirmed behavior:

- removes any existing callback for the same `HookEntry*` and `UIMessage`
- looks up the per-message callback bucket through a hashed container
- creates the bucket if it does not already exist
- walks the callback list for that message until altitude ordering says "insert here"
- clones the callback functor
- inserts the new `(altitude, entry, callback)` record

This is a very strong binary confirmation of the callback ordering model the source describes.

So the UI-message callback plane is not just "many listeners":

- listeners are grouped per `UIMessage`
- insertion is altitude-ordered
- callback state is stored as cloned callable objects

### `GW::UI::RemoveUIMessageCallback`

Binary-confirmed behavior:

- if `message_id == 0`, it recursively removes the entry from every registered message bucket
- otherwise, it locates that message bucket through the hashed container
- scans the bucket for a matching `HookEntry*`
- compacts the remaining callback entries downward on removal
- releases any stored callback state as needed

That first point is especially useful:

- `RemoveUIMessageCallback(entry, 0)` behaves like "remove this hook entry from all UI messages"

So the binary confirms a wildcard-like removal mode for UI message callbacks.

### Top-level `GW::EnableHooks` / `GW::DisableHooks`

Binary-confirmed bodies:

```cpp
void __cdecl GW::EnableHooks(void)
{
  if (DAT_1008a090 == '\0') return;
  Hook::EnableHooks(0);
  for (module in module_list) {
    if (module->on_enable) module->on_enable();
  }
  MemoryPatcher::EnableHooks();
}

void __cdecl GW::DisableHooks(void)
{
  Hook::DisableHooks(0);
  for (module in module_list) {
    if (module->on_disable) module->on_disable();
  }
  MemoryPatcher::DisableHooks();
}
```

This gives us a nicer top-level model for GWCA startup/shutdown:

- there is a global hook layer
- modules get enable/disable callbacks
- memory patchers are toggled alongside the hook layer

That is helpful context for the UI module too, because it explains how the various scanner-discovered hook points get turned on as part of the larger GWCA lifecycle rather than one-off ad hoc calls.

### What this pass changes in the overall model

This pass finally connects the three layers cleanly:

1. source-declared API
2. binary-exported wrapper
3. scanner/hook-backed internal helper

We can now say with much more confidence that GWCA's UI layer is built from:

- exported helper wrappers like `CreateUIComponent`, `DestroyUIComponent`, `SetFrameMargins`
- callback registries with ordered insertion/removal semantics
- scanner-populated internal helper pointers
- hook lifecycle management at both module and global levels

That is substantially deeper than "GWCA wraps Guild Wars UI messages."

### Strongest new concrete takeaways

- `CreateUIComponent` is a real compiled GWCA export, not just source theory
- `DestroyUIComponent` is a real compiled GWCA export and destroys by `frame_id`
- create-component callbacks are altitude-ordered in the compiled build
- UI-message callbacks are bucketed by message and support wildcard removal via `message_id == 0`
- the UI subsystem sits inside a broader GWCA enable/disable lifecycle

---

## Next Disassembly Pass: Hook Engine Core Under `CreateHook`

I went one level deeper under the UI exports and decompiled the hook-engine core that `GW::Hook::CreateHook(...)` delegates to.

This matters because all of the UI mediation we have been mapping ultimately depends on this layer working correctly:

- `SendUIMessage` hooks
- `DoAction` hooks
- `CreateUIComponent` hooks
- the frame-margins trampoline

### `GW::Hook::CreateHook`

Binary-confirmed body:

```cpp
int __cdecl GW::Hook::CreateHook(void **target_ptr, void *detour, void **out_original)
{
  if ((target_ptr != 0) && (*target_ptr != 0)) {
    resolved = Scanner::FunctionFromNearCall((uint)*target_ptr, false);
    if (resolved != 0) {
      *target_ptr = resolved;
    }
    return FUN_10029730(*target_ptr, detour, out_original);
  }
  return -1;
}
```

Important findings:

- GWCA does not blindly trust the incoming target pointer
- it first tries to canonicalize near-call targets via `Scanner::FunctionFromNearCall(...)`
- only then does it invoke the real hook installer

That explains why the UI scanner/bootstrap layer can sometimes store a callsite-ish address and still end up with a proper hookable function start.

### `GW::Hook::EnterHook` / `GW::Hook::LeaveHook`

Binary-confirmed bodies:

```cpp
void EnterHook() {
  LOCK();
  DAT_1008a0e4++;
  UNLOCK();
}

void LeaveHook() {
  LOCK();
  DAT_1008a0e4--;
  UNLOCK();
}
```

These are simpler than they might look from the higher-level source:

- they are essentially a locked recursion-depth / active-hook-depth counter
- they do not do dispatch logic themselves
- their job is hook-state accounting, not callback fanout

That fits the thin frame-margins trampoline perfectly:

- enter hook
- call original
- leave hook

### `FUN_10029730`: the real hook installer

This is the most important result from this pass.

Binary-confirmed behavior, simplified:

```cpp
int FUN_10029730(target, detour, out_original)
{
    initialize_hook_subsystem();
    if (heap_handle == 0) return 2;

    if (!validate_address(target) || !validate_address(detour)) return 7;
    if (find_existing_hook(target) != 0xffffffff) return 3;

    hook_record = allocate_hook_record();
    if (!hook_record) return 9;

    if (!analyze_target_and_build_patch_plan(&plan)) {
        free_hook_record(hook_record);
        return 8;
    }

    ensure_global_hook_table_capacity();
    store_hook_record_in_global_table(plan, hook_record, target, detour);

    if (out_original) *out_original = hook_record;
    clear_hook_creation_lockflag();
    return 0;
}
```

Even with some helper names still unresolved, several strong points are clear:

- there is a dedicated hook heap / allocator
- both the original target and detour are validated before installation
- duplicate hooks on the same target are rejected
- the installer analyzes the target and builds a patch/trampoline plan before committing
- hook metadata is stored in a resizable global table
- the "original" pointer returned to callers is really a hook-record/trampoline pointer, not simply the untouched target function

### Hook-record layout clues

From the recovered stores into the global hook table, each entry appears to retain:

- target function pointer
- detour function pointer
- allocated hook/trampoline record
- flags describing patch mode / relocation mode
- several copied dwords that look like saved original bytes or trampoline metadata

The important practical point is:

- GWCA is not using a one-byte or one-struct toy patcher
- it maintains per-hook state rich enough to restore, disable, and probably relocate patched prologues safely

That is consistent with the higher-level enable/disable lifecycle we already recovered.

### What this means for UI reversing

This hook-engine pass changes the confidence level of the UI conclusions:

- UI helper interception is not accidental or ad hoc
- GWCA has a real internal detour framework with:
  - target normalization
  - validation
  - duplicate detection
  - per-hook records
  - global hook lifecycle control

So when we say GWCA "hooks `CreateUIComponent`" or "hooks the frame-margins helper," that now rests on more than source declarations. We have binary evidence for the machinery that makes those hooks possible.

### Best current layered model

At this point the full stack under GWCA's UI layer looks like:

1. scanner resolves internal game helpers or near-call sites
2. `Hook::CreateHook` canonicalizes targets
3. the internal hook installer (`FUN_10029730`) validates and allocates a hook/trampoline record
4. GWCA exported wrappers and callback registries expose the mediated surface
5. control-family helpers implement frame/message protocols
6. AutoIt / Py4GW / bot code consume the top of that stack

That is about as deep as we can get without turning this into a full hook-engine reverse-engineering project.

---

## Deepest Practical Path: Compiled `SendUIMessage` Dispatch

I followed the deepest useful path in the UI subsystem: the compiled `GW::UI::SendUIMessage(...)` export itself.

This is the highest-value single decompile in the entire UI-message story because it confirms, in the binary, how GWCA actually:

- looks up callback buckets
- runs pre-callbacks vs post-callbacks
- decides whether the raw game dispatcher still runs
- treats internal `0x30000000` GWCA messages differently from game-facing UI messages

### `GW::UI::SendUIMessage`

Binary-confirmed body:

```cpp
bool __cdecl GW::UI::SendUIMessage(UIMessage msgid, void *wParam, void *lParam)
{
    blocked = false;
    class_mask = msgid & 0x30000000;

    bucket = find_callback_bucket(msgid);
    if (bucket not found) {
        if (class_mask != 0x30000000) {
            (*DAT_1008a36c)(msgid, wParam, lParam);
        }
        return true;
    }

    // pre callbacks: altitude < 1
    for (cb in bucket while cb.altitude < 1) {
        cb.callback(&status_like_state, msgid, wParam, lParam);
        callback_count++;
    }

    if (!blocked) {
        if (class_mask != 0x30000000) {
            (*DAT_1008a36c)(msgid, wParam, lParam);
        }
        result = true;
    }

    if (msgid == 0x1000008c) {
        DAT_1008a420 = (Map::GetMapInfo(0)->flags & 0x40001) != 0;
    }

    // post callbacks: remaining entries
    for (remaining cb in bucket) {
        cb.callback(&status_like_state, msgid, wParam, lParam);
        callback_count++;
    }

    return result;
}
```

### What the compiled dispatch proves

This one body confirms several critical things at once.

#### 1. Callback buckets are really per-message

The binary first hashes/looks up the exact `UIMessage` in the same callback container we previously saw through `RegisterUIMessageCallback(...)`.

So the runtime path is:

- registration inserts ordered entries into the per-message bucket
- `SendUIMessage` looks up that exact message bucket at dispatch time

That makes the callback registry story fully closed from both ends now:

- insertion side
- removal side
- dispatch side

#### 2. Pre vs post callbacks are altitude-split in the binary

The compiled loop boundary is:

- run callbacks while `altitude < 1`
- then do the raw send
- then run the rest

That means the effective split is:

- altitude `<= 0`: pre-send callback phase
- altitude `>= 1`: post-send callback phase

This is the strongest binary confirmation yet that altitude is not just storage metadata. It directly controls dispatch phase.

#### 3. Internal GWCA `0x30000000` UI messages are never forwarded to the raw game UI dispatcher

The binary checks:

```cpp
if ((msgid & 0x30000000) == 0x30000000)
```

and skips the raw send when true.

That confirms the core design:

- `0x10000000` family messages can flow through to the game's real UI dispatcher
- `0x30000000` family messages are GWCA-internal relay/hook messages

This is not just a comment-level or source-level convention. It is hardcoded into the compiled dispatch path.

#### 4. Blocking happens before the raw send, not after

The raw send only happens when the local blocked flag stays false after the pre-callback phase.

So the dispatch contract is:

- pre callbacks may observe and block
- raw game/UI send happens only if not blocked
- post callbacks run afterward regardless of whether the raw send ran

That gives the exact behavioral meaning of GWCA UI-message hooks:

- pre phase = interception and veto point
- post phase = observation/cleanup point

#### 5. The callback state object in the binary is simpler-looking than the source abstraction

Ghidra does not reconstruct the full `HookStatus` type here, but the binary clearly keeps:

- a blocked byte/flag
- a callback count / altitude-like counter

So the source `HookStatus` abstraction is not imaginary ceremony. It is backed by real state threaded through the callback loop.

### Special-case side effect inside `SendUIMessage`

One interesting binary-only detail is this special case:

```cpp
if (msgid == 0x1000008c) {
    DAT_1008a420 = (Map::GetMapInfo(0)->flags & 0x40001) != 0;
}
```

Even without fully naming that message in this pass, the important point is:

- `SendUIMessage` is not a perfectly pure dispatcher
- it also updates at least one cached UI/map-related boolean based on a specific message id

So the exported dispatcher has a small amount of side-state maintenance beyond just "run callbacks then call original."

### `Keydown`, `Keyup`, and `Keypress` in the compiled build

I also decompiled the key helpers again in this pass to place them beside the `SendUIMessage` result.

#### `Keydown`

Binary-confirmed behavior:

```cpp
action = { key, 0, 0 };
if (frame == 0) {
    frame = resolve_default_input_frame();
}
return SendFrameUIMessage(frame, 0x20, &action, 0);
```

#### `Keyup`

Binary-confirmed behavior:

```cpp
action = { key, 0, 0 };
if (frame == 0) {
    frame = resolve_default_input_frame();
}
return SendFrameUIMessage(frame, 0x22, &action, 0);
```

#### `Keypress`

Binary-confirmed behavior:

```cpp
action = { key, 0, 0 };
if (frame == 0) {
    frame = resolve_default_input_frame();
}
ok = SendFrameUIMessage(frame, 0x20, &action, 0);
if (ok) GameThread::Enqueue();
```

This is interesting for two reasons:

1. these helpers are frame-plane input helpers, not `SendUIMessage` helpers
2. `Keypress` adds an extra queued step after the initial frame dispatch

So the input path and the global UI-message path are now even more clearly separate:

- input keys -> frame bus + action context + possible queued follow-up
- UI messages -> callback bucket + optional raw global dispatcher

### Best current end-to-end model for UI-message dispatch

At this point, the full compiled dispatch path is:

1. caller invokes `GW::UI::SendUIMessage(msgid, wParam, lParam)`
2. GWCA looks up the per-message callback bucket
3. callbacks with altitude `< 1` run first
4. if not blocked, and if message is not a GWCA-internal `0x30000000` command, GWCA calls the raw game UI dispatcher
5. message-specific side-state updates may run
6. callbacks with altitude `>= 1` run afterward
7. boolean result is returned

That is the deepest practical statement we can currently make about the UI-message layer without fully reversing the hashed callback container internals.

### Why this is the deepest useful path

This decompile ties together almost everything we mapped before:

- callback registration ordering
- hook/block semantics
- the `0x30000000` internal message family
- the distinction between global UI messages and frame messages
- the role of altitude as an actual execution-phase control

In other words, this is the point where the document stops being "a lot of correct local notes" and becomes a genuine compiled execution model.

---

## Adjacent To `SendUIMessage`: Direct Frame Helpers In The Same Neighborhood

I also decompiled the helpers immediately adjacent to `SendUIMessage` in the export range:

- `SetFrameDisabled`
- `SetFrameTitle`
- `SetFrameVisible`
- both `SetCommandLinePref` overloads

This pass is useful because it shows that even right next to the global dispatcher, GWCA still uses multiple control styles:

- direct helper calls by frame id
- state-bit checks plus internal toggles
- direct preference mutation

So the area around `SendUIMessage` is not a pure "all roads lead to the message bus" zone.

### `GW::UI::SetFrameDisabled`

Binary-confirmed body:

```cpp
bool __cdecl GW::UI::SetFrameDisabled(Frame *frame, bool disabled)
{
  if (frame == 0) return false;
  if (current_disabled_state(frame) != disabled) {
    if (DAT_1008a3a4 == 0) return false;
    (*DAT_1008a3a4)(0, 0, 0x10);
  }
  return true;
}
```

Important findings:

- the current disabled bit is read directly from `frame + 0x18C`
- if a change is needed, GWCA calls a cached internal helper with flag `0x10`
- this is not implemented through `SendUIMessage`

So "disable a frame" lives on yet another direct-helper path.

### `GW::UI::SetFrameVisible`

Binary-confirmed body:

```cpp
bool __cdecl GW::UI::SetFrameVisible(Frame *frame, bool visible)
{
  if (frame == 0) return false;
  if (current_visible_state(frame) != visible) {
    if (DAT_1008a3a4 == 0) return false;
    (*DAT_1008a3a4)(0, 0, 0x200);
  }
  return true;
}
```

Important findings:

- visibility is also derived from state bits at `frame + 0x18C`
- the same cached helper `DAT_1008a3a4` is reused
- visibility toggling uses flag `0x200`

So `SetFrameDisabled` and `SetFrameVisible` appear to be sibling wrappers over the same internal frame-state helper, differentiated mainly by the control flag they pass.

### `GW::UI::SetFrameTitle`

Binary-confirmed body:

```cpp
bool __cdecl GW::UI::SetFrameTitle(Frame *frame, wchar_t *title)
{
  if (frame && DAT_1008a380 && title && *title) {
    (*DAT_1008a380)(frame->frame_id, title);
    return true;
  }
  return false;
}
```

Important findings:

- title updates are frame-id driven
- the wrapper checks for a non-empty string
- actual work goes through another cached internal helper

Again, this is not a `SendUIMessage` path. It is a direct helper path beside the dispatcher.

### `GW::UI::SetCommandLinePref`

I decompiled both overloads:

```cpp
bool SetCommandLinePref(wchar_t *name, uint value)
{
    descriptor = lookup_pref_descriptor(name);
    if (descriptor && ((*descriptor & 0xff00) == 0x400)) {
        slot = lookup_pref_storage(name);
        if (slot) {
            *slot = value;
            return true;
        }
    }
    return false;
}

bool SetCommandLinePref(wchar_t *name, wchar_t *value)
{
    descriptor = lookup_pref_descriptor(name);
    if (descriptor && ((*descriptor & 0xff00) == 0x300)) {
        slot = lookup_pref_storage(name);
        if (slot) {
            copy_string(slot, value, 0x104);
            return true;
        }
    }
    return false;
}
```

Important findings:

- these are direct preference mutations, not UI-message sends
- the preference descriptor type gates whether numeric or string assignment is legal
- successful writes go straight to the preference storage slot

This is useful context because it shows the exported UI namespace includes a lot more than dispatchers. It also contains direct state mutators living alongside the messaging surface.

### What this changes in the model

This little neighborhood around `SendUIMessage` reinforces a now very strong conclusion:

- GWCA's `UI` namespace is not organized around a single universal transport
- even adjacent exports may use completely different mechanisms:
  - global UI-message dispatch
  - frame-local message dispatch
  - direct internal helper calls
  - direct state/slot mutation

That mixed design is not accidental edge-case behavior; it is the compiled shape of the subsystem.

---

## Deeper Callback And Input Trampolines Around The UI Layer

I kept going deeper around the key/UI dispatch zone and recovered several anonymous compiled trampolines that line up very closely with the source-side helper names in `UIMgr.cpp`.

This matters because it gives us binary bodies for some of the exact shims we had previously only from source:

- the `OnSendUIMessage`-style hook shim
- a frame-message hook shim
- the open-template callback
- a cleanup helper that removes all callback registrations for one hook entry
- a window/input hook that tracks recent keyboard/mouse activity

### `FUN_100268a0`: compiled `OnSendUIMessage`-style trampoline

Binary-confirmed body:

```cpp
void __cdecl FUN_100268a0(UIMessage msgid, void *wParam, void *lParam)
{
  GW::Hook::EnterHook();
  GW::UI::SendUIMessage(msgid, wParam, lParam);
  GW::Hook::LeaveHook();
}
```

This is the exact shape we expected from the source `OnSendUIMessage(...)` shim:

- enter hook
- call the exported GWCA dispatcher
- leave hook

That gives us direct binary confirmation that the raw hooked game send is re-routed back through GWCA's higher-level `SendUIMessage(...)` logic rather than bypassing the callback system.

### `FUN_10026860`: frame-message trampoline

Binary-confirmed body:

```cpp
void __thiscall FUN_10026860(void *this, UIMessage msgid, void *wParam, void *lParam)
{
  GW::Hook::EnterHook();
  DAT_1008a350 = 1;
  GW::UI::SendFrameUIMessage((Frame *)((int)this - 0xa8), msgid, wParam, lParam);
  DAT_1008a350 = 0;
  GW::Hook::LeaveHook();
}
```

Important findings:

- this is the frame-plane analogue of the `OnSendUIMessage` shim
- it uses `this - 0xA8` to reconstruct the actual `Frame*` from a callback/dispatcher context pointer
- it toggles a guard flag `DAT_1008a350` around the nested `SendFrameUIMessage(...)` call

That guard flag is especially interesting:

- it strongly suggests GWCA is preventing accidental recursive re-entry or distinguishing "inside frame hook replay" from ordinary frame-message dispatch

So the frame bus has its own thin trampoline layer, not just the global UI-message bus.

### `FUN_10026760`: compiled open-template callback

Binary-confirmed behavior:

- asserts that `msgid == 0x100001c1` and `wParam != 0`
- checks that the feature toggle is enabled
- reads the template/link payload
- only cares about URLs beginning with `http://` or `https://`
- sets the blocked flag byte through its first parameter
- launches the URL with `ShellExecuteW(...)`

This lines up cleanly with the source-side `OnOpenTemplate_UIMessage(...)` logic.

Important implication:

- the "block and open externally" behavior for template/chat URLs is not just source commentary
- the compiled callback really does set the blocked flag before shelling out

That is a very concrete example of GWCA's pre-callback veto model in action.

### `FUN_100265e0`: remove all callbacks for one hook entry

Binary-confirmed behavior:

- looks up a hook-entry-owned registration bundle from `DAT_1008a418`
- copies the registered `HookEntry*` list into a temporary vector
- for each registered hook entry:
  - removes create-component callbacks
  - removes all matching UI-message callbacks across message buckets
  - removes frame-UI-message callbacks
- removes the entry from the owning container afterward

This is a deeper infrastructure result than it first appears.

It implies GWCA maintains a higher-level "what did this hook entry register?" ownership map above the individual callback buckets themselves.

So callback lifetime is managed at two levels:

- per-subsystem callback registries
- a higher-level ownership registry that can bulk-remove everything associated with one hook entry

That is exactly the kind of thing you want in a real hook framework, and now we have binary evidence for it.

### `FUN_10026800`: input/window activity hook

Binary-confirmed behavior:

```cpp
void __cdecl FUN_10026800(int msg_struct, undefined4 arg2)
{
  GW::Hook::EnterHook();
  if (msg_struct != 0 && (*(int *)(msg_struct + 4) == 0x101 || *(int *)(msg_struct + 4) == 0x200)) {
    if (GetFocus() == GW::MemoryMgr::GetGWWindowHandle()) {
      _DAT_1008a35c = _clock();
    }
  }
  GW::Hook::LeaveHook();
  (*DAT_1008a364)(msg_struct, arg2);
}
```

Important points:

- this watches Windows message ids `0x101` and `0x200`
- those correspond to:
  - `0x101` = `WM_KEYUP`
  - `0x200` = `WM_MOUSEMOVE`
- when the Guild Wars window has focus, it updates a last-input timestamp via `_clock()`
- then forwards to the original handler

This is the first binary-confirmed input-side hook that touches real Win32 keyboard/mouse traffic rather than only GWCA's abstracted `DoAction`/frame-message layer.

So the input story is now broader:

- frame input helpers (`Keydown`, `Keyup`, `Keypress`)
- `DoAction` hook path for control actions
- a Win32-side activity tracker for keyboard/mouse focus activity

### `FUN_100265c0` and `FUN_100268c0`: thin hook trampolines

Recovered bodies:

```cpp
void FUN_100265c0(a,b,c) {
  GW::Hook::EnterHook();
  (*DAT_1008a3ac)(a,b,c);
  GW::Hook::LeaveHook();
}

void FUN_100268c0(x) {
  GW::Hook::EnterHook();
  (*DAT_1008a374)(x);
  GW::Hook::LeaveHook();
}
```

These continue the same theme we already saw with the frame-margins helper:

- some GWCA hooks are deliberately minimal
- the real value is that the call participates in GWCA's hook-depth / ownership / lifecycle system

Even when the trampoline itself is tiny, it still matters architecturally.

### `FUN_10025e00`: default action-frame resolver

I also decompiled the helper that the key exports use when no explicit frame is provided:

```cpp
void FUN_10025e00(void)
{
  if (DAT_1008a424 == 0) {
    if (DAT_1008a3b0 != 0) {
      game_hash = GW::HashWString(L"Game", -1);
      for (entry in *DAT_1008a3b0) {
        DAT_1008a424 = *entry;
        if (DAT_1008a424 != 0 &&
            DAT_1008a424 != -1 &&
            *(uint *)(DAT_1008a424 + 0x134) == game_hash) {
          return;
        }
      }
    }
    DAT_1008a424 = 0;
  }
}
```

Important interpretation:

- the default frame/context for key helpers is cached in `DAT_1008a424`
- when uncached, GWCA scans a frame collection and looks for the frame whose hash matches `"Game"`
- this is how the key helpers recover their default input target when the caller does not specify a frame

That explains the earlier compiled `Keydown`/`Keyup`/`Keypress` logic much better:

- they are not choosing a random root frame
- they are targeting the `"Game"` frame context by default

### What this changes in the input/mouse model

This pass gives the input side a more complete compiled shape:

- Win32-side activity hook updates last-user-input time
- key helpers resolve a default `"Game"` frame target when needed
- global UI-message sends go through a compiled `OnSendUIMessage` trampoline
- frame-message sends go through a separate frame trampoline with a re-entry guard
- callback ownership can be bulk-cleaned up across multiple registries for one hook entry

So the key/mouse side is not just:

`Keydown -> SendFrameUIMessage`

It is closer to:

`default "Game" frame resolution -> frame-message trampoline/guard -> frame dispatch`

plus separate Win32 input-activity tracking and `DoAction`-side key callback plumbing.

---

## Deepening The Input Story: Compiled Key Callback Registration vs Source `OnDoAction`

I pushed directly on the `DoAction` question and hit an important source/binary divergence.

The checked-in GWCA source still says:

- key callbacks live in `OnKeydown_callbacks` / `OnKeyup_callbacks`
- `OnDoAction(...)` handles action types `0x1E` and `0x20`
- key helpers call `OnDoAction(...)` with an action context and a `KeypressPacket`

But the compiled `gwca.dll` build I decompiled shows a different surface for key callback registration.

### Source model

From `Source/UIMgr.cpp`:

```cpp
void __fastcall OnDoAction(void* ecx, void* edx, uint32_t action_type, void* arg1, void* arg2) {
    HookBase::EnterHook();
    switch (action_type) {
    case 0x1E:
    case 0x20: {
        HookStatus status;
        const uint32_t key_pressed = *static_cast<uint32_t*>(arg1);
        const auto& callbacks = action_type == 0x1e ? OnKeydown_callbacks : OnKeyup_callbacks;
        for (const auto& it : callbacks) {
            it.second(&status, key_pressed);
            ++status.altitude;
        }
        if (!status.blocked)
            RetDoAction(ecx, edx, action_type, arg1, arg2);
    } break;
    default:
         RetDoAction(ecx, edx, action_type, arg1, arg2);
         break;
    }
    HookBase::LeaveHook();
}
```

And the source-side `GetActionContext()` is:

```cpp
static uintptr_t GetActionContext()
{
    if (!(s_FrameCache && s_FrameCache->size() > 1))
        return 0;
    return (*s_FrameCache)[1] + 0xA0;
}
```

So the source intends a `DoAction`-centered input model.

### Compiled key registration model

The compiled build tells a different story for the exported key callback registration APIs.

#### `GW::UI::RegisterKeydownCallback`

Binary-confirmed behavior:

```cpp
RegisterKeydownCallback(entry, key_callback)
    -> wrap key_callback inside a frame-message callback lambda
    -> RegisterFrameUIMessageCallback(entry, 0x20, wrapped_lambda, -0x8000)
```

#### `GW::UI::RegisterKeyupCallback`

Binary-confirmed behavior:

```cpp
RegisterKeyupCallback(entry, key_callback)
    -> wrap key_callback inside a frame-message callback lambda
    -> RegisterFrameUIMessageCallback(entry, 0x22, wrapped_lambda, -0x8000)
```

That is a major result.

In this compiled build:

- key callback registration is implemented in terms of frame UI message callbacks
- `0x20` is the keydown frame message
- `0x22` is the keyup frame message
- the exported key callback APIs are not simply storing callbacks into a `DoAction`-native key callback map the way the source suggests

So the practical binary model is:

`RegisterKeydownCallback -> RegisterFrameUIMessageCallback(msg=0x20)`

`RegisterKeyupCallback -> RegisterFrameUIMessageCallback(msg=0x22)`

That lines up much better with the already recovered compiled key helpers:

- `Keydown(...) -> SendFrameUIMessage(..., 0x20, ...)`
- `Keyup(...) -> SendFrameUIMessage(..., 0x22, ...)`

### `GW::UI::RemoveKeydownCallback`

Binary-confirmed behavior:

- iterates the frame-UI-message callback storage
- removes any records whose stored `HookEntry*` matches
- compacts the container and releases callback functor state

This is structurally almost identical to:

- `RemoveFrameUIMessageCallback`

In fact, the decompiled body is effectively the same container-removal logic in this build.

That is another clue that the compiled key callback API is riding on top of the frame-message callback subsystem rather than a distinct `DoAction` callback registry.

### `GW::UI::RemoveFrameUIMessageCallback`

Binary-confirmed behavior:

- scans the frame-message callback registry
- removes matching `HookEntry*` entries
- compacts the registry in place
- releases any stored callable objects

Since `RegisterKeydownCallback` and `RegisterKeyupCallback` both route through `RegisterFrameUIMessageCallback(...)`, this function becomes part of the key callback lifecycle too.

### What to do with the `DoAction` discrepancy

The best current reading is:

- the checked-in source still documents a `DoAction`-centric key callback model
- the compiled `gwca.dll` build exposes key callback registration through frame-message callback wrappers instead

That means one of these is true:

1. the source and binary are from different GWCA revisions
2. the source path exists conceptually, but the compiled export layer has shifted to frame-message mediation
3. `DoAction` still exists internally for some input paths, but the public key callback API in this binary has already been refactored onto frame messages

Right now, the binary evidence strongly favors option 3 or some combination of 1 and 3.

### Best current compiled input model

For this compiled `gwca.dll`, the strongest model is now:

- key actuation helpers:
  - `Keydown` -> frame message `0x20`
  - `Keyup` -> frame message `0x22`
  - `Keypress` -> `0x20` + queued follow-up
- key callback registration:
  - exported key callbacks are wrapped onto frame-message callbacks for `0x20` / `0x22`
- default target:
  - unresolved frame parameter falls back to the cached `"Game"` frame path
- parallel side channel:
  - a Win32 hook updates recent input time on `WM_KEYUP` and `WM_MOUSEMOVE`

So the compiled build looks more frame-input-centric than `DoAction`-centric.

### Why this matters for reversing and bots

If you are reasoning from the source alone, you might expect:

- keyboard interception at the `DoAction` layer
- key callbacks attached to `OnDoAction`

But if you are targeting the compiled `gwca.dll` we actually have, the better working assumption is:

- keyboard interception rides primarily on frame message ids `0x20` and `0x22`
- the exported key callback API is an adapter onto the frame-message callback machinery

That is exactly the kind of source/binary drift that can waste a lot of reversing time if it is not called out explicitly.

---

## Chasing The Missing Center: Compiled `RegisterFrameUIMessageCallback`

I followed the key callback registration path all the way down and recovered the compiled `RegisterFrameUIMessageCallback(...)` export, plus a fresh decompile of `SendFrameUIMessage(...)`.

This is the missing center of gravity for the input side in the compiled build.

It lets us state the chain cleanly:

`RegisterKeydownCallback -> RegisterFrameUIMessageCallback(msg=0x20) -> SendFrameUIMessage(msg=0x20) dispatch bucket`

and likewise for keyup on `0x22`.

### `GW::UI::RegisterFrameUIMessageCallback`

Binary-confirmed behavior:

- looks up the frame-message callback bucket for the requested `UIMessage`
- creates the bucket if it does not already exist
- walks existing entries until altitude ordering says "insert here"
- clones the incoming `std::function`
- inserts a record containing:
  - altitude
  - `HookEntry*`
  - callback functor
- finalizes ownership via `FUN_100251d0((uint)entry)`

Structurally, this is extremely close to the compiled `RegisterUIMessageCallback(...)` body.

That means the frame-message callback plane is not a second-class afterthought. It has the same core machinery:

- per-message buckets
- altitude ordering
- owned callback functors
- hook-entry ownership tracking

### `GW::UI::RemoveFrameUIMessageCallback`

Binary-confirmed behavior:

- scans the frame-message callback registry for matching `HookEntry*`
- compacts the per-message bucket in place
- releases any callable state that needs destruction
- shrinks the logical end pointer for the bucket

This matches the removal logic shape we already saw for:

- UI-message callbacks
- create-component callbacks
- keydown callback removal

So the callback containers across GWCA's UI subsystem are now clearly a family, not unrelated one-off implementations.

### `GW::UI::SendFrameUIMessage`

Fresh binary reading confirms:

```cpp
bool SendFrameUIMessage(Frame *frame, UIMessage msgid, void *wParam, void *lParam)
{
    if (!frame || !raw_frame_dispatch) return false;

    bucket = find_frame_callback_bucket(msgid);
    if (bucket not found) {
        Hook::EnterHook();
        raw_frame_dispatch(msgid, wParam, lParam);
        Hook::LeaveHook();
        return true;
    }

    // pre callbacks: altitude < 1
    ...

    if (!blocked) {
        Hook::EnterHook();
        raw_frame_dispatch(msgid, wParam, lParam);
        Hook::LeaveHook();
    }

    // post callbacks
    ...
}
```

That gives the frame plane the same compiled dispatch structure as the global UI-message plane:

- per-message callback bucket lookup
- altitude-split pre/post callbacks
- blocked flag gating the raw dispatch
- hook bracketing around the raw send

So the frame-message layer is not just "like UI messages conceptually." It is implementing almost the same dispatcher pattern in compiled code.

### `GW::UI::RegisterKeydownCallback` and `RegisterKeyupCallback`, revisited

With `RegisterFrameUIMessageCallback(...)` now recovered, the compiled key callback path becomes very concrete:

#### `RegisterKeydownCallback`

Binary-confirmed high-level behavior:

```cpp
user_key_callback
    -> wrapped in lambda taking (HookStatus*, Frame*, UIMessage, wParam, lParam)
    -> RegisterFrameUIMessageCallback(entry, 0x20, wrapped_lambda, -0x8000)
```

#### `RegisterKeyupCallback`

Binary-confirmed high-level behavior:

```cpp
user_key_callback
    -> wrapped in lambda taking (HookStatus*, Frame*, UIMessage, wParam, lParam)
    -> RegisterFrameUIMessageCallback(entry, 0x22, wrapped_lambda, -0x8000)
```

That final altitude is important:

- `-0x8000` is very early in the ordering
- so keydown/keyup callback wrappers are registered as pre-dispatch frame-message observers

That makes perfect sense for interception:

- see the input message early
- optionally block
- otherwise let raw frame dispatch proceed

### What this means for the compiled input model

The strongest current compiled model is now:

1. key helper emits frame message `0x20` or `0x22`
2. `SendFrameUIMessage(...)` looks up the frame callback bucket for that message
3. early callbacks run first
4. if not blocked, raw frame dispatch executes
5. post callbacks run afterward

And the exported key callback API is just a convenience adapter that registers into that frame-message plane.

So for this compiled `gwca.dll`, the real interception center for keyboard activity is not the source-level `OnDoAction` abstraction. It is the frame-message callback registry.

### Why this is the key reconciliation result

Before this pass, we had a strong suspicion that the binary build was more frame-centric than the source suggested.

After this pass, that suspicion is now well supported:

- compiled key actuation uses frame messages
- compiled key callback registration uses frame-message callbacks
- compiled frame-message dispatch supports blocking and post callbacks

That means the entire practical keyboard path in this build can be understood without needing `DoAction` to be the primary interception surface.

This is probably the single most important source/binary reconciliation result in the document so far.

---

## Crosswalk: Binary Helpers To Public API And Bot Usage

This appendix ties the recovered binary behavior back to three outward-facing layers:

1. the GWCA header/API surface in `GWCA-master/Include/GWCA/Managers/UIMgr.h`
2. the local AutoIt frame automation layer in `lib/custom/GWA2_FrameUI.au3`
3. the Py4GW wrapper/helper surface in `tests/_py4gw_src/Py4GW-main`

The goal here is to show which abstractions are stable across layers, and where downstream code has drifted from what the compiled `gwca.dll` actually does.

### 1. GWCA header surface vs binary-confirmed implementation

`UIMgr.h` exposes the public UI namespace and the broad categories we have now recovered from the binary:

- global UI messages under `enum class UIMessage : uint32_t`
- relay-style `0x30000000` `kSend*` commands
- frame helpers like `SendFrameUIMessage`
- frame traversal/helpers like `GetChildFrame`, `GetFrameContext`, and the frame subclasses

The header is useful as the public contract, but the binary decompilation adds the operational detail the header cannot show:

- `ButtonFrame::Click()` is a two-stage action path
- `ButtonFrame::MouseAction()` is where the parent-facing `0x31` packet is built
- control families reuse numeric frame opcodes with family-specific semantics
- `UI::SetFrameMargins()` bypasses `SendFrameUIMessage` entirely and calls a separate internal helper

So the header tells us what GWCA exports, while the binary tells us how those exports really actuate the game UI.

### 2. `ButtonClick` crosswalk

#### Public/header-facing concept

GWCA exposes button/frame helpers through the UI/frame API surface in `UIMgr.h`.

#### Binary-confirmed implementation

Recovered behavior:

```cpp
GW::ButtonFrame::Click(this) {
    MouseAction(this, 6);
    MouseAction(this, 7);
}
```

And each `MouseAction()` builds a parent-facing `0x31` frame packet:

```cpp
packet.frame_id = *(button + 0xBC);
packet.child_offset_id = *(button + 0xB8);
packet.action_state = 6 or 7;
packet.wparam = 0;
packet.lparam = 0;
SendFrameUIMessage(parent_frame, 0x31, &packet, 0);
```

#### AutoIt usage layer

`lib/custom/GWA2_FrameUI.au3` contains two different interpretations:

- one section explicitly says click means `MouseDown (0x6)` then `MouseUp (0x7)`
- a later section claims GWCA uses a single `MouseUp (0x7)` for `ButtonClick`

Those two notes are not equivalent. The binary decompile supports the first interpretation, not the second.

So the strongest current reading is:

- the AutoIt earlier two-stage notes are consistent with `gwca.dll`
- the later "single MouseUp only" note is local drift or an experimental correction that does not match the recovered GWCA binary

That matters because button state machines are exactly the kind of place where a partial click can leave UI state inconsistent.

#### Py4GW usage layer

Py4GW exposes this at a much higher level:

```python
@staticmethod
def FrameClick(frame_id):
    if not UIManager.FrameExists(frame_id):
        return
    PyUIManager.UIManager.button_click(frame_id)
```

And the bot helpers consume it like this:

```python
UIManager.FrameClick(frame_id)
```

So the abstraction stack is:

`bot helper -> UIManager.FrameClick -> compiled PyUIManager.button_click -> GWCA-style button helper -> 0x31 parent commit packets`

### 3. Child-frame lookup crosswalk

#### Binary-confirmed implementation

Recovered frame navigation behavior:

- `GetFrameById(id)` = indexed lookup in the frame array
- `GetChildFrame(parent, child_offset_id)` = resolve child id, then re-map through `GetFrameById`
- `GetParentFrame(frame)` = `*(frame + 0x128) - 0x128`
- `GetFrameContext(frame)` = scan callback/context entries and return the last non-null context

#### AutoIt usage layer

`GWA2_FrameUI.au3` reimplements this pattern directly:

- resolve frame by hash
- walk child offsets
- derive context
- call the frame dispatcher manually

So AutoIt is not just using the GWCA idea at a high level; it is reproducing the same frame traversal model as a native memory/shellcode workflow.

#### Py4GW usage layer

Py4GW wraps the same idea in a friendlier API:

```python
@staticmethod
def GetChildFrameID(parent_hash: int, child_offsets: List[int]):
    return PyUIManager.UIManager.get_child_frame_id(parent_hash, child_offsets)
```

And then many bot/UI helpers do:

```python
frame_id = UIManager.GetChildFrameID(parent_hash, offsets)
UIManager.FrameClick(frame_id)
```

This is the main bridge between human-meaningful UI targets and raw frame actuation.

### 4. Scrollable-family crosswalk

#### Binary-confirmed implementation

Recovered scrollable operations:

- `ClearItems()` -> `SendFrameUIMessage(this, 0x55, 0, 0)`
- `AddItem(...)` -> `SendFrameUIMessage(this, 0x56, &local_packet, &status_slot)`
- `GetItems(...)` -> iterative `0x58` control block protocol
- `SetSortHandler(fn)` -> `SendFrameUIMessage(this, 0x63, fn, 0)`
- `GetSortHandler()` -> direct frame-context read
- `SetPage(page_ctx)` -> `SendFrameUIMessage(this, 0x7FFFFFF5, page_ctx, 0)`
- `GetPage()` -> child descent, not message dispatch

#### Public/header-facing concept

This is exactly the sort of thing the header alone cannot make fully explicit: one control family owns several opcodes, but not all reads and writes go through the frame bus.

#### Py4GW-facing analogue

Py4GW scripts and helpers rely on `GetItems()` heavily at the inventory/bag layer, but from the Python side that usually appears as "get bag contents" rather than "run a scrollable `0x58` iterator protocol."

That is a good example of abstraction compression:

- Python sees item arrays / bag contents
- GWCA binary reveals a UI/container traversal mechanism underneath

### 5. Tab-family crosswalk

#### Binary-confirmed implementation

Recovered tab-family operations:

- `AddTab(...)` -> `0x55` with structured add-tab packet and out frame id
- `DisableTab(index)` -> `0x56`
- `EnableTab(index)` -> `0x57`
- `GetCurrentTabIndex(out)` -> `0x58`
- `GetCurrentTab()` -> `0x58` selection token, then `0x59` resolve-to-frame-id
- `GetTabFrameId(index, out)` -> `0x59`
- `GetIsTabEnabled(index, out)` -> `0x5B`
- `ChooseTab(index/frame)` -> local `0x59 -> 0x5C`
- `RemoveTab()` -> direct paired-component destruction, not a frame opcode

#### Py4GW-facing analogue

Py4GW’s common use of `GetChildFrameID(parent_hash, [..., 0xFFFFFFFF])` and similar complemented offsets around tabbed windows lines up well with the tab-family binary recovery:

- the user-facing layer thinks in hashes and offset chains
- the binary shows the underlying control family uses complemented/paired ids and tab-specific resolver/query opcodes

That is strong evidence that the Py4GW frame-hash ecosystem is sitting on the same internal frame-tree structure we recovered from GWCA.

### 6. Where the layers diverge

The cleanest documented divergence so far is `ButtonClick` semantics:

- binary-confirmed GWCA behavior: `MouseAction(6)` then `MouseAction(7)`
- local AutoIt note in one later section: single `MouseUp (0x7)`

For future work, that means:

- trust the binary first when reconstructing GWCA semantics
- treat downstream comments as useful operational notes, but not authoritative when they contradict decompilation

There is also a more general abstraction divergence:

- GWCA headers present a coherent exported API
- the binary shows that implementation is a hybrid of frame dispatch, direct reads, and direct helper calls
- Py4GW and AutoIt usually flatten that into simpler verbs like `FrameClick`, `GetChildFrameID`, or `GetItems`

That flattening is useful for bots, but it hides the packet shapes and family-specific protocols that matter for reverse engineering.

### 7. Best current unified model

The strongest model after this crosswalk is:

- GWCA’s public UI API is real, but only partially descriptive
- the compiled binary implements several distinct mechanisms behind that API:
  - global `SendUIMessage`
  - local `SendFrameUIMessage`
  - parent-facing `0x31` commit packets
  - direct frame-context reads
  - direct internal helper calls like frame margins
- downstream tooling then compresses those mechanics into friendlier verbs:
  - AutoIt: shellcode and memory helpers
  - Py4GW: Python wrappers like `FrameClick()` and `GetChildFrameID()`

So when a bot "clicks a UI element," the full stack is often:

`hash/offset lookup -> frame id -> frame/context resolution -> family-specific frame dispatch -> sometimes parent 0x31 commit -> game UI state change`

That is the level where the three layers finally line up cleanly.

---

## Provenance Matrix: What Is Declared, Decompiled, And Consumed

This section is meant as a quick trust map.

Each row answers four separate questions:

1. is the concept part of GWCA's public/header-facing UI API?
2. do we have binary-confirmed behavior from `gwca.dll`?
3. is it consumed or reimplemented in the local AutoIt layer?
4. is it exposed or used in the Py4GW-facing layer?

### Legend

- `Header`: visible in the checked-in GWCA header/API surface
- `Binary`: behavior recovered directly from the compiled `gwca.dll`
- `AutoIt`: visible in `lib/custom/GWA2_FrameUI.au3`
- `Py4GW`: visible in `tests/_py4gw_src/Py4GW-main`

### High-value helpers

| Helper / concept | Header | Binary | AutoIt | Py4GW | Notes |
| --- | --- | --- | --- | --- | --- |
| `UI::SendUIMessage` | yes | yes | indirectly discussed | wrapped separately in Py4GW | global UI plane |
| `UI::SendFrameUIMessage` | yes | yes | yes | indirectly beneath `PyUIManager` | frame-local UI plane |
| `UI::GetFrameById` | yes | yes | implied by hash/ptr workflows | indirectly | frame array lookup |
| `UI::GetChildFrame` | yes | yes | reimplemented conceptually | exposed as `GetChildFrameID(...)` | child resolution path |
| `UI::GetParentFrame` | yes | yes | reimplemented conceptually | indirect only | binary-confirmed as `*(frame + 0x128) - 0x128` |
| `UI::GetFrameContext` | yes | yes | yes | indirect only | exported helper is more than `frame + constant` |
| `ButtonFrame::Click` | yes | yes | yes, but with drifted comments | indirectly as `button_click(frame_id)` | binary says `6 -> 7` |
| `ButtonFrame::MouseAction` | class/helper surface only | yes | yes, manually mirrored | indirectly via `test_mouse_action` / `test_mouse_click_action` naming | parent-facing `0x31` packet builder |
| `DropdownFrame::SelectIndex` | class/helper surface only | yes | not surfaced locally | likely behind compiled layer | local `0x60`, then parent `0x31` |
| `EditableTextFrame::SetValue` | class/helper surface only | yes | not surfaced locally | likely behind compiled layer | local `0x5D`, then parent `0x31` |
| `SliderFrame::SetValue` | class/helper surface only | yes | not surfaced locally | likely behind compiled layer | local `0x56`, then parent `0x31` |
| `CheckboxFrame::SetChecked` / `SetValue` | class/helper surface only | yes | not surfaced locally | likely behind compiled layer | query `0x57`, set `0x56` |
| `TabsFrame::AddTab` | class/helper surface only | yes | not surfaced locally | indirect structural usage | `0x55` structured packet |
| `TabsFrame::GetCurrentTabIndex` | class/helper surface only | yes | not surfaced locally | indirect structural usage | `0x58` query |
| `TabsFrame::GetCurrentTab` | class/helper surface only | yes | not surfaced locally | indirect structural usage | `0x58 -> 0x59` resolve chain |
| `ScrollableFrame::GetItems` | class/helper surface only | yes | not surfaced locally | conceptually consumed at higher layers | iterator-like `0x58` control block |
| `ScrollableFrame::SetSortHandler` | class/helper surface only | yes | not surfaced locally | not seen directly | `0x63`, family-scoped meaning |
| `ScrollableFrame::SetPage` | class/helper surface only | yes | not surfaced locally | not seen directly | `0x7FFFFFF5` special opcode |
| `UI::SetFrameMargins` | yes | yes | not surfaced locally | not seen directly | direct helper call, not frame dispatch |

### Confidence bands

#### A. Public and binary-confirmed

These are the strongest entries because both the API surface and the compiled behavior are visible:

- `SendUIMessage`
- `SendFrameUIMessage`
- `GetChildFrame`
- `GetParentFrame`
- `GetFrameContext`
- `ButtonFrame::Click`
- `UI::SetFrameMargins`

These can be treated as the most stable core of the current model.

#### B. Binary-confirmed, but mostly class-internal in practice

These are very strong reverse-engineering results, but consumers often see them only through higher-level wrappers:

- `ButtonFrame::MouseAction`
- `DropdownFrame::SelectIndex`
- `EditableTextFrame::SetValue`
- `SliderFrame::SetValue`
- tab-family helpers
- scrollable-family helpers

For these, the binary is authoritative even if the public header or downstream wrappers do not foreground the exact operation.

#### C. Downstream-visible, but semantics compressed

These are important because they show how bot code actually touches the system:

- AutoIt `ClickFrameButton(...)`
- Py4GW `UIManager.FrameClick(frame_id)`
- Py4GW `UIManager.GetChildFrameID(parent_hash, offsets)`
- Py4GW `UIManager.TestMouseAction(...)`
- Py4GW `UIManager.TestMouseClickAction(...)`

These calls are practical, but they flatten away the control-family protocol details that the binary reveals.

### Biggest current mismatch

The single clearest mismatch in the whole UI/frame story is still `ButtonClick` semantics:

- binary-confirmed GWCA behavior: two calls, `MouseAction(6)` then `MouseAction(7)`
- one local AutoIt note: single `MouseUp (0x7)` only

So if there is ever a disagreement between:

- decompiled `gwca.dll`
- local reproduction comments
- high-level wrappers

the compiled GWCA binary should currently be treated as the strongest source of truth.

### Practical use of this matrix

For future reversing, the best workflow is now:

1. start from the provenance matrix to decide how trustworthy a claim already is
2. use the field atlas when the helper is packet-shaped
3. use the crosswalk section when you need to connect binary behavior to AutoIt or Py4GW usage
4. only then branch into new decompilation work

That should keep the document from fragmenting into separate source notes, binary notes, and bot notes.

---

## Deepening The Frame Callback Registry: Adapter Layer, Ownership, And Removal Drift

After recovering `RegisterFrameUIMessageCallback(...)`, I pushed one level deeper into the compiled plumbing that makes the frame callback plane work as a system rather than a loose pile of wrappers.

This pass adds three useful clarifications:

1. how `RegisterKeydownCallback(...)` and `RegisterKeyupCallback(...)` really adapt user callbacks into frame-message callbacks
2. how GWCA tracks callback ownership by `HookEntry` / module
3. where the compiled build visibly drifts from the checked-in source on the removal side

### `RegisterKeydownCallback(...)` and `RegisterKeyupCallback(...)`: exact compiled adapter shape

The earlier high-level conclusion was correct, but the decompile is stronger than that summary alone made clear.

Both exports do the same three-step dance:

1. clone the incoming `std::function<void(HookStatus*, uint32_t)>`
2. allocate a new `0x30`-byte callable object whose vtable is a generated lambda adapter
3. register that adapter through `RegisterFrameUIMessageCallback(...)`

The only meaningful difference between the two exports is the frame message id:

- `RegisterKeydownCallback(...) -> RegisterFrameUIMessageCallback(entry, 0x20, wrapped_lambda, -0x8000)`
- `RegisterKeyupCallback(...) -> RegisterFrameUIMessageCallback(entry, 0x22, wrapped_lambda, -0x8000)`

That matters because it means the compiled key callback surface is not merely "implemented using the same idea as frame callbacks."

It is literally an adapter layer on top of the frame callback registry.

### `RegisterFrameUIMessageCallback(...)`: what the registry is really keyed by

The compiled registry behind frame callbacks lives in `DAT_1008a454`, with a sentinel/root at `DAT_1008a458`.

Binary-confirmed behavior:

- the first lookup hashes only `msgid`
- GWCA creates a bucket for that `msgid` if one does not already exist
- records are inserted in altitude order
- the callback object is cloned into registry-owned storage
- GWCA finalizes ownership with `FUN_100251d0((uint)entry)`

The important correction here is subtle but useful:

- the registry is message-bucketed first
- it is not a separate callback container per individual frame pointer

The `Frame*` still matters at dispatch time because the callback receives it, but the registry lookup itself is keyed by frame message id.

So the compiled mental model is:

`frame_msg_id -> ordered callback bucket -> callback sees concrete Frame* at dispatch time`

not:

`frame_ptr -> callback list`

### `SendFrameUIMessage(...)`: dispatch confirms the same message-bucketed model

The decompiled dispatch path matches the registration design:

- look up bucket by `msgid` in `DAT_1008a454`
- if no bucket exists, call the raw frame dispatcher directly
- otherwise run pre callbacks with altitude `< 1`
- if not blocked, execute the raw frame dispatcher inside `Hook::EnterHook()` / `Hook::LeaveHook()`
- then run the remaining callbacks

That confirms the frame plane is architecturally parallel to the global `SendUIMessage(...)` plane:

- same bucketed callback registry pattern
- same altitude split
- same block-before-raw-send model
- same hook bracketing around the real game call

### `FUN_100251d0(...)`: hook-entry ownership is module-scoped

The helper called at the end of registration is no longer a black box.

Decompiling `FUN_100251d0(...)` shows:

- it resolves a module handle from the `HookEntry`
- it falls back across two lookup modes
- if no module handle is found, it asserts in `UIMgr.cpp` under an internal helper named like `AddHookEntryByModule`
- it stores the `HookEntry` into a module-keyed container rooted at `DAT_1008a418`

In other words, registration is not just:

`hook entry -> callback bucket`

It is also:

`module -> owned hook entries -> later bulk cleanup`

That lines up cleanly with the already decompiled bulk-removal helper `FUN_100265e0(...)`, which walks one ownership bundle and removes all of that entry's create-component, UI-message, and frame-message callbacks together.

### Removal side: compiled drift is stronger than expected

The checked-in source declares and implements both:

- `RemoveKeydownCallback(HookEntry* entry)`
- `RemoveKeyupCallback(HookEntry* entry)`

But in the compiled `gwca.dll` build I can currently find only:

- `RemoveKeydownCallback @ 100271d0`
- `RemoveFrameUIMessageCallback @ 100270a0`
- `RemoveUIMessageCallback @ 100271e0`

I do not see a separately named compiled `RemoveKeyupCallback` export in the current binary function list.

That is not just a documentation omission. It is a real source/binary discrepancy.

### `RemoveKeydownCallback(...)` vs `RemoveFrameUIMessageCallback(...)`

The binary body of `RemoveKeydownCallback(...)` is effectively the same compaction/removal logic as `RemoveFrameUIMessageCallback(...)`:

- walk the frame-callback registry
- scan callback records for matching `HookEntry`
- shift later records down over the removed one
- release copied callable state
- shrink the bucket tail

That gives two reasonable compiled interpretations:

1. `RemoveKeyupCallback(...)` was folded away by the build and only one public removal export survived
2. the key removal surface was partially collapsed onto the generic frame-callback removal machinery in this binary revision

Either way, the compiled build is visibly less symmetric than the source.

### Best current compiled model for keyboard interception

Putting the full binary path together:

1. `RegisterKeydownCallback(entry, cb)`
   - clone user callback
   - build adapter lambda object
   - `RegisterFrameUIMessageCallback(entry, 0x20, adapter, -0x8000)`
2. `Keydown(key, frame?)`
   - resolve explicit frame or cached `"Game"` frame
   - build action packet
   - `SendFrameUIMessage(frame, 0x20, &action, 0)`
3. `SendFrameUIMessage(...)`
   - bucket lookup by `0x20`
   - run pre callbacks
   - raw frame dispatch
   - run post callbacks
4. ownership / cleanup
   - `FUN_100251d0(...)` links the `HookEntry` to a module-owned cleanup bundle
   - later bulk cleanup removes all owned registrations together

And the same structure applies to keyup with `0x22`.

This is a more precise and more operationally useful model than the older source-only picture centered on `OnDoAction(...)`.

### Why this matters

If you are targeting the compiled `gwca.dll` we actually have, the safest working assumption is now:

- public keyboard actuation is frame-message based
- public keyboard callback registration is adapter-on-frame-message based
- callback ownership is module-scoped
- removal behavior is not perfectly symmetric with the checked-in source

So the real center of gravity for input interception in this build is:

`key export -> frame callback adapter -> frame message bucket -> hook-bracketed raw dispatch -> module-owned cleanup`

---

## Raw Frame Dispatcher Path: Bootstrap, Hook Target, Trampoline, And Replay

I followed the raw frame-dispatch path one level deeper, and this finally resolves the most important ambiguity around `DAT_1008a3a0`.

The key correction is:

- `DAT_1008a39c` is the scanned hook target for the real game-side frame dispatcher
- `DAT_1008a3a0` is the callable out-trampoline/original pointer produced by `CreateHook(...)`
- `SendFrameUIMessage(...)` calls `DAT_1008a3a0`, not the raw scan target directly

So `DAT_1008a3a0` is not the original discovery point. It is the post-hook replay handle.

### Decoding the compiled `UIModule` structure

By decoding the `UIModule` record in `.data`, the compiled UI lifecycle becomes much clearer.

At `PTR_s_UIModule_10088288`, the function pointers decode as:

- name pointer: `0x10054164`
- param: `0x1008a35c`
- init: `0x10024690`
- exit: `0x10025020`
- enable hooks: `0x10024e80`
- disable hooks: unresolved as a named function in this pass

That matters because the real binary UI bootstrap for this build is not the source-level `::Init` symbol name. It is the compiled routine at `0x10024690`.

### `FUN_10024690`: the compiled UI bootstrap

Decompiling `0x10024690` shows a dense scanner bootstrap for the whole UI subsystem.

For the frame-dispatch question, the important assignments are:

#### 1. frame array/root plumbing

- `DAT_1008a3b0` comes from assertion `\\Code\\Engine\\Frame\\FrMsg.cpp`, string `"frame"`
- `DAT_1008a410` comes from assertion `FrRelation.cpp`, string `"s_codeTable.Head()"`
- `DAT_1008a37c` is resolved from `CtlView.cpp`, string `"pageId"`

These are the surrounding frame helpers the higher-level wrappers depend on.

#### 2. raw frame dispatcher discovery

The critical pair is:

```cpp
uVar3 = GW::Scanner::Find(&DAT_10053d60,"xxxx",3,0);
DAT_1008a39c = GW::Scanner::FunctionFromNearCall(uVar3,true);
```

This is the compiled binary confirmation that:

- GWCA locates the frame dispatcher by a pattern in the game code
- then resolves the actual target via `FunctionFromNearCall(...)`
- the result is cached in `DAT_1008a39c`

So `DAT_1008a39c` is the true scanned game-side entry point for frame message dispatch.

#### 3. adjacent frame-message helper

The same init also resolves:

```cpp
uVar3 = GW::Scanner::FindAssertion("FrMsg.cpp","hdr.reserved < frame->msg.m_classes.Count()",0,0);
DAT_1008a34c = GW::Scanner::ToFunctionStart(uVar3,0xff);
```

That is useful because it strongly suggests GWCA also caches a nearby frame-message-class/helper function rooted in the same `FrMsg.cpp` area.

So the binary is not just caching one opaque entry point. It is caching a small cluster around the frame messaging subsystem.

### The hook creation that produces `DAT_1008a3a0`

The decisive code is near the end of `FUN_10024690`:

```cpp
if (DAT_1008a39c != 0) {
    GW::Hook::CreateHook((void **)&DAT_1008a39c, FUN_10026860, (void **)&DAT_1008a3a0);
}
```

This is the missing link.

It means:

1. `DAT_1008a39c` is the hook target
2. `FUN_10026860` is the detour
3. `DAT_1008a3a0` receives the original/trampoline callable

That exactly matches the behavior we had already recovered from other directions:

- game-side inbound frame dispatch gets intercepted by `FUN_10026860`
- GWCA-side outbound replay from `SendFrameUIMessage(...)` goes through `DAT_1008a3a0`

### `FUN_10026860`: inbound detour on the raw frame dispatcher

We already had this decompile, but it is much more meaningful now that the bootstrap is known:

```cpp
void __thiscall FUN_10026860(void *this, UIMessage msgid, void *wParam, void *lParam)
{
    GW::Hook::EnterHook();
    DAT_1008a350 = 1;
    GW::UI::SendFrameUIMessage((Frame *)((int)this - 0xa8), msgid, wParam, lParam);
    DAT_1008a350 = 0;
    GW::Hook::LeaveHook();
}
```

Now we can interpret it concretely:

- the game calls the original frame dispatcher
- the hook reroutes that call into `FUN_10026860`
- GWCA reconstructs the owning `Frame*` from `this - 0xA8`
- then it re-enters the GWCA wrapper plane through `GW::UI::SendFrameUIMessage(...)`

So `FUN_10026860` is the inbound bridge from raw game dispatch into GWCA's callback-aware frame plane.

### `GW::UI::SendFrameUIMessage(...)`: outbound replay through the trampoline

The compiled wrapper body now reads more cleanly:

- callback bucket lookup happens in GWCA space
- pre callbacks run
- if not blocked, GWCA calls `DAT_1008a3a0(msgid, wParam, lParam)`
- post callbacks run

Because `DAT_1008a3a0` was filled by `CreateHook(...)`, this is not a recursive call back into the detour.

It is the replay/original trampoline path.

That is the exact same architectural pattern already seen on the global UI-message side:

- raw target
- hook detour
- GWCA wrapper
- original/trampoline replay pointer

### `FUN_10024e80`: enabling the UI hook cluster

The compiled UIModule enable-hooks routine at `0x10024e80` confirms which cached UI targets are actually activated.

For the frame path, the relevant line is:

```cpp
if (DAT_1008a39c != (void *)0x0) {
    GW::Hook::EnableHooks(DAT_1008a39c);
}
```

That proves the frame dispatcher hook is not merely prepared during init. It is explicitly enabled as part of the UIModule hook lifecycle.

The same enable routine also turns on neighboring UI hooks like:

- `DAT_1008a360`
- `DAT_1008a378`
- `DAT_1008a368`
- `DAT_1008a384`
- `DAT_1008a3a8`

So the frame dispatcher is one member of a broader compiled UI hook cluster, not a one-off special case.

### End-to-end raw frame path

With the bootstrap recovered, the raw frame-dispatch path in this binary can now be stated cleanly:

#### Game -> GWCA

1. game calls raw frame dispatcher target
   - scanned into `DAT_1008a39c`
2. installed hook redirects into `FUN_10026860`
3. `FUN_10026860`
   - enters hook guard
   - raises re-entry flag `DAT_1008a350`
   - reconstructs `Frame*`
   - calls `GW::UI::SendFrameUIMessage(...)`
4. GWCA frame callbacks run around the event

#### GWCA -> Game

1. bot/helper/export calls `GW::UI::SendFrameUIMessage(frame, msgid, wParam, lParam)`
2. GWCA runs pre frame callbacks
3. if not blocked, GWCA replays the original dispatch through `DAT_1008a3a0`
4. post callbacks run

That is the full two-way mediation loop.

### Why this matters

This resolves a subtle but important modeling error that is easy to make if you only stare at `SendFrameUIMessage(...)` in isolation.

It is tempting to think:

- `DAT_1008a3a0` is the raw frame dispatcher

But the deeper binary reading shows:

- `DAT_1008a39c` is the scanned raw target
- `DAT_1008a3a0` is the trampoline/original replay handle after hook installation

That distinction matters for any future reverse-engineering of:

- recursion guards
- callback re-entry behavior
- how GWCA avoids dispatching back into its own detour
- where to look next if we want to push below GWCA and closer to the game's true opcode/class dispatch logic

### Best next move from here

The strongest next pass is now much sharper than before:

- chase the raw `DAT_1008a39c` target neighborhood
- decompile nearby `FrMsg.cpp`-rooted helpers like the cached `DAT_1008a34c`
- look for class/opcode branching just below the hook boundary

That is the point where we should finally be able to answer whether reused opcodes like `0x56`, `0x58`, and `0x31` are dispatched primarily by:

- frame class
- per-frame message-class tables
- payload shape
- or a combination of all three

---

## Going One Layer Lower: Exact Raw-Dispatcher Scan Metadata And The Current Binary Boundary

I pushed the raw-dispatch work as far as the local artifacts allow.

This pass adds two concrete things:

1. the exact scan metadata GWCA uses to recover the frame dispatcher hook target
2. a clear statement of the current boundary: the local workspace does not currently include a game executable image to decompile below GWCA's hook layer

### Exact frame-dispatch scan bytes from the compiled UIModule init

The compiled UI bootstrap at `0x10024690` resolves the raw frame dispatcher with:

```cpp
uVar3 = GW::Scanner::Find(&DAT_10053d60,"xxxx",3,0);
DAT_1008a39c = GW::Scanner::FunctionFromNearCall(uVar3,true);
```

Dumping the pattern bytes from `gwca.dll` shows:

```text
DAT_10053d60: 83 c1 dc e8 00 00 00 00 ...
```

So the actual scan seed GWCA uses is:

```text
83 C1 DC E8
```

with the resolver logic:

- scan for that sequence
- move to offset `+3`
- treat that byte as the `E8` near-call
- resolve the callee with `FunctionFromNearCall(...)`

That lines up cleanly with the earlier recovered practical formula:

```text
83 C1 DC E8
```

and supports the previously documented game-side address around `0x007986D0` for this build.

### The neighboring `FrMsg.cpp` helper is source-locatable but not yet game-decompiled

The same compiled init also caches:

```cpp
uVar3 = GW::Scanner::FindAssertion("FrMsg.cpp","hdr.reserved < frame->msg.m_classes.Count()",0,0);
DAT_1008a34c = GW::Scanner::ToFunctionStart(uVar3,0xff);
```

That is exactly the helper we wanted to chase next, because it strongly suggests a function near the game's per-frame message-class routing.

However, there is an important distinction:

- GWCA gives us the assertion anchor and the eventual function-start address **at runtime**
- but without the local game binary image, we cannot decompile that target body itself from the current workspace

So at this point we know:

- where GWCA looks
- what assertion anchors the target
- how GWCA computes the function start

But we do not yet have:

- the actual machine code body of that game function available locally to decompile

### Current workspace boundary

I checked the local workspace for a usable game executable image and did not find one.

That means the current reverse-engineering state is:

- `gwca.dll` can be decompiled locally
- GWCA's scan patterns, hook targets, trampoline wiring, and helper wrappers can be decompiled locally
- the game-side target functions that GWCA points at cannot be directly decompiled here unless we also have the matching game binary loaded into Ghidra

This is the exact boundary we have now hit on the raw frame path.

### What we still learned from this pass

Even without the game image, this pass still tightened the model in a useful way:

- the raw frame dispatcher hook target is anchored by an exact 4-byte scan seed: `83 C1 DC E8`
- the neighboring `FrMsg.cpp` helper is anchored by the assertion string:
  - `"hdr.reserved < frame->msg.m_classes.Count()"`
- the frame array/root plumbing and the dispatcher hook target are recovered by the same compiled UI bootstrap routine
- the compiled UIModule init/enable pair confirms that all of this belongs to one intentional frame/UI hook cluster

### The real next step, now that the boundary is explicit

To go deeper than GWCA and actually answer the class-routing question, the next practical step is:

1. obtain the matching game executable image used by this GWCA build
2. import that binary into Ghidra
3. use the recovered GWCA anchors to jump directly to:
   - the raw frame dispatcher target for `DAT_1008a39c`
   - the nearby `FrMsg.cpp` helper for `DAT_1008a34c`
4. then decompile the game-side opcode/class dispatch logic directly

So the next logical reverse target is still the same one.

The difference now is that we know exactly what additional artifact is required to reach it.

---

## Key Files
- `lib/custom/GWA2_FrameUI.au3` — Frame UI system implementation
- `lib/custom/GWA2_Assembly_UISniffer.au3` — UIMessage sniffer (for discovery)
- `tests/frame_aliases.json` — 1164 known frame hashes from py4gw
- `tests/disasm_gwca.py` — gwca.dll disassembly scripts
- `tests/extract_scan_patterns.py` — scan pattern extractor
- `toolbox/GWToolboxpp-master/Dependencies/GWCA/` — GWCA headers + compiled DLL

## External Resources
- py4gw: `github.com/apoguita/Py4GW` — Python GW bot using GWCA (reference for frame hashes)
- py4gw C++: `github.com/apoguita/Py4GW_cpp_files` — C++ bridge showing GWCA API usage
- GWCA (old): `github.com/GregLando113/GWCA` — older version with UIMgr.cpp source
- GWToolboxpp: `github.com/gwdevhub/GWToolboxpp` — uses latest GWCA (binary-only)
