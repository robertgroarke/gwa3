# GWA3 Kanban Board

## Done

### GWA3-058: EffectMgr — Buff/Effect Tracking
- **Blocked by**: GWA3-059 (needs EffectArray offset)
- **Moved to**: Backlog (see below)

---

## In Progress

(empty)

---

## Ready

(empty)

---

## Backlog

### GWA3-058: EffectMgr — Buff/Effect Array Resolution
**Priority**: High — enables morale/buff tracking for Maintenance handler
**Source**: GWCA EffectMgr.cpp pattern `D95D0CD9450C8D45F8` offset -0x1C, and `f6400401741001` offset 0x9

**Problem**: The bot's `HandleMaintenance` cannot detect active buffs (blessings, consets, morale boost) because there is no EffectMgr. The `GetMorale()` helper in FroggyHM.cpp is hardcoded to return 0.

**Acceptance criteria**:
- [ ] Find the effect/buff array via BasePointer context chain (likely WorldContext or AgentContext)
- [ ] Implement `EffectMgr::GetEffects(agentId)` returning the buff array for an agent
- [ ] Implement `EffectMgr::HasEffect(agentId, skillId)` for checking specific buffs
- [ ] Implement `EffectMgr::GetEffectTimeRemaining(agentId, skillId)`
- [ ] Update `FroggyHM::GetMorale()` to read actual morale from effect array
- [ ] Integration test GWA3-060: verify effect array readable, check for at least 0 effects

**GWCA reference**: `Source/EffectMgr.cpp`, `GameEntities/Skill.h` (Effect struct)
**AutoIt reference**: `GWA2.au3` `GetEffect()`, `GetEffects()` — offset chain `[0, 0x18, 0x2C, ...]`

---

### GWA3-059: PostProcessEffect — Visual Effect Hook
**Priority**: Low — only needed for visual effect manipulation
**Source**: GWCA EffectMgr.cpp pattern `D95D0CD9450C8D45F8` offset -0x1C

**Problem**: No ability to hook or modify post-processing visual effects (bloom, blur, etc.)

**Acceptance criteria**:
- [ ] Add PostProcessEffect scan pattern to Offsets
- [ ] Resolve function pointer via pattern
- [ ] Expose as optional hook target

**GWCA reference**: `Source/EffectMgr.cpp` line ~20

---

### GWA3-061: GwEndScene Scan Pattern
**Priority**: Medium — provides proper render function address for hooking
**Source**: GWCA RenderMgr.cpp pattern `8945FC578B7D088B8F` offset -0xD

**Problem**: `ChatMgr::SetRenderingEnabled` currently hooks via the `Render` offset (hook-type pattern from AutoIt). GWCA uses a separate, more precise `GwEndScene` pattern. If the Render offset ever fails, having the GWCA pattern as fallback improves reliability.

**Acceptance criteria**:
- [ ] Add GwEndScene pattern to Offsets table (P2 priority)
- [ ] Post-process: result is function address directly (no deref needed)
- [ ] Update `ChatMgr::SetRenderingEnabled` to prefer GwEndScene over Render offset
- [ ] Integration test: verify pattern resolves and address is callable

**GWCA reference**: `Source/RenderMgr.cpp` line ~55

---

### GWA3-062: SendChat Function Resolution
**Priority**: Low — current packet-based approach works
**Source**: GWCA ChatMgr.cpp pattern `8D85E0FEFFFF50681C01` offset -0x3E

**Problem**: `ChatMgr::SendChat` manually builds chat packets with hardcoded dword packing that only handles short messages. The game's native `SendChat` function handles encoding, length, and channel selection properly.

**Acceptance criteria**:
- [ ] Add SendChat scan pattern to Offsets (P2)
- [ ] Implement `ChatMgr::SendChat` via resolved function call
- [ ] Fallback to current packet approach if pattern fails
- [ ] Integration test: send a team chat message and verify no crash

**GWCA reference**: `Source/ChatMgr.cpp` line ~50

---

### GWA3-063: AddToChatLog / PrintChat Function Resolution
**Priority**: Low — UIMessage dispatch approach works
**Source**: GWCA ChatMgr.cpp patterns `4025FF01000000` offset -0x97 (AddToChatLog), `3D000000007300282B6A` offset -0x46 (PrintChat)

**Problem**: `ChatMgr::WriteToChat` uses UIMessage dispatch which works but requires game thread. The native functions may be callable from any thread and support richer formatting (sender name, color, links).

**Acceptance criteria**:
- [ ] Add AddToChatLog and PrintChat patterns to Offsets (P2)
- [ ] Implement `ChatMgr::WriteToChatEx` with sender name and color support
- [ ] Integration test: write colored chat message with sender name

**GWCA reference**: `Source/ChatMgr.cpp` lines ~100-150

---

### GWA3-064: ItemClick Function Resolution
**Priority**: Medium — needed for chest opening and item interaction
**Source**: GWCA ItemMgr.cpp patterns `C7000F0000008948` offset -0x28 (base), `8B480883EA000F84` offset -0x1C (function)

**Problem**: Item interaction (opening chests, clicking items in inventory) currently uses raw packet sends or NPC interact. The native `ItemClick` function handles the full interaction protocol including distance checks and animation.

**Acceptance criteria**:
- [ ] Add ItemClick_Base and ItemClick patterns to Offsets (P1)
- [ ] Implement `ItemMgr::ClickItem(itemId)` via resolved function
- [ ] Implement `ItemMgr::OpenChest()` using ItemClick on nearest chest agent
- [ ] Integration test: click an item in inventory (verify no crash, item state changes)

**GWCA reference**: `Source/ItemMgr.cpp`

---

### GWA3-065: SkipCinematic Function Resolution
**Priority**: Low — packet approach works
**Source**: GWCA MapMgr.cpp pattern `8B403083780400` offset -0x5

**Problem**: `MapMgr::SkipCinematic` sends `CINEMATIC_SKIP` packet directly. The native function handles edge cases like checking if a cinematic is actually playing.

**Acceptance criteria**:
- [ ] Add SkipCinematic pattern to Offsets (P2)
- [ ] Optionally update `MapMgr::SkipCinematic` to use resolved function
- [ ] Integration test: call during non-cinematic (verify no crash)

**GWCA reference**: `Source/MapMgr.cpp`

---

### GWA3-066: Camera Update Bypass Patch
**Priority**: Low — `CameraMgr::UnlockCam` handles camera unlock via struct write
**Source**: Research — static offset 0x004EFD06, patch `EB 0F` over `8B 45`

**Problem**: The `GetCameraUnlockPatch()` in Memory.h is declared but never staged. It bypasses the camera interpolation/update code, enabling free camera. Currently camera unlock is done by writing `camera_mode = 3` to the Camera struct, which works but the game may reset it.

**Acceptance criteria**:
- [ ] Find scan pattern for the camera update instruction (near `mov eax, [ebp-...]` → `jmp +0xF`)
- [ ] Stage patch in `CameraMgr::Initialize` when pattern resolves
- [ ] Wire `CameraMgr::UnlockCam` to use patch toggle instead of struct write
- [ ] Integration test GWA3-055 already covers camera unlock — verify patch approach works

**GWCA reference**: `Source/CameraMgr.cpp` `patch_cam_update_addr`, Research `GWCA_MemoryPatcher_LiveAddendum.md`

---

### GWA3-067: Level-Data Validation Bypass Patch
**Priority**: Low — only needed for accessing out-of-bounds map data
**Source**: Research — static offset 0x00639362, patch `EB` over `74` (JZ → JMP)

**Problem**: `GetLevelDataBypassPatch()` is declared but never staged. This patch skips a validation branch with string "beyond available level data", allowing access to map data that would normally be bounds-checked.

**Acceptance criteria**:
- [ ] Find scan pattern near the "beyond available level data" string xref
- [ ] Stage patch in Offsets post-processing
- [ ] Expose toggle via `Memory::GetLevelDataBypassPatch().Enable()`
- [ ] Integration test: enable/disable patch without crash

**GWCA reference**: Research `GWCA_MemoryPatcher_LiveAddendum.md`

---

### GWA3-068: Map/Port Branch Bypass Patch
**Priority**: Low — only needed for specific travel edge cases
**Source**: Research — static offset 0x0053898C, patch `90 90` over `75 0C` (JNZ → NOP NOP)

**Problem**: `GetMapPortBypassPatch()` is declared but never staged. This NOP's a branch near GapPorts string references, bypassing map/port validation.

**Acceptance criteria**:
- [ ] Find scan pattern near GapPorts string references
- [ ] Stage patch in Offsets post-processing
- [ ] Expose toggle via `Memory::GetMapPortBypassPatch().Enable()`
- [ ] Integration test: enable/disable patch without crash

**GWCA reference**: Research `GWCA_MemoryPatcher_LiveAddendum.md`

---

### GWA3-069: Trade Offer/Cart Functions
**Priority**: Medium — needed for player-to-player trading
**Source**: GWCA TradeMgr.cpp patterns `6849040000895DE4E8` offset -0x6B (OfferTradeItem), `578B7D0C3DEF000010` offset -0x24 (UpdateTradeCart)

**Problem**: `TradeMgr::OfferItem` and related player trade functions use raw packet sends. The native functions handle the full trade protocol including cart validation and UI updates.

**Acceptance criteria**:
- [ ] Add OfferTradeItem and UpdateTradeCart patterns to Offsets (P1)
- [ ] Implement `TradeMgr::OfferItem` via resolved function
- [ ] Integration test: initiate trade with NPC hero, offer/cancel (verify no crash)

**GWCA reference**: `Source/TradeMgr.cpp`

---

### GWA3-070: Quest Log UI Callback
**Priority**: Low — quest management works via packets
**Source**: GWCA QuestMgr.cpp — FindAssertion-based, plus `684A010010FF7704` offset 0x7A (RequestQuestInfo)

**Problem**: Quest info requests use raw dialog packets. The native callback provides richer quest data including map markers and objective text.

**Acceptance criteria**:
- [ ] Add RequestQuestInfo pattern to Offsets (P2)
- [ ] Implement `QuestMgr::RequestQuestInfo` via resolved function
- [ ] Integration test: request info for active quest, verify no crash

**GWCA reference**: `Source/QuestMgr.cpp`

---

### GWA3-071: FriendList Post-Processing
**Priority**: Low — friend list functions declared but need complex resolution
**Source**: GWCA FriendListMgr.cpp — FriendList assertion + FindInRange + deref chain

**Problem**: `FriendList` offset resolves via assertion but needs complex post-processing (FindInRange within a specific code range, then deref chain). `RemoveFriend` needs FindInRange + ResolveBranchChain. Both are commented as "skip for now" in PostProcessOffsets.

**Acceptance criteria**:
- [ ] Implement FriendList post-processing: find the data pointer via the assertion site context
- [ ] Implement RemoveFriend post-processing: FindInRange + ResolveBranchChain
- [ ] Verify `FriendListMgr::AddFriend` and `RemoveFriend` work end-to-end
- [ ] Integration test: read friend list count (verify no crash)

**GWCA reference**: `Source/FriendListMgr.cpp`

---

### GWA3-072: DrawCompass Hook
**Priority**: Very Low — cosmetic only
**Source**: GWCA MapMgr.cpp pattern `568BF7` offset -0x13 (backward search)

**Problem**: No ability to customize compass drawing or add custom markers.

**Acceptance criteria**:
- [ ] Add DrawCompass pattern to Offsets (P2)
- [ ] Expose as hook target for custom compass overlay

**GWCA reference**: `Source/MapMgr.cpp`

---

### GWA3-073: Chat Color Customization
**Priority**: Very Low — cosmetic only
**Source**: GWCA ChatMgr.cpp patterns `C70060C0FFFF5DC3` offset -0x1C (GetSenderColor), `C700B0B0B0FF5DC3` offset -0x27 (GetMessageColor)

**Problem**: No ability to customize chat channel colors.

**Acceptance criteria**:
- [ ] Add GetSenderColor and GetMessageColor patterns to Offsets (P2)
- [ ] Implement color override hooks

**GWCA reference**: `Source/ChatMgr.cpp`
