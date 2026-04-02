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

---

## In Progress

(empty)

---

## Ready

(empty)

---

## Backlog

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
