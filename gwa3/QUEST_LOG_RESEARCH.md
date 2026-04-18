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
                    name_enc?, objectives_enc?, description_enc?,
                    location_enc?, npc_enc? },
    quest_log: [
        { quest_id, log_state, is_completed, is_primary, is_area_primary,
          is_active, map_from, map_to, marker_x, marker_y,
          name_enc?, location_enc?, npc_enc? },
        ...
    ]
}
```

### Encoded strings and decoding

All `*_enc` fields ship the game's encoded wide-char format (the first
wchar is a PUA sentinel, typically `0x8101`/`0x8102`, followed by a
message id). The in-game Quest Log UI runs those through
`ValidateAsyncDecodeStr` to produce "Heart or Mind: Garden in Danger"
/ "Talk to Tekks about helping the yellow Ophil tribe."

**gwa3 intentionally does not decode on the snapshot thread.**

Attempted approach — blocking `StringEncoding::DecodeStr` per quest in
`BuildQuestJson` with a pointer cache — crashed the client once the
quest log exceeded a few entries. Observations:

- `ValidateAsyncDecodeStr` is fire-and-forget. Its callback arrives on
  the game thread at an unknown later time (tens to hundreds of ms for
  an uncached name).
- Enqueueing one decode per quest per field (13 × 3 ≈ 39 calls per
  tier-2 snapshot) fills the GameThread pre-dispatch queue faster than
  it drains. Once the queue tail lagged ~25 entries behind head, GW
  raised a fatal assertion.
- Tight per-call budgets (25–60 ms) always time out because the
  callback fires later than that window.

**What we tried (2026-04-17)** — a dedicated worker thread
(`EncStringCache`) with a bounded rate (one decode every ~550 ms) and
a pointer cache keyed by the encoded string's content. The module is
checked in and wired into `dllmain`, and `StringEncoding::DecodeStr`
was hardened against its original ctx-race (heap context + atomic
state `pending / fulfilled / abandoned`, so a late callback never
touches a closed event handle or freed stack memory).

That fixed the memory-corruption crash, but GW still terminates
after ~170 cumulative `ValidateAsyncDecodeStr` calls even when every
call is paced and uses a safe context. We then split the API so the
snapshot path only reads the cache (`Lookup`, read-only, safe on
every tick) and `request_quest_info(quest_id)` would prime just five
strings per LLM action (`Prime`). That brought per-session decode
volume down from hundreds to single digits — but GW still crashed
~45 s after a single `request_quest_info` call triggered those five
decodes on BISCUIT.

That rules out a rate problem. We then tried the GWCA pattern:
fire-and-forget direct call from a worker thread, no
`GameThread::Enqueue`, no caller-side wait, heap ctx owned by the
callback. That is exactly how GWCA drives the decoder for agent name
/ item name / tooltip rendering, at the same or higher rate than
ours. The crash reproduced **identically** — BISCUIT terminated
~46 seconds after the worker thread fired its first decode call.

That narrows the problem to the scanned address itself:
`Offsets::ValidateAsyncDecodeStr` at `0x005F4C44` is either not
`ValidateAsyncDecodeStr` in this particular GW client build, or
calling the real decoder from an injected DLL requires preconditions
our injection sequence does not yet satisfy (for example, hooking
specific UI callbacks or filling a thread-local parser context that
GWCA installs as part of its own init).

Possibilities to test next:

- **Wrong function.** Our scan uses an assertion search
  (`TextApi.cpp` / `codedString`); GWCA uses a byte pattern
  (`\x83\xC4\x10\x3B\xC6\x5E\x74\x14` at offset `-0x70`). Swapping
  in the byte pattern is cheap — compare the resolved addresses. If
  they differ, try the byte-pattern result directly.
  **Result (2026-04-17):** the two scans resolve to *different*
  addresses — assertion `0x005F4C44`, GWCA pattern `0x005F5050`.
  Swapping to the GWCA-scanned address still produced the same
  delayed crash (this time ~80 s after worker start on BISCUIT).
  That's informative on its own — our original assertion scan was
  indeed landing on the wrong function — but it's not the whole
  story, since the GWCA-scanned address also crashes the client.
  Both offsets are retained in `Offsets.h` as
  `ValidateAsyncDecodeStr` (assertion, legacy) and
  `ValidateAsyncDecodeStrGwca` (byte pattern, preferred by
  EncStringCache when available).
- **Missing text-parser context.** GWCA's `AsyncDecodeStr`
  overloads read `GetGameContext()->text_parser` and temporarily
  swap its `language_id`. Our injection may not initialise that
  parser context the way vanilla game startup does.
  **Result (2026-04-17):** read-only probe on BISCUIT shows the
  chain is fully populated — `gameContext=0x0159F048`,
  `textParser=0x015CDFD0`, `languageId=0` (English). So the context
  is fine; that's not the missing piece either.
- **Missing callback hook.** GWCA hooks `AsyncDecodeStringPtr` (the
  underlying `__fastcall` method) at start-of-day even for clients
  that just want to call it. Maybe that hook is load-bearing for
  internal ref-counting.
  **Result (2026-04-17):** source inspection shows GWCA declares
  the hook target and calls `EnableHooks(AsyncDecodeStringPtr)` but
  never calls `CreateHook(...)` for it — the enable is a no-op in
  MinHook if there's no prior create. So the hook is effectively
  inactive even in upstream; not load-bearing.

### Status as of 2026-04-17

All three candidate fixes from the original list have been tried
and ruled out on BISCUIT:

1. Wrong function — partially true (our assertion scan landed
   `0x40C` before GWCA's byte-pattern result) but the correct GWCA
   address still crashes.
2. Missing text_parser context — ruled out by direct probe.
3. Missing AsyncDecodeStringPtr hook — ruled out by reading GWCA's
   own source.

At this point the honest conclusion is that `ValidateAsyncDecodeStr`
is effectively unreachable from our injection surface via any
documented GWCA path. Possible but speculative causes:

- The byte-pattern scan has multiple matches in this GW build and
  the first match is a different function (we haven't verified
  this — would need to enumerate all matches and examine each).
- Something else gwa3 hooks on init (engine, trader, dialog,
  packet, render) races with the string decoder's internal state.
- The decoder expects to be called only from the game's own
  render/UI thread, and the worker thread we call from doesn't
  satisfy some thread-affinity check that silently corrupts state.

### GWCA disassembly research — sibling-decode insight and memory probe

`research/GWCA_Disassembly_Research/GWCA_UIMessage_Research.md`
lines 3519, 3644–3656 describe a very promising layout:

> label frames store encoded and decoded strings back-to-back in
> context memory rather than via a fresh decode call at getter time
>
>     encoded = ctx->string_base;
>     decoded = encoded + wcslen(encoded) + 1;

That would be exactly the "read-only decode" primitive we want —
no function call, no thread context, no crash risk.

We probed this against the `Quest` struct's five encoded-string
pointers on BISCUIT by reading wchars past the null terminator.
Result (log excerpt):

```
name=19F04430 location=1BBCB9F8 npc=1BBCB918 description=1D0D6608
[name]        enc: 8101 0312 969C F77E 43CB
[name]        after-null hex: FFFF FFFF CB6B DF32 ...
[description] after-null hex: A82C 4DDB 0101 0164  (looks like next alloc's enc header)
```

Conclusion: each quest string lives in its own small heap
allocation. The bytes after the encoded null terminator are heap
metadata or the start of an unrelated neighbouring allocation —
*not* a decoded sibling. The back-to-back layout is specific to
`TextLabelFrame` / `MultiLineTextLabelFrame` **UI frame
contexts**, which only exist after the game has drawn that label
to screen.

### Practical implications

- The sibling-decode trick works only for strings the game has
  already rendered through a label frame. The Quest struct's
  encoded pointers never cross that path, so there's no sibling
  to read.
- A future attempt along this axis would need to iterate the
  game's UI frame tree, find label frames whose encoded string
  matches a quest pointer, and read the decoded sibling from
  that frame's context. That requires the player to have opened
  the Quest Log or related UI at least once during the session
  so the labels get populated.

### Pragmatic path forward

Drop runtime decoding entirely. Maintain a `quest_id -> name`
table in `bridge/farming_knowledge.py` alongside the existing
Tekks's War (825) entry. That covers every well-known quest
deterministically, keeps the LLM snapshot surface clean (`name`
field when known, `name_enc` raw when not), and sidesteps the
whole `ValidateAsyncDecodeStr` hazard. The `EncStringCache`
module stays checked in so if a working decode path is ever
found, it plugs in without reworking callers.

We stopped calling `Prime` from `QuestMgr::RequestQuestInfo`. The
`EncStringCache` API (`Lookup` + `Prime`) stays checked in so a
future decode mechanism can drop into it without rewiring callers.

Safer directions to explore next:

- Read-only decode via memory walk: parse the enc string to extract
  its message id, look that id up in GW's already-populated message
  table directly. No game function calls. Only returns text for
  strings the client has already fetched through normal UI, but that
  covers every name the player has ever opened.
- Opportunistic cache population from `SMSG_QUEST_GENERAL_INFO` /
  `SMSG_QUEST_DESCRIPTION` / `SMSG_QUEST_UPDATE_NAME` packet taps.
- Have the LLM invoke the decode only when the player is physically
  standing in a quest-giver dialog, so GW's decoder is "warm".

**Today**, LLM clients and tests that want human-readable names fall
back to:

1. Looking up known `quest_id`s against the `get_quest_info`
   farming-knowledge table.
2. Matching `map_from` / `map_to` against `MAP_NAMES`.
3. Tracking the quest by `quest_id` alone (it is stable across
   snapshots).

This surface is otherwise a subset of Py4GW's — we do not yet expose
the per-field `IsQuestNameReady` / `GetQuestName` async pattern.

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
