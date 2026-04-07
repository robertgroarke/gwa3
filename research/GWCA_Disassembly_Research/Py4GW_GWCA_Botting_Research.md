# Py4GW to GWCA Botting Call Map

## Scope

This document maps the visible Py4GW bot-control surface to the bundled GWCA source.

Important boundary:

- `Py4GW.dll`, `PyPlayer`, `PyParty`, `PyInventory`, `PyQuest`, `PyMerchant`, `PySkillbar`, `PyUIManager`, `PyScanner`, and `PyCallback` are compiled bindings in this checkout.
- When a wrapper call enters one of those binaries and the body is not visible, I mark it as `compiled binding / inferred`.
- When the Python source shows the actual packet or native call, I mark it as `visible`.

## Architecture Summary

Py4GW bots Guild Wars by:

1. injecting `Py4GW.dll` into the Guild Wars process,
2. rebinding internal functions with GWCA-style pattern scanning,
3. queueing work onto the game thread,
4. dispatching actions through internal UI messages or direct native calls,
5. pacing those calls with action queues, routines, FSMs, and multibox coordination.

It is not primarily screen scraping and not primarily keystroke automation.

## Core Mechanisms

### Injection

`Py4GW_Launcher.py` uses the standard remote-load pattern:

- `VirtualAllocEx`
- `WriteProcessMemory`
- `CreateRemoteThread`
- `LoadLibraryA`

This means Py4GW runs inside the Guild Wars process and can call internal functions directly.

### Scanner and rebasing

Py4GW mirrors GWCA's scanner model:

- `PyScanner` exposes pattern, assertion, and near-call helpers.
- `native_src/internals/native_function.py` wraps rebased function pointers.
- visible rebased calls include:
  - `MoveTo_Func`
  - `DepositFaction_Func`
  - `SetActiveTitle_Func`
  - `RemoveActiveTitle_Func`
  - `SkipCinematic_Func`

### Game-thread dispatch

GWCA exposes `GW::GameThread::Enqueue`; Py4GW exposes `Py4GW.Game.enqueue`.

Visible low-level helpers use `Game.enqueue` before touching internal functions or UI messages, which is the same safety pattern GWCA uses.

### UI-message transport

GWCA exposes `SendUIMessage` and `RawSendUIMessage`; Py4GW exposes:

- `UIManager.SendUIMessage(...)`
- `UIManager.SendUIMessageRaw(...)`

This is one of Py4GW's main bot-control planes.

## Call Taxonomy

| Type | Meaning |
|---|---|
| `visible / UI message` | Python builds the packet and sends a known internal UI message. |
| `visible / direct native` | Python rebases and direct-calls a native game function. |
| `compiled binding / inferred` | Python calls a compiled binding whose likely GWCA analogue is visible but whose body is not. |
| `UI-frame click fallback` | Python finds a UI frame and clicks it instead of calling a manager path directly. |
| `chat-command fallback` | Action is sent as a slash command rather than a dedicated manager call. |

## GWCA Anchors

The bundled GWCA source shows the message IDs and managers Py4GW lines up with:

- `UIMgr.h`
  - `kWriteToChatLog`
  - `kLogout`
  - `kGuildHall`
  - `kLeaveGuildHall`
  - `kTravel`
  - `kSendEnterMission`
  - `kSendMoveItem`
  - `kSendMerchantRequestQuote`
  - `kSendMerchantTransactItem`
  - `kSendUseItem`
  - `kSendSetActiveQuest`
  - `kSendAbandonQuest`
- `AgentMgr.cpp`
  - `SendDialog`
  - `ChangeTarget`
  - `Move`
  - `PickUpItem`
- `SkillbarMgr.cpp`
  - `UseSkill`
  - `LoadSkillTemplate`
- `ItemMgr.cpp`
  - `UseItem`
  - `MoveItem`
  - `DepositGold`
  - `WithdrawGold`
  - `IdentifyItem`
  - `DropGold`
- `MerchantMgr.cpp`
  - `RequestQuote`
- `QuestMgr.cpp`
  - `SetActiveQuestId`
  - `AbandonQuestId`
- `PartyMgr.cpp`
  - `ReturnToOutpost`
  - `SetHardMode`
  - `FlagHero`
  - `SetHeroBehavior`
- `MapMgr.cpp`
  - `Travel`
  - `EnterChallenge`
- `GuildMgr.cpp`
  - `TravelGH`
  - `LeaveGH`

## Player Action Surface

### Public wrapper calls in `Py4GWCoreLib/Player.py`

| Py4GW call | Immediate implementation | Transport | GWCA analogue | Detailed behavior |
|---|---|---|---|---|
| `Player.ChangeTarget(agent_id)` | queues `_do_action` calling `PyPlayer.PyPlayer().ChangeTarget(agent_id)` | `compiled binding / inferred` | `GW::Agents::ChangeTarget` and internal target-change UI path | The wrapper currently uses the compiled binding instead of the visible low-level helper. GWCA changes target through an internal manager/UI route, so the binding almost certainly fronts the same mechanism. |
| `Player.Interact(agent_id, call_target=False)` | queues `_do_action` calling `PyPlayer.PyPlayer().InteractAgent(agent_id, call_target)` | `compiled binding / inferred` | `GW::Agents::InteractAgent` or world-action path | The visible helper uses `kSendWorldAction`, but the public wrapper currently delegates to the compiled binding. |
| `Player.Move(x, y, zPlane=0)` | queues `PlayerMethods.Move` | `visible / direct native` | `GW::Agents::Move` | `PlayerMethods.Move` resolves `MoveTo_Func`, builds a 4-float buffer `(x, y, zplane, 0.0)`, and direct-calls the native function from the game thread. |
| `Player.DepositFaction(faction_id)` | queues `PlayerMethods.DepositFaction` | `visible / direct native` | direct internal faction-deposit path | Calls a scanned `DepositFaction_Func` with `(0, allegiance, 5000)`. This looks like a direct embassy/action call, not a frame click. |
| `Player.SetActiveTitle(title_id)` | queues `PlayerMethods.SetActiveTitle` | `visible / direct native` | internal title-set path | Uses assertion scanning in `AttribTitles.cpp`, resolves a near call, and direct-calls the result. |
| `Player.RemoveActiveTitle()` | queues `PlayerMethods.RemoveActiveTitle` | `visible / direct native` | internal title-remove path | Found by scanning near the title-set function and direct-called. |
| `Player.SendRawDialog(dialog_id)` | queues `PlayerMethods.SendRawDialog` | `visible / UI message` | `kSendAgentDialog` | Sends a raw dialog ID for NPC, trainer, or merchant-tab interactions. |
| `Player.BuySkill(skill_id)` | queues `PlayerMethods.SendSkillTrainerDialog` | `visible / UI message` | skill trainer dialog path | Converts skill ID to trainer dialog ID with `Utils.SkillIdToDialogId`, then sends raw agent dialog. |
| `Player.SendDialog(dialog_id)` | queues compiled `SendDialog(dialog)` | `compiled binding / inferred` | `GW::Agents::SendDialog` | GWCA reroutes dialog sends through `UI::SendUIMessage(kSendDialog, ...)`; the Python wrapper uses the compiled binding but clearly targets the same path. |
| `Player.SendChatCommand(command)` | queues compiled `SendChatCommand` | `compiled binding / inferred` | chat manager send path | The visible low-level helper maps slash commands to `SendChat('/', ...)`; the public wrapper delegates to compiled code. |
| `Player.SendChat(channel, message)` | queues compiled `SendChat` | `compiled binding / inferred` | `kSendChatMessage` | The visible helper validates the channel, builds a wide-char packet, and sends `kSendChatMessage`. |
| `Player.SendWhisper(name, message)` | queues compiled `SendWhisper` | `compiled binding / inferred` | whisper path through `kSendChatMessage` | The visible helper formats the payload as `\"name,message` and sends it as chat. |
| `Player.SendFakeChat(channel, message)` | queues compiled `SendFakeChat` | `compiled binding / inferred` | `kWriteToChatLog` | The visible helper wraps the string in Guild Wars chat markup and writes directly to the chat log. |
| `Player.SendFakeChatColored(channel, message, r, g, b)` | formats `<c=#RRGGBB>` then queues compiled `SendFakeChat` | `compiled binding / inferred` | `kWriteToChatLog` | Same fake-chat route with color markup. |
| `Player.RequestChatHistory()` | queues compiled `RequestChatHistory` | `compiled binding / inferred` | no clear GWCA analogue visible here | Runtime helper, not a game-action path. |
| `Player.IsChatHistoryReady()` | compiled getter | `compiled binding / inferred` | none visible | Read-only helper. |
| `Player.GetChatHistory()` | compiled getter | `compiled binding / inferred` | none visible | Read-only helper. |
| `Player.IsTyping()` | compiled getter | `compiled binding / inferred` | none visible | Read-only helper. |

### Visible low-level player helpers in `native_src/methods/PlayerMethods.py`

| Helper | Transport | Exact mechanism | Why it matters |
|---|---|---|---|
| `PlayerMethods.ChangeTarget` | `visible / UI message` | validates target and sends `UIMessage.kSendChangeTarget` with `UIManager.SendUIMessage(...)` inside `Game.enqueue` | Shows the intended packet path for target change. |
| `PlayerMethods.InteractAgent` | `visible / UI message` | classifies target as enemy / item / gadget / NPC / other, then sends `UIMessage.kSendWorldAction` with `[action_id, agent_id, call_target]` | This is Py4GW's clearest visible reproduction of GWCA-style world actions. |
| `PlayerMethods.Move` | `visible / direct native` | `MoveTo_Func.directCall(args)` | Internal movement call, not keyboard walking. |
| `PlayerMethods.SendChat` | `visible / UI message` | builds a `SendChatPacket` and sends `UIMessage.kSendChatMessage` raw | Exact outbound chat packet layout is visible. |
| `PlayerMethods.SendWhisper` | `visible / UI message` | builds the `\"target,message` form and sends `kSendChatMessage` raw | Confirms whispering is packet-based, not text-entry emulation. |
| `PlayerMethods.SendFakeChat` | `visible / UI message` | builds `UIChatMessage(channel, message, channel2)` and sends `kWriteToChatLog` | Fake chat is an internal log write. |
| `PlayerMethods.SendRawDialog` | `visible / UI message` | `UIManager.SendUIMessageRaw(UIMessage.kSendAgentDialog, dialog_id, 0)` | Raw dialog transport is fully visible. |
| `PlayerMethods.SendSkillTrainerDialog` | `visible / UI message` | computes trainer dialog ID and reuses raw dialog send | Skill buying is just dialog submission once the dialog ID is known. |

## Map and Travel Surface

### Public wrapper calls in `Py4GWCoreLib/Map.py`

| Py4GW call | Immediate implementation | Transport | GWCA analogue | Detailed behavior |
|---|---|---|---|---|
| `Map.SkipCinematic()` | queues `MapMethods.SkipCinematic` on the `TRANSITION` queue | `visible / direct native` | `GW::Map::SkipCinematic` | Uses a scanned `SkipCinematic_Func`; not a UI click. |
| `Map.Travel(map_id)` | queues closure calling `MapMethods.Travel(map_id, current_region, 0, current_language)` | `visible / UI message` | `GW::Map::Travel` via `kTravel` | Builds `TravelStruct(map_id, region, language, district_number)` and sends it raw. |
| `Map.TravelToDistrict(map_id, district, district_number)` | derives region/language from district enum, then calls `MapMethods.Travel` | `visible / UI message` | district-aware `GW::Map::Travel` overload | Mirrors GWCA's district-to-region/language branching. |
| `Map.TravelToRegion(map_id, server_region, district_number, language)` | queues `MapMethods.Travel` with explicit args | `visible / UI message` | explicit region-aware `GW::Map::Travel` overload | Same transport, but no inference from current district. |
| `Map.TravelGH()` | queues `MapMethods.TravelGH` | `visible / UI message` | `GW::GuildMgr::TravelGH` | Reads `GuildContext.player_gh_key` and sends it through `UIMessage.kGuildHall`. |
| `Map.LeaveGH()` | queues `MapMethods.LeaveGH` | `visible / UI message` | `GW::GuildMgr::LeaveGH` | Sends `UIMessage.kLeaveGuildHall`. |
| `Map.EnterChallenge()` | queues `MapMethods.EnterChallenge` | `visible / UI message` | `GW::Map::EnterChallenge` | Sends `UIMessage.kSendEnterMission`. |
| `Map.CancelEnterChallenge()` | queues closure that clicks `WindowFrames["CancelEnterMissionButton"]` | `UI-frame click fallback` | no manager call visible in Py4GW | Clear example of Py4GW using the frame tree rather than a known manager packet. |
| `Map.Pregame.LogoutToCharacterSelect()` | queues `MapMethods.LogouttoCharacterSelect` | `visible / UI message` | logout path using `kLogout` | Sends `UIMessage.kLogout` with `[0, 0]`. |

### Visible low-level map helpers in `native_src/methods/MapMethods.py`

| Helper | Transport | Exact mechanism | Notes |
|---|---|---|---|
| `MapMethods.SkipCinematic` | `visible / direct native` | direct call to scanner-resolved `SkipCinematic_Func` | Internal function call. |
| `MapMethods.Travel` | `visible / UI message` | sends raw `TravelStruct { map_id, region, language, district_number }` to `kTravel` | Exact packet layout is visible. |
| `MapMethods.TravelGH` | `visible / UI message` | sends address of live `player_gh_key` to `kGuildHall` | Py4GW can overwrite the live key buffer before dispatch if a custom key is supplied. |
| `MapMethods.LeaveGH` | `visible / UI message` | `UIManager.SendUIMessage(kLeaveGuildHall, [0], False)` | Thin wrapper. |
| `MapMethods.EnterChallenge` | `visible / UI message` | `UIManager.SendUIMessage(kSendEnterMission, [0], False)` | Arena/mission entry message. |
| `MapMethods.LogouttoCharacterSelect` | `visible / UI message` | queued `UIManager.SendUIMessage(kLogout, [0,0])` | Internal logout request, not menu navigation. |

## Skillbar Surface

### Public wrapper calls in `Py4GWCoreLib/Skillbar.py`

| Py4GW call | Immediate implementation | Transport | GWCA analogue | Detailed behavior |
|---|---|---|---|---|
| `SkillBar.LoadSkillTemplate(template)` | `PySkillbar.Skillbar().LoadSkillTemplate(template)` | `compiled binding / inferred` | `GW::SkillbarMgr::LoadSkillTemplate` | GWCA exposes the same call and even a hero overload. |
| `SkillBar.LoadHeroSkillTemplate(hero_index, template)` | compiled `LoadHeroSkillTemplate` | `compiled binding / inferred` | hero overload of `GW::SkillbarMgr::LoadSkillTemplate` | Same mechanism family, different target bar. |
| `SkillBar.UseSkill(skill_slot, target_agent_id=0)` | compiled `UseSkill` | `compiled binding / inferred` | `GW::SkillbarMgr::UseSkill` | Core cast path; exact body is opaque here but the analogue is strong. |
| `SkillBar.UseSkillTargetless(skill_slot)` | compiled `UseSkillTargetless` | `compiled binding / inferred` | likely `UseSkill(slot, 0, false)` | Convenience variant. |
| `SkillBar.HeroUseSkill(target_agent_id, skill_number, hero_number)` | compiled `HeroUseSkill` | `compiled binding / inferred` | hero-skill dispatch path | Exposed as skillbar control in Py4GW. |
| `SkillBar.ChangeHeroSecondary(hero_index, secondary_profession)` | compiled `ChangeHeroSecondary` | `compiled binding / inferred` | `ChangeSecondary` path in GWCA skillbar manager | Strong name and responsibility match. |

GWCA's `SkillbarMgr.cpp` shows scanning, hooks, `UseSkill`, and `LoadSkillTemplate`; Py4GW's skillbar bindings are almost the same surface, just exposed to Python.

## Party Surface

### Public wrapper calls in `Py4GWCoreLib/Party.py`

| Py4GW call | Immediate implementation | Transport | GWCA analogue | Detailed behavior |
|---|---|---|---|---|
| `Party.SetHardMode()` | compiled `PyParty.PyParty().SetHardMode(True)` after local checks | `compiled binding / inferred` | `GW::PartyMgr::SetHardMode` | Hard mode toggle. |
| `Party.SetNormalMode()` | compiled `SetHardMode(False)` | `compiled binding / inferred` | `GW::PartyMgr::SetHardMode(false)` | Reverse toggle. |
| `Party.ReturnToOutpost()` | compiled `ReturnToOutpost()` | `compiled binding / inferred` | `GW::PartyMgr::ReturnToOutpost` | Dedicated manager call, not a travel packet. |
| `Party.LeaveParty()` | compiled `LeaveParty()` | `compiled binding / inferred` | party leave path | Standard party-control binding. |
| `Party.SearchParty(...)` | compiled `SearchParty(...)` | `compiled binding / inferred` | party-search manager path | LFG/party search control. |
| `Party.SearchPartyCancel()` | compiled `SearchPartyCancel()` | `compiled binding / inferred` | party-search path | Search cancellation. |
| `Party.SearchPartyReply(accept)` | compiled `SearchPartyReply(accept)` | `compiled binding / inferred` | party-search reply path | Accept/decline response. |
| `Party.RespondToPartyRequest(party_id, accept)` | compiled `RespondToPartyRequest(...)` | `compiled binding / inferred` | party request flow | Request response path. |
| `Party.SetTicked(ticked)` | compiled `tick.SetTicked(ticked)` | `compiled binding / inferred` | party ready-check path | Ready status control. |
| `Party.ToggleTicked()` | wrapper logic then compiled `tick.SetTicked(...)` | `compiled binding / inferred` | ready-check path | Wrapper sidesteps a known faulty direct toggle. |
| `Party.Players.InvitePlayer(int_id)` | compiled `InvitePlayer(player_id)` | `compiled binding / inferred` | party invite manager path | Numeric invite route. |
| `Party.Players.InvitePlayer("name")` | `Player.SendChatCommand("invite " + name)` | `chat-command fallback` | slash command path | Name invites are not sent through a dedicated manager binding here. |
| `Party.Players.KickPlayer(login_number)` | compiled `KickPlayer(login_number)` | `compiled binding / inferred` | player kick path | Uses login number, not agent ID. |
| `Party.Heroes.AddHero(hero_id)` | compiled `AddHero(hero_id)` | `compiled binding / inferred` | hero roster add path | Adds a hero. |
| `Party.Heroes.KickHero(hero_id)` | compiled `KickHero(hero_id)` | `compiled binding / inferred` | hero roster remove path | Removes a hero. |
| `Party.Heroes.KickAllHeroes()` | compiled `KickAllHeroes()` | `compiled binding / inferred` | bulk hero clear path | Removes all heroes. |
| `Party.Heroes.UseSkill(hero_agent_id, slot, target_id)` | compiled `UseHeroSkill(...)` | `compiled binding / inferred` | hero-skill path | Wrapper comment says it had issues, but it still routes to the binding. |
| `Party.Heroes.FlagHero(hero_id, x, y)` | compiled `FlagHero(...)` | `compiled binding / inferred` | `GW::PartyMgr::FlagHero` | Hero flag placement. |
| `Party.Heroes.FlagAllHeroes(x, y)` | compiled `FlagAllHeroes(...)` | `compiled binding / inferred` | all-hero flag path | Group flag placement. |
| `Party.Heroes.UnflagHero(hero_id)` | compiled `UnflagHero(hero_id)` | `compiled binding / inferred` | hero unflag path | Clears one flag. |
| `Party.Heroes.UnflagAllHeroes()` | compiled `UnflagAllHeroes()` | `compiled binding / inferred` | all unflag path | Clears all flags. |
| `Party.Heroes.SetHeroBehavior(hero_agent_id, behavior)` | compiled `SetHeroBehavior(...)` | `compiled binding / inferred` | `GW::PartyMgr::SetHeroBehavior` | Fight/Guard/Avoid control. |
| `Party.Henchmen.AddHenchman(id)` | compiled `AddHenchman(id)` | `compiled binding / inferred` | henchman roster add path | Adds a henchman. |
| `Party.Henchmen.KickHenchman(id)` | compiled `KickHenchman(id)` | `compiled binding / inferred` | henchman roster remove path | Removes a henchman. |
| `Party.Pets.SetPetBehavior(behavior, lock_target_id)` | compiled `SetPetBehavior(...)` | `compiled binding / inferred` | `GW::PartyMgr::SetPetBehavior` | Pet behavior control. |

## Inventory and Item Surface

### Public wrapper calls in `Py4GWCoreLib/Inventory.py`

| Py4GW call | Immediate implementation | Transport | GWCA analogue | Detailed behavior |
|---|---|---|---|---|
| `Inventory.OpenXunlaiWindow()` | compiled `PyInventory.PyInventory().OpenXunlaiWindow()` | `compiled binding / inferred` | `GW::Items::OpenXunlaiWindow` | Dedicated storage-open path. |
| `Inventory.PickUpItem(item_id, call_target=False)` | compiled `PickUpItem(item_id, call_target)` | `compiled binding / inferred` | `GW::Items::PickUpItem` via agent/item manager | GWCA routes item pickup back through the agent/item path. |
| `Inventory.DropItem(item_id, quantity)` | compiled `DropItem(...)` | `compiled binding / inferred` | `GW::Items::DropItem` | Item drop path. |
| `Inventory.EquipItem(item_id, agent_id)` | compiled `EquipItem(...)` | `compiled binding / inferred` | `GW::Items::EquipItem` | Equip path on a player/hero agent. |
| `Inventory.UseItem(item_id)` | compiled `UseItem(item_id)` | `compiled binding / inferred` | `GW::Items::UseItem` and `kSendUseItem` | GWCA reroutes use-item through the internal UI-message pipeline. |
| `Inventory.DestroyItem(item_id)` | compiled `DestroyItem(item_id)` | `compiled binding / inferred` | item-destroy path not exposed in visible GWCA headers here | Likely a Py4GW-exposed internal function beyond this GWCA checkout's public surface. |
| `Inventory.IdentifyItem(item_id, id_kit_id)` | compiled `IdentifyItem(id_kit_id, item_id)` | `compiled binding / inferred` | `GW::Items::IdentifyItem` | Wrapper uses `(item, kit)` but the compiled binding expects `(kit, item)`. |
| `Inventory.DepositGold(amount)` | compiled `DepositGold(amount)` | `compiled binding / inferred` | `GW::Items::DepositGold` | Xunlai gold deposit. |
| `Inventory.WithdrawGold(amount)` | compiled `WithdrawGold(amount)` | `compiled binding / inferred` | `GW::Items::WithdrawGold` | Xunlai gold withdrawal. |
| `Inventory.DropGold(amount)` | compiled `DropGold(amount)` | `compiled binding / inferred` | `GW::Items::DropGold` | Gold drop path. |
| `Inventory.MoveItem(item_id, bag_id, slot, quantity)` | compiled `MoveItem(...)` | `compiled binding / inferred` | `GW::Items::MoveItem` and `kSendMoveItem` | GWCA encodes move-item as an internal item-move packet. |
| `Inventory.SalvageItem(item_id, salv_kit_id)` | compiled `Salvage(salv_kit_id, item_id)` | `compiled binding / inferred` | salvage manager path | Wrapper uses `(item, kit)` while the binding expects `(kit, item)`. |
| `Inventory.AcceptSalvageMaterialsWindow()` | `UIManager.GetChildFrameID(...)` then `UIManager.FrameClick(...)` | `UI-frame click fallback` | not a manager call | Hard fallback that clicks the salvage confirmation frame instead of calling a dedicated native path. |

The deposit/withdraw helper routines built later in `Inventory.py` are orchestration only. They inspect bags, merge stacks, and repeatedly call `Inventory.MoveItem(...)`; the real game actuation is still the compiled item-move binding.

## Merchant, Trader, Crafter, Collector Surface

### Public wrapper calls in `Py4GWCoreLib/Merchant.py`

| Py4GW call | Immediate implementation | Transport | GWCA analogue | Detailed behavior |
|---|---|---|---|---|
| `Trading.Trader.RequestQuote(item_id)` | compiled `trader_request_quote(item_id)` | `compiled binding / inferred` | `GW::Merchant::RequestQuote` using `kSendMerchantRequestQuote` | Trader buy-side quote request. |
| `Trading.Trader.RequestSellQuote(item_id)` | compiled `trader_request_sell_quote(item_id)` | `compiled binding / inferred` | trader sell-quote path | Same family, sell side. |
| `Trading.Trader.BuyItem(item_id, cost)` | compiled `trader_buy_item(item_id, cost)` | `compiled binding / inferred` | merchant transact path via `kSendMerchantTransactItem` | Trader purchase completion. |
| `Trading.Trader.SellItem(item_id, cost)` | compiled `trader_sell_item(item_id, cost)` | `compiled binding / inferred` | merchant transact path | Trader sale completion. |
| `Trading.Merchant.BuyItem(item_id, cost)` | compiled `merchant_buy_item(item_id, cost)` | `compiled binding / inferred` | merchant transact item path | Standard merchant purchase. |
| `Trading.Merchant.SellItem(item_id, cost)` | compiled `merchant_sell_item(item_id, cost)` | `compiled binding / inferred` | merchant transact item path | Standard merchant sale. |
| `Trading.Crafter.CraftItem(item_id, cost, item_list, item_quantities)` | compiled `crafter_buy_item(...)` | `compiled binding / inferred` | crafter transact path | Sends gold plus ingredient arrays. |
| `Trading.Collector.ExghangeItem(item_id, cost, item_list, item_quantities)` | compiled `collector_buy_item(...)` | `compiled binding / inferred` | collector transact path | Same pattern, collector flavor. |

GWCA's `MerchantMgr.cpp` explicitly exposes quote requests through `kSendMerchantRequestQuote` and transact-item sends through `kSendMerchantTransactItem`, so the Py4GW merchant surface maps very cleanly onto that manager family.

## Quest Surface

### Public wrapper calls in `Py4GWCoreLib/Quest.py`

| Py4GW call | Immediate implementation | Transport | GWCA analogue | Detailed behavior |
|---|---|---|---|---|
| `Quest.SetActiveQuest(quest_id)` | compiled `set_active_quest_id(quest_id)` | `compiled binding / inferred` | `GW::QuestMgr::SetActiveQuestId` and `kSendSetActiveQuest` | Dedicated active-quest setter. |
| `Quest.AbandonQuest(quest_id)` | compiled `abandon_quest_id(quest_id)` | `compiled binding / inferred` | `GW::QuestMgr::AbandonQuestId` and `kSendAbandonQuest` | Dedicated abandon path. |
| `Quest.RequestQuestInfo(quest_id, update_marker=False)` | compiled `request_quest_info(...)` | `compiled binding / inferred` | broader quest-info request surface not shown in visible GWCA source | Py4GW exposes more quest metadata helpers than are obvious in this GWCA checkout. |
| `Quest.RequestQuestName(quest_id)` | compiled request | `compiled binding / inferred` | opaque helper | Async data request. |
| `Quest.RequestQuestDescription(quest_id)` | compiled request | `compiled binding / inferred` | opaque helper | Async data request. |
| `Quest.RequestQuestObjectives(quest_id)` | compiled request | `compiled binding / inferred` | opaque helper | Async data request. |
| `Quest.RequestQuestLocation(quest_id)` | compiled request | `compiled binding / inferred` | opaque helper | Async data request. |
| `Quest.RequestQuestNPC(quest_id)` | compiled request | `compiled binding / inferred` | opaque helper | Async data request. |

The rest of the quest wrapper is read-only lookup or readiness checking, not actuation.

## UIManager and Frame-Level Control

### Exposed actuation calls in `Py4GWCoreLib/UIManager.py`

| Py4GW call | Transport | Meaning |
|---|---|---|
| `UIManager.SendUIMessage(...)` | `compiled binding / inferred`, but thin and explicit | High-level typed internal UI message send. |
| `UIManager.SendUIMessageRaw(...)` | `compiled binding / inferred`, but thin and explicit | Raw message send with numeric `wparam` and `lparam`. |
| `UIManager.FrameClick(frame_id)` | `UI-frame click fallback` | Calls `PyUIManager.UIManager.button_click(frame_id)`. |
| `UIManager.TestMouseAction(...)` | `UI-frame click fallback` | Synthetic mouse action against a frame. |
| `UIManager.TestMouseClickAction(...)` | `UI-frame click fallback` | Synthetic click action against a frame. |
| `UIManager.Keydown/Keyup/Keypress(...)` | `compiled binding / inferred` | Sends UI key events scoped to a frame. |
| `UIManager.SetEnum/Int/String/BoolPreference(...)` | `compiled binding / inferred` | Internal config/UI mutation. |
| `UIManager.SetWindowVisible(...)` | `compiled binding / inferred` | Show/hide named game windows. |
| `UIManager.SetWindowPosition(...)` | `compiled binding / inferred` | Move named game windows. |
| `UIManager.ClickDialogButton(choice)` | `UI-frame click fallback` | Discovers dialog button frames, then clicks the Nth visible one. |
| `UIManager.ConfirmMaxAmountDialog()` | `UI-frame click fallback` | Clicks max-amount and drop-offer confirmation frames if visible. |

This is important because Py4GW does not insist on a single control style. It can send manager-style packets when known, but it also supports raw UI-tree automation when that is more practical.

## Keystroke Surface

Py4GW also exposes `PyKeystroke` with:

- `PressKey`
- `ReleaseKey`
- `PushKey`
- combo variants

This is a real input-injection path, but it is clearly not the primary mechanism for core actions like travel, dialog submission, item movement, merchant transactions, quest selection, or direct coordinate movement. Those mostly use internal packets or native calls instead.

## Action Queues, Routines, and Bot Orchestration

`ActionQueueManager` is the pacing layer that turns raw calls into bot-safe behavior.

Queues visible in source:

- `ACTION` at 50 ms
- `LOOT` at 1250 ms
- `MERCHANT` at 750 ms
- `SALVAGE` at 125 ms
- `IDENTIFY` at 150 ms
- `FAST` at 20 ms
- `TRANSITION` at 50 ms

So a typical bot action path is:

`bot logic -> ActionQueueManager -> wrapper call -> Game.enqueue or compiled binding -> GW internal actuation`

That pacing matters because it prevents the framework from hammering merchant, loot, movement, or transition actions at unrealistic rates.

### Routines

Py4GW's higher-level routine layer does not invent new low-level game actions. It sequences the mapped calls above.

Examples:

- `Routines.Movement.FollowXY` repeatedly calls `Player.Move(...)`
- sequential routines chain movement, travel, dialog, loot, identify, and salvage steps
- example bots process the action queues every frame

### Combat events

`CombatEvents.py` says its C++ side hooks combat packets via GWCA-style infrastructure and feeds Python a thread-safe queue of events. That gives bots reactive timing data such as:

- cast start/end
- recharge start/end
- attacks
- knockdowns
- damage events

That is another sign Py4GW is operating in the same internal-hook universe as GWCA rather than as a pure external macro.

### Shared memory and multiboxing

`GlobalCache/SharedMemory.py` publishes player, hero, and pet state into shared memory. The multibox command enums include actions such as:

- `TravelToMap`
- `InviteToParty`
- `InteractWithTarget`
- `TakeDialogWithTarget`
- `OpenChest`
- `PickUpLoot`
- `UseSkill`
- `Resign`
- `IdentifyItems`
- `SalvageItems`
- `LoadSkillTemplate`
- `SendDialog`
- `TravelToGuildHall`

Those multibox commands are coordination only. The actual local actuation still goes back through the same wrapper calls mapped in this document.

## What "every call" can and cannot mean here

### Directly mappable from source

This document directly maps:

- the visible public wrapper actions in `Player.py`, `Map.py`, `Skillbar.py`, `Party.py`, `Inventory.py`, `Merchant.py`, and `Quest.py`,
- the visible low-level helpers in `native_src/methods/PlayerMethods.py` and `MapMethods.py`,
- the visible frame-click/UI-tree fallbacks in `UIManager.py`,
- the matching manager and UI-message shapes in the bundled GWCA source.

### Still inferred

For compiled bindings like:

- `PyParty.PyParty().SetHardMode`
- `PyInventory.PyInventory().MoveItem`
- `PyQuest.PyQuest().set_active_quest_id`
- `PyMerchant.PyMerchant().trader_request_quote`
- `PySkillbar.Skillbar().UseSkill`

the exact C++ body is not in this checkout, so the final step is inferred from:

- the binding name,
- the stub signature,
- the surrounding Python wrapper behavior,
- and the matching GWCA manager surface in `GWCA-master`.

## Practical Conclusions

1. Py4GW's real bot-control surface is mostly internal packet/native actuation, not keyboard emulation.
2. The strongest visible GWCA matches are:
   - scanner rebasing,
   - game-thread enqueue,
   - UI-message transport,
   - world-action interaction,
   - direct move-to-coordinate,
   - map travel,
   - title changes,
   - dialog/chat transport.
3. Party, inventory, merchant, quest, and skillbar control are mostly delegated to compiled bindings whose names line up tightly with GWCA manager APIs.
4. When Py4GW lacks a clean native path, it sometimes falls back to UI-frame automation instead, especially for:
   - challenge cancel,
   - salvage confirmation,
   - dialog button choice.

## Best Evidence Files

- `GWA Censured/Py4GW-main/Py4GW_Launcher.py`
- `GWA Censured/Py4GW-main/Py4GWCoreLib/Player.py`
- `GWA Censured/Py4GW-main/Py4GWCoreLib/Map.py`
- `GWA Censured/Py4GW-main/Py4GWCoreLib/Skillbar.py`
- `GWA Censured/Py4GW-main/Py4GWCoreLib/Party.py`
- `GWA Censured/Py4GW-main/Py4GWCoreLib/Inventory.py`
- `GWA Censured/Py4GW-main/Py4GWCoreLib/Merchant.py`
- `GWA Censured/Py4GW-main/Py4GWCoreLib/Quest.py`
- `GWA Censured/Py4GW-main/Py4GWCoreLib/UIManager.py`
- `GWA Censured/Py4GW-main/Py4GWCoreLib/native_src/methods/PlayerMethods.py`
- `GWA Censured/Py4GW-main/Py4GWCoreLib/native_src/methods/MapMethods.py`
- `GWA Censured/Py4GW-main/Py4GWCoreLib/py4gwcorelib_src/ActionQueue.py`
- `GWA Censured/Py4GW-main/Py4GWCoreLib/GlobalCache/SharedMemory.py`
- `GWA Censured/Py4GW-main/stubs/PyPlayer.pyi`
- `GWA Censured/Py4GW-main/stubs/PySkillbar.pyi`
- `GWA Censured/Py4GW-main/stubs/PyParty.pyi`
- `GWA Censured/Py4GW-main/stubs/PyInventory.pyi`
- `GWA Censured/Py4GW-main/stubs/PyMerchant.pyi`
- `GWA Censured/Py4GW-main/stubs/PyQuest.pyi`
- `GWA Censured/GWCA-master/Include/GWCA/Managers/UIMgr.h`
- `GWA Censured/GWCA-master/Source/UIMgr.cpp`
- `GWA Censured/GWCA-master/Source/GameThreadMgr.cpp`
- `GWA Censured/GWCA-master/Source/AgentMgr.cpp`
- `GWA Censured/GWCA-master/Source/SkillbarMgr.cpp`
- `GWA Censured/GWCA-master/Source/ItemMgr.cpp`
- `GWA Censured/GWCA-master/Source/MerchantMgr.cpp`
- `GWA Censured/GWCA-master/Source/QuestMgr.cpp`
- `GWA Censured/GWCA-master/Source/PartyMgr.cpp`
- `GWA Censured/GWCA-master/Source/MapMgr.cpp`
- `GWA Censured/GWCA-master/Source/GuildMgr.cpp`

## Appendix A: Exact Source Anchors

This section pins the major mappings to specific source lines so you can audit the chain quickly.

### A1. Game thread and message constants

| Concept | Py4GW / GWCA anchor |
|---|---|
| game-thread enqueue analogue | `Py4GW.Game.enqueue` is the Py4GW surface; GWCA anchor is `Include/GWCA/Managers/GameThreadMgr.h:17` and `Source/GameThreadMgr.cpp:139` |
| recurring game-thread callbacks | GWCA anchor `Include/GWCA/Managers/GameThreadMgr.h:20` and `Source/GameThreadMgr.cpp:163` |
| `kChangeTarget` | `Include/GWCA/Managers/UIMgr.h:119` |
| `kWriteToChatLog` | `Include/GWCA/Managers/UIMgr.h:136` |
| `kLogout` | `Include/GWCA/Managers/UIMgr.h:141` |
| `kGuildHall` | `Include/GWCA/Managers/UIMgr.h:173` |
| `kLeaveGuildHall` | `Include/GWCA/Managers/UIMgr.h:174` |
| `kTravel` | `Include/GWCA/Managers/UIMgr.h:175` |
| `kSendDialog` | `Include/GWCA/Managers/UIMgr.h:185` |
| `kSendEnterMission` | `Include/GWCA/Managers/UIMgr.h:186` |
| `kSendMoveItem` | `Include/GWCA/Managers/UIMgr.h:189` |
| `kSendMerchantRequestQuote` | `Include/GWCA/Managers/UIMgr.h:190` |
| `kSendMerchantTransactItem` | `Include/GWCA/Managers/UIMgr.h:191` |
| `kSendUseItem` | `Include/GWCA/Managers/UIMgr.h:192` |
| `kSendSetActiveQuest` | `Include/GWCA/Managers/UIMgr.h:193` |
| `kSendAbandonQuest` | `Include/GWCA/Managers/UIMgr.h:194` |

### A2. Player call chain anchors

| Action | Py4GW wrapper | Visible helper | GWCA analogue |
|---|---|---|---|
| `ChangeTarget` | `Player.py:561` | `native_src/methods/PlayerMethods.py:76` | `AgentMgr.h:91-92`, `AgentMgr.cpp:278` |
| `Interact` | `Player.py:575` | `native_src/methods/PlayerMethods.py:87` | `AgentMgr.cpp:78`, `AgentMgr.cpp:149-151` |
| `Move` | `Player.py:592` | `native_src/methods/PlayerMethods.py:130` | `AgentMgr.h:96-97`, `AgentMgr.cpp:292-300` |
| `DepositFaction` | `Player.py:605` | `native_src/methods/PlayerMethods.py:146` | no direct public GWCA header hit in this checkout; visible only on Py4GW side |
| `RemoveActiveTitle` | `Player.py:617` | `native_src/methods/PlayerMethods.py:164` | Py4GW-visible direct title call only |
| `SetActiveTitle` | `Player.py:628` | `native_src/methods/PlayerMethods.py:155` | Py4GW-visible direct title call only |
| `SendRawDialog` | `Player.py:640` | `native_src/methods/PlayerMethods.py:386` | `UIMgr.h:185` for `kSendDialog`, plus raw agent-dialog use is Py4GW-visible |
| `BuySkill` | `Player.py:648` | `native_src/methods/PlayerMethods.py:400` | same raw dialog family; skill buying is a dialog conversion on Py4GW side |
| `SendDialog` | `Player.py:658` | none used by wrapper | `AgentMgr.h:37`, `AgentMgr.cpp:36`, `AgentMgr.cpp:48`, `AgentMgr.cpp:166`, `AgentMgr.cpp:260` |
| `SendChatCommand` | `Player.py:713` | `native_src/methods/PlayerMethods.py:300` | chat path ultimately tied to `kWriteToChatLog` / `kSendChatMessage` style UI sends on Py4GW side |
| `SendChat` | `Player.py:724` | `native_src/methods/PlayerMethods.py:173` | Py4GW-visible raw chat packet send |
| `SendWhisper` | `Player.py:736` | `native_src/methods/PlayerMethods.py:244` | Py4GW-visible raw chat packet send |
| `SendFakeChat` | `Player.py:748` | `native_src/methods/PlayerMethods.py:308` | `UIMgr.h:136` for `kWriteToChatLog` |
| `SendFakeChatColored` | `Player.py:760` | uses compiled `SendFakeChat` wrapper after formatting | `UIMgr.h:136` for `kWriteToChatLog` |

### A3. Map call chain anchors

| Action | Py4GW wrapper | Visible helper | GWCA analogue |
|---|---|---|---|
| `SkipCinematic` | `Map.py:707` | `native_src/methods/MapMethods.py:58` | direct internal call on Py4GW side |
| `Travel` | `Map.py:715` | `native_src/methods/MapMethods.py:67` | `MapMgr.h:65-67`, `MapMgr.cpp:189`, `MapMgr.cpp:205` |
| `TravelToDistrict` | `Map.py:723` | uses `MapMethods.Travel` | `MapMgr.cpp:205-232` |
| `TravelToRegion` | `Map.py:787` | uses `MapMethods.Travel` | `MapMgr.h:65-67`, `MapMgr.cpp:189` |
| `TravelGH` | `Map.py:803` | `native_src/methods/MapMethods.py:83` | `GuildMgr.h:38-40`, `GuildMgr.cpp:65-71` |
| `LeaveGH` | `Map.py:811` | `native_src/methods/MapMethods.py:111` | `GuildMgr.h:42`, `GuildMgr.cpp:75` |
| `EnterChallenge` | `Map.py:818` | `native_src/methods/MapMethods.py:120` | `MapMgr.h:90`, `MapMgr.cpp:319` |
| `CancelEnterChallenge` | `Map.py:825` | no low-level helper; frame click only | `MapMgr.h:91`, `MapMgr.cpp:323` |
| `LogoutToCharacterSelect` | `Map.py:2005` | `native_src/methods/MapMethods.py:129` | `UIMgr.h:141` for `kLogout` |

### A4. Skillbar anchors

| Action | Py4GW wrapper | GWCA analogue |
|---|---|---|
| `LoadSkillTemplate` | `Skillbar.py:8` | `SkillbarMgr.h:73`, `SkillbarMgr.cpp:476` |
| `LoadHeroSkillTemplate` | `Skillbar.py:19` | `SkillbarMgr.h:74`, `SkillbarMgr.cpp:523` |
| `UseSkill` | `Skillbar.py:64` | `SkillbarMgr.h:42`, `SkillbarMgr.cpp:611` |
| `HeroUseSkill` | `Skillbar.py:87` | same skillbar manager family; hero-specific compiled binding |
| `ChangeHeroSecondary` | `Skillbar.py:99` | `SkillbarMgr.cpp:126-127`, `SkillbarMgr.cpp:158`, `SkillbarMgr.cpp:274-277` |

### A5. Party anchors

| Action | Py4GW wrapper | GWCA analogue |
|---|---|---|
| `SetTicked` | `Party.py:243` | compiled binding only |
| `ToggleTicked` | `Party.py:252` | compiled binding only |
| `SetHardMode` | `Party.py:269` | `PartyMgr.h:39`, `PartyMgr.cpp:273` |
| `SetNormalMode` | `Party.py:278` | `PartyMgr.h:39`, `PartyMgr.cpp:273` |
| `SearchParty` | `Party.py:288` | compiled binding only |
| `SearchPartyCancel` | `Party.py:299` | compiled binding only |
| `SearchPartyReply` | `Party.py:308` | compiled binding only |
| `RespondToPartyRequest` | `Party.py:318` | compiled binding only |
| `ReturnToOutpost` | `Party.py:329` | `PartyMgr.h:42`, `PartyMgr.cpp:234` |
| `LeaveParty` | `Party.py:337` | compiled binding only |
| `InvitePlayer` | `Party.py:398` | compiled binding for numeric route; chat-command fallback for string route |
| `KickPlayer` | `Party.py:413` | compiled binding only |
| `AddHero` | `Party.py:496` | compiled binding only |
| `KickHero` | `Party.py:515` | compiled binding only |
| `KickAllHeroes` | `Party.py:534` | compiled binding only |
| `Hero UseSkill` | `Party.py:542` | compiled binding only |
| `FlagHero` | `Party.py:553` | `PartyMgr.h:76-77`, `PartyMgr.cpp:406-410` |
| `FlagAllHeroes` | `Party.py:564` | same flagging family |
| `UnflagHero` | `Party.py:574` | same flagging family |
| `UnflagAllHeroes` | `Party.py:583` | same flagging family |
| `SetHeroBehavior` | `Party.py:619` | `PartyMgr.h:82`, `PartyMgr.cpp:435` |
| `AddHenchman` | `Party.py:630` | compiled binding only |
| `KickHenchman` | `Party.py:639` | compiled binding only |
| `SetPetBehavior` | `Party.py:649` | `PartyMgr.h:84`, `PartyMgr.cpp:449` |

### A6. Inventory anchors

| Action | Py4GW wrapper | GWCA analogue |
|---|---|---|
| `IdentifyItem` | `Inventory.py:245` | `ItemMgr.h:82`, `ItemMgr.cpp:458` |
| `SalvageItem` | `Inventory.py:285` | compiled binding only in this checkout |
| `AcceptSalvageMaterialsWindow` | `Inventory.py:328` | UI-frame click fallback only |
| `OpenXunlaiWindow` | `Inventory.py:345` | `ItemMgr.h:73`, `ItemMgr.cpp:337` |
| `PickUpItem` | `Inventory.py:362` | `ItemMgr.h:69`, `ItemMgr.cpp:345` |
| `DropItem` | `Inventory.py:373` | compiled binding only; GWCA source has drop-item path nearby but not exported in header lines cited here |
| `EquipItem` | `Inventory.py:384` | compiled binding only |
| `UseItem` | `Inventory.py:395` | `ItemMgr.h:57`, `ItemMgr.cpp:77`, `ItemMgr.cpp:374` |
| `DestroyItem` | `Inventory.py:405` | compiled binding only |
| `DepositGold` | `Inventory.py:442` | `ItemMgr.h:100`, `ItemMgr.cpp:479` |
| `WithdrawGold` | `Inventory.py:452` | `ItemMgr.h:103`, `ItemMgr.cpp:498` |
| `DropGold` | `Inventory.py:462` | `ItemMgr.h:76`, `ItemMgr.cpp:435` |
| `MoveItem` | `Inventory.py:472` | `ItemMgr.h:110-112`, `ItemMgr.cpp:113`, `ItemMgr.cpp:534-549` |

### A7. Merchant anchors

| Action | Py4GW wrapper | GWCA analogue |
|---|---|---|
| `RequestQuote` | `Merchant.py:66` | `MerchantMgr.h:46`, `MerchantMgr.cpp:73`, `MerchantMgr.cpp:79`, `MerchantMgr.cpp:152` |
| `RequestSellQuote` | `Merchant.py:76` | same merchant quote family |
| `Trader BuyItem` | `Merchant.py:86` | `MerchantMgr.cpp:45`, `MerchantMgr.cpp:105` |
| `Trader SellItem` | `Merchant.py:98` | `MerchantMgr.cpp:45`, `MerchantMgr.cpp:105` |
| `Merchant BuyItem` | `Merchant.py:111` | `MerchantMgr.cpp:45`, `MerchantMgr.cpp:105` |
| `Merchant SellItem` | `Merchant.py:123` | `MerchantMgr.cpp:45`, `MerchantMgr.cpp:105` |
| `CraftItem` | `Merchant.py:146` | same transact-item family, compiled binding specifics opaque |
| `ExghangeItem` | `Merchant.py:170` | same transact-item family, compiled binding specifics opaque |

### A8. Quest anchors

| Action | Py4GW wrapper | GWCA analogue |
|---|---|---|
| `SetActiveQuest` | `Quest.py:18` | `QuestMgr.h:22`, `QuestMgr.cpp:28`, `QuestMgr.cpp:70`, `QuestMgr.cpp:117` |
| `AbandonQuest` | `Quest.py:28` | `QuestMgr.h:30`, `QuestMgr.cpp:44`, `QuestMgr.cpp:64`, `QuestMgr.cpp:168` |
| `RequestQuestInfo` | `Quest.py:95` | compiled binding only |
| `RequestQuestName` | `Quest.py:106` | compiled binding only |
| `RequestQuestDescription` | `Quest.py:134` | compiled binding only |
| `RequestQuestObjectives` | `Quest.py:162` | compiled binding only |
| `RequestQuestLocation` | `Quest.py:190` | compiled binding only |
| `RequestQuestNPC` | `Quest.py:218` | compiled binding only |

### A9. UIManager anchors

| Action | Py4GW wrapper |
|---|---|
| `SendUIMessage` | `UIManager.py:383` |
| `SendUIMessageRaw` | `UIManager.py:387` |
| `FrameClick` | `UIManager.py:391` |
| `TestMouseAction` | `UIManager.py:402` |
| `TestMouseClickAction` | `UIManager.py:415` |
| `SetEnumPreference` | `UIManager.py:713` |
| `SetIntPreference` | `UIManager.py:717` |
| `SetStringPreference` | `UIManager.py:721` |
| `SetBoolPreference` | `UIManager.py:725` |
| `Keydown` | `UIManager.py:737` |
| `Keyup` | `UIManager.py:741` |
| `Keypress` | `UIManager.py:745` |
| `SetWindowVisible` | `UIManager.py:767` |
| `SetWindowPosition` | `UIManager.py:777` |
| `ClickDialogButton` | `UIManager.py:913` |
| `ConfirmMaxAmountDialog` | `UIManager.py:1091` |

## Appendix B: Execution Traces for High-Value Bot Actions

This section rewrites the highest-value calls as execution pipelines.

### B1. `Player.Move(x, y, zPlane)`

Trace:

1. `Player.Move` queues `PlayerMethods.Move` onto the `ACTION` queue.
   - anchor: `Player.py:592`
   - queue anchor: `ActionQueue.py:208`
2. `ActionQueueManager.ProcessQueue("ACTION")` eventually executes the queued function once the 50 ms throttle allows it.
   - queue definition anchor: `ActionQueue.py` queue init block
   - process anchor: `ActionQueue.py:239`
3. `PlayerMethods.Move` wraps the work inside `Game.enqueue`.
   - anchor: `native_src/methods/PlayerMethods.py:130`
4. On the game thread, Py4GW checks `MoveTo_Func.is_valid()`, builds a 4-float buffer `(x, y, zPlane, 0.0)`, and calls `MoveTo_Func.directCall(args)`.
   - same anchor: `native_src/methods/PlayerMethods.py:130`
5. GWCA analogue: `GW::Agents::Move`.
   - anchors: `AgentMgr.h:96-97`, `AgentMgr.cpp:292-300`

Why this matters:

- This is a real internal move call.
- It is not a `W` key press.
- It is paced by Py4GW's action queue and executed on the game thread.

### B2. `Routines.Movement.FollowXY.move_to_waypoint(x, y)`

Trace:

1. `FollowXY.move_to_waypoint` sets the waypoint and immediately calls `Player.Move(x, y)`.
   - anchor: `routines_src/Movement.py:58-76`
2. `Player.Move` goes through the trace in `B1`.
3. `FollowXY.update` later checks:
   - current player position,
   - casting,
   - moving,
   - knockdown,
   - dead state.
   - anchor: `routines_src/Movement.py:85-113`
4. If movement stalls, it reissues:
   - `Player.Move(0, 0)` to reset the move pointer,
   - `Player.Move(self.waypoint[0], self.waypoint[1])`
   - anchor: `routines_src/Movement.py:133-134`

Why this matters:

- The routine layer is not a separate movement transport.
- It is a supervisory loop around the same internal move call.

### B3. `Player.Interact(agent_id, call_target)`

Trace:

1. `Player.Interact` adds a closure to the `ACTION` queue.
   - anchor: `Player.py:575`
2. When processed, that closure calls compiled `PyPlayer.PyPlayer().InteractAgent(agent_id, call_target)`.
   - same anchor: `Player.py:575`
3. The visible low-level analogue is `PlayerMethods.InteractAgent`.
   - anchor: `native_src/methods/PlayerMethods.py:87`
4. That helper:
   - reads the target agent,
   - classifies it as enemy / item / gadget / npc / other,
   - builds `[action_id, agent_id, call_target]`,
   - sends `UIMessage.kSendWorldAction` inside `Game.enqueue`.
   - same anchor: `native_src/methods/PlayerMethods.py:87`
5. GWCA analogue is the agent interaction family around `InteractAgent_Func`.
   - anchors: `AgentMgr.cpp:78`, `AgentMgr.cpp:149-151`

Why this matters:

- Even though the public wrapper uses the compiled binding, the visible Py4GW low-level code shows that the intended transport is a world-action UI message, not a mouse click.

### B4. `Player.ChangeTarget(agent_id)`

Trace:

1. `Player.ChangeTarget` adds a closure to the `ACTION` queue.
   - anchor: `Player.py:561`
2. That closure calls compiled `PyPlayer.PyPlayer().ChangeTarget(agent_id)`.
   - same anchor: `Player.py:561`
3. The visible low-level analogue is `PlayerMethods.ChangeTarget`.
   - anchor: `native_src/methods/PlayerMethods.py:76`
4. That helper validates the agent and sends `UIMessage.kSendChangeTarget` through `UIManager.SendUIMessage(...)` inside `Game.enqueue`.
   - same anchor: `native_src/methods/PlayerMethods.py:76`
5. GWCA analogue is `GW::Agents::ChangeTarget`.
   - anchors: `AgentMgr.h:91-92`, `AgentMgr.cpp:278`
6. Related UI message constant:
   - `UIMgr.h:119` for `kChangeTarget`

### B5. `Player.SendDialog(dialog_id)`

Trace:

1. `Player.SendDialog` normalizes hex-string or int input and queues compiled `PyPlayer.PyPlayer().SendDialog(dialog)`.
   - anchor: `Player.py:658`
2. GWCA shows what this path does conceptually:
   - hook `SendDialog_Func`,
   - reroute into `UI::SendUIMessage(kSendDialog, dialog_id)`,
   - catch it in `OnSendDialog_UIMessage`,
   - then dispatch either normal dialog or signpost dialog.
   - anchors: `AgentMgr.cpp:36`, `AgentMgr.cpp:48`, `AgentMgr.cpp:166`, `AgentMgr.cpp:260`
3. Related message constant:
   - `UIMgr.h:185` for `kSendDialog`

Why this matters:

- This is one of the strongest “Py4GW follows GWCA manager design” examples.

### B6. `Player.SendRawDialog(dialog_id)`

Trace:

1. `Player.SendRawDialog` queues `PlayerMethods.SendRawDialog` on the `ACTION` queue.
   - anchor: `Player.py:640`
2. `PlayerMethods.SendRawDialog` wraps the send in `Game.enqueue`.
   - anchor: `native_src/methods/PlayerMethods.py:386`
3. On the game thread, it calls:
   - `UIManager.SendUIMessageRaw(UIMessage.kSendAgentDialog, dialog_id, 0)`
   - same anchor: `native_src/methods/PlayerMethods.py:386`

Why this matters:

- This is a more literal raw-packet dialog send than the compiled `SendDialog` wrapper path.
- Py4GW uses it for skill trainer and similar dialog-driven flows.

### B7. `Player.BuySkill(skill_id)`

Trace:

1. `Player.BuySkill` queues `PlayerMethods.SendSkillTrainerDialog`.
   - anchor: `Player.py:648`
2. `SendSkillTrainerDialog` converts the skill ID into a dialog ID with `Utils.SkillIdToDialogId(skill_id)`.
   - anchor: `native_src/methods/PlayerMethods.py:400`
3. It then calls `PlayerMethods.SendRawDialog(dialog_skill_id)`.
   - same anchor: `native_src/methods/PlayerMethods.py:400`
4. The rest of the pipeline is the raw agent-dialog trace from `B6`.

Why this matters:

- Buying skills is not a special merchant-like transaction in visible Py4GW code.
- It is expressed as “compute the trainer dialog ID, then send that dialog.”

### B8. `Player.SendChat(channel, message)`

Trace:

1. Public wrapper queues compiled `PyPlayer.PyPlayer().SendChat(...)`.
   - anchor: `Player.py:724`
2. The visible low-level analogue is `PlayerMethods.SendChat`.
   - anchor: `native_src/methods/PlayerMethods.py:173`
3. That helper:
   - validates channel,
   - widens the message,
   - writes it into a 140-wide-char buffer,
   - constructs a `SendChatPacket { message, agent_id }`,
   - sends it with `UIManager.SendUIMessageRaw(UIMessage.kSendChatMessage, ctypes.addressof(packet), False)` inside `Game.enqueue`.
   - same anchor: `native_src/methods/PlayerMethods.py:173`

Why this matters:

- Normal chat is internal packet dispatch, not keyboard text entry.

### B9. `Player.SendWhisper(name, message)`

Trace:

1. Public wrapper queues compiled `PyPlayer.PyPlayer().SendWhisper(...)`.
   - anchor: `Player.py:736`
2. Visible analogue is `PlayerMethods.SendWhisper`.
   - anchor: `native_src/methods/PlayerMethods.py:244`
3. That helper:
   - formats the payload as `\"name,message`,
   - writes it into a 140-wide-char buffer,
   - wraps it in a `SendChatPacket`,
   - sends `UIMessage.kSendChatMessage` inside `Game.enqueue`.
   - same anchor: `native_src/methods/PlayerMethods.py:244`

### B10. `Player.SendFakeChat(channel, message)`

Trace:

1. Public wrapper queues compiled `PyPlayer.PyPlayer().SendFakeChat(...)`.
   - anchor: `Player.py:748`
2. Visible analogue is `PlayerMethods.SendFakeChat`.
   - anchor: `native_src/methods/PlayerMethods.py:308`
3. That helper:
   - widens UTF-8 bytes one byte at a time,
   - wraps the message in GW chat markup,
   - builds `UIChatMessage { channel, message, channel2 }`,
   - sends `UIMessage.kWriteToChatLog` inside `Game.enqueue`.
   - same anchor: `native_src/methods/PlayerMethods.py:308`
4. Related message constant:
   - `UIMgr.h:136`

Why this matters:

- Fake chat is explicitly a log-write path, not a public chat send.

### B11. `Map.Travel(map_id)`

Trace:

1. `Map.Travel` queues a closure on the `ACTION` queue.
   - anchor: `Map.py:715`
2. The closure calls `MapMethods.Travel(map_id, Map.GetRegion()[0], 0, Map.GetLanguage()[0])`.
   - same anchor: `Map.py:715`
3. `MapMethods.Travel` builds:
   - `TravelStruct { map_id, region, language, district_number }`
   - and sends it with `UIManager.SendUIMessageRaw(UIMessage.kTravel, ctypes.addressof(TravelStruct(...)), False)`.
   - anchor: `native_src/methods/MapMethods.py:67`
4. GWCA analogue:
   - `MapMgr.h:65-67`
   - `MapMgr.cpp:189`

### B12. `Map.TravelToDistrict(...)`

Trace:

1. `Map.TravelToDistrict` computes `server_region` and `language` from the district enum locally.
   - anchor: `Map.py:723`
2. It queues a closure that calls `MapMethods.Travel(map_id, region, district_number, language)`.
   - same anchor: `Map.py:723`
3. The low-level send is the same travel packet as `B11`.
4. GWCA analogue:
   - `MapMgr.cpp:205-232`

Why this matters:

- Py4GW visibly recreates GWCA's district-routing logic in Python before sending the final travel packet.

### B13. `Map.TravelGH()`

Trace:

1. `Map.TravelGH` queues `MapMethods.TravelGH`.
   - anchor: `Map.py:803`
2. `MapMethods.TravelGH` reads `GuildContext.player_gh_key`.
   - anchor: `native_src/methods/MapMethods.py:83`
3. It sends the address of that live key buffer through:
   - `UIManager.SendUIMessageRaw(UIMessage.kGuildHall, ctypes.addressof(gh_key), 0, False)`
   - same anchor: `native_src/methods/MapMethods.py:83`
4. GWCA analogue:
   - `GuildMgr.h:38-40`
   - `GuildMgr.cpp:65-71`

### B14. `Map.LeaveGH()`

Trace:

1. `Map.LeaveGH` queues `MapMethods.LeaveGH`.
   - anchor: `Map.py:811`
2. `MapMethods.LeaveGH` sends:
   - `UIManager.SendUIMessage(UIMessage.kLeaveGuildHall, [0], False)`
   - anchor: `native_src/methods/MapMethods.py:111`
3. GWCA analogue:
   - `GuildMgr.h:42`
   - `GuildMgr.cpp:75`

### B15. `Map.EnterChallenge()` and `Map.CancelEnterChallenge()`

Enter trace:

1. `Map.EnterChallenge` queues `MapMethods.EnterChallenge`.
   - anchor: `Map.py:818`
2. `MapMethods.EnterChallenge` sends:
   - `UIManager.SendUIMessage(UIMessage.kSendEnterMission, [0], False)`
   - anchor: `native_src/methods/MapMethods.py:120`
3. GWCA analogue:
   - `MapMgr.h:90`
   - `MapMgr.cpp:319`

Cancel trace:

1. `Map.CancelEnterChallenge` queues a closure on the `ACTION` queue.
   - anchor: `Map.py:825`
2. That closure finds `WindowFrames["CancelEnterMissionButton"]`.
3. If the frame exists, it calls `FrameClick()` on the frame wrapper.
   - same anchor: `Map.py:825`
4. Py4GW does not use the visible `GW::Map::CancelEnterChallenge` analogue here even though GWCA exposes one.
   - GWCA analogue: `MapMgr.h:91`, `MapMgr.cpp:323`

Why this matters:

- This is a concrete example where Py4GW chooses UI-frame automation over a clean manager path.

### B16. `Party.SetHardMode()`

Trace:

1. `Party.SetHardMode` does local guards:
   - hard mode must be unlocked,
   - current mode must be normal.
   - anchor: `Party.py:269`
2. It then calls compiled `PyParty.PyParty().SetHardMode(True)`.
   - same anchor: `Party.py:269`
3. GWCA analogue:
   - `PartyMgr.h:39`
   - `PartyMgr.cpp:273`

### B17. `Party.ReturnToOutpost()`

Trace:

1. `Party.ReturnToOutpost` directly calls compiled `PyParty.PyParty().ReturnToOutpost()`.
   - anchor: `Party.py:329`
2. GWCA analogue:
   - `PartyMgr.h:42`
   - `PartyMgr.cpp:234`

### B18. `Party.Heroes.FlagHero(hero_id, x, y)`

Trace:

1. `Party.Heroes.FlagHero` directly calls compiled `PyParty.PyParty().FlagHero(hero_id, x, y)`.
   - anchor: `Party.py:553`
2. GWCA analogue:
   - `PartyMgr.h:76-77`
   - `PartyMgr.cpp:406-410`

### B19. `Inventory.MoveItem(item_id, bag_id, slot, quantity)`

Trace:

1. `Inventory.MoveItem` directly calls compiled `PyInventory.PyInventory().MoveItem(item_id, bag_id, slot, quantity)`.
   - anchor: `Inventory.py:472`
2. GWCA analogue:
   - message definition `UIMgr.h:189`
   - manager API `ItemMgr.h:110-112`
   - UI-message hook `ItemMgr.cpp:113`
   - final move implementation `ItemMgr.cpp:534-549`

Why this matters:

- Even though the binding body is opaque, GWCA makes it very clear this action family is packetized as an internal move-item message, not an inventory drag simulation.

### B20. `Inventory.UseItem(item_id)`

Trace:

1. `Inventory.UseItem` directly calls compiled `PyInventory.PyInventory().UseItem(item_id)`.
   - anchor: `Inventory.py:395`
2. GWCA analogue:
   - message definition `UIMgr.h:192`
   - hook `ItemMgr.cpp:77`
   - manager call `ItemMgr.h:57`, `ItemMgr.cpp:374`

### B21. `Quest.SetActiveQuest(quest_id)`

Trace:

1. `Quest.SetActiveQuest` directly calls compiled `PyQuest.PyQuest().set_active_quest_id(quest_id)`.
   - anchor: `Quest.py:18`
2. GWCA analogue:
   - message definition `UIMgr.h:193`
   - UI send `QuestMgr.cpp:28`
   - callback registration `QuestMgr.cpp:70`
   - manager API `QuestMgr.h:22`, `QuestMgr.cpp:117`

### B22. `Quest.AbandonQuest(quest_id)`

Trace:

1. `Quest.AbandonQuest` directly calls compiled `PyQuest.PyQuest().abandon_quest_id(quest_id)`.
   - anchor: `Quest.py:28`
2. GWCA analogue:
   - message definition `UIMgr.h:194`
   - UI send `QuestMgr.cpp:44`
   - callback registration `QuestMgr.cpp:64`
   - manager API `QuestMgr.h:30`, `QuestMgr.cpp:168`

### B23. `Trading.Trader.RequestQuote(item_id)`

Trace:

1. `Trading.Trader.RequestQuote` directly calls compiled `PyMerchant.PyMerchant().trader_request_quote(item_id)`.
   - anchor: `Merchant.py:66`
2. GWCA analogue:
   - message definition `UIMgr.h:190`
   - UI send `MerchantMgr.cpp:79`
   - request-hook callback `MerchantMgr.cpp:82`
   - manager API `MerchantMgr.h:46`, `MerchantMgr.cpp:152`

### B24. `SkillBar.UseSkill(slot, target)`

Trace:

1. `SkillBar.UseSkill` directly calls compiled `PySkillbar.Skillbar().UseSkill(slot, target)`.
   - anchor: `Skillbar.py:64`
2. GWCA analogue:
   - manager API `SkillbarMgr.h:42`
   - callback hook `SkillbarMgr.cpp:130`
   - final manager implementation `SkillbarMgr.cpp:611`

Why this matters:

- Even without the compiled binding body, the API surface and GWCA manager line up almost perfectly.

### B25. Example bot frame loop

Trace:

1. The sequential Vaettir example processes queues every frame:
   - `ActionQueueManager().ProcessQueue("ACTION")`
   - `ProcessQueue("SALVAGE")`
   - `ProcessQueue("IDENTIFY")`
   - `ProcessQueue("MERCHANT")`
   - `ProcessQueue("LOOT")`
   - anchor: `Bots/Example Bots/VaettirBot (Sequential).py:1171-1175`
2. Earlier in the same bot, high-level routines call:
   - hard mode setup,
   - dialogs,
   - identify,
   - salvage,
   - merchant routines.
   - anchors include `VaettirBot (Sequential).py:728`, `:750`, `:566`, `:571`, `:576`

Why this matters:

- This shows how the queue layer and the actuation layer come together in a real bot rather than just in wrappers.

## Appendix C: Packet Anatomy

This appendix focuses on the actual payloads Py4GW sends when the layout is visible, or on the exact field contract documented by GWCA when the Py4GW binding body is opaque.

I split this into three categories:

1. fully visible packet layouts in Py4GW Python source,
2. field contracts visible from GWCA UI-message comments,
3. native-call argument buffers that are not UI messages but still act as command payloads.

### C1. Fully visible Py4GW packet layouts

#### C1.1 Travel packet

Visible in:

- `native_src/methods/MapMethods.py:67`

Structure:

```python
class TravelStruct(ctypes.Structure):
    _fields_ = [
        ("map_id", ctypes.c_uint32),
        ("region", ctypes.c_int32),
        ("language", ctypes.c_int32),
        ("district_number", ctypes.c_int32),
    ]
```

Dispatch:

- `UIManager.SendUIMessageRaw(UIMessage.kTravel, ctypes.addressof(TravelStruct(...)), False)`

Meaning:

- `map_id`: destination map
- `region`: server region
- `language`: district language
- `district_number`: numbered district instance

GWCA anchor:

- `UIMgr.h:175` for `kTravel`
- `MapMgr.cpp:189`

#### C1.2 Standard chat packet

Visible in:

- `native_src/methods/PlayerMethods.py:173`

Structure:

```python
class SendChatPacket(ctypes.Structure):
    _fields_ = [
        ("message", ctypes.c_wchar_p),
        ("agent_id", ctypes.c_uint32),
    ]
```

Construction details:

- Py4GW allocates `Buffer140 = ctypes.c_wchar * 140`
- puts the channel character in `buf[0]`
- copies the message from `buf[1]` onward
- null terminates after the last character
- sets `agent_id=0`

Dispatch:

- `UIManager.SendUIMessageRaw(UIMessage.kSendChatMessage, ctypes.addressof(packet), False)`

Meaning:

- `message`: wide-char buffer beginning with the channel prefix
  - examples: `'!'`, `'@'`, `'#'`, `'$'`, `'%'`, `'"'`, `'/'`
- `agent_id`: always `0` in the visible helper

Py4GW enum anchor:

- `UI_enums.py:230` for `kSendChatMessage`

#### C1.3 Whisper packet

Visible in:

- `native_src/methods/PlayerMethods.py:244`

Structure:

- identical `SendChatPacket` layout to normal chat

Construction details:

- instead of prefixing a channel character, Py4GW formats:
  - `\"{target_name},{message}`
- that formatted string is copied into the wide-char buffer and sent as the packet message field

Dispatch:

- same `UIMessage.kSendChatMessage` raw send as standard chat

Meaning:

- whispering is packetized as a special chat payload, not a separate dedicated visible struct.

#### C1.4 Fake chat log packet

Visible in:

- `native_src/methods/PlayerMethods.py:308`

Structure:

```python
class UIChatMessage(ctypes.Structure):
    _fields_ = [
        ("channel", ctypes.c_uint32),
        ("message", ctypes.c_wchar_p),
        ("channel2", ctypes.c_uint32),
    ]
```

Construction details:

- Py4GW first widens the UTF-8 bytes one byte at a time into a pseudo-wide string
- it then wraps the payload in Guild Wars chat markup
- the default visible fake-chat form is:
  - `\u0108\u0107{message}\u0001`
- if sender markup were present, it would use a different formatting branch, but `SendFakeChat` as written uses `sender_encoded = None`

Dispatch:

- `UIManager.SendUIMessageRaw(UIMessage.kWriteToChatLog, ctypes.addressof(param), False)`

Meaning:

- `channel`: chat channel id
- `message`: encoded chat-log string
- `channel2`: duplicated channel id

GWCA anchor:

- `UIMgr.h:136` for `kWriteToChatLog`

#### C1.5 Raw Guild Hall key payload

Visible in:

- `native_src/methods/MapMethods.py:83`

Layout:

- the message payload is not copied into a new structure there
- instead, Py4GW takes the live `gh_key` object from `GuildContext.player_gh_key`
- then sends `ctypes.addressof(gh_key)`

Meaning:

- the payload is effectively the in-memory `GHKey` structure already held by the runtime
- Py4GW may overwrite `gh_key.key_data[0..3]` first if a custom key was supplied

Dispatch:

- `UIManager.SendUIMessageRaw(UIMessage.kGuildHall, ctypes.addressof(gh_key), 0, False)`

GWCA anchor:

- `UIMgr.h:173` says `kGuildHall` expects `gh key (uint32_t[4])`

#### C1.6 Direct dialog payload

Visible in:

- `native_src/methods/PlayerMethods.py:386`

Layout:

- Py4GW sends the raw numeric dialog id directly as `wparam`
- it passes `0` as `lparam`

Dispatch:

- `UIManager.SendUIMessageRaw(UIMessage.kSendAgentDialog, dialog_id, 0)`

Important note:

- `UI_enums.py:223` comments `kSendAgentDialog` as `wparam = uint32_t agent_id`
- but the actual visible usage in Py4GW clearly treats the value as a dialog id
- for this appendix, the code path is more trustworthy than the stale enum comment

#### C1.7 World action payload

Visible in:

- `native_src/methods/PlayerMethods.py:87`

Layout:

- Py4GW does not build a ctypes struct here
- it passes a Python list of three integers to `UIManager.SendUIMessage(...)`

Visible field order:

1. `action_id`
2. `agent_id`
3. `call_target`

Action ids visible in source:

- `0`: `InteractEnemy`
- `1`: `InteractPlayerOrOther`
- `2`: `InteractNPC`
- `3`: `InteractItem`
- `4`: `InteractTrade`
- `5`: `InteractGadget`

Dispatch:

- `UIManager.SendUIMessage(UIMessage.kSendWorldAction, [action_id, agent_id, call_target])`

Py4GW enum anchor:

- `UI_enums.py:234` for `kSendWorldAction`

Important limit:

- because `UIManager.SendUIMessage` takes a generic integer list, the exact binary packing happens inside the compiled `PyUIManager` layer and is not visible here.

### C2. Field contracts visible from GWCA UI-message comments

These payloads are not constructed as visible Python structs in this checkout, but GWCA documents their expected field layout in `UIMgr.h`.

#### C2.1 Change target UI payload

GWCA anchor:

- `UIMgr.h:119`

Contract:

- `kChangeTarget`: `wparam = ChangeTargetUIMsg*`

Py4GW visible usage:

- low-level helper sends `UIMessage.kSendChangeTarget` through `UIManager.SendUIMessage(...)`
- `native_src/methods/PlayerMethods.py:76`

Interpretation:

- the compiled UI manager likely packs the integer list into the internal target-change structure before dispatch.

#### C2.2 Logout payload

GWCA anchor:

- `UIMgr.h:141`

Contract:

- `kLogout`: `wparam = { bool unknown, bool character_select }`

Py4GW visible usage:

- `MapMethods.LogouttoCharacterSelect` sends `UIManager.SendUIMessage(UIMessage.kLogout, [0,0])`
- `native_src/methods/MapMethods.py:129`

Interpretation:

- Py4GW uses the two-bool payload form documented by GWCA, encoded through the integer-list sender.

#### C2.3 Enter mission / challenge payload

GWCA anchor:

- `UIMgr.h:186`

Contract:

- `kSendEnterMission`: `wparam = arena_id`

Py4GW visible usage:

- `MapMethods.EnterChallenge` sends `[0]`
- `native_src/methods/MapMethods.py:120`

Interpretation:

- Py4GW is using the scalar mission-entry form with arena id `0`.

#### C2.4 Move item payload

GWCA anchor:

- `UIMgr.h:189`

Contract:

- `kSendMoveItem`: `wparam = { uint32_t item_id, uint32_t quantity, uint32_t bag_id, uint32_t slot }`

Py4GW visible usage:

- public wrapper calls compiled `PyInventory.PyInventory().MoveItem(item_id, bag_id, slot, quantity)`
- `Inventory.py:472`

Interpretation:

- the compiled inventory binding almost certainly packs that exact four-field structure before sending or direct-calling the item manager path.

#### C2.5 Merchant quote request payload

GWCA anchor:

- `UIMgr.h:190`

Contract:

- `kSendMerchantRequestQuote`:
  - `{ Merchant::TransactionType type, uint32_t gold_give, Merchant::TransactionInfo give, uint32_t gold_recv, Merchant::TransactionInfo recv }`

Py4GW visible usage:

- compiled `trader_request_quote`, `trader_request_sell_quote`
- `Merchant.py:66`, `Merchant.py:76`

Interpretation:

- exact packing is hidden in the compiled binding, but GWCA tells us the message contract.

#### C2.6 Merchant transact payload

GWCA anchor:

- `UIMgr.h:191`

Contract:

- `kSendMerchantTransactItem`:
  - `{ Merchant::TransactionType type, uint32_t unknown, Merchant::QuoteInfo give, Merchant::QuoteInfo recv }`

Py4GW visible usage:

- compiled buy/sell operations in `Merchant.py`

Interpretation:

- this is the likely payload contract behind trader buys, trader sells, merchant buys, merchant sells, and probably crafter/collector-style item exchanges.

#### C2.7 Use item payload

GWCA anchor:

- `UIMgr.h:192`

Contract:

- `kSendUseItem`: `wparam = uint32_t item_id`

Py4GW visible usage:

- compiled `PyInventory.PyInventory().UseItem(item_id)`
- `Inventory.py:395`

Interpretation:

- simplest scalar item message in the set.

#### C2.8 Set active quest payload

GWCA anchor:

- `UIMgr.h:193`

Contract:

- `kSendSetActiveQuest`: `wparam = uint32_t quest_id`

Py4GW visible usage:

- compiled `PyQuest.PyQuest().set_active_quest_id(quest_id)`
- `Quest.py:18`

#### C2.9 Abandon quest payload

GWCA anchor:

- `UIMgr.h:194`

Contract:

- `kSendAbandonQuest`: `wparam = uint32_t quest_id`

Py4GW visible usage:

- compiled `PyQuest.PyQuest().abandon_quest_id(quest_id)`
- `Quest.py:28`

### C3. Native command buffers rather than UI-message packets

These are still command payloads, but they are direct-call argument blocks instead of UI-message `wparam` payloads.

#### C3.1 MoveTo float buffer

Visible in:

- `native_src/methods/PlayerMethods.py:130`

Layout:

```python
args = (ctypes.c_float * 4)()
args[0] = x
args[1] = y
args[2] = float(zPlane)
args[3] = 0.0
```

Dispatch:

- `MoveTo_Func.directCall(args)`

Meaning:

- a 4-float command block for the internal move function
- the first three fields are clearly `(x, y, zplane)`
- the fourth is unknown but required and always set to `0.0` in visible Py4GW code

#### C3.2 Deposit faction direct-call arguments

Visible in:

- `native_src/methods/PlayerMethods.py:146`

Direct-call form:

```python
DepositFaction_Func.directCall(0, allegiance, 5000)
```

Meaning:

- not enough symbolic information is visible to name all three arguments confidently
- but the command shape is a fixed three-argument direct native call
- visible wrapper semantics indicate:
  - second arg is faction allegiance
  - third arg is fixed at `5000`

#### C3.3 Set / remove active title direct-call arguments

Visible in:

- `native_src/methods/PlayerMethods.py:155`
- `native_src/methods/PlayerMethods.py:164`

Forms:

```python
SetActiveTitle_Func.directCall(title_id)
RemoveActiveTitle_Func.directCall()
```

Meaning:

- one is a scalar direct native call with title id,
- the other is a zero-argument direct native call.

### C4. Practical packet takeaways

1. The cleanest fully visible packet layouts in Py4GW are:
   - travel,
   - chat,
   - whisper,
   - fake chat,
   - raw Guild Hall key use.
2. The cleanest GWCA-documented but compiled-in-Py4GW contracts are:
   - move item,
   - merchant quote,
   - merchant transact,
   - use item,
   - set active quest,
   - abandon quest,
   - logout.
3. World actions and target changes sit in the middle:
   - field order is visible from Py4GW,
   - exact binary packing is hidden in the compiled UI manager layer.
4. Movement is special:
   - it is not a UI packet in the visible low-level helper,
   - it is a direct native call with a float command buffer.

## Appendix D: Bot Recipes

This appendix turns the lower-level mappings into concrete workflows that look like the way a real bot uses Py4GW.

Each recipe is written as:

1. high-level routine or wrapper call,
2. intermediate queueing / waiting behavior,
3. final packet or native actuation,
4. matching GWCA analogue where visible.

### D1. Travel to an outpost and wait for load

Representative high-level entry:

- `Routines.Sequential.Map.TravelToOutpost(outpost_id, log=False)`
- anchor: `routines_src/Sequential.py:175`

Pipeline:

1. The routine checks whether the current map already matches the destination.
   - same anchor: `Sequential.py:175`
2. If not, it calls `Map.Travel(outpost_id)`.
   - same anchor: `Sequential.py:175`
3. `Map.Travel` queues a closure on the `ACTION` queue.
   - anchor: `Map.py:715`
4. When the queue processes that closure, it calls `MapMethods.Travel(map_id, current_region, 0, current_language)`.
   - anchor: `Map.py:715`
5. `MapMethods.Travel` builds `TravelStruct { map_id, region, language, district_number }`.
   - anchor: `native_src/methods/MapMethods.py:67`
6. Py4GW sends that struct through:
   - `UIManager.SendUIMessageRaw(UIMessage.kTravel, ctypes.addressof(TravelStruct(...)), False)`
   - same anchor: `native_src/methods/MapMethods.py:67`
7. The sequential routine then loops until:
   - `Map.IsMapReady()`
   - `GLOBAL_CACHE.Party.IsPartyLoaded()`
   - `Map.IsMapIDMatch(None, outpost_id)`
   - anchor: `Sequential.py:175`
8. Example bot usage:
   - `VaettirBot (Sequential).py:716`

GWCA analogue:

- `MapMgr.h:65-67`
- `MapMgr.cpp:189`
- message contract at `UIMgr.h:175`

What this recipe shows:

- Travel itself is a packet send.
- “Waiting for the map” is a separate supervisory loop in Python, not part of the travel packet.

### D2. Set hard mode before leaving the outpost

Representative high-level entry:

- `Routines.Sequential.Map.SetHardMode(log=False)`
- anchor: `routines_src/Sequential.py:162`

Pipeline:

1. The routine calls `GLOBAL_CACHE.Party.SetHardMode()`.
   - anchor: `Sequential.py:162`
2. That ultimately resolves to `Party.SetHardMode()`.
   - public wrapper anchor: `Party.py:269`
3. The wrapper checks:
   - hard mode unlocked,
   - current mode normal.
   - same anchor: `Party.py:269`
4. It then calls compiled `PyParty.PyParty().SetHardMode(True)`.
   - same anchor: `Party.py:269`
5. Example bot usage:
   - `VaettirBot (Sequential).py:728`

GWCA analogue:

- `PartyMgr.h:39`
- `PartyMgr.cpp:273`

What this recipe shows:

- Hard mode is a direct party-manager-style control action.
- There is no visible UI-frame automation involved.

### D3. Talk to an NPC and send a dialog response

Representative high-level entry:

- `Routines.Sequential.Player.SendDialog(dialog_id)`
- anchor: `routines_src/Sequential.py:31`

Pipeline:

1. The routine converts the hex string to int and calls `Player.SendDialog(int(dialog_id, 16))`.
   - anchor: `Sequential.py:31`
2. `Player.SendDialog` normalizes input and adds compiled `PyPlayer.PyPlayer().SendDialog(dialog)` to the `ACTION` queue.
   - anchor: `Player.py:658`
3. GWCA shows the intended downstream structure:
   - hook `SendDialog_Func`,
   - reroute into `UI::SendUIMessage(kSendDialog, dialog_id)`,
   - catch it in `OnSendDialog_UIMessage`,
   - dispatch to the correct native dialog sender.
   - anchors: `AgentMgr.cpp:36`, `:48`, `:166`, `:260`
4. Example bot usage:
   - `VaettirBot (Sequential).py:750`

Alternative raw route:

1. `Player.SendRawDialog(dialog_id)` queues `PlayerMethods.SendRawDialog`.
   - `Player.py:640`
2. `PlayerMethods.SendRawDialog` calls:
   - `UIManager.SendUIMessageRaw(UIMessage.kSendAgentDialog, dialog_id, 0)`
   - `native_src/methods/PlayerMethods.py:386`

What this recipe shows:

- The user-facing “send dialog” workflow can go through either:
  - the compiled GWCA-like dialog manager path, or
  - the explicit raw dialog message path.

### D4. Buy a skill from a skill trainer

Representative high-level entry:

- `Player.BuySkill(skill_id)`
- anchor: `Player.py:648`

Pipeline:

1. `Player.BuySkill` queues `PlayerMethods.SendSkillTrainerDialog`.
   - anchor: `Player.py:648`
2. `SendSkillTrainerDialog` computes:
   - `dialog_skill_id = Utils.SkillIdToDialogId(skill_id)`
   - anchor: `native_src/methods/PlayerMethods.py:400`
3. It immediately reuses the raw dialog workflow:
   - `PlayerMethods.SendRawDialog(dialog_skill_id)`
   - same anchor: `native_src/methods/PlayerMethods.py:400`
4. That sends:
   - `UIManager.SendUIMessageRaw(UIMessage.kSendAgentDialog, dialog_id, 0)`
   - `native_src/methods/PlayerMethods.py:386`

What this recipe shows:

- Skill purchase is treated as a specialized NPC dialog transaction.
- Py4GW does not need a separate visible trainer-packet struct once it can derive the correct dialog id.

### D5. Load a skillbar

Representative high-level entry:

- `Routines.Sequential.Skills.LoadSkillbar(skill_template, log=False)`
- anchor: `routines_src/Sequential.py:108`

Pipeline:

1. The routine calls `GLOBAL_CACHE.SkillBar.LoadSkillTemplate(skill_template)`.
   - anchor: `Sequential.py:108`
2. That resolves to `SkillBar.LoadSkillTemplate(skill_template)`.
   - public wrapper anchor: `Skillbar.py:8`
3. The wrapper directly calls compiled `PySkillbar.Skillbar().LoadSkillTemplate(skill_template)`.
   - same anchor: `Skillbar.py:8`

GWCA analogue:

- `SkillbarMgr.h:73`
- `SkillbarMgr.cpp:476`

What this recipe shows:

- Skillbar loading is exposed as a direct compiled binding.
- The closest visible GWCA shape is the skill-template manager path rather than a hand-built Python packet.

### D6. Cast a skill in combat

Representative high-level entries:

- `Routines.Sequential.Skills.CastSkillID(skill_id, ...)`
- `Routines.Sequential.Skills.CastSkillSlot(slot, ...)`
- anchors: `routines_src/Sequential.py` skill section

Pipeline:

1. The routine checks:
   - map ready,
   - player energy,
   - skill readiness,
   - optional extra condition.
   - anchors: `Sequential.py` skill section shown in earlier excerpt
2. It resolves the slot if needed and calls:
   - `GLOBAL_CACHE.SkillBar.UseSkill(slot)`
3. That resolves to `SkillBar.UseSkill(skill_slot, target_agent_id=0)`.
   - public wrapper anchor: `Skillbar.py:64`
4. The wrapper directly calls compiled `PySkillbar.Skillbar().UseSkill(slot, target)`.
   - same anchor: `Skillbar.py:64`
5. There is no visible Python-side packet struct for this step.

GWCA analogue:

- `SkillbarMgr.h:42`
- `SkillbarMgr.cpp:611`

What this recipe shows:

- Most of the “smartness” in the visible Python layer is pre-cast decision making.
- The actual cast actuation is a single compiled skillbar binding call.

### D7. Buy salvage kits from a merchant

Representative high-level entry:

- `Routines.Sequential.Merchant.BuySalvageKits(kits_to_buy, log=False)`
- anchor: `routines_src/Sequential.py:440`

Pipeline:

1. The routine reads the merchant inventory list:
   - `GLOBAL_CACHE.Trading.Merchant.GetOfferedItems()`
   - anchor: `Sequential.py:440`
2. It filters for model id `2992` to find salvage kits.
   - same anchor: `Sequential.py:440`
3. For each kit requested, it computes:
   - `value = GLOBAL_CACHE.Item.Properties.GetValue(item_id) * 2`
   - same anchor: `Sequential.py:440`
4. It then calls:
   - `GLOBAL_CACHE.Trading.Merchant.BuyItem(item_id, value)`
   - same anchor: `Sequential.py:440`
5. That resolves to `Trading.Merchant.BuyItem(item_id, cost)`.
   - wrapper anchor: `Merchant.py:111`
6. The wrapper directly calls compiled `PyMerchant.PyMerchant().merchant_buy_item(item_id, cost)`.
   - same anchor: `Merchant.py:111`
7. The routine waits until the `MERCHANT` queue is empty.
   - `Sequential.py:440`
8. Example bot usage:
   - `VaettirBot (Sequential).py:566`

GWCA analogue:

- merchant transact family
- `MerchantMgr.cpp:45`
- `UIMgr.h:191`

What this recipe shows:

- The visible Python logic handles item discovery and price normalization.
- The actual purchase actuation is still the compiled merchant transact path.

### D8. Identify items in bulk

Representative high-level entry:

- `Routines.Sequential.Items.IdentifyItems(item_array, log=False)`
- anchor: `routines_src/Sequential.py:509`

Pipeline:

1. For each `item_id`, the routine enqueues:
   - `Sequential.Items._identify_item(item_id)`
   - on the `IDENTIFY` queue
   - anchor: `Sequential.py:509`
2. `_identify_item` finds the first ID kit:
   - `GLOBAL_CACHE.Inventory.GetFirstIDKit()`
   - same anchor: `Sequential.py:509`
3. It then calls:
   - `Inventory.IdentifyItem(item_id, id_kit)`
   - same anchor: `Sequential.py:509`
4. Public wrapper:
   - `Inventory.IdentifyItem(item_id, id_kit_id)`
   - anchor: `Inventory.py:245`
5. Wrapper then calls compiled:
   - `PyInventory.PyInventory().IdentifyItem(id_kit_id, item_id)`
   - same anchor: `Inventory.py:245`
6. The routine waits until the `IDENTIFY` queue drains.
   - `Sequential.py:509`
7. Example bot usage:
   - `VaettirBot (Sequential).py:571`
   - also later at `:803`

GWCA analogue:

- `ItemMgr.h:82`
- `ItemMgr.cpp:458`

What this recipe shows:

- Bulk identification is a queue-driven orchestration loop over a simple kit+item manager call.

### D9. Salvage items in bulk

Representative high-level entry:

- `Routines.Sequential.Items.SalvageItems(item_array, log=False)`
- anchor: `routines_src/Sequential.py:480`

Pipeline:

1. For each `item_id`, the routine enqueues on the `SALVAGE` queue:
   - `Sequential.Items._salvage_item(item_id)`
   - `Inventory.AcceptSalvageMaterialsWindow`
   - anchor: `Sequential.py:480`
2. `_salvage_item` finds the first salvage kit:
   - `GLOBAL_CACHE.Inventory.GetFirstSalvageKit()`
   - same anchor: `Sequential.py:480`
3. It then calls:
   - `Inventory.SalvageItem(item_id, salvage_kit)`
   - same anchor: `Sequential.py:480`
4. Public wrapper:
   - `Inventory.SalvageItem(item_id, salvage_kit_id)`
   - anchor: `Inventory.py:285`
5. Wrapper calls compiled:
   - `PyInventory.PyInventory().Salvage(salv_kit_id, item_id)`
   - same anchor: `Inventory.py:285`
6. After that, the queued confirmation step calls:
   - `Inventory.AcceptSalvageMaterialsWindow()`
   - `Inventory.py:328`
7. That function finds the salvage confirmation frame via `UIManager.GetChildFrameID(...)` and clicks it with `UIManager.FrameClick(...)`.
   - `Inventory.py:328`
8. The routine waits until the `SALVAGE` queue drains.
   - `Sequential.py:480`
9. Example bot usage:
   - `VaettirBot (Sequential).py:576`
   - also later at `:806`

What this recipe shows:

- Salvage is a hybrid workflow:
  - start salvage via compiled inventory binding,
  - finish confirmation via UI-frame click fallback.

### D10. Follow a path and reissue movement until arrival

Representative high-level entry:

- `Routines.Sequential.Movement.FollowPath(path_points, ...)`
- anchor: `routines_src/Sequential.py`, movement section near the excerpt already captured

Pipeline:

1. For each `(target_x, target_y)`, the routine calls:
   - `Player.Move(target_x, target_y)`
2. That goes through:
   - `Player.Move`
   - `ACTION` queue
   - `PlayerMethods.Move`
   - `Game.enqueue`
   - `MoveTo_Func.directCall(args)`
   - traced earlier in Appendix B
3. The routine compares current distance against previous distance.
4. If the player is not getting closer, it reissues movement with a small random offset.
   - anchor: `Sequential.py`, movement section from the extracted snippet
5. Once within tolerance, it advances to the next waypoint.

What this recipe shows:

- Path following is not a separate low-level navigation API.
- It is a supervisory loop repeatedly invoking the same direct native movement primitive.

### D11. Loot items after a fight

Representative high-level bot usage:

- `Routines.Sequential.Items.LootItems(filtered_agent_ids, log_to_console)`
- example anchor: `VaettirBot (Sequential).py:797`

Observed lower-level pattern in Py4GW:

1. Loot logic builds a filtered list of item agent ids.
2. The action queue architecture clearly reserves a `LOOT` queue with a 1250 ms throttle.
   - `ActionQueue.py` queue initialization
3. The actual pickup actuation path, when it happens, resolves to:
   - `Inventory.PickUpItem(item_id, call_target=False)`
   - wrapper anchor: `Inventory.py:362`
4. That wrapper directly calls compiled:
   - `PyInventory.PyInventory().PickUpItem(item_id, call_target)`
   - same anchor: `Inventory.py:362`
5. GWCA analogue:
   - `ItemMgr.h:69`
   - `ItemMgr.cpp:345`

What this recipe shows:

- Even when the full higher-level loot routine body is not quoted here, the actuation step still lands on the same item-pickup manager family.

### D12. Real example: Vaettir pre-run setup sequence

Using the example bot, the opening outpost sequence is effectively:

1. `Routines.Sequential.Map.TravelToOutpost(longeyes_ledge, log_to_console)`
   - `VaettirBot (Sequential).py:716`
2. wait for load
3. `Routines.Sequential.Map.SetHardMode(log_to_console)`
   - `VaettirBot (Sequential).py:728`
4. `Routines.Sequential.Player.SetTitle(TitleID.Norn.value, log_to_console)`
   - `VaettirBot (Sequential).py:729`
5. route out of outpost
6. `Routines.Sequential.Map.WaitforMapLoad(bjora_marches, log_to_console)`
   - `VaettirBot (Sequential).py:734`
7. move deeper into the route
8. `Routines.Sequential.Map.WaitforMapLoad(jaga_moraine, log_to_console)`
   - `VaettirBot (Sequential).py:744`
9. `Routines.Sequential.Player.SendDialog("0x84")`
   - `VaettirBot (Sequential).py:750`

This is a good compact illustration of how a real bot chains:

- map travel packet,
- party mode toggle,
- direct native title call,
- repeated movement actuation,
- dialog manager path.

### D13. Real example: Merchant and cleanup cycle

Using the example bot's cleanup logic:

1. sell items:
   - `Routines.Sequential.Merchant.SellItems(...)`
   - `VaettirBot (Sequential).py:562`, later `:582`
2. buy ID kits:
   - `Routines.Sequential.Merchant.BuyIDKits(...)`
   - `VaettirBot (Sequential).py:565`
3. buy salvage kits:
   - `Routines.Sequential.Merchant.BuySalvageKits(...)`
   - `VaettirBot (Sequential).py:566`
4. identify:
   - `Routines.Sequential.Items.IdentifyItems(...)`
   - `VaettirBot (Sequential).py:571`
5. salvage:
   - `Routines.Sequential.Items.SalvageItems(...)`
   - `VaettirBot (Sequential).py:576`
6. deposit items and gold:
   - `VaettirBot (Sequential).py:587`, `:590`

That whole workflow combines:

- merchant transact bindings,
- identify bindings,
- salvage binding plus frame-confirm click,
- item move / storage paths,
- queue draining between stages.

### D14. Main lesson from the recipes

The recipes make the stack very clear:

1. High-level Py4GW routines mostly do:
   - filtering,
   - readiness checks,
   - queueing,
   - waiting,
   - retries.
2. The actual actuation step is usually one of only a few families:
   - internal UI message packet,
   - direct native call,
   - compiled binding that fronts a GWCA-style manager,
   - frame-click fallback when no clean manager path is used.
3. That is why Py4GW feels like a bot framework, not just a binding dump: the recipes are composed in Python, but the control plane is still fundamentally GWCA-style.

## Appendix E. `gwca.dll` Binary Verification With Ghidra

This appendix is different from the earlier source-anchored sections: it verifies the compiled `gwca.dll` directly.

Why this matters:

- the GWCA checkout in this repo is source,
- `gwca.dll` is the actual compiled binary some surrounding code may be loading,
- binary behavior can diverge from the local source tree,
- Py4GW inference is stronger when the GWCA binary path is verified instead of assumed.

### E1. Local decompiler setup used

Portable tooling was added under the workspace `tools` directory:

- `tools/ghidra_12.0.4_PUBLIC`
- `tools/jdk-21.0.10+7`
- `tools/ghidra_scripts/ListFunctions.java`
- `tools/ghidra_scripts/DecompileMatch.java`

Purpose of the custom scripts:

- `ListFunctions.java` lists recognized functions from the analyzed program.
- `DecompileMatch.java` searches by substring and decompiles the first matching function.

Practical note:

- Ghidra headless in this environment did not expose Python scripting cleanly, so the working automation path is Java-based Ghidra scripts.

### E2. What the binary inspection confirms before decompilation

For `GWA Censured/GWToolboxpp/Dependencies/GWCA/bin/gwca.dll`:

- it is a 32-bit PE DLL,
- it exports hundreds of named GWCA-style functions,
- exported names include core manager actions such as `ChangeTarget`, `InteractAgent`, `Move`, `SendDialog`, `Travel`, `UseSkill`, and many others.

That strongly supports the claim that this DLL is a compiled GWCA-style manager layer rather than a stripped unrelated helper.

### E3. Binary-confirmed functions

The following bodies were decompiled directly from `gwca.dll`.

#### E3.1 `GW::Agents::ChangeTarget(uint32_t agent_id)`

Decompiled body:

```cpp
bool __cdecl GW::Agents::ChangeTarget(uint param_1)
{
  bool bVar1;
  uint local_c [2];

  local_c[0] = param_1;
  local_c[1] = 0;
  bVar1 = UI::SendUIMessage(0x3000000b,local_c,(void *)0x0);
  return bVar1;
}
```

What this proves:

- in this binary, target change is implemented as a UI-message send,
- the payload is a 2-field structure: `{ agent_id, 0 }`,
- this is not merely inferred from headers; it is directly visible in the compiled DLL.

How that compares to the checked-in source:

- `Include/GWCA/Managers/UIMgr.h` documents `kChangeTarget` as a UI contract,
- but `Source/AgentMgr.cpp` currently exposes `ChangeTarget(agent_id)` as a call to `ChangeTarget_Func(agent_id, 0)`.

Important implication:

- the local source tree and the shipped binary are not identical implementations here,
- but they still converge on the same two-field payload semantics.

#### E3.2 `GW::Agents::InteractAgent(const Agent*, bool call_target)`

Decompiled body:

```cpp
bool __cdecl GW::Agents::InteractAgent(Agent *param_1,bool param_2)
{
  uint uVar1;
  bool bVar2;
  undefined4 local_14;
  undefined4 local_10;
  undefined1 local_c;
  uint local_8;

  local_10 = *(undefined4 *)(param_1 + 0x2c);
  uVar1 = *(uint *)(param_1 + 0x9c);
  local_c = !param_2;
  if ((uVar1 >> 10 & 1) == 0) {
    if ((uVar1 >> 9 & 1) == 0) {
      if ((uVar1 & 0xdb) == 0) {
        param_1 = (Agent *)0x0;
      }
      if (param_1 == (Agent *)0x0) {
        return false;
      }
      if (param_1[0x1b5] == (Agent)0x3) {
        local_14 = 0;
      }
      else if (param_1[0x1b5] == (Agent)0x6) {
        local_14 = 2;
      }
      else {
        local_14 = 1;
      }
    }
    else {
      local_14 = 5;
    }
  }
  else {
    local_14 = 3;
  }
  bVar2 = UI::SendUIMessage(0x30000020,&local_14,(void *)0x0);
  return bVar2;
}
```

What this proves:

- interaction is not a blind direct native call in this binary,
- the DLL classifies the target before actuation,
- it then sends a world-action style UI message with a packed local structure.

Why that matters for Py4GW:

- Py4GW's visible `PlayerMethods.InteractAgent` already builds the same kind of classified world-action payload,
- so this is one of the strongest direct binary confirmations that Py4GW's visible low-level helper is genuinely GWCA-shaped.

#### E3.3 `GW::SkillbarMgr::UseSkill(uint32_t slot, uint32_t target)`

Decompiled body:

```cpp
bool __cdecl GW::SkillbarMgr::UseSkill(uint param_1,uint param_2)
{
  bool bVar1;
  uint uVar2;
  Skillbar *pSVar3;
  int iVar4;

  if (param_2 != 0) {
    uVar2 = Agents::GetTargetId();
    if (param_2 != uVar2) {
      Agents::ChangeTarget(param_2);
    }
  }
  if (param_1 < 8) {
    pSVar3 = GetPlayerSkillbar();
    if ((pSVar3 != (Skillbar *)0x0) && (*(int *)(pSVar3 + param_1 * 0x14 + 0x10) != 0)) {
      if (DAT_1008a308 == 0) {
        iVar4 = 0;
      }
      else {
        iVar4 = *(int *)(pSVar3 + param_1 * 0x14 + 0x10) * 0xa4 + DAT_1008a308;
      }
      if (iVar4 != 0) {
        if ((*(uint *)(iVar4 + 0x10) >> 0x16 & 1) != 0) {
          return true;
        }
        bVar1 = UI::Keypress(param_1 + 0xa4,(Frame *)0x0);
        return bVar1;
      }
    }
  }
  return false;
}
```

What this proves:

- in this compiled DLL, skill use is implemented through UI keypress simulation after sanity checks,
- the manager changes target first when needed,
- it checks the slot and skill state before sending the keypress.

Why this is important:

- the checked-in `SkillbarMgr.cpp` calls `UseSkill_Func(player_id, slot, target, call_target)`,
- the binary instead shows a client-side path that can resolve to `UI::Keypress`,
- so again the interface name matches GWCA source, but the actual shipped implementation may be a different generation or branch.

For Py4GW mapping:

- `SkillBar.UseSkill(...)` calling the compiled binding should be described as "skillbar-manager family with binary-confirmed target-change plus keypress actuation", not just "opaque use-skill call".

#### E3.4 `GW::Agents::SendDialog(uint32_t dialog_id)`

Decompiled body:

```cpp
bool __cdecl GW::Agents::SendDialog(uint param_1)
{
  bool bVar1;
  Frame *pFVar2;
  int *local_10;
  int *local_c;

  FUN_10002340(&local_10);
  for (; local_10 != local_c; local_10 = local_10 + 1) {
    pFVar2 = (Frame *)*local_10;
    if (*(uint *)(pFVar2 + 0x1c4) == param_1) goto LAB_10002e43;
  }
  pFVar2 = (Frame *)0x0;
LAB_10002e43:
  FUN_10001c30((int *)&local_10);
  bVar1 = UI::ButtonClick(pFVar2);
  return bVar1;
}
```

What this proves:

- in this specific `gwca.dll`, `SendDialog` is implemented as UI-frame lookup plus `UI::ButtonClick`,
- that is materially different from the checked-in `Source/AgentMgr.cpp`, which exposes:
  - `SendDialog(dialog_id) -> UI::SendUIMessage(kSendDialog, dialog_id)`

This is the biggest source-vs-binary discrepancy found so far.

Why it matters for the Py4GW research:

- earlier sections that mapped dialog handling strictly to GWCA's `kSendDialog` route are source-accurate for the checkout,
- but not necessarily binary-accurate for the shipped `gwca.dll`,
- therefore Py4GW dialog analysis should keep two possibilities in view:
  - GWCA source-style reroute through `kSendDialog`,
  - binary-observed frame-click execution.

#### E3.5 `GW::GuildMgr::TravelGH(...)`

Decompiled body:

```cpp
bool __cdecl GW::GuildMgr::TravelGH(void)
{
  bool bVar1;

  bVar1 = UI::SendUIMessage(0x1000017c,&stack0x00000004,(void *)0x0);
  return bVar1;
}
```

What this proves:

- at least one travel-family path in the binary is a straight UI-message wrapper,
- this matches the general GWCA pattern where map and guild-hall travel are packetized and sent via UI manager routes.

### E4. What this means for the Py4GW call map

The binary verification tightens several earlier claims:

- `ChangeTarget`
  - stronger than "GWCA analogue inferred"
  - binary-confirmed as a UI-message path with a 2-field payload
- `InteractAgent`
  - stronger than "GWCA analogue inferred"
  - binary-confirmed as classified world-action packaging plus UI dispatch
- `UseSkill`
  - still a compiled binding from Py4GW's perspective,
  - but its GWCA-side implementation is now known to include target-change and a final `UI::Keypress` path in this binary
- `SendDialog`
  - must be treated as version-sensitive
  - GWCA source says `kSendDialog`
  - this binary says "find frame by dialog id and click it"

### E5. Confidence update after decompilation

After this binary pass, the confidence levels should be thought of as:

- `source-proven`
  - visible in Py4GW Python source or visible GWCA checkout
- `binary-proven`
  - decompiled directly from `gwca.dll`
- `source/binary mismatch`
  - same public API name exists in both places, but implementation differs
- `compiled-binding opaque`
  - still inside `Py4GW.dll` or other compiled extension with no body yet recovered

The most important `source/binary mismatch` currently identified is:

- `GW::Agents::SendDialog`

Secondary mismatches or implementation-shape differences identified:

- `GW::Agents::ChangeTarget`
- `GW::SkillbarMgr::UseSkill`

### E6. Bottom line

Yes, GWCA can be usefully decompiled in this workspace, but the best use of that effort is not recovering GWCA from scratch.

The real value is:

- verifying which GWCA claims are true in the compiled DLL actually present here,
- identifying where the binary diverges from the source checkout,
- using that to sharpen the Py4GW-to-GWCA mapping so the document stays honest about what is source-proven versus binary-proven.
