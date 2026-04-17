# Quest Log — Reference Implementation Research

This file documents how each of the four reference codebases we study in this
repo handles reading and manipulating the Guild Wars quest log, and how
`gwa3` is wired on top. Last updated 2026-04-17.

## Summary

| Codebase | Reads | Actuation | Notes |
|----------|-------|-----------|-------|
| **BotsHub / GWA Censured** (AutoIt) | Direct memory read of `WorldContext + 0x52C` (GWArray) + `+0x528` active id | `SendPacket(0x11)` abandon, `SendPacket(0x3B)` dialog accept/reward. No `0x14` set-active observed | Treats `log_state` as a flag bitmask in `Utils.au3` predicates |
| **GWCA / GWToolbox++** (C++) | `WorldContext::quest_log` (GWArray<Quest>) + `active_quest_id` | Native `SetActiveQuest_Func` / `AbandonQuest_Func` / `RequestQuestInfo_Func` resolved via scanner, fronted by `UIMessage::kSendSetActiveQuest`/`kSendAbandonQuest` hooks | UIMessages (`0x30000009`, `0x3000000A`) raised by the native functions, not sent by the client |
| **Py4GW** (Python via compiled GWCA bindings) | `Quest.GetQuestLog()`, `Quest.GetActiveQuest()`, per-field `RequestQuestName/Description/Objectives/Location/NPC` | `Quest.SetActiveQuest(id)`, `Quest.AbandonQuest(id)` → compiled bindings → GWCA UIMessages | All write actions are queued on a 50 ms ActionQueueManager |
| **gwa3** (our C++ DLL + Python bridge) | `QuestMgr::GetQuestLogSize/GetQuestByIndex/GetQuestById/GetActiveQuestId` (WorldContext+0x528/+0x52C), exposed in the LLM snapshot via `quests.quest_log` and `quests.active_quest` | `QuestMgr::SetActiveQuest` / `AbandonQuest` / `RequestQuestInfo` — native fn via Scanner with UIMessage + raw-packet fallbacks. LLM actions: `set_active_quest`, `abandon_quest`, `request_quest_info` | Packet headers `QUEST_ABANDON=0x11`, `QUEST_REQUEST_INFOS=0x12`, `QUEST_SET_ACTIVE=0x14` |

## Quest struct (52 bytes, matches across all references)

From `toolbox/.../GWCA/GameEntities/Quest.h`, reproduced in
`gwa3/include/gwa3/game/Quest.h`:

```
+0x00 uint32  quest_id
+0x04 uint32  log_state        // bitfield — see below
+0x08 wchar_t* location        // encoded
+0x0C wchar_t* name            // encoded
+0x10 wchar_t* npc             // encoded
+0x14 uint32  map_from         // MapID
+0x18 float   marker_x
+0x1C float   marker_y
+0x20 uint32  marker_plane
+0x24 uint32  h0024            // unused / padding
+0x28 uint32  map_to           // MapID
+0x2C wchar_t* description     // encoded — lazy-loaded
+0x30 wchar_t* objectives      // encoded — lazy-loaded
```

`log_state` bitfield (GWCA `Quest::IsPrimary`, `IsAreaPrimary`,
`IsCurrentMissionQuest`, inline at GameEntities/Quest.h:12):

| Bit | Meaning |
|-----|---------|
| 0x02 | completed |
| 0x10 | current mission quest (not in log) |
| 0x20 | primary (story) |
| 0x40 | area primary (e.g. Primary Echovald Forest Quests) |

## Native function resolution (GWCA → gwa3)

GWCA's `QuestMgr::Init()`
(`GWA Censured/GWCA-master/Source/QuestMgr.cpp`):

```cpp
DWORD address = Scanner::FindAssertion(
    "p:\\code\\gw\\ui\\game\\quest\\questlog.cpp",
    "MISSION_MAP_OUTPOST == MissionCliGetMap()", -0x128);
AbandonQuest_Func   = FunctionFromNearCall(address + 0x100);
SetActiveQuest_Func = FunctionFromNearCall(address + 0x96);

RequestQuestInfo_Func = FunctionFromNearCall(
    Scanner::Find("\x68\x4a\x01\x00\x10\xff\x77\x04", "xxxxxxxx", 0x7a));
```

`gwa3`'s `QuestMgr::Initialize()` now mirrors all three resolutions.
All three are called via `GameThread::EnqueuePost` to keep the engine
hook single-action rule intact.

## Fallback hierarchy (gwa3)

For each of `SetActiveQuest`, `AbandonQuest`, `RequestQuestInfo`:

1. Native function pointer, posted to `GameThread`
2. `UIMgr::SendUIMessage(0x30000009 | 0x3000000A, quest_id, null)` via `GameThread`
3. `UIMgr::SendUIMessage` synchronously (if `GameThread` is not yet initialized)
4. Raw `CtoS::QuestSetActive` / `QuestAbandon` packet (`CtoS::SendPacket(2, 0x14 / 0x11, id)`)

This matches GWCA's hook-raises-UIMessage pattern while staying safe when we
inject into a client we didn't hook (UIMessage path still works for
kSend*Quest because the GW client itself dispatches them).

## Gotchas

- **`QuestAbandon` is irreversible on the server** — the quest disappears
  from the log. Treat it like drop-item: require explicit `quest_id`, never
  infer.
- **`RequestQuestInfo` is lazy** — `description` and `objectives` pointers
  can be null until fetched. After firing the request, the next tier-2
  snapshot usually carries the populated text.
- **Outpost-only for some transitions** — the native callback path runs the
  `MISSION_MAP_OUTPOST == MissionCliGetMap()` assertion. Calling set-active
  in an explorable works (the packet path is unaffected), but the UI
  round-trip inside the client may be a no-op.
- **`active_quest_id == 0` means no quest tracked** — not "quest 0".
- **`quest_log_size > 256` implies corrupt / unloaded WorldContext** —
  all gwa3 accessors bail out in that case.
- **Engine hook rule**: only one native call per tick. Quest dispatches
  are posted via `GameThread::EnqueuePost` so they land on a future tick.

## LLM bridge wiring

`tool_schema.py` exposes three tools:

- `set_active_quest(quest_id)`
- `abandon_quest(quest_id)`
- `request_quest_info(quest_id)`

The system prompt in `bridge/agent_loop.py` teaches Gemma the flow:

> Typical flow: read `quests.quest_log`, pick a `quest_id`, call
> `set_active_quest` to focus it, then move toward
> `quests.active_quest.marker_x / marker_y`.

### LLM snapshot shape

Each tier-2 snapshot includes:

```
quests = {
    active_quest_id: int,
    quest_log_size: int,
    active_quest: { quest_id, log_state, is_completed, is_primary,
                    map_from, map_to, marker_x, marker_y,
                    name?, objectives?, description? },
    quest_log: [
        { quest_id, log_state, is_completed, is_primary, is_area_primary,
          is_active, map_from, map_to, marker_x, marker_y,
          name?, location?, npc? },
        ...
    ]
}
```

This surface is a subset of Py4GW's — we do not yet expose the per-field
`IsQuestNameReady` / `GetQuestName` async pattern. For our use case
(Gemma driving a farming bot), the synchronous batch read on every tier-2
snapshot is enough.

## Tests

- `FroggyHM.cpp::RunFroggyUnitTests` — packet header values, `Quest`
  struct size (52), offset of every field, `log_state` bitmask semantics,
  safety of `QuestMgr` accessors on uninitialized WorldContext, and
  (when the log is loaded) round-trip via `GetQuestByIndex` +
  `GetQuestById`.
- `bridge/tests/test_b_observations.py` — tier-2 snapshot structure
  assertions for `quests.*` (already present).
- `bridge/tests/test_m_quest_log.py` (new) — tool schema presence,
  `_execute_tool_calls` forwards quest actions to the pipe (not locally
  handled), action validation errors (`missing quest_id`,
  `quest_id_zero`, `quest_not_in_log`), and the live round-trip that
  `set_active_quest` eventually flips `active_quest_id`.

## File references

- `gwa3/include/gwa3/managers/QuestMgr.h`
- `gwa3/src/managers/QuestMgr.cpp`
- `gwa3/include/gwa3/game/Quest.h`
- `gwa3/include/gwa3/packets/CtoS.h` (`QuestAbandon`, `QuestSetActive`)
- `gwa3/include/gwa3/packets/Headers.h` (`QUEST_ABANDON`, `QUEST_SET_ACTIVE`, `QUEST_REQUEST_INFOS`)
- `gwa3/src/llm/GameSnapshot.cpp::BuildQuestJson`
- `gwa3/src/llm/ActionExecutor.cpp::HandleSetActiveQuest/HandleAbandonQuest/HandleRequestQuestInfo`
- `gwa3/bridge/tool_schema.py`
- `gwa3/bridge/agent_loop.py` (system prompt)
- `gwa3/bridge/tests/test_m_quest_log.py`
- `toolbox/GWToolboxpp-master/Dependencies/GWCA/include/GWCA/Managers/QuestMgr.h`
- `GWA Censured/GWCA-master/Source/QuestMgr.cpp`
- `GWA Censured/lib/botshub/GWA2.au3` (AcceptQuest 1793, AbandonQuest 1805, GetQuestByID 1812)
- `GWA Censured/lib/botshub/GWA2_Assembly.au3` ($QUEST_STRUCT_TEMPLATE 111)
