# GWA3 Integration Test Catalog

This file documents the slices inside `IntegrationTest.cpp`, what each slice is checking, when it runs, and the current intent of the harness.

## Session Flow

The integration suite is a single injected session that starts at character select and then takes one of two branches:

- `explorable-start`
  - Used when bootstrap lands directly in an explorable instance.
  - Prioritizes explorable action coverage first.
- `outpost-start`
  - Used when bootstrap lands in an outpost.
  - Performs outpost-safe setup first, then enters Sparkfly Swamp for explorable coverage.

In the current stable harness:

- `GWA3-033 Outpost Travel` is intentionally skipped with:
  - `Session reserved for explorable skill coverage`
- `GWA3-043 Hero Flagging` is intentionally skipped in the main explorable branch with:
  - `Disabled in explorable pending Sparkfly hero-command stabilization`

## Test Slices

### GWA3-028: Character Select + Login

- Purpose: validate bootstrap from character select into an in-game session.
- Acceptance:
  - bootstrap reaches a map
  - bootstrap produces a valid `MyID`
  - `GameThread` is initialized
  - a queued `GameThread` callback fires
  - `MapID` and `MyID` are plausible
  - ping is plausible
- Runs:
  - first in every integration session

### GWA3-029: Hero Setup + Consumables

- Purpose: add heroes and set stable baseline party behavior.
- Acceptance:
  - heroes added without crashing
  - hero behaviors set without crashing
- Runs:
  - early in `outpost-start`
  - deferred until after explorable actions in `explorable-start`

### GWA3-030: Travel + Movement

- Purpose: verify local movement actuation and post-move position reads.
- Acceptance:
  - character moves more than `50` units
- Runs:
  - after login in both branches when world state is stable

### GWA3-030 continued: Targeting

- Purpose: verify target selection against a nearby agent.
- Acceptance:
  - current target matches requested target
- Runs:
  - after movement when player world state is stable

### GWA3-031: Loot Pickup

- Purpose: create or find a nearby loot opportunity, move to the item, and pick it up.
- Acceptance:
  - nearby loot opportunity created, or nearby loot already exists
  - ground item can be found
  - pickup changes inventory/gold state or makes the ground item disappear
- Runs:
  - explorable-only coverage after the session is in a skill-castable map
- Notes:
  - current stable harness avoids combat-time hero flagging inside the loot loop

### GWA3-032: NPC + Dialog

- Purpose: verify basic NPC interaction and dialog sends.
- Acceptance:
  - `InteractNPC` sent without crashing
  - dialog sent without crashing
  - `CancelAction` sent without crashing
- Runs:
  - only in the dedicated NPC/dialog test path today

### GWA3-032: Merchant + Trader Quote

- Purpose: verify merchant/trader interaction flow and trader quote observation.
- Acceptance:
  - travel to merchant/trader area succeeds
  - merchant context becomes available
  - merchant item list populates
  - trader quote request is queued
  - trader quote response is observed with matching item and positive cost
- Runs:
  - only in the dedicated merchant/quote test path today

### GWA3-033: Outpost Travel

- Purpose: smoke-test outpost-to-outpost travel.
- Acceptance:
  - target outpost map is reached
  - `MyID` becomes valid after travel
- Runs:
  - historically in the `outpost-start` branch before Sparkfly entry
  - currently intentionally skipped in the stable harness

### GWA3-035: Explorable Entry

- Purpose: enter Sparkfly Swamp from Gadd's Encampment and verify runtime stabilization.
- Acceptance:
  - player position is readable before leaving the outpost
  - exit waypoints are reached
  - Sparkfly Swamp is entered
  - `MyID` is valid after explorable load
  - instance type is explorable
  - explorable runtime stabilizes for repeated samples
- Runs:
  - `outpost-start` branch after outpost-safe setup

### GWA3-036: Player Data Introspection

- Purpose: validate player-array and player-self queries.
- Acceptance:
  - player number is valid
  - self player entry exists
  - player name is non-empty
  - player agent id matches `MyID`
  - player count is at least one
- Runs:
  - advanced introspection phase

### GWA3-037: Camera Introspection

- Purpose: validate camera pointer resolution and basic fields.
- Acceptance:
  - camera struct resolves
  - camera position is non-zero
  - FOV is plausible
  - yaw is finite
- Runs:
  - advanced introspection phase

### GWA3-038: Client / Memory Info

- Purpose: validate high-level client/memory metadata reads.
- Acceptance:
  - GW version is plausible
  - skill timer is running and advances
  - GW window handle is non-null and valid
- Runs:
  - advanced introspection phase

### GWA3-039: Inventory Deep Introspection

- Purpose: validate bag enumeration and basic inventory coherence.
- Acceptance:
  - inventory exists
  - gold values are plausible
  - API gold matches raw struct gold
  - at least one bag and one item are populated
  - active weapon set is in range
  - `GetItemById` and `GetBag(1)` behave sanely
- Runs:
  - advanced introspection phase

### GWA3-040: Agent Array Enumeration

- Purpose: verify raw agent-array enumeration and self-agent lookup.
- Acceptance:
  - agent array pointer and max-agent count are plausible
  - self is found in the array
  - at least one living agent exists
  - `GetMyAgent` and `GetAgentByID` are consistent
- Runs:
  - advanced introspection phase

### GWA3-041: UI Frame Validation

- Purpose: validate common UI frame lookups and states.
- Acceptance:
  - root frame exists or is safely absent
  - root/log-out/play button state checks succeed
  - null frame queries do not crash
- Runs:
  - advanced introspection phase

### GWA3-042: AreaInfo Cross-Validation

- Purpose: cross-check area metadata and map-load state.
- Acceptance:
  - current map `AreaInfo` exists
  - campaign/continent/party-size values are plausible
  - known map expectations match
  - map-loaded and region queries return sane values
- Runs:
  - advanced introspection phase

### GWA3-043: Hero Flagging

- Purpose: verify hero flagging commands in explorable instances.
- Acceptance:
  - `FlagHero(1)` sent without crashing
  - `FlagAll` sent without crashing
  - `UnflagHero(1)` sent without crashing
  - `UnflagAll` sent without crashing
- Runs:
  - intended for explorable-only coverage
  - currently skipped in the stable harness pending Sparkfly stabilization

### GWA3-044: Chat Write (Local)

- Purpose: verify local chat writes and ping stability.
- Acceptance:
  - `WriteToChat` sent without crashing
  - ping remains stable/plausible
- Runs:
  - advanced introspection phase

### GWA3-045: Skillbar Data Validation

- Purpose: validate skillbar contents and skill constant data.
- Acceptance:
  - skillbar exists
  - skillbar owner matches `MyID`
  - loaded skill entries have plausible metadata
  - at least one skill is loaded
- Runs:
  - advanced introspection phase

### GWA3-046: Hard Mode Toggle

- Purpose: verify hard-mode toggles in outpost-safe contexts.
- Acceptance:
  - `SetHardMode(true)` sent without crashing
  - `SetHardMode(false)` sent without crashing
- Runs:
  - `outpost-start` branch before Sparkfly entry

### GWA3-047: Return to Outpost

- Purpose: verify return-to-outpost behavior after explorable coverage.
- Acceptance:
  - explorable instance is left
  - `MyID` is valid after return
  - returned map is outpost-like
- Runs:
  - after explorable coverage
- Notes:
  - this currently behaves as a practical smoke test, not a true defeat-dialog flow model

### GWA3-048: Party State Validation

- Purpose: validate high-level party defeat state.
- Acceptance:
  - party is not defeated
- Runs:
  - advanced introspection phase

### GWA3-049: TargetLog Hook Validation

- Purpose: verify TargetLog hook initialization and target capture behavior.
- Acceptance:
  - hook is initialized
  - target can be set for test
  - hook query returns without crashing
- Runs:
  - advanced introspection phase

### GWA3-050: Guild Data Introspection

- Purpose: validate guild arrays and guild-player lookups.
- Acceptance:
  - guild queries run without crashing
  - guild array contents are sane when present
  - player guild lookups are consistent
  - guild announcement query runs without crashing
- Runs:
  - advanced introspection phase

### GWA3-051: Map State Queries

- Purpose: validate observer/cinematic/map-time queries.
- Acceptance:
  - not in observer mode
  - not in cinematic
  - map is loaded
  - instance time is positive
- Runs:
  - advanced introspection phase

### GWA3-052: Ping Stability

- Purpose: sample ping repeatedly and verify it remains sane.
- Acceptance:
  - all sampled pings are greater than zero
  - all sampled pings are below `5000`
  - spread is below `2000`
- Runs:
  - advanced introspection phase

### GWA3-053: Weapon Set Validation

- Purpose: validate active weapon set and basic weapon-set occupancy.
- Acceptance:
  - active weapon set is in range
  - at least one weapon set has items
- Runs:
  - advanced introspection phase

### GWA3-054: Agent Distance Cross-Check

- Purpose: compare manual and API distance calculations.
- Acceptance:
  - two distance calculations agree within `1.0`
  - distance is positive and below search radius
- Runs:
  - advanced introspection phase

### GWA3-055: Camera Controls

- Purpose: validate camera controls and optional fog toggles.
- Acceptance:
  - max distance set succeeds
  - camera distance changes as expected
  - fog toggle does not crash when available
  - camera unlock succeeds
- Runs:
  - advanced introspection phase

### GWA3-056: StoC Packet Hook

- Purpose: validate StoC callback registration, emulation, and optional live traffic.
- Acceptance:
  - callback registration succeeds
  - emulated packet hits callback with correct header
  - callbacks stop firing after removal
  - optional live packet callback fires, or cleanly skips
- Runs:
  - advanced introspection phase

### GWA3-057: Rendering Toggle

- Purpose: verify rendering-enable toggles.
- Acceptance:
  - disabling rendering does not crash
  - enabling rendering does not crash
- Runs:
  - advanced introspection phase

### GWA3-059: PostProcessEffect Offset

- Purpose: verify post-process and drop-buff offsets resolve.
- Acceptance:
  - relevant offsets resolve, or cleanly skip
- Runs:
  - advanced introspection phase

### GWA3-060: Effect / Buff Array

- Purpose: validate effect-array access and effect/buff query helpers.
- Acceptance:
  - effect queries run without crashing
  - party effects array is sane when present
  - bogus effect/buff lookups return false/zero
- Runs:
  - advanced introspection phase

### GWA3-061: GwEndScene Offset

- Purpose: verify `GwEndScene` offset resolution and region sanity.
- Acceptance:
  - offset resolves independently or into the expected render region
- Runs:
  - advanced introspection phase

### GWA3-062: SendChat Offset

- Purpose: verify `SendChatFunc` offset resolution.
- Acceptance:
  - offset resolves, or cleanly skips
- Runs:
  - advanced introspection phase

### GWA3-063: AddToChatLog Offset

- Purpose: verify `AddToChatLog` offset resolution.
- Acceptance:
  - offset resolves, or cleanly skips
- Runs:
  - advanced introspection phase

### GWA3-064: ItemClick Offset

- Purpose: verify `ItemClick` offset resolution and clickability of a known item.
- Acceptance:
  - offset resolves
  - click function is available for a backpack item, or cleanly skips
- Runs:
  - advanced introspection phase

### GWA3-065: SkipCinematic Offset

- Purpose: verify `SkipCinematicFunc` offset resolution.
- Acceptance:
  - offset resolves, or cleanly skips
- Runs:
  - advanced introspection phase

### GWA3-066: Camera Update Bypass Patch

- Purpose: verify camera-update bypass patch staging and toggling.
- Acceptance:
  - offset resolves
  - patch is staged
  - enable/disable operations do not crash
- Runs:
  - advanced introspection phase

### GWA3-067: Level-Data Bypass Patch

- Purpose: verify level-data bypass patch staging and toggling.
- Acceptance:
  - offset resolves
  - patch is staged
  - enable/disable operations do not crash
- Runs:
  - advanced introspection phase

### GWA3-068: Map/Port Bypass Patch

- Purpose: verify map/port bypass patch staging and toggling.
- Acceptance:
  - offset resolves
  - patch is staged
  - enable/disable operations do not crash
- Runs:
  - advanced introspection phase

### GWA3-069: Trade Function Offsets

- Purpose: verify trade helper offsets resolve.
- Acceptance:
  - `OfferTradeItem` and `UpdateTradeCart` offsets resolve, or cleanly skip
- Runs:
  - advanced introspection phase

### GWA3-070: RequestQuestInfo Offset

- Purpose: verify `RequestQuestInfo` offset resolution.
- Acceptance:
  - offset resolves, or cleanly skips
- Runs:
  - advanced introspection phase

### GWA3-071: FriendList Offsets

- Purpose: verify friend-list address and event-handler offsets resolve.
- Acceptance:
  - both offsets resolve, or cleanly skip
- Runs:
  - advanced introspection phase

### GWA3-072: DrawOnCompass Offset

- Purpose: verify `DrawOnCompass` offset resolution.
- Acceptance:
  - offset resolves, or cleanly skips
- Runs:
  - advanced introspection phase

### GWA3-073: Chat Color Offsets

- Purpose: verify sender/message chat-color offset resolution.
- Acceptance:
  - both offsets resolve, or cleanly skip
- Runs:
  - advanced introspection phase

## Current Stable Harness Notes

- Sparkfly stabilization improved by:
  - waiting for consecutive stable explorable samples after load
  - avoiding hero-command timing in the current stable lane
- Current known-good lane keeps:
  - movement
  - targeting
  - skill activation
  - loot pickup
  - return to outpost
- Current intentionally disabled lane:
  - explorable hero flagging until Sparkfly hero-command timing is better understood
