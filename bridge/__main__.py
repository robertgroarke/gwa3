"""Entry point for the GWA3 LLM Bridge."""

from __future__ import annotations

import asyncio
import os
import sys
from pathlib import Path
from typing import Any

from .agent_loop import AgentLoop
from .codex_exec_client import CodexExecLLMClient, check_codex_exec_available
from .config import BridgeRuntimeConfig, RUNTIME_CONFIG, parse_args
from .http_api import BridgeHttpServer, BridgeHttpState
from .ipc_client import IpcClient
from .kamadan_client import KamadanClient
try:
    from .lane_launcher import LaneLauncher
except ImportError:
    from .lane_launcher import BridgeLaneLauncher as LaneLauncher
from .llm_client import LLMClient, LLMRoleClients, check_models_available
from .run_summary_memory import RunSummaryMemory
from .two_model_agent_loop import TwoModelAgentLoop


HTTP_LAUNCHER_LANES = frozenset({
    "blumpkins",
    "beastrit",
    "disco",
    "marvin",
    "biscuit",
})


def should_enable_chat(stdin=sys.stdin) -> bool:
    return stdin is not None and stdin.isatty()


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def _infer_lane(pipe: str, runtime_config: BridgeRuntimeConfig | None = None) -> str:
    runtime_config = runtime_config or RUNTIME_CONFIG
    configured = runtime_config.dll_lane
    if configured:
        return configured
    normalized = (pipe or "").lower()
    for lane in ("blumpkins", "beastrit", "disco", "marvin", "biscuit", "trade"):
        if lane in normalized:
            return lane
    return "default"


def _should_configure_lane_launcher(lane: str) -> bool:
    return bool(os.environ.get("GWA3_LAUNCHER_SCRIPT")) or (lane or "").lower() in HTTP_LAUNCHER_LANES


def _create_lane_launcher(repo_root: Path, lane: str):
    try:
        return LaneLauncher(repo_root, lane=lane)
    except TypeError:
        return LaneLauncher(repo_root)


def _role_retry_wall_timeout(timeout: float) -> float:
    return timeout + LLMClient.ROLE_RETRY_WALL_EXTRA_SECONDS


def _make_client(
    provider: str,
    llm_url: str,
    model: str,
    timeout: float,
    retry_wall_timeout: float | None = None,
) -> Any:
    if provider == "codex-exec":
        return CodexExecLLMClient(model, timeout=timeout, workdir=str(_repo_root()))
    return LLMClient(llm_url, model, timeout=timeout, retry_wall_timeout=retry_wall_timeout)


def _seed_http_run_summaries(http_state: BridgeHttpState, run_summaries: RunSummaryMemory) -> None:
    for summary in run_summaries.list():
        http_state.run_summaries.appendleft(summary)


async def _check_model(provider: str, llm_url: str, model: str, timeout: float) -> dict[str, Any]:
    if provider == "codex-exec":
        return await check_codex_exec_available(model, timeout=max(timeout, 30.0))
    return await check_models_available(llm_url, [model], timeout=min(timeout, 5.0))


async def _preflight_two_model(args) -> dict[str, Any]:
    checks = [("planner", args.planner_provider, args.planner_model, args.planner_timeout)]
    if getattr(args, "preflight_executor", True) and args.executor_mode == "llm":
        checks.append(("executor", args.executor_provider, args.executor_model, args.executor_timeout))
    for role, provider, model, timeout in checks:
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
    print(f"  Profile:   {args.profile}")
    if args.two_model:
        print(f"  Planner:   {args.planner_provider}:{args.planner_model}")
        print(f"  Executor:  {args.executor_provider}:{args.executor_model}")
        print(f"  Modes:     supervisor={args.supervisor_mode} executor={args.executor_mode} planner={args.planner_mode} prompt={args.prompt_mode}")
    print(f"  Autonomy:  {args.autonomy}")
    print(f"  Pipe:      {args.pipe}")
    lane = _infer_lane(args.pipe)
    print(f"  Lane:      {lane}")
    if args.objective:
        print(f"  Objective: {args.objective}")
    else:
        print("  Objective: (default - farm continuously)")
    print("=" * 60)

    run_summaries = RunSummaryMemory()
    http_state = BridgeHttpState(status="stopped", lane=lane, profile=args.profile_state)
    _seed_http_run_summaries(http_state, run_summaries)
    http_server = BridgeHttpServer(http_state, args.http_host, args.http_port)
    await http_server.start()
    print(f"[Bridge] HTTP/SSE listening on http://{args.http_host}:{http_server.port}")

    lane_launcher = _create_lane_launcher(_repo_root(), lane) if _should_configure_lane_launcher(lane) else None
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
                await http_state.emit("bridge.status", {
                    "status": "connecting",
                    "lane": lane,
                })
            ipc = IpcClient(args.pipe)
            print(f"[Bridge] Connecting to gwa3 pipe {args.pipe}...")
            if not await ipc.connect(timeout=timeout):
                print("[Bridge] Could not connect to gwa3 pipe yet; HTTP launch remains available.")
                await http_state.emit("bridge.status", {
                    "status": "stopped",
                    "lane": lane,
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
                        "lane": lane,
                        "error": model_check.get("error"),
                    })
                    await close_clients()
                    return {"ok": False, "status": "degraded", **model_check}

                planner_timeout = (
                    max(args.planner_timeout, args.codex_exec_timeout)
                    if args.planner_provider == "codex-exec"
                    else args.planner_timeout
                )
                executor_timeout = (
                    max(args.executor_timeout, args.codex_exec_timeout)
                    if args.executor_provider == "codex-exec"
                    else args.executor_timeout
                )
                role_clients = LLMRoleClients(
                    planner=_make_client(
                        args.planner_provider,
                        args.llm_url,
                        args.planner_model,
                        planner_timeout,
                        retry_wall_timeout=_role_retry_wall_timeout(planner_timeout),
                    ),
                    executor=_make_client(
                        args.executor_provider,
                        args.llm_url,
                        args.executor_model,
                        executor_timeout,
                        retry_wall_timeout=_role_retry_wall_timeout(executor_timeout),
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
                    run_summary_memory=run_summaries,
                    profile=args.profile_state,
                    supervisor_mode=args.supervisor_mode,
                    executor_mode=args.executor_mode,
                    planner_mode=args.planner_mode,
                    prompt_mode=args.prompt_mode,
                    replan_policy=args.replan_policy,
                )
            else:
                agent = AgentLoop(
                    ipc,
                    llm,
                    autonomy=args.autonomy,
                    objective=args.objective,
                    kamadan_client=kamadan,
                    event_bus=http_state,
                )
            http_state.agent_loop = agent

            async def run_agent() -> None:
                try:
                    await agent.run()
                finally:
                    http_state.agent_loop = None
                    await close_clients()

            runtime_task = asyncio.create_task(run_agent())
            await http_state.emit("bridge.status", {"status": "connected", "lane": lane})
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
                    "lane": lane,
                    "error": model_check.get("error"),
                })
                return {"ok": False, "status": "degraded", **model_check}
        if lane_launcher is None:
            result = {
                "ok": False,
                "status": "stopped",
                "lane": lane,
                "error": "launcher_not_configured_for_lane",
                "message": (
                    f"HTTP launch is not configured for the {lane.upper()} lane; "
                    "start this lane through its approved runner and attach "
                    "the bridge to the lane-specific pipe."
                ),
            }
            await http_state.emit("degradation", {
                "reason": result["error"],
                "surface": result["message"],
            })
            await http_state.emit("bridge.status", {
                "status": "stopped",
                "lane": lane,
                "error": result["error"],
            })
            return result
        result = await lane_launcher.launch_and_inject()
        if not result.get("ok"):
            await http_state.emit("degradation", {
                "reason": result.get("error") or "launch_failed",
                "surface": result.get("message") or result.get("status"),
            })
            await http_state.emit("bridge.status", {
                "status": "stopped",
                "lane": lane,
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
        if lane_launcher is not None:
            stop_result = await lane_launcher.stop_started_client()
        else:
            stop_result = {
                "ok": True,
                "status": "no_lane_client_started_by_bridge",
            }
        await http_state.emit("bridge.status", {"status": "stopped", "lane": lane})
        return {
            "ok": bool(stop_result.get("ok", True)),
            "lane": lane,
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
