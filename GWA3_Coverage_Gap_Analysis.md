# GWA3 Coverage Gap Analysis

> Cross-reference of GWCA's actual public API (from headers at
> `toolbox/GWToolboxpp-master/Dependencies/GWCA/include/GWCA/Managers/`)
> against the GWA3 kanban plan. Identifies missing capabilities.

## Source of Truth

The GWCA headers in our repo define **22 manager modules** with **~200 exported functions**.
This is the definitive API surface — not guesswork from research docs.

**Header location:** `toolbox/GWToolboxpp-master/Dependencies/GWCA/include/GWCA/`

---

## Coverage Summary

| Manager | Functions | Kanban Ticket | Coverage |
|---------|-----------|--------------|----------|
| AgentMgr | 28 | GWA3-016 | **90%** — missing MapAgentArray, CountAllegianceInRange |
| ItemMgr | 44 | GWA3-018 | **75%** — missing PickUpItem, OpenXunlai, EquipmentVisibility, PvP items |
| SkillbarMgr | 20 | GWA3-017 | **85%** — missing ChangeSecondProfession, Encode/DecodeSkillTemplate |
| MapMgr | 25 | GWA3-019 | **70%** — missing QueryAltitude, PathingMap, FoesKilled, Cinematic, Challenge |
| PartyMgr | 40 | GWA3-022 | **75%** — missing Tick, PartySearch, PetBehavior, Unflag, RespondToRequest |
| ChatMgr | 20 | GWA3-024 | **60%** — missing timestamps, colors, CreateCommand, AddToChatLog |
| TradeMgr | 8 | GWA3-024 | **100%** |
| QuestMgr | 11 | GWA3-023 | **80%** — missing GetQuestLog, RequestQuestInfo, GetQuestEntryGroupName |
| UIMgr | 60+ | GWA3-020/021/039/040 | **50%** — massive API, many uncovered |
| GameThreadMgr | 6 | GWA3-006 | **100%** |
| EffectMgr | 12 | GWA3-014 | **60%** — missing GetAlcoholLevel, DropBuff, GetDrunkAf |
| FriendListMgr | 15 | GWA3-024 | **40%** — mostly stubs, missing callback, types |
| **CameraMgr** | **10** | **NONE** | **0%** |
| **PlayerMgr** | **15** | **NONE** | **0%** |
| **RenderMgr** | **17** | **partial (024)** | **20%** — only SetRendering, missing DirectX, viewport, callbacks |
| **StoCMgr** | **5** | **NONE** | **0%** |
| **MerchantMgr** | **3** | **partial (018)** | **30%** — TransactItems not separate |
| **EventMgr** | **3** | **NONE** | **0%** |
| **MemoryMgr** | **7** | **NONE** | **0%** |
| **GuildMgr** | **10** | **NONE** | **0%** |
| Module | internal | — | N/A |
| Hook/Scanner/Patcher | utility | GWA3-003/005/037/038 | covered |

---

## Managers With ZERO Coverage (No Ticket)

### CameraMgr — 10 functions

```cpp
Camera* GetCamera();
bool SetMaxDist(float dist = 900.0f);
bool SetFieldOfView(float fov);
Vec3f ComputeCamPos(float dist = 0);
bool UpdateCameraPos();
float GetFieldOfView();
float GetYaw();
bool UnlockCam(bool flag);
bool GetCameraUnlock();
bool SetFog(bool flag);
```

**Bot relevance:** Medium. Camera unlock useful for observation/screenshots. Not needed for Froggy HM core loop but useful for debugging.

### PlayerMgr — 15 functions

```cpp
bool SetActiveTitle(Constants::TitleID title_id);
bool RemoveActiveTitle();
uint32_t GetPlayerAgentId(uint32_t player_id);
uint32_t GetAmountOfPlayersInInstance();
PlayerArray* GetPlayerArray();
PlayerNumber GetPlayerNumber();
Player* GetPlayerByID(uint32_t player_id = 0);
wchar_t* GetPlayerName(uint32_t player_id = 0);
wchar_t* SetPlayerName(uint32_t player_id, const wchar_t* replace_name);
bool ChangeSecondProfession(Constants::Profession prof, uint32_t hero_index = 0);
Player* GetPlayerByName(const wchar_t* name);
Title* GetTitleTrack(Constants::TitleID title_id);
Constants::TitleID GetActiveTitleId();
Title* GetActiveTitle();
TitleClientData* GetTitleData(Constants::TitleID title_id);
```

**Bot relevance:** HIGH. Title tracking is used heavily by Froggy (GetVanguardTitle, GetNornTitle, etc.). ChangeSecondProfession needed for hero builds. GetPlayerName used for chat/whisper.

### StoCMgr — 5 functions

```cpp
bool RegisterPacketCallback(HookEntry*, uint32_t header, const PacketCallback&, int altitude);
bool RegisterPostPacketCallback(HookEntry*, uint32_t header, const PacketCallback&);
size_t RemoveCallback(uint32_t header, HookEntry*);
size_t RemoveCallbacks(HookEntry*);
bool EmulatePacket(Packet::StoC::PacketBase* packet);
```

**Bot relevance:** Medium. Useful for monitoring incoming game events (damage, deaths, loot drops) without polling. Not strictly needed if we read game state directly.

### EventMgr — 3 functions

```cpp
void RegisterEventCallback(HookEntry*, EventID, const EventCallback&, int altitude);
void RemoveEventCallback(HookEntry*, EventID);
bool SendEventMessage(EventID, void*);
```

**Bot relevance:** Low. Event notification system. Useful for reactive behavior but not needed for polling-based bot.

### MemoryMgr — 7 functions

```cpp
uint32_t GetGWVersion();
DWORD GetSkillTimer();
bool GetPersonalDir(size_t buf_len, wchar_t* buf);
HWND GetGWWindowHandle();
void* MemAlloc(size_t size);
void* MemRealloc(void* buf, size_t newSize);
void MemFree(void* buf);
```

**Bot relevance:** Medium. GetGWVersion important for pattern validation. GetSkillTimer used for timing. GetGWWindowHandle needed for window management. MemAlloc/Free for in-process allocation.

### GuildMgr — 10 functions

```cpp
GuildArray* GetGuildArray();
Guild* GetPlayerGuild();
Guild* GetCurrentGH();
Guild* GetGuildInfo(uint32_t guild_id);
uint32_t GetPlayerGuildIndex();
wchar_t* GetPlayerGuildAnnouncement();
wchar_t* GetPlayerGuildAnnouncer();
bool TravelGH();
bool TravelGH(GHKey key);
bool LeaveGH();
```

**Bot relevance:** Low-Medium. TravelGH/LeaveGH already handled via packets. Guild data reading is nice-to-have.

---

## Significant Gaps in EXISTING Tickets

### UIMgr (GWA3-020/021) — Missing ~30 functions

**String encoding/decoding (CRITICAL):**
```cpp
void AsyncDecodeStr(const wchar_t* enc_str, wchar_t* buffer, size_t size);
void AsyncDecodeStr(const wchar_t* enc_str, DecodeStr_Callback callback, void* param, Language lang);
bool IsValidEncStr(const wchar_t* enc_str);
bool UInt32ToEncStr(uint32_t value, wchar_t* buffer, size_t count);
uint32_t EncStrToUInt32(const wchar_t* enc_str);
```
Used for: quest objectives, item names, skill descriptions, NPC names. The existing AutoIt code uses `ValidateAsyncDecodeStr` for this.

**Frame manipulation:**
```cpp
bool SelectDropdownOption(Frame* frame, uint32_t value);
uint32_t CreateUIComponent(uint32_t parent, uint32_t flags, uint32_t tab, callback, void*, const wchar_t*);
bool DestroyUIComponent(Frame* frame);
bool SetFrameVisible(Frame* frame, bool flag);
bool SetFrameDisabled(Frame* frame, bool flag);
bool SetFrameTitle(Frame* frame, const wchar_t* title);
bool TriggerFrameRedraw(Frame* frame);
bool SetFramePosition(Frame* frame, const FramePosition* position);
```

**Window management:**
```cpp
WindowPosition* GetWindowPosition(WindowID window_id);
bool SetWindowVisible(WindowID window_id, bool is_visible);
bool SetWindowPosition(WindowID window_id, WindowPosition* info);
```

**Compass/minimap:**
```cpp
bool DrawOnCompass(unsigned session_id, unsigned pt_count, CompassPoint* pts);
```

**UI state queries:**
```cpp
bool GetIsUIDrawn();
bool GetIsWorldMapShowing();
TooltipInfo* GetCurrentTooltip();
bool IsInControllerMode();
```

### MapMgr (GWA3-019) — Missing ~8 functions

```cpp
float QueryAltitude(const GamePos* pos, float radius, MapContext*);  // Movement support
PathingMapArray* GetPathingMap();                                      // Pathfinding
uint32_t GetFoesKilled();                                             // Vanquish tracking
uint32_t GetFoesToKill();                                             // Vanquish tracking
bool GetIsInCinematic();                                              // Cinematic detection
bool SkipCinematic();                                                 // Skip cutscenes
bool EnterChallenge();                                                // Mission entry
bool CancelEnterChallenge();                                          // Cancel entry
MissionMapIconArray* GetMissionMapIconArray();                        // Mission map UI
```

### ItemMgr (GWA3-018) — Missing ~10 functions

```cpp
bool PickUpItem(const Item* item, uint32_t call_target = 0);         // Ground loot!
bool OpenXunlaiWindow(bool anniversary, bool storage);                // Storage chest
bool CanAccessXunlaiChest();                                          // Storage check
EquipmentStatus GetEquipmentVisibility(EquipmentType type);           // Helm/cape/costume
bool SetEquipmentVisibility(EquipmentType type, EquipmentStatus);     // Toggle visibility
bool PingWeaponSet(uint32_t agent_id, uint32_t weapon, uint32_t off); // Weapon ping
Item* GetHoveredItem();                                                // UI context
uint32_t GetMaterialStorageStackSize();                                // Storage info
```

### EffectMgr (GWA3-014) — Missing ~5 functions

```cpp
uint32_t GetAlcoholLevel();                                           // Drunkard title
void GetDrunkAf(float intensity, uint32_t tint);                     // Visual effect
bool DropBuff(uint32_t buff_id);                                      // Remove buff
Effect* GetPlayerEffectBySkillId(Constants::SkillID skill_id);        // Direct lookup
Buff* GetPlayerBuffBySkillId(Constants::SkillID skill_id);            // Direct lookup
```

---

## Priority Assessment for Froggy HM

### Must-Have (blocks Froggy HM)

| Function | Why | Current Ticket |
|----------|-----|----------------|
| Title tracking (GetTitleTrack, GetTitleData) | Froggy tracks Vanguard/Norn/Asura/Deldrimor progress | **NONE** → needs PlayerMgr ticket |
| PickUpItem | Ground loot pickup in dungeons | GWA3-018 (implicit, needs explicit) |
| SkipCinematic | Skip dungeon cutscenes | GWA3-019 (missing, needs adding) |
| EnterChallenge | Enter dungeon/mission | GWA3-019 (missing, needs adding) |
| ReturnToOutpost | Return after wipe/completion | GWA3-019 (missing, needs adding) |
| AsyncDecodeStr | Quest text, item names | **NONE** → needs UIMgr expansion |
| GetGWWindowHandle | Window management for multi-client | **NONE** → needs MemoryMgr ticket |
| GetInstanceTime | Run timer tracking | GWA3-011 (implicit, needs explicit) |
| GetFoesKilled/ToKill | Vanquish tracking | GWA3-019 (missing) |
| SetActiveTitle | Display title for reputation gain | **NONE** → needs PlayerMgr ticket |
| ChangeSecondProfession | Hero build setup | **NONE** → needs PlayerMgr ticket |
| DropBuff | Remove unwanted enchantments | GWA3-014 (missing) |

### Nice-to-Have (not blocking Froggy)

| Function | Why | Priority |
|----------|-----|----------|
| CameraMgr (all) | Debugging, screenshots | Low |
| StoCMgr (packet callbacks) | Reactive event handling | Medium |
| GuildMgr (data reading) | Guild info display | Low |
| RenderMgr (DirectX access) | Custom overlay | Low |
| EventMgr (event callbacks) | Notification system | Low |
| DrawOnCompass | Custom map markers | Low |
| Chat colors/timestamps | Chat UI customization | Low |
| Party search functions | LFG automation | Low |
| PvP item system | Not relevant to PvE bot | None |
| Equipment visibility | Cosmetic only | None |

---

## Recommended New Tickets

### GWA3-049: PlayerMgr (Titles + Profession + Player Data)

**Must-have.** Title tracking is used ~300 times in Froggy HM.

Functions: SetActiveTitle, RemoveActiveTitle, GetPlayerName, ChangeSecondProfession, GetTitleTrack, GetTitleData, GetActiveTitleId, GetPlayerByID, GetPlayerArray, GetAmountOfPlayersInInstance

### GWA3-050: MemoryMgr (Version + Timers + Window)

**Must-have.** GetGWVersion for pattern validation, GetSkillTimer for timing, GetGWWindowHandle for multi-client.

Functions: GetGWVersion, GetSkillTimer, GetGWWindowHandle, GetPersonalDir, MemAlloc/MemFree

### GWA3-051: String Encoding/Decoding

**Must-have.** Used for quest objectives, item names, NPC names throughout the bot.

Functions: AsyncDecodeStr (both overloads), IsValidEncStr, UInt32ToEncStr, EncStrToUInt32

### GWA3-052: CameraMgr

**Nice-to-have.** Camera control for debugging and observation.

Functions: GetCamera, SetMaxDist, SetFieldOfView, UnlockCam, SetFog, GetYaw

### GWA3-053: StoCMgr (Server-to-Client Packet Callbacks)

**Nice-to-have.** Reactive event monitoring without polling.

Functions: RegisterPacketCallback, RegisterPostPacketCallback, RemoveCallback, EmulatePacket

### GWA3-054: GuildMgr

**Nice-to-have.** Guild data and guild hall travel.

Functions: GetPlayerGuild, GetGuildArray, TravelGH, LeaveGH, GetPlayerGuildAnnouncement

---

## Updates to Existing Tickets

### GWA3-018 (ItemMgr) — Add:
- `PickUpItem()` — ground loot pickup (critical for Froggy)
- `OpenXunlaiWindow()` — storage chest interaction
- `CanAccessXunlaiChest()` — storage availability check

### GWA3-019 (MapMgr) — Add:
- `SkipCinematic()` — skip dungeon cutscenes
- `EnterChallenge()` / `CancelEnterChallenge()` — mission/dungeon entry
- `ReturnToOutpost()` — return after completion/wipe
- `GetFoesKilled()` / `GetFoesToKill()` — vanquish tracking
- `QueryAltitude()` — movement altitude support
- `GetIsInCinematic()` — cinematic state detection

### GWA3-014 (EffectMgr) — Add:
- `DropBuff(buff_id)` — remove buff/enchantment
- `GetAlcoholLevel()` — drunkard title tracking
- `GetPlayerEffectBySkillId()` / `GetPlayerBuffBySkillId()` — direct lookups

### GWA3-022 (PartyMgr) — Add:
- `Tick()` / `SetTickToggle()` — party ready check
- `UnflagHero()` / `UnflagAll()` — cancel hero flags
- `GetIsPartyDefeated()` — wipe detection
- `GetIsPartyInHardMode()` — hard mode state check
- `ReturnToOutpost()` — (also in MapMgr, either location works)
- `SetPetBehavior()` — pet AI control

### GWA3-023 (QuestMgr) — Add:
- `GetQuestLog()` — full quest log access
- `RequestQuestInfo()` — quest detail request

### GWA3-020 (UIMgr) — Add:
- `SelectDropdownOption()` — dropdown interaction
- `SetFrameVisible()` / `SetFrameDisabled()` — frame state control
- `GetFrameByLabel()` — label-based frame lookup (alternative to hash)
- `WindowPosition` management functions
