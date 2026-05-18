"""Entry point for the GWA3 LLM Bridge.

Usage:
    python -m bridge --llm-url http://localhost:11434/v1 --model deepseek-v4-pro:cloud
    python -m bridge --objective "Farm Bogroot Growths HM repeatedly"
    python -m bridge --objective "Go to Kamadan and buy 10 Iron Ingots"
"""

import asyncio
import sys

from .config import parse_args
from .ipc_client import IpcClient
from .llm_client import LLMClient
from .codex_exec_client import CodexExecLLMClient
from .agent_loop import AgentLoop
from .chat_interface import chat_input_loop
from .kamadan_client import KamadanClient
from .token_budget import RemoteLlmNotAllowed, TokenBudgetError, TokenBudgetGuard


def should_enable_chat(stdin=sys.stdin) -> bool:
    return stdin is not None and stdin.isatty()


async def main():
    args = parse_args()

    print("=" * 60)
    print("  GWA3 LLM Bridge - Autonomous Agent")
    print("=" * 60)
    print(f"  LLM:       {args.llm_url}")
    print(f"  Provider:  {args.llm_provider}")
    print(f"  Model:     {args.model}")
    print(f"  Autonomy:  {args.autonomy}")
    print(f"  Pipe:      {args.pipe}")
    print(f"  Budget:    {args.llm_hourly_token_cap:,} tokens/hour")
    if args.objective:
        print(f"  Objective: {args.objective}")
    else:
        print(f"  Objective: (default - farm continuously)")
    print("=" * 60)

    try:
        if args.llm_provider == "codex-exec":
            token_budget = TokenBudgetGuard.for_codex_exec(
                hourly_token_cap=args.llm_hourly_token_cap,
                allow_remote=args.allow_remote_llm,
            )
        else:
            token_budget = TokenBudgetGuard.for_openai_endpoint(
                args.llm_url,
                hourly_token_cap=args.llm_hourly_token_cap,
                allow_remote=args.allow_remote_llm,
            )
    except RemoteLlmNotAllowed as e:
        print(f"[Bridge] ERROR: {e}")
        print("[Bridge] Use --allow-remote-llm only when an operator-approved budget is in place.")
        return 2
    except TokenBudgetError as e:
        print(f"[Bridge] ERROR: {e}")
        return 2

    # Connect to gwa3 named pipe
    ipc = IpcClient(args.pipe)
    print("[Bridge] Connecting to gwa3...")
    if not await ipc.connect(timeout=180.0):
        print("[Bridge] ERROR: Could not connect to gwa3 pipe. Is gwa3.dll injected with --llm?")
        return 1

    print("[Bridge] Connected to gwa3!")

    # Create LLM client
    if args.llm_provider == "codex-exec":
        llm = CodexExecLLMClient(
            args.model,
            timeout=args.codex_exec_timeout,
        )
    else:
        llm = LLMClient(args.llm_url, args.model)

    # Create Kamadan client from CLI-configured search settings
    kamadan = KamadanClient(
        timeout=args.kamadan_timeout,
        cache_ttl=args.kamadan_cache_ttl,
    )

    # Create agent loop with objective
    agent = AgentLoop(
        ipc,
        llm,
        autonomy=args.autonomy,
        objective=args.objective,
        kamadan_client=kamadan,
        token_budget=token_budget,
    )

    # Run agent loop and optional chat interface concurrently. The autonomous
    # agent owns process lifetime; redirected/hidden harness runs may make the
    # chat loop finish immediately, and that must not stop the bridge.
    try:
        agent_task = asyncio.create_task(agent.run())
        chat_task = None
        if should_enable_chat(sys.stdin):
            chat_task = asyncio.create_task(chat_input_loop(agent))

        await agent_task

        if chat_task is not None and not chat_task.done():
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
