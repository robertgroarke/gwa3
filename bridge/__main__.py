"""Entry point for the GWA3 LLM Bridge.

Usage:
    python -m bridge --llm-url http://localhost:11434/v1 --model gemma4:27b
    python -m bridge --objective "Farm Bogroot Growths HM repeatedly"
    python -m bridge --objective "Go to Kamadan and buy 10 Iron Ingots"
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
    print("  GWA3 LLM Bridge — Gemma 4 Autonomous Agent")
    print("=" * 60)
    print(f"  LLM:       {args.llm_url}")
    print(f"  Model:     {args.model}")
    print(f"  Autonomy:  {args.autonomy}")
    print(f"  Pipe:      {args.pipe}")
    if args.objective:
        print(f"  Objective: {args.objective}")
    else:
        print(f"  Objective: (default — farm continuously)")
    print("=" * 60)

    # Connect to gwa3 named pipe
    ipc = IpcClient(args.pipe)
    print("[Bridge] Connecting to gwa3...")
    if not await ipc.connect(timeout=60.0):
        print("[Bridge] ERROR: Could not connect to gwa3 pipe. Is gwa3.dll injected with --llm?")
        return 1

    print("[Bridge] Connected to gwa3!")

    # Create LLM client
    llm = LLMClient(args.llm_url, args.model)

    # Create agent loop with objective
    agent = AgentLoop(ipc, llm, autonomy=args.autonomy, objective=args.objective)

    # Run agent loop and optional chat interface concurrently.
    # Agent task is primary — chat ending (e.g. stdin EOF) should not stop the bot.
    try:
        agent_task = asyncio.create_task(agent.run())
        chat_task = asyncio.create_task(chat_input_loop(agent))

        # Wait for the agent task (primary). Chat is background/optional.
        try:
            await agent_task
        except Exception as e:
            print(f"[Bridge] Agent loop error: {e}")
        finally:
            chat_task.cancel()
            try:
                await chat_task
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
