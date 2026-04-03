"""Entry point for the GWA3 LLM Bridge.

Usage:
    python -m bridge
    python -m bridge --llm-url http://localhost:8000/v1 --model gemma-4-32b-it
    python -m bridge --autonomy tactical
"""

import asyncio
import sys

from .config import parse_args
from .ipc_client import IpcClient
from .llm_client import LLMClient
from .agent_loop import AgentLoop
from .chat_interface import chat_input_loop


async def main():
    args = parse_args()

    print("=" * 60)
    print("  GWA3 LLM Bridge — Gemma 4 Agent")
    print("=" * 60)
    print(f"  LLM:      {args.llm_url}")
    print(f"  Model:    {args.model}")
    print(f"  Autonomy: {args.autonomy}")
    print(f"  Pipe:     {args.pipe}")
    print("=" * 60)

    # Connect to gwa3 named pipe
    ipc = IpcClient(args.pipe)
    print("[Bridge] Connecting to gwa3...")
    if not await ipc.connect(timeout=60.0):
        print("[Bridge] ERROR: Could not connect to gwa3 pipe. Is gwa3.dll injected in LLM mode?")
        return 1

    print("[Bridge] Connected to gwa3!")

    # Create LLM client
    llm = LLMClient(args.llm_url, args.model)

    # Create agent loop
    agent = AgentLoop(ipc, llm, autonomy=args.autonomy)

    # Run agent loop and chat interface concurrently
    try:
        agent_task = asyncio.create_task(agent.run())
        chat_task = asyncio.create_task(chat_input_loop(agent))

        # Wait for either task to complete (chat exits on Ctrl+C)
        done, pending = await asyncio.wait(
            [agent_task, chat_task],
            return_when=asyncio.FIRST_COMPLETED,
        )

        # Cancel remaining tasks
        for task in pending:
            task.cancel()
            try:
                await task
            except asyncio.CancelledError:
                pass

    except KeyboardInterrupt:
        print("\n[Bridge] Shutting down...")
    finally:
        agent.stop()
        await llm.close()
        ipc.disconnect()

    print("[Bridge] Goodbye.")
    return 0


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
