"""Route-spec helpers for LLM planner context."""

from __future__ import annotations

import json
from functools import lru_cache
from pathlib import Path
from typing import Any


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ROUTE_PATH = REPO_ROOT / "routes" / "bogroot_hm_v1.json"


@lru_cache(maxsize=8)
def load_route_script(path: str | Path = DEFAULT_ROUTE_PATH) -> dict[str, Any]:
    route_path = Path(path)
    return json.loads(route_path.read_text(encoding="utf-8"))


def build_planner_route_guidance(path: str | Path = DEFAULT_ROUTE_PATH) -> dict[str, Any]:
    """Build compact planner-facing route guidance from route JSON data."""
    route = load_route_script(path)
    maps = route.get("maps") or {}
    recovery_maps = route.get("recovery_maps") or {}
    steps = route.get("steps") or []
    phase_order: list[str] = []
    phases: dict[str, dict[str, Any]] = {}
    for step in steps:
        phase = str(step.get("phase") or "unknown")
        if phase not in phases:
            phase_order.append(phase)
            phases[phase] = {
                "map_id": step.get("map_id"),
                "first_order": step.get("order"),
                "last_order": step.get("order"),
                "kinds": [],
            }
        phase_info = phases[phase]
        phase_info["last_order"] = step.get("order")
        kind = step.get("kind")
        if kind and kind not in phase_info["kinds"]:
            phase_info["kinds"].append(kind)

    return {
        "script_id": route.get("script_id"),
        "bot": route.get("bot"),
        "mode": route.get("mode"),
        "maps": {
            name: data.get("map_id")
            for name, data in maps.items()
        },
        "recovery_maps": {
            name: data.get("map_id")
            for name, data in recovery_maps.items()
        },
        "phase_order": [
            {"phase": phase, **phases[phase]}
            for phase in phase_order
        ],
        "route_tools": (route.get("planner_guidance") or {}).get("route_tools", []),
        "rule": "Select explicit route/control tool calls. The bridge will not infer or replace route tools from Plan prose.",
    }


def route_map_ids(path: str | Path = DEFAULT_ROUTE_PATH) -> dict[str, int]:
    """Return route map IDs keyed by the route script's map names."""
    return _map_ids_from_section(load_route_script(path), "maps")


def route_recovery_map_ids(path: str | Path = DEFAULT_ROUTE_PATH) -> dict[str, int]:
    """Return recovery map IDs keyed by the route script's recovery map names."""
    return _map_ids_from_section(load_route_script(path), "recovery_maps")


def route_map_names(path: str | Path = DEFAULT_ROUTE_PATH) -> dict[str, str]:
    """Return route map display names keyed by the route script's map names."""
    return _map_names_from_section(load_route_script(path), "maps")


def route_recovery_map_names(path: str | Path = DEFAULT_ROUTE_PATH) -> dict[str, str]:
    """Return recovery map display names keyed by the route script's recovery map names."""
    return _map_names_from_section(load_route_script(path), "recovery_maps")


def _map_ids_from_section(route: dict[str, Any], section: str) -> dict[str, int]:
    result: dict[str, int] = {}
    for name, data in (route.get(section) or {}).items():
        map_id = data.get("map_id") if isinstance(data, dict) else None
        if isinstance(map_id, int):
            result[str(name)] = map_id
    return result


def _map_names_from_section(route: dict[str, Any], section: str) -> dict[str, str]:
    result: dict[str, str] = {}
    for name, data in (route.get(section) or {}).items():
        map_name = data.get("name") if isinstance(data, dict) else None
        if isinstance(map_name, str) and map_name:
            result[str(name)] = map_name
    return result


def observation_hint_for_map(map_id: int, path: str | Path = DEFAULT_ROUTE_PATH) -> str | None:
    """Return a compact route hint for the current map, if the route spec defines one."""
    route = load_route_script(path)
    hints = route.get("observation_hints") or {}
    hint = hints.get(str(map_id))
    return hint if isinstance(hint, str) and hint else None


def route_safety_map_ids(safety_key: str, path: str | Path = DEFAULT_ROUTE_PATH) -> set[int]:
    """Resolve a named safety map set from route map keys to map ids."""
    route = load_route_script(path)
    maps = _combined_route_maps(route)
    safety = route.get("safety") or {}
    if safety_key not in safety:
        raise ValueError(f"route safety set {safety_key!r} is not defined")

    ids: set[int] = set()
    unknown_keys: list[str] = []
    for map_key in safety.get(safety_key, []) or []:
        map_key = str(map_key)
        map_info = maps.get(map_key) or {}
        map_id = map_info.get("map_id")
        if isinstance(map_id, int):
            ids.add(map_id)
        else:
            unknown_keys.append(map_key)
    if unknown_keys:
        joined = ", ".join(sorted(unknown_keys))
        raise ValueError(f"route safety set {safety_key!r} references unknown map keys: {joined}")
    return ids


def validate_route_tool_choice(
    tool_name: str,
    snapshot: dict | None,
    path: str | Path = DEFAULT_ROUTE_PATH,
) -> dict[str, Any] | None:
    """Return a route-spec mismatch for an explicit planner tool choice.

    This is intentionally advisory: callers should surface the mismatch to the
    model/operator, not rewrite the tool call.
    """
    route = load_route_script(path)
    tool_info = _route_tool_info(route).get(tool_name)
    if tool_info is None:
        return None

    map_id = _snapshot_map_id(snapshot)
    maps = route.get("maps") or {}
    combined_maps = _combined_route_maps(route)
    route_map_ids_by_key = {
        str(key): data.get("map_id")
        for key, data in combined_maps.items()
        if isinstance(data, dict) and isinstance(data.get("map_id"), int)
    }
    route_map_ids_set = {
        data.get("map_id")
        for data in maps.values()
        if isinstance(data, dict) and isinstance(data.get("map_id"), int)
    }
    allowed_keys = [
        str(key)
        for key in tool_info.get("allowed_map_keys", []) or []
    ]
    unknown_keys = sorted(key for key in allowed_keys if key not in route_map_ids_by_key)
    if unknown_keys:
        return {
            "tool": tool_name,
            "reason": "route_tool_references_unknown_map_keys",
            "unknown_map_keys": unknown_keys,
        }

    allowed_ids = [route_map_ids_by_key[key] for key in allowed_keys]
    allow_outside_route_maps = bool(tool_info.get("allow_outside_route_maps"))
    if allow_outside_route_maps and map_id and map_id not in route_map_ids_set:
        return None
    if map_id and map_id in allowed_ids:
        return None

    return {
        "tool": tool_name,
        "reason": "current_map_not_allowed_for_route_tool",
        "current_map_id": map_id,
        "allowed_map_keys": allowed_keys,
        "allowed_map_ids": allowed_ids,
        "allow_outside_route_maps": allow_outside_route_maps,
    }


def _route_tool_info(route: dict[str, Any]) -> dict[str, dict[str, Any]]:
    return {
        str(tool.get("tool")): tool
        for tool in ((route.get("planner_guidance") or {}).get("route_tools") or [])
        if isinstance(tool, dict) and tool.get("tool")
    }


def _combined_route_maps(route: dict[str, Any]) -> dict[str, Any]:
    return {
        **(route.get("maps") or {}),
        **(route.get("recovery_maps") or {}),
    }


def _snapshot_map_id(snapshot: dict | None) -> int:
    try:
        return int((((snapshot or {}).get("map") or {}).get("map_id")) or 0)
    except (TypeError, ValueError):
        return 0
