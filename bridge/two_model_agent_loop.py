"""Two-model planner/executor bridge orchestrator."""

from __future__ import annotations

import asyncio
import time
import uuid
from pathlib import Path
from typing import Any

from .action_dispatcher import ActionDispatcher
from .event_bus import BridgeEventBus
from .executor_loop import ExecutorLoop
from .ipc_client import IpcClient
from .kamadan_client import KamadanClient
from .llm_client import LLMClient
from .observation import ObservationWindow
from .plan import PlanState
from .plan_controller import PlanController
from .planner_loop import PlannerLoop
from .run_history import RunHistoryStore
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
        run_history_store: RunHistoryStore | None = None,
    ):
        self.ipc = ipc
        self.planner_llm = planner_llm
        self.executor_llm = executor_llm
        self.plan_state = plan_state
        self.autonomy = autonomy
        self.objective = objective or DEFAULT_OBJECTIVE
        self.observations = ObservationWindow()
        self.event_bus = event_bus
        self.telemetry = telemetry or BridgeTelemetry()
        self.run_history_store = run_history_store or RunHistoryStore()
        self._running = False
        self._user_message_queue: asyncio.Queue[str] = asyncio.Queue()
        self._pending_user_messages: list[str] = []
        self._cycle_count = 0
        self._last_heartbeat_time = 0.0
        self._saw_heartbeat = False
        self._last_summary_key: str | None = None
        self._last_telemetry_emit_time = 0.0
        self._last_missing_snapshot_query_time = 0.0
        self._last_missing_snapshot_degradation_time = 0.0
        self._loop_started_at = 0.0
        self._missing_snapshot_query_outstanding = False
        self._stop_error: str | None = None
        self._seen_tool_result_request_ids: set[str] = set()

        self.dispatcher = ActionDispatcher(
            ipc=ipc,
            event_bus=event_bus,
            kamadan_client=kamadan_client,
            latest_snapshot=lambda: self.observations.latest,
            collect_observations=self._collect_observations_safe,
            on_tool_result=self._maybe_summarize_tool_result,
            autonomy=autonomy,
        )
        self.controller = PlanController(min_interval_seconds=5.0)
        self.planner = PlannerLoop(
            llm=planner_llm,
            plan_state=plan_state,
            dispatcher=self.dispatcher,
            event_bus=event_bus,
            telemetry=self.telemetry,
            objective=self.objective,
            run_history=self.run_history_store.list(),
        )
        self.executor = ExecutorLoop(
            executor_llm=executor_llm,
            fallback_llm=planner_llm,
            dispatcher=self.dispatcher,
            event_bus=event_bus,
            telemetry=self.telemetry,
        )

    async def emit(self, event: str, data: dict | None = None) -> None:
        if self.event_bus is not None:
            await self.event_bus.emit(event, data or {})

    async def inject_user_message(self, message: str) -> None:
        await self._user_message_queue.put(message)
        await self.emit("chat.user", {"message": message})

    async def _collect_observations(self, max_messages: int = 32, max_seconds: float = 0.15) -> None:
        started = time.monotonic()
        read_count = 0
        while read_count < max_messages and time.monotonic() - started < max_seconds:
            msg = await asyncio.wait_for(self.ipc.read_message(), timeout=0.05)
            if msg is None:
                break
            read_count += 1
            await self._dispatch_observation_message(msg)

    async def _dispatch_observation_message(self, msg: dict) -> None:
        msg_type = msg.get("type", "")
        if msg_type == "snapshot":
            self._missing_snapshot_query_outstanding = False
            self.observations.add_snapshot(msg)
            merged = self.observations.latest or msg
            await self.emit("snapshot.summary", self._snapshot_summary(merged))
            await self._maybe_summarize_snapshot(merged)
        elif msg_type == "event":
            self.observations.add_event(msg)
            await self._maybe_summarize_event(msg)
        elif msg_type == "action_result":
            self.observations.add_event(msg)
            request_id = str(msg.get("request_id") or "")
            if request_id and request_id not in self._seen_tool_result_request_ids:
                await self.emit("tool.result", {
                    "request_id": request_id,
                    "success": bool(msg.get("success", False)),
                    "error": msg.get("error"),
                    "orphan": True,
                })
        elif msg_type == "heartbeat":
            self._last_heartbeat_time = time.monotonic()
            self._saw_heartbeat = True

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
                    await self.emit("degradation", {
                        "reason": disconnect_reason,
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
                decision = self.controller.evaluate(plan, snapshot, events)
                if self._pending_user_messages and not decision.should_replan:
                    decision = type(decision)(True, "user_message")

                if decision.should_replan or plan.phase == "idle":
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
        while not self._user_message_queue.empty():
            message = self._user_message_queue.get_nowait()
            self._pending_user_messages.append(message)

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
        if not self._saw_heartbeat:
            return False
        return time.monotonic() - self._last_heartbeat_time > HEARTBEAT_TIMEOUT_SECONDS

    def _connection_disconnect_reason(self, snapshot: dict | None = None) -> str | None:
        snap = snapshot or self.observations.latest or {}
        connection = snap.get("connection", {}) or {}
        if connection.get("disconnected"):
            reason = connection.get("reason") or connection.get("state") or "unknown"
            return f"gw_disconnected:{reason}"

        map_state = snap.get("map", {}) or {}
        if map_state.get("loading_state") == 2:
            return "gw_disconnected:loading_state_disconnected"
        return None

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
            "stats": {
                "runs": stats.get("run_count"),
                "fails": stats.get("fail_count"),
                "wipes": stats.get("monitoring_wipes"),
                "route_wipes": stats.get("route_wipe_count"),
                "chests_opened": stats.get("chests_opened"),
                "gold_items": stats.get("gold_items"),
                "rare_skins": stats.get("rare_skins"),
                "tomes": stats.get("tomes"),
            },
        }
        item = self.run_history_store.append(summary)
        self.planner.run_history = self.run_history_store.list()
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
            await self._dispatch_observation_message(msg)
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

    @staticmethod
    def _snapshot_summary(snapshot: dict) -> dict:
        me = snapshot.get("me", {}) or {}
        party = snapshot.get("party", {}) or {}
        inv = snapshot.get("inventory", {}) or {}
        bot = snapshot.get("bot", {}) or {}
        map_state = snapshot.get("map", {}) or {}
        connection = snapshot.get("connection", {}) or {}
        return {
            "map": map_state.get("map_id"),
            "connection": {
                "state": connection.get("state"),
                "disconnected": connection.get("disconnected"),
                "reason": connection.get("reason"),
                "likely_disconnect_code": connection.get("likely_disconnect_code"),
            },
            "hp": me.get("hp"),
            "party": {
                "size": party.get("size"),
                "dead": party.get("dead_count"),
                "defeated": party.get("is_defeated"),
            },
            "free_slots": inv.get("free_slots_total"),
            "current_phase": bot.get("state"),
        }

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
            "heartbeat_age_seconds": (
                max(0.0, time.monotonic() - self._last_heartbeat_time)
                if self._saw_heartbeat else None
            ),
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
