# GWA3 Kanban Board

## Done

### GWA3-058: EffectMgr — Buff/Effect Array Resolution
- Effect/Buff/AgentEffects structs, EffectMgr via WorldContext+0x508
- **Commit**: `6090dde`

### GWA3-059: PostProcessEffect — Visual Effect Hook
- Pattern `D9 5D 0C...` at -0x1C, plus DropBuff via FunctionFromNearCall
- **Commit**: `c162c11`

### GWA3-061: GwEndScene — Render Function Pattern
- Pattern `89 45 FC 57 8B 7D 08 8B 8F` at -0xD, render hook prefers over Render fallback
- **Commit**: `c162c11`

### GWA3-064: ItemClick — Item Interaction Dispatch
- Pattern `8B 48 08 83 EA 00 0F 84` at -0x1C, ItemMgr::ClickItem implemented
- **Commit**: `c162c11`

### GWA3-062: SendChat — Native Chat Send
- Pattern `8D 85 E0 FE FF FF 50 68 1C 01` at -0x3E, ChatMgr prefers native over packet
- **Commit**: `bd5fd78`

### GWA3-063: AddToChatLog — Native Chat Log Write
- Pattern `40 25 FF 01 00 00` at -0x97, WriteToChat prefers native over UIMessage
- **Commit**: `bd5fd78`

### GWA3-065: SkipCinematic — Native Cinematic Skip
- Pattern `8B 40 30 83 78 04 00` at -0x5, SkipCinematic prefers native via GameThread
- **Commit**: `bd5fd78`

### GWA3-067: Level-Data Validation Bypass Patch
- Pattern `F6 C4 01 74 1D 68 A9 01 00 00` at +3, patches JZ→JMP. Staged, not enabled.
- **Commit**: `addb02b`

### GWA3-068: Map/Port Branch Bypass Patch
- Pattern `83 C4 0C 85 C0 75 0C 6A 01` at +5, patches JNZ→NOP NOP. Staged, not enabled.
- **Commit**: `addb02b`

### GWA3-066: Camera Update Bypass Patch
- Pattern `89 0E DD D9 89 56 04 DD` at offset 0, patches to `EB 0C`. UnlockCam prefers patch.
- **Commit**: `45c1754`

### GWA3-069: Trade Offer/Cart Functions
- OfferTradeItem `68 49 04 00 00 89 5D E4 E8` at -0x6B, UpdateTradeCart at -0x24
- **Commit**: `45c1754`

### GWA3-070: RequestQuestInfo
- Pattern `68 4A 01 00 10 FF 77 04` at +0x7A → FunctionFromNearCall
- **Commit**: `ff876b7`

### GWA3-071: FriendList Resolution
- FriendListAddr `74 30 8D 47 FF 83 F8 01` at -0xB → deref, FriendEventHandler at -0xC
- **Commit**: `ff876b7`

### GWA3-072: DrawOnCompass
- Assertion pattern charmsg.cpp / "knotCount <= arrsize(message.knotData)" at -0x2E
- **Commit**: `ff876b7`

### GWA3-073: Chat Color Functions
- GetSenderColor `C7 00 60 C0 FF FF 5D C3` at -0x1C, GetMessageColor at -0x27
- **Commit**: `ff876b7`

---

## In Progress

(empty)

---

## Ready

(empty)

---

## Backlog — Advanced Integration Tests

### GWA3-074: Item Workflow Tests (Equip, Move, Split, Identify)
**Priority**: High — core bot inventory management untested
**Coverage gap**: 15 untested ItemMgr functions

**Tests**:
- [ ] `TestItemMove`: MoveItem between bags → verify item appears in target bag/slot, disappears from source
- [ ] `TestItemEquip`: EquipItem(weaponId) → verify `item.equipped == true`, weapon set updates
- [ ] `TestStackSplit`: SplitStack(stackableId, half_qty) → verify two stacks with correct quantities
- [ ] `TestItemIdentify`: IdentifyItem(unidentifiedId, idKitId) → verify `interaction & 0x1` set after
- [ ] `TestGoldTransfer`: ChangeGold(charGold-1000, storageGold+1000) → verify both values updated
- [ ] `TestMemAllocFree`: MemAlloc(1024) → write pattern → MemFree → no crash

**Preconditions**: Requires items in inventory. Tests should snapshot inventory before and restore after.

---

### GWA3-075: Salvage Session Workflow
**Priority**: High — salvage loop is critical for Froggy bot maintenance
**Coverage gap**: 5 untested salvage functions

**Tests**:
- [ ] `TestSalvageSession`: Find salvageable item + salvage kit → SalvageSessionOpen → SalvageMaterials → SalvageSessionDone
- [ ] Verify material count increases after salvage
- [ ] Verify salvage kit quantity decreases by 1
- [ ] `TestSalvageCancel`: Open session → SalvageSessionCancel → verify item still exists
- [ ] Verify no crash when opening session with invalid kit/item IDs

**Preconditions**: Requires a salvage kit and a salvageable item in inventory.

---

### GWA3-076: Skillbar Management Tests
**Priority**: Medium — skill loading is used in bot setup but untested
**Coverage gap**: 4 untested SkillMgr functions

**Tests**:
- [ ] `TestLoadSkillbar`: LoadSkillbar([8 known skill IDs]) → verify all 8 slots match via GetPlayerSkillbar
- [ ] `TestSetSingleSkill`: SetSkillbarSkill(slot=3, skillId=X) → verify GetSkillbarSkill(3).skill_id == X
- [ ] `TestHeroSkillUse`: UseHeroSkill(heroIdx=1, slot=1, targetId) → verify hero's skill enters recharge
- [ ] `TestToggleHeroSlot`: ToggleHeroSkillSlot(1, 5) → verify slot state changes (disabled/enabled)

**Preconditions**: Requires outpost (for skillbar load) and explorable (for hero skill use).

---

### GWA3-077: Party Management Tests (Henchmen, Invites, Tick)
**Priority**: Medium — party composition affects all farming runs
**Coverage gap**: 9 untested PartyMgr functions

**Tests**:
- [ ] `TestAddKickHenchman`: AddHenchman(validId) → verify party size increases; KickHenchman → size decreases
- [ ] `TestKickAllHeroes`: Add 3 heroes → KickAllHeroes → verify party size = 1 (self only)
- [ ] `TestHeroTargetLock`: LockHeroTarget(1, foeId) → verify hero attacks target; unlock → hero returns to guard
- [ ] `TestPartyTick`: Tick(true) → verify ready state sent; Tick(false) → unready
- [ ] `TestLeaveParty`: LeaveParty → verify party dissolved (solo); re-add heroes to restore

**Preconditions**: Requires outpost for hero/henchman management.

---

### GWA3-078: Title Management Tests
**Priority**: Medium — title tracking used for reputation farming
**Coverage gap**: 3 untested PlayerMgr title functions

**Tests**:
- [ ] `TestSetActiveTitle`: SetActiveTitle(knownTitleId) → verify GetActiveTitleId() returns it
- [ ] `TestRemoveActiveTitle`: RemoveActiveTitle → verify GetActiveTitleId() returns 0
- [ ] `TestTitleClientData`: GetTitleData(titleId) → verify title_flags and name_id are plausible
- [ ] `TestTitleProgress`: GetTitleTrack(titleId) → verify current_points >= 0, max_title_rank > 0

**Preconditions**: Requires a character with at least one earned title.

---

### GWA3-079: NPC Merchant Buy/Sell Workflow
**Priority**: High — merchant interaction is critical for bot economy
**Coverage gap**: TransactItems sell tested in FroggyHM, but buy flow untested

**Tests**:
- [ ] `TestMerchantBuy`: Open merchant → BuyMaterials(ironIngotModel, 10) → verify gold decreases, material count increases
- [ ] `TestMerchantSell`: Open merchant → TransactItems(SELL, qty, itemId) → verify item removed, gold increases
- [ ] `TestRequestQuote`: RequestQuote(itemId) → verify TraderHook captures quoteId and costValue
- [ ] `TestMerchantWindowPersistence`: Open merchant → verify GetMerchantItemCount > 0; close → verify returns to 0

**Preconditions**: Requires outpost with merchant NPC and gold + items in inventory.

---

### GWA3-080: CallbackRegistry Tests
**Priority**: High — core infrastructure for reactive event handling, 0% tested
**Coverage gap**: 8 untested functions

**Tests**:
- [ ] `TestUIMessageCallback`: Register callback for known msgId → trigger via SendUIMessage → verify fires
- [ ] `TestFrameUIMessageCallback`: Register for frame message → SendFrameUIMessage → verify fires
- [ ] `TestCallbackRemoval`: Register → Remove → re-trigger → verify doesn't fire
- [ ] `TestBulkRemoveCallbacks`: Register 3 callbacks with same HookEntry → RemoveCallbacks(entry) → verify all removed
- [ ] `TestCallbackAltitude`: Register pre (alt=-1) and post (alt=1) → verify pre fires before post

**Preconditions**: Requires game thread running for UI message dispatch.

---

### GWA3-081: GameThread Persistent Callbacks
**Priority**: Medium — per-frame callbacks used by hooks but never directly tested
**Coverage gap**: RegisterCallback/RemoveCallback untested

**Tests**:
- [ ] `TestRegisterCallback`: RegisterCallback(entry, [&count]{count++}) → sleep 500ms → verify count > 0
- [ ] `TestRemoveCallback`: Register → Remove → reset count → sleep 500ms → verify count stays 0
- [ ] `TestCallbackAltitude`: Register cb1(alt=1000) and cb2(alt=8000) → verify execution order via timestamps
- [ ] `TestMultipleCallbacks`: Register 5 callbacks → verify all fire each frame → remove all → verify stopped

**Preconditions**: GameThread must be initialized.

---

### GWA3-082: StoC Packet Type Coverage
**Priority**: Medium — enables reactive bot behavior for specific game events
**Coverage gap**: ~50+ packet headers with no callback tests

**Tests**:
- [ ] `TestAgentUpdatePacket`: Register for header 0xE1 → wait 3s → verify hits > 0 (agent position updates)
- [ ] `TestPingPacket`: Register for PING_REPLY header → wait 5s → verify hits > 0
- [ ] `TestMapLoadPacket`: Register for instance load header → Travel → verify callback fires on map change
- [ ] `TestMultiHeaderCallbacks`: Register on 5 different headers → verify independent hit counters
- [ ] `TestPacketBlocking`: Register pre-callback that blocks → verify original handler not called (via post-callback absence)

**Preconditions**: Requires in-game session with active server traffic.

---

### GWA3-083: Quest Management Tests
**Priority**: Low — quest packet approach works, but native functions untested
**Coverage gap**: 3 untested QuestMgr functions

**Tests**:
- [ ] `TestSetActiveQuest`: SetActiveQuest(activeQuestId) → verify quest compass marker updates
- [ ] `TestAbandonQuest`: AbandonQuest(questId) → verify quest removed from log
- [ ] `TestRequestQuestInfo`: RequestQuestInfo(questId) → verify no crash; check if quest data StoC packet fires

**Preconditions**: Requires an active quest in the quest log.

---

### GWA3-084: Guild Hall Travel Tests
**Priority**: Low — guild features rarely used in farming bots
**Coverage gap**: 3 untested GuildMgr travel functions

**Tests**:
- [ ] `TestTravelGH`: TravelGH() → verify MapId changes to guild hall type
- [ ] `TestLeaveGH`: TravelGH → LeaveGH → verify returns to outpost
- [ ] Verify round-trip: outpost → guild hall → outpost preserves party state

**Preconditions**: Requires character in a guild with a guild hall.

---

### GWA3-085: UI Frame Interaction Tests
**Priority**: Low — frame queries tested, but interaction functions untested
**Coverage gap**: 6 untested UIMgr functions

**Tests**:
- [ ] `TestGetChildOffset`: GetChildOffsetId(knownFrame) → verify offset is 4-byte aligned
- [ ] `TestGetFrameContext`: GetFrameContext(button) → verify returns non-null parent
- [ ] `TestSendUIMessage`: SendUIMessage(safe_msgId, nullptr, nullptr) → verify no crash
- [ ] `TestButtonClick`: ButtonClick(logoutFrame) — **DO NOT ACTUALLY CLICK** — just verify frame resolves and function pointer is non-null

**Preconditions**: Requires in-game with UI loaded.

---

### GWA3-086: Agent Interaction Tests
**Priority**: Low — most agent APIs tested, 3 remaining
**Coverage gap**: CallTarget, InteractPlayer, InteractSignpost

**Tests**:
- [ ] `TestCallTarget`: CallTarget(nearbyAllyId) → verify no crash; check for "target called" chat event
- [ ] `TestInteractSignpost`: Move near a signpost agent → InteractSignpost(signpostId) → verify interact initiated
- [ ] `TestAgentExists`: GetAgentExists(myId) → true; GetAgentExists(99999) → false

**Preconditions**: Requires outpost or explorable with nearby agents.

---

### GWA3-087: Camera FOV Test
**Priority**: Very Low — cosmetic, only 1 function untested
**Coverage gap**: SetFieldOfView

**Tests**:
- [ ] `TestFOVAdjustment`: Save original → SetFieldOfView(1.5) → verify GetFieldOfView() ≈ 1.5 → restore original

**Preconditions**: Camera struct must be resolved.

---

### GWA3-088: Full Bot Loop Stress Test
**Priority**: High — end-to-end validation of the complete farming cycle
**Coverage gap**: No test exercises the full bot state machine

**Tests**:
- [ ] Start bot in CharSelect state → verify transitions through all states
- [ ] Complete one full Bogroot run: CharSelect → Town → Travel → Dungeon L1 → L2 → Boss → Loot → Merchant
- [ ] Verify run counter increments, gold increases, inventory changes
- [ ] Verify error recovery: inject bad state → verify Error handler → recovery to InTown
- [ ] Measure: run time, wipe count, items sold, gold earned per cycle
- [ ] Verify maintenance triggers when inventory is full or kits are depleted

**Preconditions**: Requires fresh GW client at character select. Long-running test (~15-30 min).
**Execution**: Separate test flag `GWA3_TEST_BOTLOOP` — not part of standard integration suite.

---

### GWA3-089: Memory Personal Dir and Heap Test
**Priority**: Very Low
**Coverage gap**: GetPersonalDir, MemAlloc/MemFree

**Tests**:
- [ ] `TestPersonalDir`: GetPersonalDir(buf, 256) → verify starts with drive letter, contains "Guild Wars"
- [ ] `TestMemAllocFree`: MemAlloc(4096) → write 0xAA pattern → read back → verify match → MemFree → no crash
- [ ] `TestMemAllocZero`: MemAlloc(0) → verify returns null or valid (implementation-defined)

**Preconditions**: MemoryMgr must be initialized.
