"""Core agent loop: observe game state, think via LLM, act via gwa3.

Designed for long-running autonomous play. Gemma receives a standing objective
at startup and pursues it indefinitely. The user can optionally send messages
to adjust behavior, but no input is required.
"""

import asyncio
import httpx
import json
import uuid
import time

from .ipc_client import IpcClient
from .kamadan_client import KamadanClient
from .llm_client import LLMClient, LLMResponse
from .tool_schema import ALL_TOOLS
from .observation import ObservationWindow
from . import farming_knowledge


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
    ):
        self.ipc = ipc
        self.llm = llm
        self.autonomy = autonomy
        self.objective = objective or DEFAULT_OBJECTIVE
        self.observations = ObservationWindow()
        self.kamadan = kamadan_client or KamadanClient()
        self.history: list[dict] = []
        self.max_history = 40
        self._running = False
        self._user_message_queue: asyncio.Queue[str] = asyncio.Queue()
        self._cycle_count = 0
        self._last_action_time = 0.0
        self._consecutive_no_action = 0

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
                pass  # logged but not blocking
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
        messages = [{"role": "system", "content": SYSTEM_PROMPT}]

        # Inject the standing objective as the first user message (always present)
        messages.append({
            "role": "user",
            "content": f"[OBJECTIVE] {self.objective}",
        })

        # Add conversation history (user overrides, past actions)
        messages.extend(self.history)

        # Add current game state
        state_summary = self.observations.build_context_summary()
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

    async def _execute_tool_calls(self, response: LLMResponse):
        """Send tool calls to gwa3 and collect results."""
        for tc in response.tool_calls:
            req_id = str(uuid.uuid4())[:8]
            params = tc.parsed_arguments

            # Handle wait locally
            if tc.name == "wait":
                ms = params.get("milliseconds", 500)
                await asyncio.sleep(ms / 1000.0)
                self.history.append({
                    "role": "tool",
                    "tool_call_id": tc.id,
                    "content": json.dumps({"success": True}),
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

            # Brief pause between sequential actions
            await asyncio.sleep(0.1)
            await self._collect_observations_safe()

            self.history.append({
                "role": "tool",
                "tool_call_id": tc.id,
                "content": json.dumps({"success": True, "action": tc.name}),
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
                    print(f"[User] {user_msg}")

                # 3. Wait for game state before acting
                if self.observations.latest is None:
                    await asyncio.sleep(0.5)
                    continue

                # 4. Build prompt and call LLM
                messages = self._build_messages()
                response = await self.llm.chat_completion(
                    messages=messages,
                    tools=ALL_TOOLS,
                    tool_choice="auto",
                )

                # 5. Handle text response (Gemma explains something important)
                if response.content:
                    print(f"[Gemma] {response.content}")
                    self.history.append({
                        "role": "assistant",
                        "content": response.content,
                    })

                # 6. Handle tool calls
                if response.tool_calls:
                    tc_names = [tc.name for tc in response.tool_calls]
                    print(f"[Gemma -> GW] {', '.join(tc_names)}")
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
                    # No tool calls — Gemma chose not to act this cycle
                    self._consecutive_no_action += 1
                    if self._consecutive_no_action >= 10:
                        # Gemma has been idle too long, nudge it
                        self._consecutive_no_action = 0
                        self.history.append({
                            "role": "user",
                            "content": "[SYSTEM] You have been idle for 10 cycles. "
                                       "Take action toward your objective or explain what you're waiting for.",
                        })

                # 7. Trim history
                self._trim_history()

                # 8. Pace the loop — don't hammer the LLM
                await asyncio.sleep(0.3)

            except asyncio.CancelledError:
                break
            except Exception as e:
                print(f"[Agent] Error in loop: {e}")
                await asyncio.sleep(2.0)

        print("[Agent] Agent loop stopped.")

    def stop(self):
        self._running = False
