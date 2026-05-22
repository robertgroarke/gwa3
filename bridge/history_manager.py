"""Conversation history ownership for bridge loops."""

from __future__ import annotations

import json


class HistoryManager:
    """Own prompt history and common trimming policy."""

    def __init__(self, max_messages: int = 40) -> None:
        self.max_messages = max_messages
        self.messages: list[dict] = []

    def append(self, message: dict) -> None:
        self.messages.append(message)

    def trim_preserving_user_messages(self) -> None:
        if len(self.messages) <= self.max_messages:
            return
        groups = self._message_groups(self.messages)
        recent_groups: list[list[dict]] = []
        dropped_groups: list[list[dict]] = []
        recent_start = len(groups)
        recent_count = 0

        for index in range(len(groups) - 1, -1, -1):
            group = groups[index]
            if not self._valid_prompt_group(group):
                dropped_groups.append(group)
                recent_count += len(group)
                recent_start = index
                if recent_count >= self.max_messages:
                    break
                continue
            if recent_count >= self.max_messages and recent_groups:
                break
            recent_groups.insert(0, group)
            recent_count += len(group)
            recent_start = index

        old_groups = groups[:recent_start]
        keep: list[dict] = []
        dropped_message_count = sum(len(group) for group in dropped_groups)
        for group in old_groups:
            for msg in group:
                if msg.get("role") == "user" and not msg.get("content", "").startswith("[GAME STATE"):
                    keep.append(msg)
                else:
                    dropped_message_count += 1

        if dropped_message_count:
            keep.append(self._trim_summary(dropped_message_count))

        for group in recent_groups:
            keep.extend(group)
        self.messages[:] = keep

    def for_prompt(
        self,
        *,
        compact_waits: bool = False,
        wait_compact_min: int = 4,
        wait_pair_keep: int = 2,
    ) -> list[dict]:
        prompt_messages = self._valid_prompt_messages(self.messages)
        if not compact_waits:
            return prompt_messages

        compacted: list[dict] = []
        i = 0
        while i < len(prompt_messages):
            pair = self._wait_pair_result(prompt_messages, i)
            if pair is None:
                compacted.append(prompt_messages[i])
                i += 1
                continue

            run_pairs: list[tuple[dict, dict, dict]] = []
            while True:
                pair = self._wait_pair_result(prompt_messages, i)
                if pair is None:
                    break
                run_pairs.append(pair)
                i += 2

            if len(run_pairs) < wait_compact_min:
                for assistant_msg, tool_msg, _result in run_pairs:
                    compacted.extend([assistant_msg, tool_msg])
                continue

            old_pairs = run_pairs[:-wait_pair_keep]
            recent_pairs = run_pairs[-wait_pair_keep:]
            compacted.append(
                self._wait_history_summary([result for _a, _t, result in old_pairs])
            )
            for assistant_msg, tool_msg, _result in recent_pairs:
                compacted.extend([assistant_msg, tool_msg])

        return compacted

    def recent_for_prompt(self, max_messages: int) -> list[dict]:
        """Return recent prompt history without orphaned tool-call messages."""
        if max_messages <= 0:
            return []

        groups = self._message_groups(self.messages)
        selected_groups: list[list[dict]] = []
        selected_count = 0
        for group in reversed(groups):
            if not self._valid_prompt_group(group):
                continue
            if selected_groups and selected_count + len(group) > max_messages:
                break
            selected_groups.insert(0, group)
            selected_count += len(group)
            if selected_count >= max_messages:
                break

        return [msg for group in selected_groups for msg in group]

    def _wait_pair_result(
        self,
        messages: list[dict],
        index: int,
    ) -> tuple[dict, dict, dict] | None:
        if index + 1 >= len(messages):
            return None
        assistant_msg = messages[index]
        tool_msg = messages[index + 1]
        if assistant_msg.get("role") != "assistant" or tool_msg.get("role") != "tool":
            return None

        tool_calls = assistant_msg.get("tool_calls") or []
        if len(tool_calls) != 1:
            return None
        tool_call = tool_calls[0] or {}
        function = tool_call.get("function") or {}
        if function.get("name") != "wait":
            return None
        if tool_msg.get("tool_call_id") != tool_call.get("id"):
            return None

        try:
            result = json.loads(tool_msg.get("content") or "{}")
        except (TypeError, ValueError, json.JSONDecodeError):
            return None
        if result.get("action") != "wait" or not result.get("success"):
            return None
        return assistant_msg, tool_msg, result

    @classmethod
    def _valid_prompt_messages(cls, messages: list[dict]) -> list[dict]:
        valid: list[dict] = []
        for group in cls._message_groups(messages):
            if cls._valid_prompt_group(group):
                valid.extend(group)
        return valid

    @classmethod
    def _message_groups(cls, messages: list[dict]) -> list[list[dict]]:
        groups: list[list[dict]] = []
        index = 0
        while index < len(messages):
            msg = messages[index]
            tool_calls = msg.get("tool_calls") or []
            if msg.get("role") != "assistant" or not tool_calls:
                groups.append([msg])
                index += 1
                continue

            expected_ids = [
                call.get("id")
                for call in tool_calls
                if isinstance(call, dict) and call.get("id")
            ]
            group = [msg]
            index += 1
            while index < len(messages):
                next_msg = messages[index]
                if next_msg.get("role") != "tool":
                    break
                if next_msg.get("tool_call_id") not in expected_ids:
                    break
                group.append(next_msg)
                index += 1
                if cls._tool_reply_ids(group) >= set(expected_ids):
                    break
            groups.append(group)
        return groups

    @staticmethod
    def _valid_prompt_group(group: list[dict]) -> bool:
        if not group:
            return False
        first = group[0]
        if first.get("role") == "tool":
            return False
        tool_calls = first.get("tool_calls") or []
        if first.get("role") != "assistant" or not tool_calls:
            return True
        expected = {
            call.get("id")
            for call in tool_calls
            if isinstance(call, dict) and call.get("id")
        }
        return bool(expected) and HistoryManager._tool_reply_ids(group) >= expected

    @staticmethod
    def _tool_reply_ids(group: list[dict]) -> set[str]:
        return {
            str(msg.get("tool_call_id"))
            for msg in group[1:]
            if msg.get("role") == "tool" and msg.get("tool_call_id")
        }

    @staticmethod
    def _wait_history_summary(wait_results: list[dict]) -> dict:
        latest = wait_results[-1]
        summary = {
            "wait_turns_compacted": len(wait_results),
            "latest_request_id": latest.get("request_id"),
            "latest_success": latest.get("success"),
            "latest_bot_state": latest.get("bot_state"),
            "latest_bot_phase": latest.get("bot_phase"),
            "latest_route_progress": latest.get("route_progress"),
            "latest_route_next_step": latest.get("route_next_step"),
            "note": (
                "Repeated successful advisory wait turns were omitted from "
                "prompt history; full results remain in bridge telemetry."
            ),
        }
        return {
            "role": "system",
            "content": (
                "[WAIT HISTORY COMPACTED]\n"
                + json.dumps(summary, ensure_ascii=False, separators=(",", ":"), default=str)
            ),
        }

    @staticmethod
    def _trim_summary(dropped_message_count: int) -> dict:
        return {
            "role": "system",
            "content": (
                "[HISTORY TRIMMED]\n"
                + json.dumps(
                    {
                        "messages_dropped": dropped_message_count,
                        "note": (
                            "Older prompt history was summarized during trimming; "
                            "assistant tool calls and tool replies are kept or dropped together."
                        ),
                    },
                    ensure_ascii=False,
                    separators=(",", ":"),
                )
            ),
        }
