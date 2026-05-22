"""Prompt-history event builders for bridge agent loops."""

from __future__ import annotations

import json
from typing import Any


def build_assistant_content_message(content: str) -> dict[str, str]:
    return {
        "role": "assistant",
        "content": content,
    }


def build_assistant_tool_calls_message(
    *,
    content: str | None,
    tool_calls: list[Any],
) -> dict[str, Any]:
    return {
        "role": "assistant",
        "content": content,
        "tool_calls": [
            {
                "id": tool_call.id,
                "type": "function",
                "function": {
                    "name": tool_call.name,
                    "arguments": tool_call.arguments,
                },
            }
            for tool_call in tool_calls
        ],
    }


def build_assistant_response_message(
    *,
    content: str | None,
    tool_calls: list[Any],
) -> dict[str, Any] | None:
    if tool_calls:
        return build_assistant_tool_calls_message(
            content=content,
            tool_calls=tool_calls,
        )
    if content:
        return build_assistant_content_message(content)
    return None


def add_request_id_to_result(result: dict, request_id: str = "") -> dict:
    if request_id and "request_id" not in result:
        return {**result, "request_id": request_id}
    return result


def build_tool_result_message(
    *,
    tool_call_id: str,
    result: dict,
) -> dict[str, str]:
    return {
        "role": "tool",
        "tool_call_id": tool_call_id,
        "content": json.dumps(result),
    }


def build_run_summary_recorded_message(event_type: Any, summary: str) -> dict[str, str]:
    return {
        "role": "user",
        "content": (
            "[RUN SUMMARY RECORDED]\n"
            + json.dumps({
                "event_type": event_type,
                "summary": summary,
            })
        ),
    }


def build_froggy_harness_fallback_executed_message(
    enriched_result: dict[str, Any],
) -> dict[str, str]:
    return {
        "role": "user",
        "content": (
            "[FROGGY HARNESS FALLBACK EXECUTED]\n"
            + json.dumps(enriched_result)
        ),
    }


def build_froggy_harness_fallback_deferred_message(
    *,
    trigger: str,
    action: Any,
    reason: Any,
) -> dict[str, str]:
    return {
        "role": "user",
        "content": (
            "[FROGGY HARNESS FALLBACK DEFERRED]\n"
            + json.dumps({
                "harness_fallback_deferred": True,
                "fallback_trigger": trigger,
                "fallback_action": action,
                "fallback_reason": reason,
                "instruction": (
                    "Harness fallback is disabled; choose the next tool "
                    "yourself from current state and the advisory recommendation."
                ),
            })
        ),
    }


def build_llm_rate_limit_message(
    *,
    provider_backoff_seconds: int,
    retry_after_local_time: str,
    harness_fallback_enabled: bool,
    error_tail: str,
) -> dict[str, str]:
    return {
        "role": "user",
        "content": (
            "[LLM RATE LIMIT]\n"
            + json.dumps({
                "provider_backoff_seconds": provider_backoff_seconds,
                "retry_after_local_time": retry_after_local_time,
                "harness_fallback_enabled": harness_fallback_enabled,
                "error": error_tail,
            })
        ),
    }


def build_idle_nudge_message(
    *,
    idle_limit: int,
    recommended_action: Any = None,
    reason: Any = None,
) -> dict[str, str]:
    recommendation_text = (
        f" Recommended Froggy action: {recommended_action} "
        f"(reason: {reason})."
        if recommended_action
        else ""
    )
    return {
        "role": "user",
        "content": (
            f"[SYSTEM] You have been idle for {idle_limit} cycles."
            f"{recommendation_text} Take action toward your "
            "objective or explain what you're waiting for."
        ),
    }
