"""Core agent loop: observe game state, think via LLM, act via gwa3.

Designed for long-running autonomous play. The LLM receives a standing objective
at startup and pursues it indefinitely. The user can optionally send messages
to adjust behavior, but no input is required.
"""

import asyncio
import datetime as _dt
import httpx
import json
import os
import re
import uuid
import time
import sys

from .ipc_client import IpcClient
from .kamadan_client import KamadanClient
from .llm_client import LLMClient, LLMResponse
from .protocol import IPC_PROTOCOL_VERSION, TOOL_SCHEMA_VERSION
from .token_budget import TokenBudgetExceeded, TokenBudgetGuard
from .tool_schema import (
    FROGGY_AUTONOMOUS_TOOLS,
    FROGGY_AUTONOMOUS_TOOL_NAMES,
    filter_tools_for_observation,
    tools_for_observation,
)
from .observation import ObservationWindow
from . import farming_knowledge

try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")
except (AttributeError, ValueError):
    pass


FROGGY_MAINTENANCE_FREE_SLOT_THRESHOLD = 10
FROGGY_CRITICAL_FREE_SLOT_THRESHOLD = 2
FROGGY_CHARACTER_GOLD_PRESSURE_THRESHOLD = 90000
FROGGY_STORAGE_GOLD_CONSET_THRESHOLD = 800000
FROGGY_RECENT_MAINTENANCE_COOLDOWN_SECONDS = 1800.0
FROGGY_CONSET_MATERIAL_STACK_TRIGGER = 10
FROGGY_CONSET_MATERIAL_PRESSURE_FREE_SLOT_THRESHOLD = 10
FROGGY_STORED_CONSET_LOW_THRESHOLD = 30
FROGGY_MAP_NOT_LOADED_WARNING_SECONDS = 60.0
FROGGY_MAP_NOT_LOADED_STOP_SECONDS = 180.0
FROGGY_FAILED_ACTION_WINDOW_SECONDS = 180.0
FROGGY_FAILED_ACTION_RETRY_LIMIT = 2
FROGGY_AUTOPILOT_DIRECT_ACTIONS = {
    "wait",
    "froggy_run_full_maintenance",
    "froggy_run_town_setup",
    "froggy_travel_to_gadds",
    "froggy_travel_to_sparkfly",
    "froggy_run_sparkfly_route_to_tekks",
    "froggy_prepare_tekks_dungeon_entry",
    "froggy_run_dungeon_loop",
}
FROGGY_AUTOPILOT_ENABLED = os.environ.get(
    "GWA3_FROGGY_AUTOPILOT", ""
).strip().lower() in {"1", "true", "yes", "on"}
FROGGY_HARNESS_FALLBACK_ENABLED = os.environ.get(
    "GWA3_FROGGY_HARNESS_FALLBACK", ""
).strip().lower() in {"1", "true", "yes", "on"}

FROGGY_HIGH_LEVEL_RUNBOOK = """\
## Froggy HM High-Level Runbook
You are the run supervisor. Use the current map, inventory, quest state, and last
tool result to decide which phase should run next. The harness recommendation is
advice, not an automatic driver.

Decision checkpoints:
1. If map is not loaded, wait briefly and re-check.
2. If in Gadd's Encampment:
   - Run full maintenance when inventory slots are low/unknown, conset materials
     are pressuring inventory, loose consets need storage, gold is high, or stored
     consets are low.
   - Otherwise run town setup, then travel to Sparkfly.
3. If travel to Sparkfly just completed, wait for a loaded Sparkfly snapshot before
   starting froggy_run_sparkfly_route_to_tekks. Do not call froggy_run_dungeon_loop
   immediately after Sparkfly travel; use the segmented Tekks route first. If the
   first Sparkfly route call fails immediately after travel, wait/retry once because
   party/runtime spawn readiness may lag the map transition.
4. If in Sparkfly Swamp, prefer the segmented Froggy flow:
   - froggy_run_sparkfly_route_to_tekks to reach the quest-giver side.
   - froggy_prepare_tekks_dungeon_entry to accept/refresh Tekks and enter Bogroot.
     This tool owns repeated Tekks accept failures and dialog-reset recovery;
     retry it once after a fresh failure instead of using generic dialog tools.
   - wait/query if the map is transitioning.
   Use froggy_run_dungeon_loop from Sparkfly only as recovery if a segmented step
   cannot make progress.
5. If in Bogroot Growths level 1 or 2, call froggy_run_dungeon_loop to resume the
   native route from the current map.
6. If froggy_run_dungeon_loop succeeds, treat the run as complete and reward
   claimed. Query/refresh state, then choose maintenance or the next setup/travel
   cycle from inventory state.
7. If a Froggy action fails, inspect the error and map:
   - In Gadd's, prefer maintenance or town setup.
   - In Sparkfly/Bogroot, retry froggy_run_dungeon_loop unless the map or quest
     state clearly says to recover to Gadd's.
   - In Embark or an unexpected outpost, recover to Gadd's.

Do not use generic movement, combat, dialog, merchant, or return-to-outpost tools
for Froggy farming unless the high-level Froggy tool surface is missing a required
recovery. A garbled Tekks dialog such as [128] is not a reason to call send_dialog;
use froggy_prepare_tekks_dungeon_entry so the native reset logic can handle it.\
"""


SYSTEM_PROMPT = """\
You are an autonomous Guild Wars bot. You control a character through function calls \
and play the game independently for extended periods without human supervision.

You receive game state snapshots every few seconds. Based on the current state, you \
decide and execute actions immediately. You do NOT wait for human instructions — you \
act on your own judgment to pursue your current objective.

## Your Behavior
- You are PROACTIVE. Every time you see game state, decide what to do next and do it.
- You are PERSISTENT. If something fails, try a different approach. If you die, recover.
- You are EFFICIENT. Don't issue redundant actions. Check if your last action completed \
  before issuing a new one (e.g., don't spam move_to if you're already moving).
- You are SILENT by default. Only output text when something important happens \
  (run completed, error, unusual situation). Don't narrate every action.

## Decision Flow (every tick)
1. Am I alive? If not → wait for party recovery or return to outpost.
2. Is the party defeated? → return_to_outpost.
3. Am I in the right map? If not → travel to the target area.
4. Am I in an outpost/town? → set up party, hard mode, then enter mission.
5. Am I in an explorable? → move toward objectives, fight enemies, loot items.
6. Is my inventory full? → return to outpost, sell/salvage, resume.
7. Is a dialog open? → read the options, choose the right one for my objective.
8. Is a merchant open? → buy/sell as needed.

## Game Knowledge
- Allegiance: 1=ally, 3=foe/enemy, 6=spirit, 0=neutral/NPC
- Agent type: 0xDB (219)=living, 0x200 (512)=signpost/gadget, 0x400 (1024)=item
- HP and Energy are 0.0-1.0 fractions (0.85 = 85%)
- Skill slots are 0-7, recharge > 0 means cooling down
- Hero behavior: 0=fight, 1=guard, 2=avoid combat
- To clear a party, remove heroes individually with repeated kick_hero calls for the currently present hero IDs.
- Do not assume any bulk kick-all-heroes action exists or is reliable.
- Item rarity: white < blue < purple < gold < green. Always pick up gold and green items.
- has_hex=true means the foe already has a hex on it (don't re-hex)
- has_enchantment=true means the foe has an enchantment (consider enchant removal)
- is_casting=true + casting_skill_id tells you what the enemy is casting (interrupt if dangerous)

## Combat Priority
1. Use defensive skills first if HP < 30%
2. Call target on priority enemies (monks/healers first)
3. Use interrupt skills on enemies casting dangerous spells
4. Use AoE skills when 3+ enemies are clustered
5. Pick up dropped items between fights when safe
6. Resurrect dead party members when area is clear

## Error Recovery
- If stuck in the same position for 3+ cycles, try moving to a different nearby point
- If a skill keeps failing, skip it and try another
- If party wipes, return to outpost and restart the run
- If disconnected (map not loaded), stop acting and wait

## Froggy HM Control
When the objective is Froggy HM / Bogroot Growths, use high-level Froggy tools
instead of generic travel, dialogs, return_to_outpost, individual movement, or
combat tools:
- In Gadd's Encampment, call froggy_run_town_setup, then froggy_travel_to_sparkfly.
- If inventory/gold/material pressure needs upkeep in Gadd's Encampment, call
  froggy_run_full_maintenance before town setup/travel.
- In Sparkfly Swamp, prefer the segmented flow so you own more of the run:
  call froggy_run_sparkfly_route_to_tekks, then
  froggy_prepare_tekks_dungeon_entry, then wait/query through the map transition.
  Use froggy_run_dungeon_loop from Sparkfly only as recovery if segmented entry
  fails or state is ambiguous.
- In Bogroot Growths level 1 or 2, call froggy_run_dungeon_loop.
- If froggy_run_dungeon_loop returns success, treat that as a completed dungeon
  run with reward turn-in handled. The native Froggy loop may use resign/return
  after reward, so do not describe that successful return as a wipe.
- After a successful froggy_run_dungeon_loop, refresh state before starting the
  next run. If free inventory slots are low or unknown in Gadd's Encampment,
  call froggy_run_full_maintenance before town setup/travel.
- Do not use generic travel to enter Sparkfly/Bogroot for Froggy. Use the Froggy
  route tools because explorable entry requires the validated waypoint/zone path.
- If a Froggy tool result includes recommended_next_action, follow it unless the
  current game state clearly contradicts it.
- When snapshot.route.deviation is null and a high-level Froggy route tool is
  already controlling progress, prefer wait/query_state/no-op over raw movement
  or duplicate route calls.
- In Froggy mode, do not handle open dialogs with generic dialog actions. Town,
  merchant, Tekks, reward, blessing, and dungeon-entry dialogs are owned by the
  high-level Froggy tools. If a dialog is open in Gadd's Encampment, still follow
  the Froggy recommendation instead of guessing that it is a quest reward.
- You are still responsible for phase decisions. The harness may recommend a
  next action, but normal Froggy mode does not silently execute it for you.

## Communication
If the user sends a message, respond briefly and adjust your objective if asked. \
Then resume autonomous play. The user's messages are optional — you don't need them to operate.
## Price Discovery
Before agreeing to player-to-player trades, use search_trade_prices("item name") to check
recent Kamadan trade chat history. Look for WTS (want to sell) and WTB (want to buy) offers
to understand the current market price range. This prevents you from overpaying or underselling.
Example: search_trade_prices("Ecto") returns recent offers so you know Ectos trade around 4e each.

## Farming Knowledge Lookups (answered locally — NO game-thread cost)
Before embarking on any farming task, query these lookups instead of guessing. They
are free (resolved from a static table, not a game round trip) and return structured
JSON you can feed directly into action calls:

- get_recipe(consumable_model_id) — for crafting. Returns the required materials
  (model_id + quantity per craft), the crafter NPC name + coords, the outpost map_id,
  and the gold cost. Known: 24861 Grail of Might, 24859 Essence of Celerity,
  24860 Armor of Salvation.

- get_outpost_info(map_id) — for knowing where everything is in a town. Returns
  material trader / Xunlai chest / merchant / crafter coords. Embark Beach (857) and
  Gadd's Encampment (638) are fully populated; other outposts return at least a
  material_trader_npc_model_id you can scan the agent list for.

- get_material_info(model_id) — material model_id → human name. Useful when you see
  an item in inventory or at a trader and want to match it to a recipe.

- get_dungeon_info(name) — FULL run procedure: entry outpost, required quest (giver
  NPC + map + coords + dialog codes), prep (hard_mode flag, recommended consumables,
  7-hero team with builds, blessings to acquire), every level's spawn + key_points
  (blessings / keys / doors / portals / boss / end_chest), hazards, and the quest
  reward turn-in NPC. ALWAYS call this before attempting a dungeon you haven't done.

- get_blessing_info(blessing_type) — dialog codes to send (0x84 / 0x85) + effect_ids
  to look for on me.effects after to verify receipt. Types: asuran, norn, dwarven,
  vanguard, sunspears, lightbringer.

- get_hero_build(hero_name) — standard Mercenary skillbar templates for Xandra,
  Olias, Livia, Master of Whispers, Gwen, Norgu, Razah, Dunham. Returns hero_id
  (to pass to add_hero) + skillbar_template (to pass to load_skillbar).

- get_quest_info(name_or_id) — look up a quest by name ("Tekks's War") or quest_id
  (825). Returns giver NPC coords + dialog codes. Use this when a dungeon's quest
  field points at a quest you need to accept.

If a lookup returns error:"unknown_*", the response includes a known_* list so you
can see what IS supported and either pick a valid option or report back that a
table needs updating.

## Quest Log Manipulation
Each snapshot carries quests.quest_log (array of {quest_id, name, log_state,
is_completed, is_primary, is_active, map_from, map_to, marker_x, marker_y,
location, npc}) and quests.active_quest for the quest currently tracked on
the compass. To drive the quest log:

- set_active_quest(quest_id) — switches which quest's marker shows on the
  compass. quest_id must already be present in quests.quest_log.
- abandon_quest(quest_id) — irreversibly drops a quest from the log. Only
  use when the quest is blocking the log or is known to be safely
  re-accept-able from its giver.
- request_quest_info(quest_id) — asks the server to populate the full
  description + objectives text on a quest entry. After firing, wait for
  the next snapshot and re-read quests.active_quest.objectives.

Typical flow: read quests.quest_log, pick a quest_id, call set_active_quest
to focus it, then move toward quests.active_quest.marker_x / marker_y.
"""


DEFAULT_OBJECTIVE = (
    "Farm continuously. Complete dungeon/mission runs, sell loot when inventory is full, "
    "restock consumables, and repeat. Maximize gold and valuable drops per hour."
)


class AgentLoop:
    """Autonomous observe-think-act agent loop."""

    def __init__(
        self,
        ipc: IpcClient,
        llm: LLMClient,
        autonomy: str = "tactical",
        objective: str | None = None,
        kamadan_client: KamadanClient | None = None,
        token_budget: TokenBudgetGuard | None = None,
    ):
        self.ipc = ipc
        self.llm = llm
        self.autonomy = autonomy
        self.objective = objective or DEFAULT_OBJECTIVE
        self.observations = ObservationWindow()
        self.kamadan = kamadan_client or KamadanClient()
        self.token_budget = token_budget
        self.history: list[dict] = []
        self.max_history = 40
        self._running = False
        self._user_message_queue: asyncio.Queue[str] = asyncio.Queue()
        self._cycle_count = 0
        self._last_action_time = 0.0
        self._last_froggy_full_maintenance_success_time = 0.0
        self._consecutive_no_action = 0
        self._llm_rate_limit_until = 0.0
        self._llm_rate_limit_backoff_seconds = 15.0
        self._last_rate_limit_fallback_time = 0.0
        self._last_successful_froggy_action = ""
        self._last_successful_froggy_action_time = 0.0
        self._consecutive_froggy_maintenance_failures = 0
        self._last_froggy_maintenance_failure_time = 0.0
        self._froggy_force_llm_once = False
        self._map_not_loaded_since = 0.0
        self._map_not_loaded_warned = False
        self._last_wait_log_time = 0.0
        self._failed_froggy_actions: dict[str, list[float]] = {}

    async def inject_user_message(self, message: str):
        """Inject a user chat message into the agent loop."""
        await self._user_message_queue.put(message)

    async def _collect_observations(self):
        """Read all pending messages from the pipe and update observation state."""
        while True:
            msg = await asyncio.wait_for(self.ipc.read_message(), timeout=0.05)
            if msg is None:
                break

            msg_type = msg.get("type", "")
            if msg_type == "snapshot":
                self.observations.add_snapshot(msg)
            elif msg_type == "event":
                self.observations.add_event(msg)
            elif msg_type == "action_result":
                pass  # action results are collected by _wait_for_action_result
            elif msg_type == "heartbeat":
                pass

    async def _collect_observations_safe(self):
        """Collect observations, swallowing timeouts (expected when pipe is idle)."""
        try:
            await self._collect_observations()
        except asyncio.TimeoutError:
            pass

    def _build_messages(self) -> list[dict]:
        """Build the message list for the LLM call."""
        messages = [
            {"role": "system", "content": SYSTEM_PROMPT},
            {
                "role": "system",
                "content": (
                    "[GWA3 CONTRACT]\n"
                    f"ipc_protocol_version={IPC_PROTOCOL_VERSION}\n"
                    f"tool_schema_version={TOOL_SCHEMA_VERSION}"
                ),
            },
        ]

        # Inject the standing objective as the first user message (always present)
        messages.append({
            "role": "user",
            "content": f"[OBJECTIVE] {self.objective}",
        })

        # Add conversation history (user overrides, past actions)
        messages.extend(self.history)

        # Add current game state
        tier1_only = bool(self.token_budget and self.token_budget.tier1_only)
        state_summary = self.observations.build_context_summary(tier1_only=tier1_only)
        if tier1_only:
            state_summary += (
                "\n\n[TOKEN BUDGET]\n"
                "Cost spike guard is active; context is temporarily restricted "
                "to Tier1/core state. Prefer high-level tools and avoid generic "
                "query_state calls unless necessary for safety."
            )
        if self._is_froggy_objective():
            recommendation = self._recommend_froggy_action()
            next_action = recommendation.get("recommended_next_action", "query_state")
            reason = recommendation.get("reason", "no_reason")
            state_summary += (
                "\n\n[FROGGY HIGH-LEVEL RUNBOOK]\n"
                f"{FROGGY_HIGH_LEVEL_RUNBOOK}\n\n"
                "[FROGGY HARNESS RECOMMENDATION]\n"
                f"recommended_next_action={next_action}\n"
                f"reason={reason}\n"
                "Use this as advisory context. You must choose and call the next "
                "tool yourself unless the current visible game state clearly "
                "requires a different action."
            )
        events = self.observations.drain_events()
        event_text = ""
        if events:
            event_lines = [f"- {e.get('event', '?')}" for e in events]
            event_text = "\nNew events:\n" + "\n".join(event_lines)

        # Autonomous prompt — no question, just state delivery
        messages.append({
            "role": "user",
            "content": f"[GAME STATE — cycle {self._cycle_count}]\n{state_summary}{event_text}",
        })

        return messages

    def _is_froggy_objective(self) -> bool:
        text = (self.objective or "").lower()
        return any(token in text for token in ("froggy", "bogroot", "tekks"))

    def _tools_for_current_objective(self) -> list[dict]:
        latest = self.observations.latest
        if self._is_froggy_objective():
            return filter_tools_for_observation(FROGGY_AUTONOMOUS_TOOLS, latest)
        return tools_for_observation(latest)

    def _track_runtime_health(self) -> bool:
        """Stop cleanly if Froggy snapshots stay unloaded for too long."""
        snap = self.observations.latest or {}
        loading_state = (snap.get("map") or {}).get("loading_state")
        now = time.monotonic()
        if loading_state == 1:
            self._map_not_loaded_since = 0.0
            self._map_not_loaded_warned = False
            return True

        if self._map_not_loaded_since <= 0.0:
            self._map_not_loaded_since = now
            self._map_not_loaded_warned = False
            return True

        elapsed = now - self._map_not_loaded_since
        if elapsed >= FROGGY_MAP_NOT_LOADED_WARNING_SECONDS and not self._map_not_loaded_warned:
            print(f"[Agent] map_not_loaded for {elapsed:.0f}s; waiting for client recovery")
            self._map_not_loaded_warned = True
        if elapsed >= FROGGY_MAP_NOT_LOADED_STOP_SECONDS:
            print(f"[Agent] map_not_loaded for {elapsed:.0f}s; stopping bridge so the lane can be relaunched cleanly")
            self._running = False
            return False
        return True

    def _recommend_froggy_action(self) -> dict:
        snap = self.observations.latest or {}
        m = snap.get("map", {})
        map_id = int(m.get("map_id") or 0)
        loading_state = m.get("loading_state")
        inv = snap.get("inventory", {}) or {}
        free_slots = inv.get("free_slots_total")
        gold_character = inv.get("gold_character")
        gold_storage = inv.get("gold_storage")
        maintenance = inv.get("froggy_maintenance", {}) or {}
        conset_material_stacks = maintenance.get("conset_material_stacks_inventory")
        loose_consets = maintenance.get("loose_consets_inventory_total")
        grail_inventory = maintenance.get("grail_inventory")
        essence_inventory = maintenance.get("essence_inventory")
        armor_inventory = maintenance.get("armor_inventory")
        stored_consets = maintenance.get("stored_consets_total")
        stored_grail = maintenance.get("stored_grail")
        stored_essence = maintenance.get("stored_essence")
        stored_armor = maintenance.get("stored_armor")

        if loading_state != 1:
            return {
                "recommended_next_action": "wait",
                "reason": "map_not_loaded",
            }

        if map_id == 638:
            if self._last_successful_froggy_action == "froggy_run_town_setup":
                return {
                    "recommended_next_action": "froggy_travel_to_sparkfly",
                    "reason": "town_setup_complete_travel_to_entry_map",
                }
            if self._last_successful_froggy_action == "froggy_travel_to_sparkfly":
                elapsed = time.monotonic() - self._last_successful_froggy_action_time
                if elapsed < 60.0:
                    return {
                        "recommended_next_action": "wait",
                        "reason": "waiting_for_sparkfly_load_after_entry_travel",
                    }
                return {
                    "recommended_next_action": "froggy_travel_to_sparkfly",
                    "reason": "entry_travel_still_in_gadds_retry",
                }
            if not isinstance(free_slots, int):
                return {
                    "recommended_next_action": "query_state",
                    "reason": "inventory_unknown_in_gadds",
                }
            recent_maintenance = (
                self._last_froggy_full_maintenance_success_time > 0.0
                and (
                    time.monotonic() - self._last_froggy_full_maintenance_success_time
                    <= FROGGY_RECENT_MAINTENANCE_COOLDOWN_SECONDS
                )
            )
            recent_exhausted_maintenance = (
                self._consecutive_froggy_maintenance_failures >= 2
                and self._last_froggy_maintenance_failure_time > 0.0
                and (
                    time.monotonic() - self._last_froggy_maintenance_failure_time
                    <= FROGGY_RECENT_MAINTENANCE_COOLDOWN_SECONDS
                )
            )
            if (
                isinstance(grail_inventory, int)
                and isinstance(essence_inventory, int)
                and isinstance(armor_inventory, int)
                and min(grail_inventory, essence_inventory, armor_inventory) < 1
            ):
                return {
                    "recommended_next_action": "froggy_run_full_maintenance",
                    "reason": "character_conset_set_incomplete",
                }
            if (
                isinstance(stored_grail, int)
                and isinstance(stored_essence, int)
                and isinstance(stored_armor, int)
                and min(stored_grail, stored_essence, stored_armor) < 1
            ):
                return {
                    "recommended_next_action": "froggy_run_full_maintenance",
                    "reason": "stored_conset_set_incomplete",
                }
            if recent_maintenance and free_slots >= FROGGY_CRITICAL_FREE_SLOT_THRESHOLD:
                return {
                    "recommended_next_action": "froggy_run_town_setup",
                    "reason": "recent_maintenance_slots_acceptable",
                }
            if recent_exhausted_maintenance and free_slots >= FROGGY_CRITICAL_FREE_SLOT_THRESHOLD:
                return {
                    "recommended_next_action": "froggy_run_town_setup",
                    "reason": "recent_maintenance_exhausted_slots_acceptable",
                }
            if (
                isinstance(conset_material_stacks, int)
                and conset_material_stacks > FROGGY_CONSET_MATERIAL_STACK_TRIGGER
                and free_slots <= FROGGY_CONSET_MATERIAL_PRESSURE_FREE_SLOT_THRESHOLD
            ):
                return {
                    "recommended_next_action": "froggy_run_full_maintenance",
                    "reason": "conset_material_pressure_in_gadds",
                }
            if (
                isinstance(loose_consets, int)
                and loose_consets > 0
                and free_slots <= FROGGY_CONSET_MATERIAL_PRESSURE_FREE_SLOT_THRESHOLD
            ):
                return {
                    "recommended_next_action": "froggy_run_full_maintenance",
                    "reason": "loose_consets_need_storage",
                }
            if free_slots <= FROGGY_MAINTENANCE_FREE_SLOT_THRESHOLD:
                return {
                    "recommended_next_action": "froggy_run_full_maintenance",
                    "reason": "low_inventory_space_in_gadds",
                }
            if isinstance(gold_character, int) and gold_character >= FROGGY_CHARACTER_GOLD_PRESSURE_THRESHOLD:
                return {
                    "recommended_next_action": "froggy_run_full_maintenance",
                    "reason": "character_gold_near_cap",
                }
            if (
                isinstance(stored_consets, int)
                and stored_consets < FROGGY_STORED_CONSET_LOW_THRESHOLD
                and isinstance(gold_storage, int)
                and gold_storage >= FROGGY_STORAGE_GOLD_CONSET_THRESHOLD
            ):
                return {
                    "recommended_next_action": "froggy_run_full_maintenance",
                    "reason": "stored_consets_low_with_gold_available",
                }
            if isinstance(gold_storage, int) and gold_storage >= FROGGY_STORAGE_GOLD_CONSET_THRESHOLD:
                return {
                    "recommended_next_action": "froggy_run_full_maintenance",
                    "reason": "storage_gold_above_conset_conversion_threshold",
                }
            return {
                "recommended_next_action": "froggy_run_town_setup",
                "reason": "ready_for_outpost_setup",
            }
        if map_id == 857:
            return {
                "recommended_next_action": "froggy_travel_to_gadds",
                "reason": "recover_from_embark_to_gadds",
            }
        if map_id == 558:
            if self._last_successful_froggy_action == "froggy_run_sparkfly_route_to_tekks":
                return {
                    "recommended_next_action": "froggy_prepare_tekks_dungeon_entry",
                    "reason": "sparkfly_route_complete_prepare_tekks_entry",
                }
            if self._last_successful_froggy_action == "froggy_prepare_tekks_dungeon_entry":
                return {
                    "recommended_next_action": "wait",
                    "reason": "tekks_entry_prepared_wait_for_bogroot_load",
                }
            return {
                "recommended_next_action": "froggy_run_sparkfly_route_to_tekks",
                "reason": "sparkfly_entry_route_to_tekks",
            }
        if map_id in (615, 616):
            return {
                "recommended_next_action": "froggy_run_dungeon_loop",
                "reason": "inside_bogroot",
            }
        return {
            "recommended_next_action": "query_state",
            "reason": f"unexpected_map_{map_id}",
        }

    def _enrich_tool_result(self, tool_name: str, result: dict) -> dict:
        enriched = {"action": tool_name, **result}
        if (
            self._is_froggy_objective()
            and result.get("success")
            and tool_name in FROGGY_AUTONOMOUS_TOOL_NAMES
            and tool_name != "query_state"
        ):
            self._last_successful_froggy_action = tool_name
            self._last_successful_froggy_action_time = time.monotonic()
            self._failed_froggy_actions.pop(tool_name, None)
        elif (
            self._is_froggy_objective()
            and not result.get("success")
            and tool_name in FROGGY_AUTONOMOUS_TOOL_NAMES
            and tool_name not in {"query_state", "wait"}
        ):
            self._record_failed_froggy_action(tool_name)
        if self._is_froggy_objective() and tool_name == "froggy_run_full_maintenance":
            if result.get("success"):
                self._consecutive_froggy_maintenance_failures = 0
            else:
                self._consecutive_froggy_maintenance_failures += 1
                self._last_froggy_maintenance_failure_time = time.monotonic()
        if self._is_froggy_objective():
            snap = self.observations.latest or {}
            m = snap.get("map", {}) or {}
            inv = snap.get("inventory", {}) or {}
            party = snap.get("party", {}) or {}
            map_id = 0
            free_slots = None
            if m:
                map_id = int(m.get("map_id") or 0)
                enriched["current_map_id"] = map_id
                enriched["current_map_name"] = farming_knowledge.MAP_NAMES.get(map_id, "unknown")
                enriched["loading_state"] = m.get("loading_state")
            if inv:
                enriched["free_slots_total"] = inv.get("free_slots_total")
                enriched["gold_character"] = inv.get("gold_character")
                enriched["gold_storage"] = inv.get("gold_storage")
                enriched["froggy_maintenance"] = inv.get("froggy_maintenance")
            if party:
                enriched["party_defeated"] = party.get("is_defeated")
                enriched["dead_count"] = party.get("dead_count")
            enriched.update(self._recommend_froggy_action())
            if tool_name == "froggy_run_dungeon_loop" and result.get("success"):
                if inv:
                    free_slots = inv.get("free_slots_total")
                maintenance_reason = None
                if map_id == 638 and isinstance(free_slots, int) and free_slots <= FROGGY_MAINTENANCE_FREE_SLOT_THRESHOLD:
                    maintenance_reason = "completed_run_low_inventory_space"
                elif map_id == 638 and not isinstance(free_slots, int):
                    maintenance_reason = "completed_run_inventory_unknown"

                enriched.update({
                    "completed_dungeon_run": True,
                    "quest_reward_claimed": True,
                    "return_to_outpost_was_expected": True,
                    "interpretation": (
                        "Froggy completed a full Bogroot run and handled the "
                        "post-reward return; this is not a wipe."
                    ),
                })
                if maintenance_reason:
                    enriched["recommended_next_action"] = "froggy_run_full_maintenance"
                    enriched["reason"] = maintenance_reason
                else:
                    enriched["recommended_next_action"] = "query_state"
                    enriched["reason"] = "completed_run_refresh_state_before_next_run"
            elif tool_name == "froggy_run_full_maintenance" and result.get("success"):
                self._last_froggy_full_maintenance_success_time = time.monotonic()
                enriched.update({
                    "completed_full_maintenance": True,
                    "recommended_next_action": "query_state",
                    "reason": "maintenance_complete_refresh_state_before_setup",
                    "interpretation": (
                        "Froggy completed town maintenance and can resume "
                        "setup/travel unless inventory pressure remains."
                    ),
                })
            elif tool_name == "froggy_run_town_setup" and result.get("success"):
                enriched.update({
                    "completed_town_setup": True,
                    "recommended_next_action": "froggy_travel_to_sparkfly",
                    "reason": "town_setup_complete_travel_to_entry_map",
                    "interpretation": (
                        "Froggy completed outpost setup; the next step is "
                        "travel to Sparkfly Swamp."
                    ),
                })
            elif tool_name == "froggy_travel_to_sparkfly" and result.get("success"):
                enriched.update({
                    "completed_entry_travel": True,
                    "recommended_next_action": "wait",
                    "reason": "entry_travel_complete_wait_for_loaded_snapshot",
                    "interpretation": (
                        "Froggy traveled to Sparkfly Swamp; wait briefly or "
                        "refresh state before starting the dungeon loop."
                    ),
                })
            elif tool_name == "froggy_run_sparkfly_route_to_tekks" and result.get("success"):
                enriched.update({
                    "completed_sparkfly_route_to_tekks": True,
                    "recommended_next_action": "froggy_prepare_tekks_dungeon_entry",
                    "reason": "sparkfly_route_complete_prepare_tekks_entry",
                    "interpretation": (
                        "Froggy reached the Tekks side of Sparkfly; the next "
                        "LLM-owned step is preparing Tekks and entering Bogroot."
                    ),
                })
            elif tool_name == "froggy_prepare_tekks_dungeon_entry" and result.get("success"):
                enriched.update({
                    "completed_tekks_entry_prepare": True,
                    "recommended_next_action": "query_state",
                    "reason": "tekks_entry_prepared_refresh_map_state",
                    "interpretation": (
                        "Froggy prepared Tekks dungeon entry. Refresh state; "
                        "if the map is Bogroot, run froggy_run_dungeon_loop."
                    ),
                })
            elif tool_name == "froggy_travel_to_gadds" and result.get("success"):
                enriched.update({
                    "completed_recovery_travel": True,
                    "recommended_next_action": "query_state",
                    "reason": "returned_to_gadds_refresh_state",
                    "interpretation": (
                        "Froggy recovered to Gadd's Encampment; refresh state "
                        "before maintenance or setup."
                    ),
                })
        return enriched

    def _record_failed_froggy_action(self, tool_name: str):
        now = time.monotonic()
        failures = [
            t for t in self._failed_froggy_actions.get(tool_name, [])
            if now - t <= FROGGY_FAILED_ACTION_WINDOW_SECONDS
        ]
        failures.append(now)
        self._failed_froggy_actions[tool_name] = failures

    def _froggy_action_blocked_by_failure_budget(self, tool_name: str) -> dict | None:
        if tool_name in {"query_state", "wait"}:
            return None

        now = time.monotonic()
        failures = [
            t for t in self._failed_froggy_actions.get(tool_name, [])
            if now - t <= FROGGY_FAILED_ACTION_WINDOW_SECONDS
        ]
        self._failed_froggy_actions[tool_name] = failures
        if len(failures) < FROGGY_FAILED_ACTION_RETRY_LIMIT:
            return None

        return {
            "success": False,
            "error": "froggy_action_retry_budget_exhausted",
            "blocked_tool": tool_name,
            "recent_failures": len(failures),
            "failure_window_seconds": FROGGY_FAILED_ACTION_WINDOW_SECONDS,
            "recommended_next_action": "query_state",
            "reason": f"recent_{tool_name}_failures",
            "instruction": (
                "Do not retry this same high-level action immediately. Query "
                "state, wait for recovery, or choose a different recovery path."
            ),
        }

    async def _wait_for_action_result(self, request_id: str, timeout: float) -> dict:
        """Wait for a specific action_result while keeping snapshots/events fresh."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            remaining = max(0.05, min(1.0, deadline - time.monotonic()))
            try:
                msg = await asyncio.wait_for(self.ipc.read_message(), timeout=remaining)
            except asyncio.TimeoutError:
                continue
            if msg is None:
                return {"success": False, "error": "pipe_disconnected"}

            msg_type = msg.get("type", "")
            if msg_type == "snapshot":
                self.observations.add_snapshot(msg)
            elif msg_type == "event":
                self.observations.add_event(msg)
            elif msg_type == "action_result":
                if msg.get("request_id") == request_id:
                    return {
                        k: v for k, v in msg.items()
                        if k not in {"type", "request_id"}
                    }
            elif msg_type == "heartbeat":
                pass

        return {"success": False, "error": "action_timeout"}

    async def _execute_froggy_recommended_action(self, trigger: str) -> bool:
        """Execute the current high-level Froggy recommendation as a safety net."""
        if not self._is_froggy_objective():
            return False

        recommendation = self._recommend_froggy_action()
        action = recommendation.get("recommended_next_action")
        if not action or action not in FROGGY_AUTONOMOUS_TOOL_NAMES:
            return False

        result: dict
        if action == "wait":
            await asyncio.sleep(0.5)
            result = {"success": True}
        elif action in {
            "get_recipe",
            "get_outpost_info",
            "get_material_info",
            "get_dungeon_info",
            "get_blessing_info",
            "get_hero_build",
            "get_quest_info",
        }:
            return False
        else:
            req_id = str(uuid.uuid4())[:8]
            await self.ipc.send_action(action, {}, req_id)
            self._last_action_time = time.monotonic()
            result = await self._wait_for_action_result(
                req_id,
                self._tool_timeout_seconds(action),
            )

        should_log = True
        if action == "wait" and recommendation.get("reason") == "map_not_loaded":
            now = time.monotonic()
            should_log = now - self._last_wait_log_time >= 15.0
            if should_log:
                self._last_wait_log_time = now
        if should_log:
            enriched = self._enrich_tool_result(action, result)
            enriched.update({
                "harness_fallback_executed": True,
                "fallback_trigger": trigger,
                "fallback_action": action,
                "fallback_reason": recommendation.get("reason"),
            })
            self.history.append({
                "role": "user",
                "content": (
                    "[FROGGY HARNESS FALLBACK EXECUTED]\n"
                    + json.dumps(enriched)
                ),
            })
        if should_log:
            print(f"[Harness -> GW] {action} ({recommendation.get('reason')})")
        return bool(result.get("success", False))

    async def _maybe_execute_froggy_recommended_action(self, trigger: str) -> bool:
        """Execute harness fallback only when explicitly opted in.

        Default Froggy LLM mode keeps harness recommendations advisory so the
        model owns phase selection. This method still records the rejected/idle
        state as a prompt nudge, but it does not move the character unless
        GWA3_FROGGY_HARNESS_FALLBACK is enabled.
        """
        if FROGGY_HARNESS_FALLBACK_ENABLED:
            return await self._execute_froggy_recommended_action(trigger)

        if not self._is_froggy_objective():
            return False

        recommendation = self._recommend_froggy_action()
        action = recommendation.get("recommended_next_action")
        reason = recommendation.get("reason")
        self.history.append({
            "role": "user",
            "content": (
                "[FROGGY HARNESS FALLBACK DEFERRED]\n"
                + json.dumps({
                    "harness_fallback_deferred": True,
                    "fallback_trigger": trigger,
                    "fallback_action": action,
                    "fallback_reason": reason,
                    "instruction": (
                        "Harness fallback is disabled; choose the next tool "
                        "yourself from current state and the advisory "
                        "recommendation."
                    ),
                })
            ),
        })
        print(f"[Harness] deferred {action} ({reason}); awaiting LLM decision")
        return False

    async def _execute_froggy_autopilot_action(self) -> bool:
        """Advance validated Froggy phase actions without spending an LLM request."""
        if not FROGGY_AUTOPILOT_ENABLED:
            return False
        if not self._is_froggy_objective() or self._froggy_force_llm_once:
            return False

        recommendation = self._recommend_froggy_action()
        action = recommendation.get("recommended_next_action")
        if action not in FROGGY_AUTOPILOT_DIRECT_ACTIONS:
            return False

        return await self._execute_froggy_recommended_action("froggy_autopilot")

    @staticmethod
    def _is_llm_rate_limit_error(error: Exception) -> bool:
        if (
            isinstance(error, httpx.HTTPStatusError)
            and error.response is not None
            and error.response.status_code == 429
        ):
            return True

        message = str(error).lower()
        return (
            "usage limit" in message
            or "rate limit" in message
            or "try again at" in message
        )

    @staticmethod
    def _codex_retry_after_seconds(error: Exception) -> float | None:
        """Parse Codex CLI usage-limit reset text when it is available."""
        match = re.search(
            r"try again at\s+(\d{1,2}):(\d{2})\s*([ap]\.?m\.?)",
            str(error),
            flags=re.IGNORECASE,
        )
        if not match:
            return None

        hour = int(match.group(1))
        minute = int(match.group(2))
        ampm = match.group(3).lower().replace(".", "")
        if ampm == "pm" and hour != 12:
            hour += 12
        elif ampm == "am" and hour == 12:
            hour = 0

        now = _dt.datetime.now()
        retry_at = now.replace(hour=hour, minute=minute, second=0, microsecond=0)
        if retry_at <= now:
            retry_at += _dt.timedelta(days=1)
        return max(30.0, (retry_at - now).total_seconds() + 15.0)

    async def _handle_llm_rate_limit(self, error: Exception) -> bool:
        if not self._is_llm_rate_limit_error(error):
            return False

        retry_after = None
        if isinstance(error, httpx.HTTPStatusError):
            raw_retry_after = error.response.headers.get("retry-after")
            if raw_retry_after:
                try:
                    retry_after = float(raw_retry_after)
                except ValueError:
                    retry_after = None
        else:
            retry_after = self._codex_retry_after_seconds(error)

        backoff = self._llm_rate_limit_backoff_seconds
        if retry_after is not None:
            backoff = max(backoff, retry_after)
        backoff = min(backoff, 21600.0)

        fallback_note = " with harness fallback" if FROGGY_HARNESS_FALLBACK_ENABLED else ""
        reset_at = _dt.datetime.now() + _dt.timedelta(seconds=backoff)
        print(
            "[Agent] LLM rate limited; backing off "
            f"{backoff:.0f}s until ~{reset_at.strftime('%I:%M %p').lstrip('0')}"
            f"{fallback_note}"
        )
        if self._is_froggy_objective():
            await self._collect_observations_safe()
            executed = await self._maybe_execute_froggy_recommended_action("llm_rate_limited")
            if executed:
                self._consecutive_no_action = 0
                self._last_rate_limit_fallback_time = time.monotonic()
                self._froggy_force_llm_once = False

        self._llm_rate_limit_until = time.monotonic() + backoff
        self._llm_rate_limit_backoff_seconds = min(backoff * 2.0, 300.0)
        self.history.append({
            "role": "user",
            "content": (
                "[LLM RATE LIMIT]\n"
                + json.dumps({
                    "provider_backoff_seconds": int(backoff),
                    "retry_after_local_time": reset_at.strftime("%Y-%m-%d %I:%M:%S %p"),
                    "harness_fallback_enabled": FROGGY_HARNESS_FALLBACK_ENABLED,
                    "error": str(error)[-500:],
                })
            ),
        })
        return True

    @staticmethod
    def _tool_timeout_seconds(tool_name: str) -> float:
        """Long-running route helpers need much longer than ordinary actions."""
        if tool_name == "froggy_run_dungeon_loop":
            return 7200.0
        if tool_name == "froggy_run_full_maintenance":
            return 1200.0
        if tool_name in {
            "froggy_run_town_setup",
            "froggy_travel_to_gadds",
            "froggy_travel_to_sparkfly",
            "froggy_run_sparkfly_route_to_tekks",
            "froggy_prepare_tekks_dungeon_entry",
            "froggy_run_maintenance_cycle",
            "aggro_move_to",
        }:
            return 600.0
        return 30.0

    async def _execute_tool_calls(self, response: LLMResponse):
        """Send tool calls to gwa3 and collect results."""
        for tc in response.tool_calls:
            req_id = str(uuid.uuid4())[:8]
            try:
                params = tc.parsed_arguments
            except (json.JSONDecodeError, TypeError, ValueError) as e:
                result = {
                    "success": False,
                    "error": "invalid_tool_arguments_json",
                    "tool": tc.name,
                    "detail": str(e),
                }
                if self._is_froggy_objective():
                    result.update(self._recommend_froggy_action())
                self.history.append({
                    "role": "tool",
                    "tool_call_id": tc.id,
                    "content": json.dumps(result),
                })
                if self._is_froggy_objective():
                    await self._maybe_execute_froggy_recommended_action("invalid_tool_arguments_json")
                continue

            if self._is_froggy_objective() and tc.name not in FROGGY_AUTONOMOUS_TOOL_NAMES:
                result = {
                    "success": False,
                    "error": "tool_blocked_in_froggy_autonomous_mode",
                    "blocked_tool": tc.name,
                    **self._recommend_froggy_action(),
                }
                self.history.append({
                    "role": "tool",
                    "tool_call_id": tc.id,
                    "content": json.dumps(result),
                })
                await self._maybe_execute_froggy_recommended_action("blocked_tool")
                continue

            if self._is_froggy_objective() and tc.name in FROGGY_AUTONOMOUS_TOOL_NAMES:
                blocked = self._froggy_action_blocked_by_failure_budget(tc.name)
                if blocked:
                    self.history.append({
                        "role": "tool",
                        "tool_call_id": tc.id,
                        "content": json.dumps(blocked),
                    })
                    continue

            if (
                self._is_froggy_objective()
                and tc.name in FROGGY_AUTOPILOT_DIRECT_ACTIONS
                and tc.name != "wait"
            ):
                recommendation = self._recommend_froggy_action()
                recommended_action = recommendation.get("recommended_next_action")
                if (
                    recommended_action
                    and (
                        recommended_action in FROGGY_AUTOPILOT_DIRECT_ACTIONS
                        or recommended_action == "query_state"
                    )
                    and recommended_action != tc.name
                ):
                    result = {
                        "success": False,
                        "error": "froggy_tool_contradicts_current_state",
                        "blocked_tool": tc.name,
                        **recommendation,
                    }
                    self.history.append({
                        "role": "tool",
                        "tool_call_id": tc.id,
                        "content": json.dumps(result),
                    })
                    if (
                        FROGGY_HARNESS_FALLBACK_ENABLED
                        and recommended_action == "query_state"
                    ):
                        await self.ipc.send_action("query_state", {}, req_id)
                        fallback_result = await self._wait_for_action_result(
                            req_id,
                            self._tool_timeout_seconds("query_state"),
                        )
                        self.history.append({
                            "role": "user",
                            "content": (
                                "[FROGGY HARNESS FALLBACK EXECUTED]\n"
                                + json.dumps({
                                    "harness_fallback_executed": True,
                                    "fallback_trigger": "contradictory_froggy_tool",
                                    "fallback_action": "query_state",
                                    "fallback_reason": recommendation.get("reason"),
                                    **self._enrich_tool_result("query_state", fallback_result),
                                })
                            ),
                        })
                        print(f"[Harness -> GW] query_state ({recommendation.get('reason')})")
                    else:
                        await self._maybe_execute_froggy_recommended_action(
                            "contradictory_froggy_tool"
                        )
                    continue

            # Handle wait locally
            if tc.name == "wait":
                ms = params.get("milliseconds", 500)
                await asyncio.sleep(ms / 1000.0)
                self.history.append({
                    "role": "tool",
                    "tool_call_id": tc.id,
                    "content": json.dumps(self._enrich_tool_result(tc.name, {"success": True})),
                })
                continue

            # Handle price lookup locally (HTTP, not game pipe)
            if tc.name == "search_trade_prices":
                query = params.get("query", "")
                count = min(params.get("count", 10), 25)
                try:
                    data = await self.kamadan.search_for_llm(query, count=count)
                    content = json.dumps(data)
                except (asyncio.TimeoutError, httpx.HTTPError, OSError, ValueError) as e:
                    content = json.dumps({"error": str(e)})
                self.history.append({
                    "role": "tool",
                    "tool_call_id": tc.id,
                    "content": content,
                })
                continue

            # Static farming knowledge lookups — no game-thread round trip
            if tc.name == "get_recipe":
                data = farming_knowledge.get_recipe(
                    params.get("consumable_model_id", 0))
                self.history.append({
                    "role": "tool", "tool_call_id": tc.id,
                    "content": json.dumps(data),
                })
                continue
            if tc.name == "get_outpost_info":
                data = farming_knowledge.get_outpost_info(params.get("map_id", 0))
                self.history.append({
                    "role": "tool", "tool_call_id": tc.id,
                    "content": json.dumps(data),
                })
                continue
            if tc.name == "get_material_info":
                data = farming_knowledge.get_material_info(params.get("model_id", 0))
                self.history.append({
                    "role": "tool", "tool_call_id": tc.id,
                    "content": json.dumps(data),
                })
                continue
            if tc.name == "get_dungeon_info":
                data = farming_knowledge.get_dungeon_info(params.get("name", ""))
                self.history.append({
                    "role": "tool", "tool_call_id": tc.id,
                    "content": json.dumps(data),
                })
                continue
            if tc.name == "get_blessing_info":
                data = farming_knowledge.get_blessing_info(
                    params.get("blessing_type", ""))
                self.history.append({
                    "role": "tool", "tool_call_id": tc.id,
                    "content": json.dumps(data),
                })
                continue
            if tc.name == "get_hero_build":
                data = farming_knowledge.get_hero_build(
                    params.get("hero_name", ""))
                self.history.append({
                    "role": "tool", "tool_call_id": tc.id,
                    "content": json.dumps(data),
                })
                continue
            if tc.name == "get_quest_info":
                data = farming_knowledge.get_quest_info(params.get("key", ""))
                self.history.append({
                    "role": "tool", "tool_call_id": tc.id,
                    "content": json.dumps(data),
                })
                continue

            await self.ipc.send_action(tc.name, params, req_id)
            self._last_action_time = time.monotonic()

            result = await self._wait_for_action_result(
                req_id,
                self._tool_timeout_seconds(tc.name),
            )

            self.history.append({
                "role": "tool",
                "tool_call_id": tc.id,
                "content": json.dumps(self._enrich_tool_result(tc.name, result)),
            })

    def _trim_history(self):
        """Keep history within bounds, preserving user override messages."""
        if len(self.history) > self.max_history:
            # Keep recent entries but preserve any user messages (objective changes)
            keep = []
            old = self.history[:-self.max_history]
            for msg in old:
                if msg.get("role") == "user" and not msg.get("content", "").startswith("[GAME STATE"):
                    keep.append(msg)
            self.history = keep + self.history[-self.max_history:]

    async def run(self):
        """Main autonomous agent loop."""
        self._running = True
        print(f"[Agent] Starting autonomous agent loop")
        print(f"[Agent] Objective: {self.objective}")

        while self._running:
            try:
                self._cycle_count += 1

                # 1. Collect latest observations
                await self._collect_observations_safe()

                # 2. Check for user messages (optional, non-blocking)
                while not self._user_message_queue.empty():
                    user_msg = self._user_message_queue.get_nowait()
                    self.history.append({"role": "user", "content": user_msg})
                    self._froggy_force_llm_once = True
                    print(f"[User] {user_msg}")

                # 3. Wait for game state before acting
                if self.observations.latest is None:
                    await asyncio.sleep(0.5)
                    continue

                if self._is_froggy_objective() and not self._track_runtime_health():
                    break

                if await self._execute_froggy_autopilot_action():
                    self._consecutive_no_action = 0
                    self._trim_history()
                    continue

                if self._llm_rate_limit_until > time.monotonic():
                    if (
                        self._is_froggy_objective()
                        and time.monotonic() - self._last_rate_limit_fallback_time >= 15.0
                        and FROGGY_HARNESS_FALLBACK_ENABLED
                    ):
                        await self._collect_observations_safe()
                        if await self._maybe_execute_froggy_recommended_action("llm_rate_limit_backoff"):
                            self._consecutive_no_action = 0
                            self._last_rate_limit_fallback_time = time.monotonic()
                    await asyncio.sleep(min(2.0, self._llm_rate_limit_until - time.monotonic()))
                    continue

                # 4. Build prompt and call LLM
                if self.token_budget:
                    self.token_budget.check_before_request()
                messages = self._build_messages()
                response = await self.llm.chat_completion(
                    messages=messages,
                    tools=self._tools_for_current_objective(),
                    tool_choice="auto",
                )
                if self.token_budget:
                    used = self.token_budget.record_usage(response.usage)
                    if used:
                        print(
                            "[Budget] "
                            f"LLM used {used:,} tokens; "
                            f"{self.token_budget.remaining_tokens_last_hour:,} remain this hour."
                        )
                self._froggy_force_llm_once = False
                self._llm_rate_limit_until = 0.0
                self._llm_rate_limit_backoff_seconds = 15.0
                self._last_rate_limit_fallback_time = 0.0

                # 5. Handle text response
                if response.content:
                    print(f"[LLM] {response.content}")
                    self.history.append({
                        "role": "assistant",
                        "content": response.content,
                    })

                # 6. Handle tool calls
                if response.tool_calls:
                    tc_names = [tc.name for tc in response.tool_calls]
                    print(f"[LLM -> GW] {', '.join(tc_names)}")
                    self._consecutive_no_action = 0

                    self.history.append({
                        "role": "assistant",
                        "content": response.content,
                        "tool_calls": [
                            {
                                "id": tc.id,
                                "type": "function",
                                "function": {
                                    "name": tc.name,
                                    "arguments": tc.arguments,
                                },
                            }
                            for tc in response.tool_calls
                        ],
                    })

                    await self._execute_tool_calls(response)
                else:
                    # No tool calls - the LLM chose not to act this cycle
                    self._consecutive_no_action += 1
                    idle_limit = 2 if self._is_froggy_objective() else 10
                    if self._consecutive_no_action >= idle_limit:
                        # The LLM has been idle too long, nudge it
                        self._consecutive_no_action = 0
                        recommendation = self._recommend_froggy_action() if self._is_froggy_objective() else {}
                        recommended_action = recommendation.get("recommended_next_action")
                        reason = recommendation.get("reason")
                        recommendation_text = (
                            f" Recommended Froggy action: {recommended_action} "
                            f"(reason: {reason})."
                            if recommended_action
                            else ""
                        )
                        self.history.append({
                            "role": "user",
                            "content": (
                                f"[SYSTEM] You have been idle for {idle_limit} cycles."
                                f"{recommendation_text} Take action toward your "
                                "objective or explain what you're waiting for."
                            ),
                        })
                        if self._is_froggy_objective() and recommended_action:
                            await self._maybe_execute_froggy_recommended_action("idle_cycles")

                # 7. Trim history
                self._trim_history()

                # 8. Pace the loop — don't hammer the LLM
                await asyncio.sleep(0.3)

            except asyncio.CancelledError:
                break
            except TokenBudgetExceeded as e:
                print(f"[Agent] {e}")
                break
            except Exception as e:
                if await self._handle_llm_rate_limit(e):
                    continue
                print(f"[Agent] Error in loop: {e}")
                await asyncio.sleep(2.0)

        self._running = False
        print("[Agent] Agent loop stopped.")

    def stop(self):
        self._running = False
