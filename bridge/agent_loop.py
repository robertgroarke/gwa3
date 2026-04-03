"""Core agent loop: observe game state, think via LLM, act via gwa3."""

import asyncio
import json
import uuid

from .ipc_client import IpcClient
from .llm_client import LLMClient, LLMResponse
from .tool_schema import ALL_TOOLS
from .observation import ObservationWindow


SYSTEM_PROMPT = """\
You are a Guild Wars bot controller. You observe real-time game state snapshots and issue \
actions through function calls to control a character in Guild Wars.

You receive periodic game state updates showing your position, health, energy, skillbar, \
nearby agents, inventory, and map information. Based on this state, you decide what actions \
to take using the available tools.

## Game Knowledge
- Allegiance: 1=ally, 3=foe/enemy, 6=spirit, 0=neutral/NPC
- Agent type: 0xDB=living creature, 0x200=signpost/gadget, 0x400=item on ground
- HP and Energy are 0.0-1.0 fractions (e.g., 0.85 = 85%)
- Skill slots are 0-7 (8 skills on the skillbar)
- Recharge > 0 means the skill is still cooling down
- Hero behavior: 0=fight, 1=guard, 2=avoid combat
- Hero index is 1-7 (heroes in party order)

## Rules
- Never drop gold or purple rarity items
- Always pick up gold-rarity items when safe to do so
- If the party is defeated, return to outpost
- If HP < 20%, consider retreating or using defensive skills
- Don't spam actions — wait for previous actions to complete
- When moving, verify you've arrived before issuing new movement commands

## Communication
When the user sends you a message, respond naturally and adjust your behavior accordingly. \
You can explain what you're doing, report status, or ask for clarification.
"""


class AgentLoop:
    """The observe-think-act agent loop."""

    def __init__(
        self,
        ipc: IpcClient,
        llm: LLMClient,
        autonomy: str = "tactical",
    ):
        self.ipc = ipc
        self.llm = llm
        self.autonomy = autonomy
        self.observations = ObservationWindow()
        self.history: list[dict] = []
        self.max_history = 50
        self._running = False
        self._user_message_queue: asyncio.Queue[str] = asyncio.Queue()
        self._action_results: dict[str, asyncio.Future] = {}

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
                req_id = msg.get("request_id", "")
                if req_id in self._action_results:
                    self._action_results[req_id].set_result(msg)
            elif msg_type == "heartbeat":
                pass  # alive

    async def _collect_observations_safe(self):
        """Collect observations, swallowing timeouts."""
        try:
            await self._collect_observations()
        except (asyncio.TimeoutError, Exception):
            pass

    def _build_messages(self) -> list[dict]:
        """Build the message list for the LLM call."""
        messages = [{"role": "system", "content": SYSTEM_PROMPT}]

        # Add conversation history
        messages.extend(self.history)

        # Add current game state as the latest user message
        state_summary = self.observations.build_context_summary()
        events = self.observations.drain_events()
        event_text = ""
        if events:
            event_lines = [f"- {e.get('event', '?')}" for e in events]
            event_text = "\nNew events:\n" + "\n".join(event_lines)

        messages.append({
            "role": "user",
            "content": f"[GAME STATE]\n{state_summary}{event_text}\n\nWhat actions should be taken?",
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

            await self.ipc.send_action(tc.name, params, req_id)

            # Wait briefly for result
            await asyncio.sleep(0.1)
            await self._collect_observations_safe()

            # Record tool result in history
            self.history.append({
                "role": "tool",
                "tool_call_id": tc.id,
                "content": json.dumps({"success": True, "action": tc.name}),
            })

    def _trim_history(self):
        """Keep history within bounds."""
        if len(self.history) > self.max_history:
            # Keep the most recent messages
            self.history = self.history[-self.max_history:]

    async def run(self):
        """Main agent loop."""
        self._running = True
        print("[Agent] Starting agent loop...")

        while self._running:
            try:
                # 1. Collect latest observations
                await self._collect_observations_safe()

                # 2. Check for user messages
                while not self._user_message_queue.empty():
                    user_msg = self._user_message_queue.get_nowait()
                    self.history.append({"role": "user", "content": user_msg})
                    print(f"[User -> Agent] {user_msg}")

                # 3. Only call LLM if we have game state
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

                # 5. Handle text response
                if response.content:
                    print(f"[Gemma] {response.content}")
                    self.history.append({
                        "role": "assistant",
                        "content": response.content,
                    })

                # 6. Handle tool calls
                if response.tool_calls:
                    # Log tool calls
                    tc_names = [tc.name for tc in response.tool_calls]
                    print(f"[Gemma -> GW] {', '.join(tc_names)}")

                    # Record assistant message with tool calls
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

                # 7. Trim history
                self._trim_history()

                # 8. Brief pause before next cycle
                await asyncio.sleep(0.2)

            except asyncio.CancelledError:
                break
            except Exception as e:
                print(f"[Agent] Error in loop: {e}")
                await asyncio.sleep(1.0)

        print("[Agent] Agent loop stopped.")

    def stop(self):
        self._running = False
