"""Category F: Two-client player-trade bridge tests."""

from __future__ import annotations

import asyncio
import json
import os

from .base import BridgeTestCase
from .base import TestFailure, assert_gt, assert_true
from .trade_harness import (
    _helper_name,
    _main_name,
    ensure_trade_helper_running,
    write_trade_helper_config,
    request_helper_send_chat,
    request_helper_send_whisper,
    _helper_status_path,
    _helper_config_path,
)
from ..farming_knowledge import MAP_NAMES
from ..gamedata import ITEM_NAMES


def _item_model_id(name: str) -> int:
    return next(model_id for model_id, item_name in ITEM_NAMES.items() if item_name == name)


def _map_id(name: str) -> int:
    return next(map_id for map_id, map_name in MAP_NAMES.items() if map_name == name)


MODEL_ID_KIT = _item_model_id("Identification Kit")
MODEL_SALVAGE_KIT = _item_model_id("Salvage Kit")
MODEL_EXPERT_SALVAGE_KIT = _item_model_id("Expert Salvage Kit")
SAFE_SINGLETON_TRADE_MODELS = (
    MODEL_ID_KIT,
    MODEL_SALVAGE_KIT,
    MODEL_EXPERT_SALVAGE_KIT,
)
TRADE_TEST_MAP = _map_id("Longeye's Ledge")
TRADE_TEST_REGION = 4
TRADE_TEST_DISTRICT = 99
TRADE_TEST_LANGUAGE = 8
_HELPER_MOVE_SEQ = 0


def _find_inventory_item_id_by_model(snap: dict, model_id: int) -> int:
    inv = snap.get("inventory", {})
    for bag in inv.get("bags", []):
        for item in bag.get("items", []):
            if item.get("model_id") == model_id and not item.get("equipped", False):
                return int(item.get("item_id", 0))
    return 0


def _find_singleton_trade_offer_candidate(snap: dict) -> tuple[int, int]:
    for model_id in SAFE_SINGLETON_TRADE_MODELS:
        item_id = _find_inventory_item_id_by_model(snap, model_id)
        if item_id <= 0:
            continue
        item_ref = _find_inventory_item_ref_by_id(snap, item_id)
        if item_ref is not None:
            return item_id, item_ref[2]

    return 0, 0


def _find_trade_offer_candidate_item_id(snap: dict) -> int:
    for model_id in SAFE_SINGLETON_TRADE_MODELS:
        item_id = _find_inventory_item_id_by_model(snap, model_id)
        if item_id > 0:
            return item_id
    inv = snap.get("inventory", {})
    for bag in inv.get("bags", []):
        for item in bag.get("items", []):
            if item.get("equipped", False):
                continue
            item_id = int(item.get("item_id", 0) or 0)
            model_id = int(item.get("model_id", 0) or 0)
            if item_id > 0 and model_id > 0:
                return item_id
    return 0


def _find_stackable_trade_offer_candidate(snap: dict) -> tuple[int, int, int]:
    inv = snap.get("inventory", {})
    for bag in inv.get("bags", []):
        for item in bag.get("items", []):
            if item.get("equipped", False):
                continue
            item_id = int(item.get("item_id", 0) or 0)
            model_id = int(item.get("model_id", 0) or 0)
            quantity = int(item.get("quantity", 0) or 0)
            if item_id > 0 and model_id > 0 and quantity >= 2:
                requested = 1 if quantity == 2 else min(2, quantity - 1)
                return item_id, model_id, requested
    return 0, 0, 0


def _find_stackable_trade_offer_candidate_for_model(snap: dict, model_id: int) -> tuple[int, int, int]:
    if model_id <= 0:
        return 0, 0, 0
    inv = snap.get("inventory", {})
    for bag in inv.get("bags", []):
        for item in bag.get("items", []):
            if item.get("equipped", False):
                continue
            item_id = int(item.get("item_id", 0) or 0)
            item_model_id = int(item.get("model_id", 0) or 0)
            quantity = int(item.get("quantity", 0) or 0)
            if item_id <= 0 or item_model_id != model_id or quantity < 2:
                continue
            requested = 1 if quantity == 2 else min(2, quantity - 1)
            return item_id, item_model_id, requested
    return 0, 0, 0


def _find_inventory_item_ref_by_id(snap: dict, item_id: int) -> tuple[int, int, int, int] | None:
    inv = snap.get("inventory", {})
    for bag in inv.get("bags", []):
        bag_index = int(bag.get("bag_index", 0) or 0)
        for item in bag.get("items", []):
            if int(item.get("item_id", 0) or 0) != item_id:
                continue
            return (
                bag_index,
                int(item.get("slot", 0) or 0),
                int(item.get("model_id", 0) or 0),
                int(item.get("quantity", 0) or 0),
            )
    return None


def _inventory_item_refs_by_model(snap: dict, model_id: int) -> list[tuple[int, int]]:
    refs: list[tuple[int, int]] = []
    inv = snap.get("inventory", {})
    for bag in inv.get("bags", []):
        for item in bag.get("items", []):
            if item.get("equipped", False):
                continue
            if int(item.get("model_id", 0) or 0) != model_id:
                continue
            refs.append((
                int(item.get("item_id", 0) or 0),
                int(item.get("quantity", 0) or 0),
            ))
    return refs


def _inventory_item_refs_debug(snap: dict, model_id: int) -> list[dict]:
    refs: list[dict] = []
    inv = snap.get("inventory", {})
    for bag in inv.get("bags", []):
        bag_index = int(bag.get("bag_index", 0) or 0)
        for item in bag.get("items", []):
            if item.get("equipped", False):
                continue
            if int(item.get("model_id", 0) or 0) != model_id:
                continue
            refs.append(
                {
                    "item_id": int(item.get("item_id", 0) or 0),
                    "quantity": int(item.get("quantity", 0) or 0),
                    "bag_index": bag_index,
                    "slot": int(item.get("slot", 0) or 0),
                }
            )
    refs.sort(key=lambda ref: (ref["bag_index"], ref["slot"], ref["item_id"]))
    return refs


def _find_received_singleton_item_id(before_snap: dict, after_snap: dict, model_id: int) -> int:
    before_ids = {item_id for item_id, _ in _inventory_item_refs_by_model(before_snap, model_id)}
    for item_id, _ in _inventory_item_refs_by_model(after_snap, model_id):
        if item_id > 0 and item_id not in before_ids:
            return item_id
    return 0


def _find_received_stack_item_id(before_snap: dict, after_snap: dict, model_id: int, received_quantity: int) -> int:
    before_quantities = {item_id: quantity for item_id, quantity in _inventory_item_refs_by_model(before_snap, model_id)}
    after_refs = _inventory_item_refs_by_model(after_snap, model_id)

    for item_id, quantity in after_refs:
        if item_id > 0 and item_id not in before_quantities and quantity >= received_quantity:
            return item_id

    for item_id, quantity in after_refs:
        delta = quantity - before_quantities.get(item_id, 0)
        if item_id > 0 and delta >= received_quantity:
            return item_id

    for item_id, quantity in after_refs:
        if item_id > 0 and quantity >= received_quantity:
            return item_id
    return 0


def _count_inventory_quantity_by_model(snap: dict, model_id: int) -> int:
    total = 0
    inv = snap.get("inventory", {})
    for bag in inv.get("bags", []):
        for item in bag.get("items", []):
            if item.get("equipped", False):
                continue
            if int(item.get("model_id", 0) or 0) != model_id:
                continue
            total += int(item.get("quantity", 0) or 0)
    return total


def _inventory_free_slots_total(snap: dict) -> int:
    return int(snap.get("inventory", {}).get("free_slots_total", 0) or 0)


def _helper_inventory_free_slots_total(helper: dict | None) -> int:
    return int((helper or {}).get("inventory_free_slots_total", 0) or 0)


def _helper_stackable_offer_candidate(helper: dict | None) -> tuple[int, int]:
    payload = helper or {}
    return (
        int(payload.get("helper_stackable_offer_model_id", 0) or 0),
        int(payload.get("helper_stackable_offer_quantity", 0) or 0),
    )


def _helper_stackable_offer_total_quantity(helper: dict | None) -> int:
    return int((helper or {}).get("helper_stackable_offer_total_quantity", 0) or 0)


def _helper_safe_singleton_offer_model(helper: dict | None) -> int:
    return int((helper or {}).get("helper_safe_singleton_offer_model_id", 0) or 0)


def _helper_safe_singleton_offer_total_quantity(helper: dict | None) -> int:
    return int((helper or {}).get("helper_safe_singleton_offer_total_quantity", 0) or 0)


def _helper_recent_chat(helper: dict | None) -> list[dict]:
    payload = helper or {}
    recent = payload.get("recent_chat", [])
    return recent if isinstance(recent, list) else []


def _chat_entry_matches(
    entry: dict,
    *,
    message: str,
    channel: str | None = None,
    sender_contains: str | None = None,
) -> bool:
    if not isinstance(entry, dict):
        return False
    actual_message = str(entry.get("message", "") or "")
    if message not in actual_message:
        return False
    if channel is not None and str(entry.get("channel", "") or "") != channel:
        return False
    if sender_contains is not None:
        actual_sender = str(entry.get("sender", "") or "")
        if sender_contains.lower() not in actual_sender.lower():
            return False
    return True


def _trade_items_cover_expected_models(items: list[dict], expected: list[tuple[int, int]]) -> bool:
    remaining = [(int(model_id), int(quantity)) for model_id, quantity in expected]
    for item in items:
        model_id = int(item.get("model_id", 0) or 0)
        quantity = int(item.get("quantity", 0) or 0)
        for idx, (expected_model_id, expected_quantity) in enumerate(remaining):
            if model_id == expected_model_id and quantity == expected_quantity:
                remaining.pop(idx)
                break
    return not remaining


def _trade_item_quantities_match(items: list[dict], expected_quantities: list[int]) -> bool:
    actual = sorted(int(item.get("quantity", 0) or 0) for item in items)
    expected = sorted(int(quantity) for quantity in expected_quantities)
    return actual == expected


async def _merge_split_stack_cleanup(tc: BridgeTestCase, model_id: int, prefer_item_id: int = 0):
    snap = await tc.wait_for_snapshot(tier=3, timeout=10.0)
    refs: list[tuple[int, int, int, int]] = []
    inv = snap.get("inventory", {})
    for bag in inv.get("bags", []):
        bag_index = int(bag.get("bag_index", 0) or 0)
        for item in bag.get("items", []):
            if item.get("equipped", False):
                continue
            if int(item.get("model_id", 0) or 0) != model_id:
                continue
            refs.append((
                int(item.get("item_id", 0) or 0),
                bag_index,
                int(item.get("slot", 0) or 0),
                int(item.get("quantity", 0) or 0),
            ))
    if len(refs) < 2:
        return

    refs.sort(key=lambda r: (r[3], r[0]))
    src = refs[0]
    dst = refs[-1]
    if prefer_item_id:
        for ref in refs:
            if ref[0] == prefer_item_id:
                dst = ref
                break
    if src[0] == dst[0]:
        return

    result = await tc.send_action(
        "move_item",
        {"item_id": src[0], "bag_id": dst[1], "slot": dst[2]},
        timeout=5.0,
    )
    tc.assert_action_success(result)
    await asyncio.sleep(1.0)


def _trade_chat_debug(snap: dict) -> list[dict]:
    out: list[dict] = []
    for entry in snap.get("chat", []) or []:
        if not isinstance(entry, dict):
            continue
        out.append(
            {
                "channel": entry.get("channel"),
                "sender": entry.get("sender"),
                "message": entry.get("message"),
                "timestamp": entry.get("timestamp"),
            }
        )
    return out


def _find_helper_agent(snap: dict) -> dict | None:
    helper_x = None
    helper_y = None
    helper_status = _helper_status_path()
    if helper_status.exists():
        try:
            payload = json.loads(helper_status.read_text(encoding="utf-8", errors="ignore"))
            helper_x = float(payload.get("x", 0.0) or 0.0)
            helper_y = float(payload.get("y", 0.0) or 0.0)
        except Exception:
            helper_x = None
            helper_y = None

    best_agent = None
    best_dist = float("inf")
    for agent in snap.get("agents", []):
        if agent.get("agent_type") != "living":
            continue
        if not agent.get("is_alive", False):
            continue
        if agent.get("name") == _helper_name():
            return agent
        if helper_x is None or helper_y is None:
            continue
        ax = float(agent.get("x", 0.0) or 0.0)
        ay = float(agent.get("y", 0.0) or 0.0)
        dist = ((ax - helper_x) ** 2 + (ay - helper_y) ** 2) ** 0.5
        if dist < best_dist:
            best_dist = dist
            best_agent = agent
    if best_agent is not None and best_dist <= 250.0:
        return best_agent
    return None



__all__ = [name for name in list(globals()) if not name.startswith('__')]
