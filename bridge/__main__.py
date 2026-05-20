"""Entry point for the GWA3 LLM Bridge."""

from __future__ import annotations

import asyncio
import sys
from pathlib import Path
from typing import Any

from .agent_loop import AgentLoop
from .codex_exec_client import CodexExecLLMClient, check_codex_exec_available
from .config import parse_args
from .http_api import BridgeHttpServer, BridgeHttpState, configured_lane
from .ipc_client import IpcClient
from .kamadan_client import KamadanClient
from .lane_launcher import BridgeLaneLauncher
from .llm_client import LLMClient, LLMRoleClients, check_models_available
from .run_history import RunHistoryStore
from .token_budget import RemoteLlmNotAllowed, TokenBudgetError, TokenBudgetGuard
from .two_model_agent_loop import TwoModelAgentLoop


def should_enable_chat(stdin=sys.stdin) -> bool:
    return stdin is not None and stdin.isatty()


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def _make_client(provider: str, llm_url: str, model: str, timeout: float) -> Any:
    if provider == "codex-exec":
        return CodexExecLLMClient(model, timeout=timeout, workdir=str(_repo_root()))
    return LLMClient(llm_url, model, timeout=timeout)


def _seed_http_run_summaries(http_state: BridgeHttpState, run_history_store: RunHistoryStore) -> None:
    for summary in run_history_store.list():
        http_state.run_summaries.appendleft(summary)


async def _check_model(provider: str, llm_url: str, model: str, timeout: float) -> dict[str, Any]:
    if provider == "codex-exec":
        return await check_codex_exec_available(model, timeout=max(timeout, 30.0))
    return await check_models_available(llm_url, [model], timeout=min(timeout, 5.0))


async def _preflight_two_model(args) -> dict[str, Any]:
    for role, provider, model, timeout in [
        ("planner", args.planner_provider, args.planner_model, args.planner_timeout),
        ("executor", args.executor_provider, args.executor_model, args.executor_timeout),
    ]:
        result = await _check_model(provider, args.llm_url, model, timeout)
        if not result.get("ok"):
            return {
                "ok": False,
                "error": "role_model_preflight_failed",
                "message": "Planner/executor model preflight failed.",
                "failures": [{"role": role, **result}],
            }
    return {"ok": True}


async def main():
    args = parse_args()

    print("=" * 60)
    print("  GWA3 LLM Bridge")
    print("=" * 60)
    print(f"  LLM:       {args.llm_url}")
    print(f"  Provider:  {args.llm_provider}")
    print(f"  Model:     {args.model}")
    if args.two_model:
        print(f"  Planner:   {args.planner_provider}:{args.planner_model}")
        print(f"  Executor:  {args.executor_provider}:{args.executor_model}")
    print(f"  Autonomy:  {args.autonomy}")
    print(f"  Pipe:      {args.pipe}")
    if args.objective:
        print(f"  Objective: {args.objective}")
    else:
        print("  Objective: (default - farm continuously)")
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
    except RemoteLlmNotAllowed as exc:
        print(f"[Bridge] ERROR: {exc}")
        print("[Bridge] Use --allow-remote-llm only when an operator-approved budget is in place.")
        return 2
    except TokenBudgetError as exc:
        print(f"[Bridge] ERROR: {exc}")
        return 2

    lane_name = configured_lane()
    run_history_store = RunHistoryStore()
    http_state = BridgeHttpState(status="stopped")
    _seed_http_run_summaries(http_state, run_history_store)
    http_server = BridgeHttpServer(http_state, args.http_host, args.http_port)
    await http_server.start()
    print(f"[Bridge] HTTP/SSE listening on http://{args.http_host}:{http_server.port}")

    lane_launcher = BridgeLaneLauncher(_repo_root())
    runtime_lock = asyncio.Lock()
    runtime_task: asyncio.Task | None = None
    ipc: IpcClient | None = None
    llm: Any | None = None
    role_clients: LLMRoleClients | None = None

    async def close_clients() -> None:
        nonlocal llm, role_clients, ipc
        if role_clients is not None:
            await role_clients.close()
            role_clients = None
        elif llm is not None:
            await llm.close()
            llm = None
        if ipc is not None:
            ipc.disconnect()
            ipc = None

    async def start_runtime(timeout: float = 60.0, surface_connecting: bool = True) -> dict:
        nonlocal runtime_task, ipc, llm, role_clients
        async with runtime_lock:
            if runtime_task is not None and not runtime_task.done():
                return {"ok": True, "status": http_state.status, "noop": True}

            if surface_connecting:
                await http_state.emit("bridge.status", {"status": "connecting", "lane": lane_name})
            ipc = IpcClient(args.pipe)
            print(f"[Bridge] Connecting to gwa3 pipe {args.pipe}...")
            if not await ipc.connect(timeout=timeout):
                print("[Bridge] Could not connect to gwa3 pipe yet; HTTP launch remains available.")
                await http_state.emit("bridge.status", {
                    "status": "stopped",
                    "lane": lane_name,
                    "error": "pipe_connect_failed",
                })
                ipc.disconnect()
                ipc = None
                return {"ok": False, "error": "pipe_connect_failed", "status": "stopped"}

            if args.two_model:
                model_check = await _preflight_two_model(args)
                if not model_check.get("ok"):
                    await http_state.emit("degradation", {
                        "reason": model_check.get("error"),
                        "surface": "planner/executor model preflight failed",
                        "details": model_check,
                    })
                    await http_state.emit("bridge.status", {
                        "status": "degraded",
                        "lane": lane_name,
                        "error": model_check.get("error"),
                    })
                    await close_clients()
                    return {"ok": False, "status": "degraded", **model_check}

                role_clients = LLMRoleClients(
                    planner=_make_client(
                        args.planner_provider,
                        args.llm_url,
                        args.planner_model,
                        max(args.planner_timeout, args.codex_exec_timeout)
                        if args.planner_provider == "codex-exec"
                        else args.planner_timeout,
                    ),
                    executor=_make_client(
                        args.executor_provider,
                        args.llm_url,
                        args.executor_model,
                        max(args.executor_timeout, args.codex_exec_timeout)
                        if args.executor_provider == "codex-exec"
                        else args.executor_timeout,
                    ),
                )
                llm = None
            else:
                timeout_seconds = (
                    args.codex_exec_timeout
                    if args.llm_provider == "codex-exec"
                    else 120.0
                )
                llm = _make_client(args.llm_provider, args.llm_url, args.model, timeout_seconds)
                role_clients = None

            kamadan = KamadanClient(
                timeout=args.kamadan_timeout,
                cache_ttl=args.kamadan_cache_ttl,
            )

            if args.two_model and role_clients is not None:
                agent = TwoModelAgentLoop(
                    ipc=ipc,
                    planner_llm=role_clients.planner,
                    executor_llm=role_clients.executor,
                    plan_state=http_state.plan_state,
                    autonomy=args.autonomy,
                    objective=args.objective,
                    kamadan_client=kamadan,
                    event_bus=http_state,
                    run_history_store=run_history_store,
                )
            else:
                agent = AgentLoop(
                    ipc,
                    llm,
                    autonomy=args.autonomy,
                    objective=args.objective,
                    kamadan_client=kamadan,
                    token_budget=token_budget,
                )
            http_state.agent_loop = agent

            async def run_agent() -> None:
                try:
                    await agent.run()
                finally:
                    http_state.agent_loop = None
                    await close_clients()

            runtime_task = asyncio.create_task(run_agent())
            return {"ok": True, "status": "connected"}

    async def launch_runtime() -> dict:
        if args.two_model:
            model_check = await _preflight_two_model(args)
            if not model_check.get("ok"):
                await http_state.emit("degradation", {
                    "reason": model_check.get("error"),
                    "surface": "planner/executor model preflight failed",
                    "details": model_check,
                })
                await http_state.emit("bridge.status", {
                    "status": "degraded",
                    "lane": lane_name,
                    "error": model_check.get("error"),
                })
                return {"ok": False, "status": "degraded", **model_check}
        result = await lane_launcher.launch_and_inject()
        if not result.get("ok"):
            await http_state.emit("degradation", {
                "reason": result.get("error") or "launch_failed",
                "surface": result.get("message") or result.get("status"),
            })
            await http_state.emit("bridge.status", {
                "status": "stopped",
                "lane": lane_name,
                "error": result.get("error") or result.get("status"),
            })
            return result
        bridge = await start_runtime(timeout=45.0)
        result["bridge"] = bridge
        return result

    async def stop_runtime() -> dict:
        nonlocal runtime_task
        if http_state.agent_loop is not None:
            http_state.agent_loop.stop()
        if runtime_task is not None and not runtime_task.done():
            try:
                await asyncio.wait_for(runtime_task, timeout=3.0)
            except asyncio.TimeoutError:
                runtime_task.cancel()
                try:
                    await runtime_task
                except asyncio.CancelledError:
                    pass
        runtime_task = None
        await close_clients()
        stop_result = await lane_launcher.stop_started_client()
        await http_state.emit("bridge.status", {"status": "stopped", "lane": lane_name})
        return {
            "ok": bool(stop_result.get("ok", True)),
            "lane": lane_name,
            "runtime_stopped": True,
            "lane_stop": stop_result,
        }

    http_state.launcher = launch_runtime
    http_state.stopper = stop_runtime

    await start_runtime(timeout=3.0, surface_connecting=False)

    try:
        while True:
            await asyncio.sleep(3600)
    except KeyboardInterrupt:
        print("\n[Bridge] Shutting down...")
    finally:
        await stop_runtime()
        await http_server.stop()

    print("[Bridge] Goodbye.")
    return 0


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
