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

## Backlog

(empty — all 16 stories completed)
