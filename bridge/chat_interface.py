"""Optional terminal chat interface — user can send messages to adjust Gemma's behavior.

Gemma runs autonomously without input. This is only for overriding objectives
or asking Gemma questions while it plays.
"""

import asyncio
import sys

from .agent_loop import AgentLoop


async def chat_input_loop(agent: AgentLoop):
    """Read user input from stdin and inject into the agent loop."""
    loop = asyncio.get_event_loop()
    print("[Chat] Gemma is playing autonomously. Type to send messages (optional).")
    print("[Chat] Examples: 'go sell at merchant', 'what's your status?', 'stop farming'")
    print("[Chat] Press Ctrl+C to stop.\n")

    while True:
        try:
            line = await loop.run_in_executor(None, sys.stdin.readline)
            if line == "":  # EOF
                break
            line = line.strip()
            if not line:
                continue
            await agent.inject_user_message(line)
        except (EOFError, KeyboardInterrupt):
            break
