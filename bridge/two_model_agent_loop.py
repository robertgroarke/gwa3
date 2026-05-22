"""Two-model planner/executor bridge orchestrator."""

from __future__ import annotations

import asyncio
import time
import uuid
from pathlib import Path
from typing import Any

from .action_dispatcher import ActionDispatcher, build_tool_result_event_payload
from .bridge_core import (
    HeartbeatMonitor,
    HeartbeatPayloadState,
    ObservationPump,
    RunSummarizer,
    UserMessageInbox,
)
from .event_bus import BridgeEventBus
from .executor_loop import ExecutorLoop
from .ipc_client import IpcClient
from .kamadan_client import KamadanClient
from .froggy_supervisor import FroggySupervisor, SUPERVISOR_TOOL_NAMES
from .llm_client import LLMClient, LLMResponse
from .observation import ObservationWindow
from .plan import PlanState
from .plan_controller import PlanController
from .planner_loop import PlannerLoop
from .run_summary_events import build_froggy_summary_stats
from .run_summary_memory import RunSummaryMemory
from .runtime_health import connection_disconnect_reason
from .disconnect import disconnect_details
from .snapshot_events import build_snapshot_summary
from .telemetry import BridgeTelemetry


DEFAULT_OBJECTIVE = (
    "Farm Bogroot Growths HM repeatedly with Froggy HM in advisory mode. "
    "Use the configured lane only."
)
HEARTBEAT_TIMEOUT_SECONDS = 15.0
MISSING_SNAPSHOT_QUERY_INTERVAL_SECONDS = 2.0
FIRST_SNAPSHOT_DEGRADATION_SECONDS = 10.0
FIRST_SNAPSHOT_DEGRADATION_INTERVAL_SECONDS = 10.0
RUN_SUMMARY_POST_QUERY_DRAIN_SECONDS = 0.6
RUN_SUMMARY_QUERY_RESULT_DRAIN_SECONDS = 5.0
RUN_SUMMARY_TOOL_REASONS = {
    "froggy_run_dungeon_loop": "tool:froggy_run_dungeon_loop",
    "froggy_run_full_maintenance": "tool:froggy_run_full_maintenance",
}
DETERMINISTIC_SUPERVISOR_OWNED_REPLAN_REASONS = {
    "plan_expired",
    "map_changed",
    "instance_load",
}


class TwoModelAgentLoop:
    """Coordinates observations, planner wake-ups, and executor ticks."""

    def __init__(
        self,
        ipc: IpcClient,
        planner_llm: LLMClient,
        executor_llm: LLMClient,
        plan_state: PlanState,
        autonomy: str = "advisory",
        objective: str | None = None,
        kamadan_client: KamadanClient | None = None,
        event_bus: BridgeEventBus | None = None,
        telemetry: BridgeTelemetry | None = None,
        run_summary_memory: RunSummaryMemory | None = None,
        profile: dict[str, Any] | None = None,
        supervisor_mode: str = "llm",
        executor_mode: str = "llm",
        planner_mode: str = "sync",
        prompt_mode: str = "full",
        replan_policy: str = "balanced",
    ):
        self.ipc = ipc
        self.planner_llm = planner_llm
        self.executor_llm = executor_llm
        self.plan_state = plan_state
        self.autonomy = autonomy
        self.objective = objective or DEFAULT_OBJECTIVE
        self.observations = ObservationWindow()
        self.event_bus = event_bus
        self.profile = profile or {}
        self.supervisor_mode = supervisor_mode
        self.executor_mode = executor_mode
        self.planner_mode = planner_mode
        self.prompt_mode = prompt_mode
        self.replan_policy = replan_policy
        self.telemetry = telemetry or BridgeTelemetry(profile=self.profile)
        self.telemetry.set_profile(self.profile)
        self.run_summaries = RunSummarizer(run_summary_memory or RunSummaryMemory())
        self._running = False
        self.user_messages = UserMessageInbox()
        self._pending_user_messages: list[str] = []
        self._cycle_count = 0
        self.heartbeat = HeartbeatMonitor(HEARTBEAT_TIMEOUT_SECONDS)
        self._last_summary_key: str | None = None
        self._last_telemetry_emit_time = 0.0
        self._last_missing_snapshot_query_time = 0.0
        self._last_missing_snapshot_degradation_time = 0.0
        self._loop_started_at = 0.0
        self._missing_snapshot_query_outstanding = False
        self._stop_error: str | None = None
        self._seen_tool_result_request_ids: set[str] = set()
        self._async_planner_task: asyncio.Task | None = None
        self._async_planner_key: tuple[int, str] | None = None
        self._last_supervisor_signature: str | None = None
        self._supervisor_native_active = False
        self.supervisor = FroggySupervisor() if supervisor_mode in {"deterministic", "hybrid"} else None
        self.heartbeat_payload = HeartbeatPayloadState(self.telemetry)
        self.observation_pump = ObservationPump(
            ipc,
            on_snapshot=self._record_snapshot_observation,
            on_event=self._record_event_observation,
            on_action_result=self._record_action_result_observation,
            on_heartbeat=self.heartbeat.mark,
            on_heartbeat_message=self._record_heartbeat_message,
        )

        self.dispatcher = ActionDispatcher(
            ipc=ipc,
            event_bus=event_bus,
            kamadan_client=kamadan_client,
            latest_snapshot=lambda: self.observations.latest,
            collect_observations=self._collect_observations_safe,
            on_tool_result=self._maybe_summarize_tool_result,
            autonomy=autonomy,
        )
        self.controller = PlanController(min_interval_seconds=5.0, policy=replan_policy)
        self.planner = PlannerLoop(
            llm=planner_llm,
            plan_state=plan_state,
            dispatcher=self.dispatcher,
            event_bus=event_bus,
            telemetry=self.telemetry,
            objective=self.objective,
            run_history=self.run_summaries.list(),
            prompt_mode=prompt_mode,
            profile_name=str(self.profile.get("name") or ""),
        )
        self.executor = ExecutorLoop(
            executor_llm=executor_llm,
            fallback_llm=planner_llm,
            dispatcher=self.dispatcher,
            event_bus=event_bus,
            telemetry=self.telemetry,
            executor_mode=executor_mode,
        )

    async def emit(self, event: str, data: dict | None = None) -> None:
        if self.event_bus is not None:
            await self.event_bus.emit(event, data or {})

    async def inject_user_message(self, message: str) -> None:
        await self.user_messages.put(message)
        await self.emit("chat.user", {"message": message})

    async def _collect_observations(self, max_messages: int = 32, max_seconds: float = 0.15) -> None:
        await self.observation_pump.drain(
            max_messages=max_messages,
            max_seconds=max_seconds,
            read_timeout=0.05,
        )

    async def _record_snapshot_observation(self, msg: dict) -> None:
        self._missing_snapshot_query_outstanding = False
        self.observations.add_snapshot(msg)
        merged = self.observations.latest or msg
        await self.emit("snapshot.summary", build_snapshot_summary(merged))
        await self._maybe_summarize_snapshot(merged)

    async def _record_event_observation(self, msg: dict) -> None:
        self.observations.add_event(msg)
        await self._maybe_summarize_event(msg)

    async def _record_action_result_observation(self, msg: dict) -> None:
        self.observations.add_event(msg)
        if self.supervisor is not None:
            self.supervisor.observe_tool_result(msg)
        request_id = str(msg.get("request_id") or "")
        if not request_id or request_id in self._seen_tool_result_request_ids:
            return
        await self.emit(
            "tool.result",
            build_tool_result_event_payload(request_id, msg, orphan=True),
        )

    def _record_heartbeat_message(self, msg: dict) -> None:
        self.heartbeat_payload.record(msg)

    async def _collect_observations_safe(self) -> None:
        try:
            await self._collect_observations()
        except asyncio.TimeoutError:
            pass

    async def run(self) -> None:
        self._running = True
        self._loop_started_at = time.monotonic()
        await self.emit("bridge.status", {"status": "connected"})
        await self.emit("plan.updated", (await self.plan_state.get()).to_dict())
        await self._emit_telemetry_snapshot("loop_start")
        print("[TwoModel] Starting planner/executor loop", flush=True)
        print(f"[TwoModel] Objective: {self.objective}", flush=True)

        while self._running:
            try:
                self._cycle_count += 1
                await self._collect_observations_safe()
                if self._heartbeat_timed_out():
                    self._stop_error = "heartbeat_timeout"
                    await self.emit("bridge.status", {
                        "status": "stopped",
                        "error": self._stop_error,
                    })
                    self._running = False
                    break
                disconnect_reason = self._connection_disconnect_reason()
                if disconnect_reason:
                    self._stop_error = disconnect_reason
                    details = disconnect_details(
                        self.observations.latest,
                        heartbeat_age_s=self.heartbeat.age_seconds(),
                    ) or {"reason": disconnect_reason, "heartbeat_age_s": self.heartbeat.age_seconds()}
                    details.setdefault("reason", disconnect_reason)
                    await self.emit("disconnect_detected", details)
                    await self.emit("degradation", {
                        "reason": disconnect_reason,
                        "disconnect": details,
                        **self._telemetry_payload(error=disconnect_reason),
                    })
                    await self.emit("bridge.status", {
                        "status": "stopped",
                        "error": disconnect_reason,
                    })
                    self._running = False
                    break

                await self._drain_user_messages()
                snapshot = self.observations.latest
                if snapshot is None:
                    await self._request_snapshot_if_missing()
                    await asyncio.sleep(0.3)
                    continue

                events = self.observations.drain_events()
                plan = await self.plan_state.get()
                completed_plan = await self._poll_async_planner()
                if completed_plan is not None:
                    plan = completed_plan
                supervisor_needs_planner = await self._run_supervisor_tick(snapshot, events)
                if self.supervisor is not None:
                    plan = await self.plan_state.get()
                decision = self.controller.evaluate(plan, snapshot, events)
                if decision.suppression_reason:
                    self.telemetry.record_replan_suppressed(decision.suppression_reason)
                decision = self._apply_supervisor_replan_suppression(
                    decision,
                    supervisor_needs_planner=supervisor_needs_planner,
                )
                if self._pending_user_messages and not decision.should_replan:
                    decision = type(decision)(True, "user_message")
                if supervisor_needs_planner and not decision.should_replan:
                    decision = type(decision)(True, "supervisor_unknown_state")

                if self._should_run_planner(decision, plan, supervisor_needs_planner):
                    if self.planner_mode == "async":
                        self._schedule_async_replan(
                            decision.reason or "initial",
                            snapshot,
                            events,
                            self._pending_user_messages,
                        )
                        self._pending_user_messages.clear()
                    else:
                        new_plan = await self.planner.replan(
                            decision.reason or "initial",
                            snapshot,
                            events,
                            self._pending_user_messages,
                        )
                        self._pending_user_messages.clear()
                        if new_plan is not None:
                            plan = new_plan

                if self.planner.unhealthy and self.executor.unhealthy:
                    await self._handoff_to_froggy("both_llms_unhealthy")
                    break

                summary = await self.executor.tick(plan, snapshot)
                self.controller.observe_executor_tick(summary.counts_as_stall)
                await self._emit_telemetry_snapshot("periodic", min_interval_seconds=5.0)
                await asyncio.sleep(0.3)
            except asyncio.CancelledError:
                break
            except Exception as exc:
                await self.emit("degradation", {
                    "reason": "two_model_loop_error",
                    "error": str(exc),
                })
                await asyncio.sleep(2.0)

        status_payload = {"status": "stopped"}
        if self._stop_error:
            status_payload["error"] = self._stop_error
        await self.emit("bridge.status", status_payload)
        await self._emit_telemetry_snapshot("loop_stop")
        self._write_latest_benchmark("latest_two_model_telemetry", "automatic bridge shutdown snapshot")
        print("[TwoModel] Loop stopped", flush=True)

    async def _drain_user_messages(self) -> None:
        self._pending_user_messages.extend(self.user_messages.drain_nowait())

    async def _run_supervisor_tick(self, snapshot: dict, events: list[dict]) -> bool:
        if self.supervisor is None:
            return False
        decision = self.supervisor.decide(snapshot, events)
        if decision is None:
            self._supervisor_native_active = False
            return False
        self._supervisor_native_active = (
            decision.action == "wait"
            and decision.reason.startswith("native_")
            and decision.reason.endswith("_in_progress")
        )
        if decision.signature != self._last_supervisor_signature:
            supervisor_plan = decision.to_plan()
            await self.plan_state.replace(supervisor_plan)
            await self.emit("plan.updated", supervisor_plan.to_dict())
            self._last_supervisor_signature = decision.signature
        if decision.control_required and self.supervisor.should_execute(decision):
            if await self._supervisor_enter_llm_control_if_needed(snapshot):
                return decision.needs_planner
            response = LLMResponse(tool_calls=[decision.to_tool_call()])
            summary = await self.dispatcher.execute_response(
                response,
                role="planner",
                allowed_tool_names=SUPERVISOR_TOOL_NAMES,
                history=[],
            )
            for result in summary.results:
                self.supervisor.observe_tool_result(result)
                self.telemetry.record_tool_result("supervisor", bool(result.get("success")))
        return decision.needs_planner

    def _apply_supervisor_replan_suppression(self, decision, supervisor_needs_planner: bool = False):
        if (
            self._supervisor_native_active
            and decision.should_replan
            and decision.reason == "plan_expired"
        ):
            self.telemetry.record_replan_suppressed("native_active:plan_expired")
            return type(decision)(False, suppression_reason="native_active:plan_expired")
        if (
            self.supervisor is not None
            and self.supervisor_mode == "deterministic"
            and decision.should_replan
            and not supervisor_needs_planner
            and decision.reason in DETERMINISTIC_SUPERVISOR_OWNED_REPLAN_REASONS
        ):
            reason = f"deterministic_supervisor:{decision.reason}"
            self.telemetry.record_replan_suppressed(reason)
            return type(decision)(False, suppression_reason=reason)
        return decision

    def _should_run_planner(self, decision, plan, supervisor_needs_planner: bool) -> bool:
        if decision.should_replan:
            return True
        if plan.phase != "idle":
            return False
        if (
            self.supervisor is not None
            and self.supervisor_mode == "deterministic"
            and not supervisor_needs_planner
            and not self._pending_user_messages
        ):
            self.telemetry.record_replan_suppressed("deterministic_supervisor:idle")
            return False
        return True

    async def _supervisor_enter_llm_control_if_needed(self, snapshot: dict) -> bool:
        bot = snapshot.get("bot") or {}
        if bot.get("state") == "llm_controlled":
            return False
        block_reason = self.dispatcher.advisory_block_reason(
            "set_bot_state",
            {"state": "llm_controlled"},
        )
        if block_reason is not None:
            return False
        result = await self.dispatcher.force_game_action(
            "set_bot_state",
            {"state": "llm_controlled"},
            role="planner",
            request_id="supervisor-control",
        )
        self.telemetry.record_tool_result("supervisor", bool(result.get("success")))
        return True

    def _schedule_async_replan(
        self,
        reason: str,
        snapshot: dict,
        events: list[dict],
        user_messages: list[str],
    ) -> None:
        if self._async_planner_task is not None and not self._async_planner_task.done():
            self.telemetry.record_replan_suppressed("planner_async_inflight")
            return
        key = self._planner_context_key(snapshot)
        self._async_planner_key = key
        self._async_planner_task = asyncio.create_task(
            self.planner.replan(
                reason,
                snapshot,
                list(events),
                list(user_messages),
                accept_plan=lambda _plan, expected=key: self._planner_context_key(self.observations.latest) == expected,
            )
        )

    async def _poll_async_planner(self):
        if self._async_planner_task is None or not self._async_planner_task.done():
            return None
        task = self._async_planner_task
        self._async_planner_task = None
        self._async_planner_key = None
        try:
            return task.result()
        except Exception as exc:
            await self.emit("degradation", {
                "reason": "planner_async_error",
                "error": str(exc),
            })
            self.telemetry.record_degradation("planner_async_error")
            return None

    @staticmethod
    def _planner_context_key(snapshot: dict | None) -> tuple[int, str]:
        snapshot = snapshot or {}
        try:
            map_id = int((snapshot.get("map") or {}).get("map_id") or 0)
        except (TypeError, ValueError):
            map_id = 0
        bot = snapshot.get("bot") or {}
        phase = str(bot.get("phase") or bot.get("state") or "")
        return (map_id, phase)

    async def _handoff_to_froggy(self, reason: str) -> None:
        await self.emit("degradation", {
            "reason": reason,
            "surface": "agent paused, Froggy in control",
        })
        self.telemetry.record_degradation(reason)
        await self.dispatcher.force_game_action(
            "set_bot_state",
            {"state": "in_dungeon"},
            role="planner",
            request_id="handoff",
        )
        self._running = False

    def stop(self) -> None:
        self._running = False

    async def _request_snapshot_if_missing(self) -> None:
        now = time.monotonic()
        await self._maybe_emit_missing_snapshot_degradation(now)
        if self._missing_snapshot_query_outstanding:
            return
        if now - self._last_missing_snapshot_query_time < MISSING_SNAPSHOT_QUERY_INTERVAL_SECONDS:
            return
        self._last_missing_snapshot_query_time = now
        send_action = getattr(self.ipc, "send_action", None)
        if not callable(send_action):
            return
        try:
            await send_action("query_state", {"wait_ms": 250}, "")
            self._missing_snapshot_query_outstanding = True
        except Exception:
            return

    async def _maybe_emit_missing_snapshot_degradation(self, now: float) -> None:
        if self.observations.latest is not None or self._loop_started_at <= 0.0:
            return
        if now - self._loop_started_at < FIRST_SNAPSHOT_DEGRADATION_SECONDS:
            return
        if now - self._last_missing_snapshot_degradation_time < FIRST_SNAPSHOT_DEGRADATION_INTERVAL_SECONDS:
            return
        self._last_missing_snapshot_degradation_time = now
        await self.emit("degradation", {
            "reason": "awaiting_first_snapshot",
            "surface": (
                "connected, waiting for first game snapshot; "
                "a long native Froggy action may still be finishing"
            ),
            **self._telemetry_payload(error="awaiting_first_snapshot"),
        })

    def _heartbeat_timed_out(self) -> bool:
        return self.heartbeat.timed_out()

    def _connection_disconnect_reason(self, snapshot: dict | None = None) -> str | None:
        return connection_disconnect_reason(snapshot or self.observations.latest)

    async def _maybe_summarize_event(self, event: dict) -> None:
        name = str(event.get("event") or event.get("type") or "")
        if name in {"run_complete", "chest_interacted", "merchant_cycle_complete", "merchant_complete"}:
            await self._summarize_run(name)

    async def _maybe_summarize_snapshot(self, snapshot: dict) -> None:
        bot = snapshot.get("bot") or {}
        run_number = bot.get("run_number") or bot.get("run")
        state = str(bot.get("state") or "")
        if run_number is None or state.lower() not in {"complete", "completed", "maintenance"}:
            return
        key = f"{run_number}:{state}"
        if key == self._last_summary_key:
            return
        self._last_summary_key = key
        await self._summarize_run(f"snapshot:{state}")

    async def _maybe_summarize_tool_result(self, result: dict) -> None:
        request_id = str(result.get("request_id") or "")
        if request_id:
            self._seen_tool_result_request_ids.add(request_id)
        if not result.get("success"):
            return
        action = str(result.get("action") or "")
        reason = RUN_SUMMARY_TOOL_REASONS.get(action)
        if reason is None:
            return
        request_id = str(result.get("request_id") or "")
        key = f"{reason}:{request_id}"
        if key == self._last_summary_key:
            return
        self._last_summary_key = key
        await self._summarize_run(reason)

    async def _summarize_run(self, reason: str) -> None:
        await self._refresh_snapshot_for_summary()
        snap = self.observations.latest or {}
        bot = snap.get("bot") or {}
        map_state = snap.get("map") or {}
        inv = snap.get("inventory") or {}
        stats = bot.get("froggy_monitoring") or bot.get("froggy_stats") or {}
        last_outcome = bot.get("froggy_dungeon_loop") or {}
        summary = {
            "title": f"Run summary - {reason}",
            "reason": reason,
            "phase": bot.get("state"),
            "map": map_state.get("map_id"),
            "free_slots": inv.get("free_slots_total"),
            "run_number": bot.get("run_number") or bot.get("run"),
            "instance_time": map_state.get("instance_time"),
            "last_outcome": last_outcome,
            "stats": build_froggy_summary_stats(stats),
        }
        item = self.run_summaries.append(summary)
        self.planner.run_history = self.run_summaries.list()
        self.telemetry.record_run_summary(item)
        await self.emit("run.summary", item)
        await self._emit_telemetry_snapshot("run_summary")

    async def _refresh_snapshot_for_summary(self) -> None:
        """Force a post-action snapshot before persisting run history.

        Long route helpers can leave old periodic snapshots queued behind their
        action_result. A summary card is operator-facing evidence, so it should
        wait briefly for a fresh query_state response instead of snapshotting the
        first stale frame still in the pipe queue.
        """
        if hasattr(self.ipc, "read_message"):
            await self._collect_observations_safe()
        send_action = getattr(self.ipc, "send_action", None)
        if not callable(send_action):
            await self._drain_summary_observations(RUN_SUMMARY_POST_QUERY_DRAIN_SECONDS)
            return

        request_id = f"summary-{uuid.uuid4().hex[:8]}"
        pending_result = None
        expect_result = getattr(self.ipc, "expect_action_result", None)
        if callable(expect_result):
            try:
                pending_result = expect_result(request_id)
            except Exception:
                pending_result = None

        try:
            await send_action("query_state", {"wait_ms": 250}, request_id)
        except Exception:
            self._forget_summary_query_result(request_id, pending_result)
            await self._drain_summary_observations(RUN_SUMMARY_POST_QUERY_DRAIN_SECONDS)
            return

        await self._drain_summary_observations_until_result(
            request_id,
            seconds=RUN_SUMMARY_QUERY_RESULT_DRAIN_SECONDS,
            pending_result=pending_result,
        )

        await self._drain_summary_observations(RUN_SUMMARY_POST_QUERY_DRAIN_SECONDS)
        self._forget_summary_query_result(request_id, pending_result)

    async def _drain_summary_observations_until_result(
        self,
        request_id: str,
        *,
        seconds: float,
        pending_result,
    ) -> bool:
        if not hasattr(self.ipc, "read_message"):
            return False
        deadline = time.monotonic() + seconds
        saw_result = False
        while time.monotonic() < deadline:
            try:
                msg = await asyncio.wait_for(self.ipc.read_message(), timeout=0.05)
            except asyncio.TimeoutError:
                if saw_result or (pending_result is not None and pending_result.done()):
                    return saw_result
                continue
            if msg is None:
                return saw_result
            await self.observation_pump._dispatch(msg)
            if msg.get("type") == "action_result" and msg.get("request_id") == request_id:
                saw_result = True
                return True
        return saw_result

    async def _drain_summary_observations(self, seconds: float) -> None:
        if not hasattr(self.ipc, "read_message"):
            return
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            await self._collect_observations_safe()
            await asyncio.sleep(0.1)

    def _forget_summary_query_result(self, request_id: str, pending_result) -> None:
        forget_result = getattr(self.ipc, "forget_action_result", None)
        if callable(forget_result):
            try:
                forget_result(request_id, pending_result)
            except Exception:
                return

    def _telemetry_payload(
        self,
        *,
        error: str | None = None,
    ) -> dict:
        snapshot = self.observations.latest or {}
        bot = snapshot.get("bot", {}) or {}
        route = snapshot.get("route", {}) or {}
        connection = snapshot.get("connection", {}) or {}
        payload = {
            "autonomy": self.autonomy,
            "error": error,
            "heartbeat_age_seconds": self.heartbeat.age_seconds(),
            "outbound_snapshots_dropped": self.heartbeat_payload.outbound_snapshots_dropped,
            "bot_phase": bot.get("phase") or bot.get("state"),
            "route_progress": route.get("progress"),
            "connection_state": connection.get("state"),
            "connection_reason": connection.get("reason"),
            "connection_disconnected": connection.get("disconnected"),
        }
        return {key: value for key, value in payload.items() if value is not None}

    def _write_latest_benchmark(self, phase: str, notes: str) -> None:
        path = Path(__file__).with_name("benchmarks") / f"{phase}.json"
        self.telemetry.write_benchmark(path, phase, notes)

    async def _emit_telemetry_snapshot(self, reason: str, min_interval_seconds: float = 0.0) -> None:
        now = time.monotonic()
        if min_interval_seconds > 0.0 and now - self._last_telemetry_emit_time < min_interval_seconds:
            return
        self._last_telemetry_emit_time = now
        await self.emit("llm.telemetry", self.telemetry.snapshot("live", reason))
