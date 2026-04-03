"""Terminal chat interface — user types messages that get injected into the agent loop."""

import asyncio
import sys

from .agent_loop import AgentLoop


async def chat_input_loop(agent: AgentLoop):
    """Read user input from stdin and inject into the agent loop."""
    loop = asyncio.get_event_loop()
    print("[Chat] Type messages to communicate with Gemma. Press Ctrl+C to quit.")
    print("[Chat] Waiting for game connection...")

    while True:
        try:
            # Read input in a thread to avoid blocking the event loop
            line = await loop.run_in_executor(None, sys.stdin.readline)
            line = line.strip()
            if not line:
                continue
            await agent.inject_user_message(line)
        except (EOFError, KeyboardInterrupt):
            break
