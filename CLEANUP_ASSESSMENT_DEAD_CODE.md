# Dead Code Cleanup Assessment

## Summary

Removed ~47 unused functions/declarations across C++ and Python, plus 6 empty stub header files.

## Empty Stub Headers Deleted

These headers contained only an empty namespace with a TODO comment and were never `#include`d anywhere:

| File | Content |
|---|---|
| `gwa3/include/gwa3/managers/CraftMgr.h` | `namespace GWA3::CraftMgr {}` |
| `gwa3/include/gwa3/managers/EventMgr.h` | `namespace GWA3::EventMgr {}` |
| `gwa3/include/gwa3/managers/GameThreadMgr.h` | `namespace GWA3::GameThreadMgr {}` |
| `gwa3/include/gwa3/managers/MerchantMgr.h` | `namespace GWA3::MerchantMgr {}` |
| `gwa3/include/gwa3/managers/RenderMgr.h` | `namespace GWA3::RenderMgr {}` |
| `gwa3/include/gwa3/managers/SkillbarMgr.h` | `namespace GWA3::SkillbarMgr {}` |

## C++ Functions Removed

Each function below was declared in a header and defined in a .cpp but never called anywhere in the codebase (only 2 total references: declaration + definition).

### Packets (CtoS / CtoSHook)

| Function | File | Reason |
|---|---|---|
| `CtoS::AttackAgent` | CtoS.h/cpp | Superseded by `ActionAttack` (which includes callTarget param) |
| `CtoS::SwitchWeaponSet` | CtoS.h/cpp | Never called |
| `CtoS::TradePlayer` | CtoS.h/cpp | Trade initiation uses TradeMgr path instead |
| `CtoSHook::SendPacketCommand` | CtoSHook.h/cpp | Experimental packet lane, never integrated |

### Managers

| Function | File | Reason |
|---|---|---|
| `PartyMgr::HasNativeAddHero` | PartyMgr.h/cpp | Internal check, never queried externally |
| `PartyMgr::AddHenchman` | PartyMgr.h/cpp | Henchman management not used by any bot module |
| `PartyMgr::KickHenchman` | PartyMgr.h/cpp | Same |
| `PartyMgr::LeaveParty` | PartyMgr.h/cpp | Never called |
| `PartyMgr::InvitePlayer` | PartyMgr.h/cpp | Never called |
| `PartyMgr::KickPlayer` | PartyMgr.h/cpp | Never called |
| `PartyMgr::AcceptInvite` | PartyMgr.h/cpp | Never called |
| `PartyMgr::RefuseInvite` | PartyMgr.h/cpp | Never called |
| `PartyMgr::CountVisibleHeroes` | PartyMgr.h/cpp | Replaced by `CountPartyHeroes` |
| `TradeMgr::BuyMerchantItemByModelId` | TradeMgr.h/cpp | Never called (BuyMerchantItemByPosition is used) |
| `TradeMgr::SellMerchantItem` | TradeMgr.h/cpp | `SellInventoryItem` is used instead |
| `TradeMgr::RequestTraderQuoteByModelId` | TradeMgr.h/cpp | `RequestTraderQuoteByItemId` is used instead |
| `UIMgr::SendFrameUIMessage` | UIMgr.h/cpp | Never called (ButtonClick paths used instead) |
| `UIMgr::ControlActionForSkillSlot` | UIMgr.h/cpp | Never called |
| `ItemMgr::DestroyItem` | ItemMgr.h/cpp | Never called |
| `ItemMgr::SplitStack` | ItemMgr.h/cpp | Never called |
| `ItemMgr::SalvageUpgrade` | ItemMgr.h/cpp | Never called (SalvageMaterials is used) |
| `ItemMgr::SalvageSessionCancel` | ItemMgr.h/cpp | Never called |
| `QuestMgr::AbandonQuest` | QuestMgr.h/cpp | Never called |
| `MapMgr::TravelGuildHall` | MapMgr.h/cpp | Never called |
| `MapMgr::LeaveGuildHall` | MapMgr.h/cpp | Never called |
| `MapMgr::EnterChallenge` | MapMgr.h/cpp | Never called |
| `MapMgr::CancelEnterChallenge` | MapMgr.h/cpp | Never called |
| `GuildMgr::LeaveGH` | GuildMgr.h/cpp | Never called |
| `PlayerMgr::ChangeSecondProfession` | PlayerMgr.h/cpp | Never called |
| `SkillMgr::GetSparkflyPlayerUseSkillCount` | SkillMgr.h/cpp | Never called (Reset variant is used) |
| `FriendListMgr::AddFriend` | FriendListMgr.h/cpp | Never called |
| `FriendListMgr::RemoveFriend` | FriendListMgr.h/cpp | Never called |
| `FriendListMgr::SetPlayerStatus` | FriendListMgr.h/cpp | Never called |
| `ChatLogMgr::GetMessage` | ChatLogMgr.h/cpp | Never called (GetMessagesSince is used) |

### Core

| Function | File | Reason |
|---|---|---|
| `Offsets::IsResolved` | Offsets.h/cpp | Never called |
| `Offsets::RefreshBasePointer` | Offsets.h/cpp | Never called (ResolveWorldContext used instead) |
| `Memory::EnableAllPatches` | Memory.h/cpp | Never called (patches enabled individually) |
| `Memory::DisableAllPatches` | Memory.h/cpp | Never called |
| `RenderHook::IsCrashDetected` | RenderHook.h/cpp | Stub returning false, never called |
| `DialogHook::GetWatchedUIMessageId` | DialogHook.h/cpp | Alias for GetArmedUIMessageId, never called |
| `CallbackRegistry::RegisterFrameUIMessageCallback` | CallbackRegistry.h/cpp | Never called |
| `CallbackRegistry::RemoveFrameUIMessageCallback` | CallbackRegistry.h/cpp | Never called |
| `CallbackRegistry::RegisterCreateUIComponentCallback` | CallbackRegistry.h/cpp | Never called |
| `CallbackRegistry::RemoveCreateUIComponentCallback` | CallbackRegistry.h/cpp | Never called |
| `CallbackRegistry::DispatchFrameUIMessage` | CallbackRegistry.h/cpp | Never called |
| `CallbackRegistry::DispatchCreateUIComponent` | CallbackRegistry.h/cpp | Never called |

### Bot

| Function | File | Reason |
|---|---|---|
| `FroggyHM::DebugResolveFirstSkillTarget` | FroggyHM.h/cpp | Never called (other Debug* variants are used) |

### Utils

| Function | File | Reason |
|---|---|---|
| `StringEncoding::DecodeStrAsync` | StringEncoding.h/cpp | Never called (blocking DecodeStr is used) |

## Python Changes

| Item | File | Type |
|---|---|---|
| `latest_full` property | `bridge/observation.py` | Unused method on ObservationBuffer |
| `_drain` method | `bridge/tests/base.py` | Unused method on BridgeTestCase |
| `import json` | `bridge/tests/base.py` | Unused import |
| `snapshot_get` function | `bridge/tests/helpers.py` | Never called |
| `assert_equal` function | `bridge/tests/helpers.py` | Never called |
| `assert_gte` import | `bridge/tests/test_c_actions.py` | Imported but unused in this file |
| `_move_once_and_wait_near` | `bridge/tests/test_f_player_trade.py` | Never called |
| `trade_runtime_debug` | `bridge/tests/trade_harness.py` | Never called |
| `_select_healthy_character_pid` | `bridge/tests/trade_harness.py` | Duplicate of marvin_harness, never called here |
| `_cleanup_stale_character_processes` | `bridge/tests/trade_harness.py` | Duplicate of marvin_harness, never called here |

## Items Intentionally Kept

- **Struct member functions** (`Title::IsPercentageBased`, `Title::HasTiers`, `Patch::SetRedirect`): Part of data structure API, reasonable to keep even if currently unused
- **Offset scan patterns** (`AddFriend`, `RemoveFriend` in Offsets.cpp): These resolve game addresses; the patterns are still valid even if the wrapper functions were removed
- **FriendListMgr::Initialize**: Called from dllmain.cpp, kept even though its API functions were removed
- **Test entry-point functions** (`run_soak`, `run_marvin_soak`, etc.): Designed to be called from external runners
- **`s_frameCallbacks`/`s_createCallbacks` data structures**: Still referenced by `Shutdown()` and `RemoveCallbacks()` for cleanup
