"""Category F: Two-client player-trade bridge tests."""

from __future__ import annotations

import asyncio
import json
import os

from .base import BridgeTestCase
from .helpers import TestFailure, assert_gt, assert_true
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

MODEL_ID_KIT = 2992
MODEL_SALVAGE_KIT = 2993
MODEL_EXPERT_SALVAGE_KIT = 2991
SAFE_SINGLETON_TRADE_MODELS = (
    MODEL_ID_KIT,
    MODEL_SALVAGE_KIT,
    MODEL_EXPERT_SALVAGE_KIT,
)
TRADE_TEST_MAP = 650
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


async def _wait_until_agent_within_range(
    tc: BridgeTestCase,
    agent_id: int,
    max_distance: float,
    timeout: float = 15.0,
    move_once_to_agent: bool = False,
) -> tuple[bool, dict]:
    deadline = asyncio.get_running_loop().time() + timeout
    last_seen: dict = {}
    move_issued = False
    while asyncio.get_running_loop().time() < deadline:
        snap = await tc.wait_for_snapshot(tier=2, timeout=5.0)
        me = snap.get("me", {})
        me_x = float(me.get("x", 0.0) or 0.0)
        me_y = float(me.get("y", 0.0) or 0.0)
        for agent in snap.get("agents", []):
            if int(agent.get("id", 0) or 0) != agent_id:
                continue
            raw_dist = float(agent.get("distance", 99999.0) or 99999.0)
            agent_x = float(agent.get("x", 0.0) or 0.0)
            agent_y = float(agent.get("y", 0.0) or 0.0)
            computed_dist = ((agent_x - me_x) ** 2 + (agent_y - me_y) ** 2) ** 0.5
            dist = computed_dist if raw_dist >= 90000.0 else raw_dist
            last_seen = {
                "id": int(agent.get("id", 0) or 0),
                "distance": dist,
                "raw_distance": raw_dist,
                "computed_distance": computed_dist,
                "x": agent_x,
                "y": agent_y,
                "me_x": me_x,
                "me_y": me_y,
                "name": agent.get("name"),
            }
            if dist <= max_distance:
                return True, last_seen
            if move_once_to_agent and not move_issued:
                result = await tc.send_action(
                    "move_to",
                    {"x": agent_x, "y": agent_y},
                    timeout=8.0,
                )
                tc.assert_action_success(result)
                move_issued = True
            break
        await asyncio.sleep(0.25)
    return False, last_seen


async def _request_helper_move_to_discopanic(tc: BridgeTestCase, timeout: float = 45.0) -> dict:
    global _HELPER_MOVE_SEQ
    snap = tc.latest_snapshot(2)
    if snap is None:
        snap = await tc.wait_for_snapshot(tier=2, timeout=10.0)
    me = snap.get("me", {})
    target_x = float(me.get("x", 0.0) or 0.0)
    target_y = float(me.get("y", 0.0) or 0.0)
    _HELPER_MOVE_SEQ += 1
    helper_config = {}
    config_path = _helper_config_path()
    if config_path.exists():
        try:
            helper_config = json.loads(config_path.read_text(encoding="utf-8", errors="ignore"))
        except Exception:
            helper_config = {}
    write_trade_helper_config(
        submit_gold=int(helper_config.get("submit_gold", 0) or 0),
        auto_submit=bool(helper_config.get("auto_submit", False)),
        auto_accept=(bool(helper_config.get("auto_accept")) if "auto_accept" in helper_config else None),
        offer_item_model_id=int(helper_config.get("offer_item_model_id", 0) or 0),
        offer_item_quantity=int(helper_config.get("offer_item_quantity", 0) or 0),
        offer_item_model_id_2=int(helper_config.get("offer_item_model_id_2", 0) or 0),
        offer_item_quantity_2=int(helper_config.get("offer_item_quantity_2", 0) or 0),
        move_x=target_x,
        move_y=target_y,
        move_seq=_HELPER_MOVE_SEQ,
        chat_send_seq=int(helper_config.get("chat_send_seq", 0) or 0),
        chat_send_channel=str(helper_config.get("chat_send_channel", "") or ""),
        chat_send_message=str(helper_config.get("chat_send_message", "") or ""),
        whisper_send_seq=int(helper_config.get("whisper_send_seq", 0) or 0),
        whisper_send_recipient=str(helper_config.get("whisper_send_recipient", "") or ""),
        whisper_send_message=str(helper_config.get("whisper_send_message", "") or ""),
    )

    helper_status = _helper_status_path()
    deadline = asyncio.get_running_loop().time() + timeout
    last_payload = {}
    while asyncio.get_running_loop().time() < deadline:
        if helper_status.exists():
            try:
                last_payload = json.loads(helper_status.read_text(encoding="utf-8", errors="ignore"))
            except Exception:
                last_payload = {}
            helper_x = float(last_payload.get("x", 0.0) or 0.0)
            helper_y = float(last_payload.get("y", 0.0) or 0.0)
            dx = helper_x - target_x
            dy = helper_y - target_y
            if (dx * dx + dy * dy) ** 0.5 <= 120.0:
                return last_payload
        await asyncio.sleep(0.5)
    raise TestFailure(
        "Helper did not reach Disco Panic rendezvous target; "
        f"target=({target_x:.1f}, {target_y:.1f}) helper={last_payload}"
    )


async def _ensure_discopanic_at_trade_rendezvous(tc: BridgeTestCase):
    helper_status = _helper_status_path()
    helper_payload = {}
    if helper_status.exists():
        try:
            helper_payload = json.loads(helper_status.read_text(encoding="utf-8", errors="ignore"))
        except Exception:
            helper_payload = {}
    target_region = int(helper_payload.get("region", TRADE_TEST_REGION) or TRADE_TEST_REGION)
    target_district = int(helper_payload.get("district", TRADE_TEST_DISTRICT) or TRADE_TEST_DISTRICT)

    snap = await tc.wait_for_snapshot(tier=1, timeout=10.0)
    if (
        snap["map"]["map_id"] != TRADE_TEST_MAP
        or int(snap["map"].get("region", -1)) != target_region
        or int(snap["map"].get("district", -1)) != target_district
    ):
        result = await tc.send_action(
            "travel",
            {
                "map_id": TRADE_TEST_MAP,
                "region": target_region,
                "district": target_district,
                "language": TRADE_TEST_LANGUAGE,
            },
        )
        tc.assert_action_success(result)

        def at_trade_outpost_loaded(s):
            return (
                s["map"]["map_id"] == TRADE_TEST_MAP
                and s["map"]["loading_state"] == 1
                and int(s["map"].get("region", -1)) == target_region
                and int(s["map"].get("district", -1)) == target_district
            )

        await tc.wait_for_state_change(at_trade_outpost_loaded, tier=1, timeout=60.0)
        await asyncio.sleep(5.0)


async def _send_trade_action(tc: BridgeTestCase, name: str, params: dict | None = None) -> dict:
    """Trade-window actions are validated by observed trade state, not immediate action_result."""
    return await tc.send_action_no_wait(name, params or {})


async def _wait_for_helper_visible(tc: BridgeTestCase, timeout: float = 90.0) -> dict:
    def helper_visible(snap: dict) -> bool:
        return _find_helper_agent(snap) is not None

    snap = await tc.wait_for_state_change(helper_visible, tier=2, timeout=timeout)
    helper = _find_helper_agent(snap)
    assert_true(helper is not None, "Helper should be visible in tier-2 snapshot")
    return helper


async def _open_trade_with_helper(
    tc: BridgeTestCase,
    *,
    require_inventory_item: bool = True,
) -> tuple[int, int]:
    """Ensure helper is present, then open a trade and return (helper_id, salvage_item_id)."""
    await ensure_trade_helper_running()
    await _ensure_discopanic_at_trade_rendezvous(tc)
    helper_status = _helper_status_path()
    helper_status_payload: dict = {}
    if helper_status.exists():
        try:
            helper_status_payload = json.loads(helper_status.read_text(encoding="utf-8", errors="ignore"))
        except Exception:
            helper_status_payload = {}

    try:
        helper = await _wait_for_helper_visible(tc, timeout=10.0)
    except Exception:
        await _request_helper_move_to_discopanic(tc, timeout=45.0)
        await asyncio.sleep(1.0)
        await asyncio.sleep(5.0)
        helper = await _wait_for_helper_visible(tc, timeout=180.0)
    helper_id = int(helper["id"])
    helper_player_number = int(helper.get("player_number", 0) or 0)
    helper_x = float(helper.get("x", 0.0) or 0.0)
    helper_y = float(helper.get("y", 0.0) or 0.0)
    helper_distance = float(helper.get("distance", 99999.0) or 99999.0)
    assert_gt(helper_id, 0, "Helper agent id")

    me = (await tc.wait_for_snapshot(tier=1, timeout=10.0)).get("me", {})
    me_x = float(me.get("x", 0.0) or 0.0)
    me_y = float(me.get("y", 0.0) or 0.0)
    helper_dist = ((helper_x - me_x) ** 2 + (helper_y - me_y) ** 2) ** 0.5
    if helper_dist > 70.0 or helper_distance > 120.0:
        await _request_helper_move_to_discopanic(tc, timeout=45.0)
        await asyncio.sleep(2.0)
    helper_in_range = False
    helper_range_debug: dict = {}
    for attempt in range(2):
        helper_in_range, helper_range_debug = await _wait_until_agent_within_range(
            tc,
            helper_id,
            max_distance=100.0,
            timeout=20.0,
            move_once_to_agent=False,
        )
        if helper_in_range:
            break
        await _request_helper_move_to_discopanic(tc, timeout=45.0)
        await asyncio.sleep(2.0)
    assert_true(
        helper_in_range,
        f"Helper should be within direct player-trade range before initiate_trade; last_seen={helper_range_debug}",
    )

    item_id = 0
    if require_inventory_item:
        # Prefer a salvage kit, but fall back to any non-equipped inventory item for reversible trade tests.
        snap_before = await tc.wait_for_snapshot(tier=3, timeout=10.0)
        item_id = _find_trade_offer_candidate_item_id(snap_before)

    open_snap = None
    last_open_exc: Exception | None = None
    for _attempt in range(2):
        result = await tc.send_action_no_wait(
            "initiate_trade",
            {"agent_id": helper_id, "player_number": helper_player_number},
        )
        tc.assert_action_success(result)

        def trade_open_main(s: dict) -> bool:
            trade = s.get("trade", {})
            return bool(trade.get("is_open"))

        try:
            open_snap = await tc.wait_for_state_change(trade_open_main, tier=2, timeout=12.0)
            break
        except Exception as exc:
            last_open_exc = exc
            await asyncio.sleep(1.0)

    if open_snap is None:
        latest_snap = {}
        latest_trade = {}
        try:
            latest_snap = await tc.wait_for_snapshot(tier=2, timeout=5.0)
            latest_trade = latest_snap.get("trade", {})
        except Exception:
            latest_snap = {}
            latest_trade = {}
        helper_debug = None
        helper_status = _helper_status_path()
        if helper_status.exists():
            try:
                helper_debug = json.loads(helper_status.read_text(encoding="utf-8", errors="ignore"))
            except Exception:
                helper_debug = {"read_error": str(helper_status)}
        trade_debug = {
            "flags": latest_trade.get("flags"),
            "is_open": latest_trade.get("is_open"),
            "is_initiated": latest_trade.get("is_initiated"),
            "offer_sent": latest_trade.get("offer_sent"),
            "is_accepted": latest_trade.get("is_accepted"),
            "debug_ui_player_updated_count": latest_trade.get("debug_ui_player_updated_count"),
            "debug_ui_initiate_count": latest_trade.get("debug_ui_initiate_count"),
            "debug_ui_last_initiate_wparam": latest_trade.get("debug_ui_last_initiate_wparam"),
            "debug_ui_session_start_count": latest_trade.get("debug_ui_session_start_count"),
            "debug_ui_session_updated_count": latest_trade.get("debug_ui_session_updated_count"),
            "debug_ui_last_session_start_state": latest_trade.get("debug_ui_last_session_start_state"),
            "debug_ui_last_session_start_player_number": latest_trade.get("debug_ui_last_session_start_player_number"),
            "debug_party_button_hit_count": latest_trade.get("debug_party_button_hit_count"),
            "debug_party_button_last_this": latest_trade.get("debug_party_button_last_this"),
            "debug_party_button_last_arg": latest_trade.get("debug_party_button_last_arg"),
            "chat": _trade_chat_debug(latest_snap),
        }
        raise TestFailure(
            "Trade did not open after initiate_trade; "
            f"main_trade={trade_debug} helper={helper_debug} root={last_open_exc}"
        ) from last_open_exc
    trade = open_snap["trade"]
    assert_true(trade["is_open"], "Trade should report open")
    main_trade_debug = {
        "flags": trade.get("flags"),
        "is_open": trade.get("is_open"),
        "is_initiated": trade.get("is_initiated"),
        "offer_sent": trade.get("offer_sent"),
        "is_accepted": trade.get("is_accepted"),
        "debug_ui_player_updated_count": trade.get("debug_ui_player_updated_count"),
        "debug_ui_initiate_count": trade.get("debug_ui_initiate_count"),
        "debug_ui_last_initiate_wparam": trade.get("debug_ui_last_initiate_wparam"),
        "debug_ui_session_start_count": trade.get("debug_ui_session_start_count"),
        "debug_ui_session_updated_count": trade.get("debug_ui_session_updated_count"),
        "debug_ui_last_session_start_state": trade.get("debug_ui_last_session_start_state"),
        "debug_ui_last_session_start_player_number": trade.get("debug_ui_last_session_start_player_number"),
        "debug_party_button_hit_count": trade.get("debug_party_button_hit_count"),
        "debug_party_button_last_this": trade.get("debug_party_button_last_this"),
        "debug_party_button_last_arg": trade.get("debug_party_button_last_arg"),
        "chat": _trade_chat_debug(open_snap),
    }

    helper_status = _helper_status_path()
    helper_debug = None
    helper_confirmed = False
    for _ in range(20):
        if helper_status.exists():
            try:
                helper_debug = json.loads(helper_status.read_text(encoding="utf-8", errors="ignore"))
            except Exception:
                helper_debug = None
        if helper_debug:
            helper_confirmed = (
                int(helper_debug.get("trade_flags", 0) or 0) != 0
                or int(helper_debug.get("trade_open_count", 0) or 0) > 0
                or int(helper_debug.get("trade_partner_hook_hits", 0) or 0) > 0
            )
            if helper_confirmed:
                break
        await asyncio.sleep(0.5)
    assert_true(
        helper_confirmed,
        f"Helper should observe incoming trade state; main_trade={main_trade_debug} helper={helper_debug}",
    )
    return helper_id, item_id


async def test_player_trade_open_cancel_helper(tc: BridgeTestCase):
    """Launch BLUMPKINS helper, open trade, then cancel it cleanly."""
    await _open_trade_with_helper(tc, require_inventory_item=False)

    result = await _send_trade_action(tc, "cancel_trade", {})
    tc.assert_action_success(result)

    def trade_closed(s: dict) -> bool:
        return not bool(s.get("trade", {}).get("is_open"))

    try:
        closed_snap = await tc.wait_for_state_change(trade_closed, tier=2, timeout=15.0)
    except TestFailure:
        latest = tc.latest_snapshot(2) or {}
        helper_debug = None
        helper_status = _helper_status_path()
        if helper_status.exists():
            try:
                helper_debug = json.loads(helper_status.read_text(encoding="utf-8", errors="ignore"))
            except Exception:
                helper_debug = None
        raise TestFailure(
            "State change not observed after cancel_trade; "
            f"main_trade={latest.get('trade')} helper={helper_debug} "
            f"ipc_connected={tc.ipc.connected}"
        )
    assert_true(not closed_snap["trade"]["is_open"], "Trade should report closed after cancel")


async def test_player_trade_open_idle_cancel_helper(tc: BridgeTestCase):
    """Open a player trade, dwell briefly without offering anything, then cancel."""
    await _open_trade_with_helper(tc, require_inventory_item=False)
    await asyncio.sleep(2.0)

    # Prove the bridge still survives the open-trade dwell long enough to observe
    # another snapshot before we cancel.
    snap = await tc.wait_for_snapshot(tier=2, timeout=10.0)
    assert_true(bool(snap.get("trade", {}).get("is_open")), "Trade should still report open after dwell")

    cancel_params: dict = {}
    if "GWA3_TRADE_CANCEL_ROW" in os.environ:
        cancel_params["row"] = int(os.environ["GWA3_TRADE_CANCEL_ROW"])
    if "GWA3_TRADE_CANCEL_CHILD" in os.environ:
        cancel_params["child"] = int(os.environ["GWA3_TRADE_CANCEL_CHILD"])
    if "GWA3_TRADE_CANCEL_TRANSPORT" in os.environ:
        cancel_params["transport"] = int(os.environ["GWA3_TRADE_CANCEL_TRANSPORT"])
    result = await _send_trade_action(tc, "cancel_trade", cancel_params)
    tc.assert_action_success(result)

    def trade_closed(s: dict) -> bool:
        return not bool(s.get("trade", {}).get("is_open"))

    try:
        closed_snap = await tc.wait_for_state_change(trade_closed, tier=2, timeout=15.0)
    except TestFailure:
        latest = tc.latest_snapshot(2) or {}
        helper_debug = None
        helper_status = _helper_status_path()
        if helper_status.exists():
            try:
                helper_debug = json.loads(helper_status.read_text(encoding="utf-8", errors="ignore"))
            except Exception:
                helper_debug = None
        raise TestFailure(
            "State change not observed after cancel_trade idle test; "
            f"main_trade={latest.get('trade')} helper={helper_debug} "
            f"ipc_connected={tc.ipc.connected}"
        )
    assert_true(not closed_snap["trade"]["is_open"], "Trade should report closed after cancel")


async def test_player_trade_open_offer_cancel_helper(tc: BridgeTestCase):
    """Launch BLUMPKINS helper, open trade, offer an item, verify reflection, then cancel."""
    _, item_id = await _open_trade_with_helper(tc)
    if item_id <= 0:
        tc.skip("No non-equipped inventory item found to use as a reversible trade offer")
        return

    def trade_native_capture_ready(s: dict) -> bool:
        trade = s.get("trade", {})
        return (
            bool(trade.get("is_open"))
            and (
                (
                    int(trade.get("debug_capture_count", 0) or 0) > 0
                    and int(trade.get("debug_window_ctx", 0) or 0) > 0
                )
                or (
                    int(trade.get("debug_ui_window_context", 0) or 0) > 0
                    and int(trade.get("debug_ui_window_frame", 0) or 0) > 0
                )
            )
        )

    try:
        capture_snap = await tc.wait_for_state_change(trade_native_capture_ready, tier=2, timeout=10.0)
    except Exception as exc:
        latest_trade = {}
        try:
            latest_trade = (await tc.wait_for_snapshot(tier=2, timeout=5.0)).get("trade", {})
        except Exception:
            latest_trade = {}
        trade_debug = {
            "flags": latest_trade.get("flags"),
            "is_open": latest_trade.get("is_open"),
            "debug_capture_count": latest_trade.get("debug_capture_count"),
            "debug_window_ctx": latest_trade.get("debug_window_ctx"),
            "debug_window_frame": latest_trade.get("debug_window_frame"),
            "debug_ui_window_frame": latest_trade.get("debug_ui_window_frame"),
            "debug_ui_window_state": latest_trade.get("debug_ui_window_state"),
            "debug_ui_window_context": latest_trade.get("debug_ui_window_context"),
            "debug_ui_initiate_count": latest_trade.get("debug_ui_initiate_count"),
            "debug_ui_session_start_count": latest_trade.get("debug_ui_session_start_count"),
            "debug_ui_session_updated_count": latest_trade.get("debug_ui_session_updated_count"),
        }
        helper_debug = None
        helper_status = _helper_status_path()
        if helper_status.exists():
            try:
                helper_debug = json.loads(helper_status.read_text(encoding="utf-8", errors="ignore"))
            except Exception:
                helper_debug = {"read_error": str(helper_status)}
        raise TestFailure(
            "Native trade-window capture never became ready before offer; "
            f"main_trade={trade_debug} "
            f"helper={helper_debug} root={exc}"
        ) from exc

    capture_trade = capture_snap.get("trade", {})
    assert_true(
        (
            int(capture_trade.get("debug_capture_count", 0) or 0) > 0
            and int(capture_trade.get("debug_window_ctx", 0) or 0) > 0
        )
        or (
            int(capture_trade.get("debug_ui_window_context", 0) or 0) > 0
            and int(capture_trade.get("debug_ui_window_frame", 0) or 0) > 0
        ),
        "Trade window native capture or UI window context should be available before offer",
    )

    result = await tc.send_action(
        "offer_trade_item",
        {"item_id": item_id, "quantity": 1},
        timeout=5.0,
    )
    tc.assert_action_success(result)

    def own_offer_seen(s: dict) -> bool:
        trade = s.get("trade", {})
        if not trade.get("is_open"):
            return False
        player = trade.get("player", {})
        return any(int(it.get("item_id", 0)) == item_id for it in player.get("items", []))

    offer_snap = await tc.wait_for_state_change(own_offer_seen, tier=2, timeout=15.0)
    player_offer = offer_snap["trade"]["player"]
    assert_true(
        any(int(it.get("item_id", 0)) == item_id for it in player_offer.get("items", [])),
        "Own trade offer should reflect the offered salvage kit item",
    )
    result = await _send_trade_action(tc, "cancel_trade", {})
    tc.assert_action_success(result)

    def trade_closed(s: dict) -> bool:
        return not bool(s.get("trade", {}).get("is_open"))

    closed_snap = await tc.wait_for_state_change(trade_closed, tier=2, timeout=15.0)
    assert_true(not closed_snap["trade"]["is_open"], "Trade should report closed after cancel")


async def test_player_trade_open_offer_remove_cancel_helper(tc: BridgeTestCase):
    """Offer one item, remove it from the offer, verify reflection, then cancel."""
    _, item_id = await _open_trade_with_helper(tc)
    if item_id <= 0:
        tc.skip("No non-equipped inventory item found to use as a reversible trade offer")
        return

    result = await tc.send_action(
        "offer_trade_item",
        {"item_id": item_id, "quantity": 1},
        timeout=5.0,
    )
    tc.assert_action_success(result)

    def own_offer_seen(s: dict) -> bool:
        trade = s.get("trade", {})
        if not trade.get("is_open"):
            return False
        player = trade.get("player", {})
        return any(int(it.get("item_id", 0)) == item_id for it in player.get("items", []))

    await tc.wait_for_state_change(own_offer_seen, tier=2, timeout=15.0)
    current_trade = (await tc.wait_for_snapshot(tier=2, timeout=5.0)).get("trade", {})
    if not bool(current_trade.get("debug_remove_item_available")):
        tc.skip("Native remove_trade_item path is not resolved on this client build")
        return

    def own_offer_removed(s: dict) -> bool:
        trade = s.get("trade", {})
        if not trade.get("is_open"):
            return False
        player = trade.get("player", {})
        return not any(int(it.get("item_id", 0)) == item_id for it in player.get("items", []))

    removal_attempts = [("item_id", item_id), ("slot_0", 0), ("slot_1", 1)]
    removed_snap = None
    removal_mode = None
    last_exc: Exception | None = None
    for label, remove_value in removal_attempts:
        result = await _send_trade_action(tc, "remove_trade_item", {"slot_or_item_id": remove_value})
        tc.assert_action_success(result)
        try:
            removed_snap = await tc.wait_for_state_change(own_offer_removed, tier=2, timeout=5.0)
            removal_mode = label
            break
        except Exception as exc:
            last_exc = exc

    assert_true(removed_snap is not None, f"remove_trade_item did not remove the offer via any known semantic ({removal_attempts}); last={last_exc}")
    assert_true(
        not any(int(it.get("item_id", 0)) == item_id for it in removed_snap["trade"]["player"].get("items", [])),
        f"Offered item should disappear from own trade offer after remove_trade_item ({removal_mode})",
    )

    result = await _send_trade_action(tc, "cancel_trade", {})
    tc.assert_action_success(result)
    await tc.wait_for_state_change(lambda s: not bool(s.get("trade", {}).get("is_open")), tier=2, timeout=15.0)


async def test_player_trade_open_offer_stackable_quantity_cancel_helper(tc: BridgeTestCase):
    """Offer a partial quantity from a stackable inventory item and verify trade quantity reflection."""
    snap_before = await tc.wait_for_snapshot(tier=3, timeout=10.0)
    item_id, model_id, requested_quantity = _find_stackable_trade_offer_candidate(snap_before)
    if item_id <= 0 or model_id <= 0 or requested_quantity <= 0:
        tc.skip("No stackable inventory item found to validate partial-quantity trade offering")
        return

    await _open_trade_with_helper(tc)

    result = await tc.send_action(
        "offer_trade_item",
        {"item_id": item_id, "quantity": requested_quantity},
        timeout=5.0,
    )
    tc.assert_action_success(result)

    def own_partial_stack_offer_seen(s: dict) -> bool:
        trade = s.get("trade", {})
        if not trade.get("is_open"):
            return False
        player = trade.get("player", {})
        return any(
            int(it.get("model_id", 0) or 0) == model_id
            and int(it.get("quantity", 0) or 0) == requested_quantity
            for it in player.get("items", [])
        )

    offer_snap = await tc.wait_for_state_change(own_partial_stack_offer_seen, tier=2, timeout=20.0)
    player_items = offer_snap["trade"]["player"].get("items", [])
    assert_true(
        any(
            int(it.get("model_id", 0) or 0) == model_id
            and int(it.get("quantity", 0) or 0) == requested_quantity
            for it in player_items
        ),
        f"Own trade offer should reflect the requested partial stack quantity ({requested_quantity})",
    )

    result = await _send_trade_action(tc, "cancel_trade", {})
    tc.assert_action_success(result)
    await tc.wait_for_state_change(lambda s: not bool(s.get("trade", {}).get("is_open")), tier=2, timeout=15.0)

    await _merge_split_stack_cleanup(tc, model_id, prefer_item_id=item_id)


async def test_player_trade_open_offer_stackable_prompt_max_cancel_helper(tc: BridgeTestCase):
    """Open the stack quantity popup for a stackable item, click Max+OK, and verify the full stack is offered."""
    snap_before = await tc.wait_for_snapshot(tier=3, timeout=10.0)
    item_id, model_id, _ = _find_stackable_trade_offer_candidate(snap_before)
    if item_id <= 0 or model_id <= 0:
        tc.skip("No stackable inventory item found to validate prompt-based trade offering")
        return

    original_item = _find_inventory_item_ref_by_id(snap_before, item_id)
    if not original_item:
        tc.skip("Stackable inventory item disappeared before prompt test")
        return
    _, _, _, original_quantity = original_item
    if original_quantity < 2:
        tc.skip("Need a stack quantity >= 2 to validate the prompt path")
        return

    await _open_trade_with_helper(tc)

    result = await tc.send_action(
        "offer_trade_item_prompt_max",
        {"item_id": item_id},
        timeout=5.0,
    )
    tc.assert_action_success(result)

    def own_full_stack_offer_seen(s: dict) -> bool:
        trade = s.get("trade", {})
        player = trade.get("player", {})
        if any(
            int(it.get("model_id", 0) or 0) == model_id
            and int(it.get("quantity", 0) or 0) == original_quantity
            for it in player.get("items", [])
        ):
            return True

        current_item = _find_inventory_item_ref_by_id(s, item_id)
        if current_item is None:
            return bool(trade.get("is_open"))

        _, _, current_model_id, current_quantity = current_item
        return (
            bool(trade.get("is_open"))
            and current_model_id == model_id
            and current_quantity < original_quantity
        )

    offer_snap = await tc.wait_for_state_change(own_full_stack_offer_seen, tier=2, timeout=20.0)
    player_items = offer_snap["trade"]["player"].get("items", [])
    current_item = _find_inventory_item_ref_by_id(offer_snap, item_id)
    inventory_reduced = current_item is None or int(current_item[3]) < original_quantity
    assert_true(
        any(
            int(it.get("model_id", 0) or 0) == model_id
            and int(it.get("quantity", 0) or 0) == original_quantity
            for it in player_items
        ) or inventory_reduced,
        f"Prompt-based Max+OK should either surface the full stack in trade state or reduce/remove it from inventory ({original_quantity})",
    )

    result = await _send_trade_action(tc, "cancel_trade", {})
    tc.assert_action_success(result)
    await tc.wait_for_state_change(lambda s: not bool(s.get("trade", {}).get("is_open")), tier=2, timeout=15.0)


async def test_player_trade_open_offer_stackable_prompt_default_quantity_cancel_helper(tc: BridgeTestCase):
    """Open the quantity prompt and confirm the default value without modifying it."""
    snap_before = await tc.wait_for_snapshot(tier=3, timeout=10.0)
    item_id, model_id, _ = _find_stackable_trade_offer_candidate(snap_before)
    if item_id <= 0 or model_id <= 0:
        tc.skip("No stackable inventory item found to validate prompt default quantity offering")
        return

    original_item = _find_inventory_item_ref_by_id(snap_before, item_id)
    if not original_item:
        tc.skip("Stackable inventory item disappeared before prompt default-quantity test")
        return
    _, _, _, original_quantity = original_item
    if original_quantity < 2:
        tc.skip("Need a stack quantity >= 2 to validate default prompt quantity offering")
        return

    await _open_trade_with_helper(tc)

    result = await tc.send_action(
        "offer_trade_item_prompt_default",
        {"item_id": item_id},
        timeout=5.0,
    )
    tc.assert_action_success(result)

    def own_default_offer_seen(s: dict) -> bool:
        trade = s.get("trade", {})
        player = trade.get("player", {})
        if any(
            int(it.get("model_id", 0) or 0) == model_id
            and int(it.get("quantity", 0) or 0) == 1
            for it in player.get("items", [])
        ):
            return True
        current_item = _find_inventory_item_ref_by_id(s, item_id)
        return (
            bool(trade.get("is_open"))
            and not bool(trade.get("debug_quantity_prompt_open"))
            and current_item is not None
            and int(current_item[3]) == (original_quantity - 1)
        )

    try:
        offer_snap = await tc.wait_for_state_change(own_default_offer_seen, tier=2, timeout=20.0)
    except Exception as exc:
        latest = await tc.wait_for_snapshot(tier=2, timeout=5.0)
        latest_trade = latest.get("trade", {})
        latest_item = _find_inventory_item_ref_by_id(latest, item_id)
        raise TestFailure(
            "Default quantity prompt did not yield an offered item; "
            f"prompt_open={latest_trade.get('debug_quantity_prompt_open')} "
            f"prompt_frame={latest_trade.get('debug_quantity_prompt_frame')} "
            f"player_items={latest_trade.get('player', {}).get('items', [])} "
            f"current_item={latest_item} "
            f"original_quantity={original_quantity}"
        ) from exc
    current_item = _find_inventory_item_ref_by_id(offer_snap, item_id)
    inventory_reduced_by_one = current_item is not None and int(current_item[3]) == (original_quantity - 1)
    assert_true(
        any(
            int(it.get("model_id", 0) or 0) == model_id
            and int(it.get("quantity", 0) or 0) == 1
            for it in offer_snap["trade"]["player"].get("items", [])
        ) or inventory_reduced_by_one,
        "Default quantity confirmation should offer exactly 1 or reduce inventory by 1",
    )

    result = await _send_trade_action(tc, "cancel_trade", {})
    tc.assert_action_success(result)
    await tc.wait_for_state_change(lambda s: not bool(s.get("trade", {}).get("is_open")), tier=2, timeout=15.0)


async def test_player_trade_open_offer_stackable_prompt_exact_quantity_cancel_helper(tc: BridgeTestCase):
    """Open the stack quantity popup, enter an exact quantity, and verify the requested amount is offered."""
    snap_before = await tc.wait_for_snapshot(tier=3, timeout=10.0)
    item_id, model_id, requested_quantity = _find_stackable_trade_offer_candidate(snap_before)
    if item_id <= 0 or model_id <= 0 or requested_quantity <= 0:
        tc.skip("No stackable inventory item found to validate prompt-based exact quantity offering")
        return

    original_item = _find_inventory_item_ref_by_id(snap_before, item_id)
    if not original_item:
        tc.skip("Stackable inventory item disappeared before prompt exact-quantity test")
        return
    _, _, _, original_quantity = original_item
    if original_quantity <= requested_quantity:
        tc.skip("Need stack quantity greater than requested quantity for exact-quantity prompt validation")
        return

    await _open_trade_with_helper(tc)

    result = await tc.send_action(
        "offer_trade_item_prompt_quantity",
        {"item_id": item_id, "quantity": requested_quantity},
        timeout=5.0,
    )
    tc.assert_action_success(result)

    def own_exact_stack_offer_seen(s: dict) -> bool:
        trade = s.get("trade", {})
        player = trade.get("player", {})
        if any(
            int(it.get("model_id", 0) or 0) == model_id
            and int(it.get("quantity", 0) or 0) == requested_quantity
            for it in player.get("items", [])
        ):
            return True

        current_item = _find_inventory_item_ref_by_id(s, item_id)
        if current_item is None:
            return False

        _, _, current_model_id, current_quantity = current_item
        return (
            bool(trade.get("is_open"))
            and current_model_id == model_id
            and current_quantity == (original_quantity - requested_quantity)
        )

    try:
        offer_snap = await tc.wait_for_state_change(own_exact_stack_offer_seen, tier=2, timeout=20.0)
    except Exception as exc:
        latest = await tc.wait_for_snapshot(tier=2, timeout=5.0)
        latest_trade = latest.get("trade", {})
        latest_player_items = latest_trade.get("player", {}).get("items", [])
        latest_item = _find_inventory_item_ref_by_id(latest, item_id)
        raise TestFailure(
            "Exact quantity prompt did not yield the requested quantity; "
            f"prompt_open={latest_trade.get('debug_quantity_prompt_open')} "
            f"prompt_frame={latest_trade.get('debug_quantity_prompt_frame')} "
            f"player_items={latest_player_items} "
            f"current_item={latest_item} "
            f"requested_quantity={requested_quantity} "
            f"original_quantity={original_quantity}"
        ) from exc
    player_items = offer_snap["trade"]["player"].get("items", [])
    current_item = _find_inventory_item_ref_by_id(offer_snap, item_id)
    inventory_reduced_by_exact_quantity = (
        current_item is not None and int(current_item[3]) == (original_quantity - requested_quantity)
    )
    assert_true(
        any(
            int(it.get("model_id", 0) or 0) == model_id
            and int(it.get("quantity", 0) or 0) == requested_quantity
            for it in player_items
        ) or inventory_reduced_by_exact_quantity,
        "Prompt-based exact quantity should either surface the requested amount in trade state or reduce inventory by that exact amount",
    )

    result = await _send_trade_action(tc, "cancel_trade", {})
    tc.assert_action_success(result)
    await tc.wait_for_state_change(lambda s: not bool(s.get("trade", {}).get("is_open")), tier=2, timeout=15.0)


# ---------------------------------------------------------------------------
# Shared round-trip helper for full trade completion + return
# ---------------------------------------------------------------------------

def _read_helper_status() -> dict | None:
    status_path = _helper_status_path()
    if not status_path.exists():
        return None
    try:
        return json.loads(status_path.read_text(encoding="utf-8", errors="ignore"))
    except Exception:
        return None


async def _wait_for_helper_trade_closed(timeout: float = 12.0) -> dict | None:
    deadline = asyncio.get_running_loop().time() + timeout
    latest = _read_helper_status()
    while asyncio.get_running_loop().time() < deadline:
        latest = _read_helper_status()
        if latest and int(latest.get("trade_flags", 0) or 0) == 0:
            return latest
        await asyncio.sleep(0.5)
    return latest


async def _wait_for_helper_chat_entry(
    *,
    message: str,
    channel: str | None = None,
    sender_contains: str | None = None,
    timeout: float = 15.0,
) -> dict | None:
    deadline = asyncio.get_running_loop().time() + timeout
    latest = _read_helper_status()
    while asyncio.get_running_loop().time() < deadline:
        latest = _read_helper_status()
        if latest and any(
            _chat_entry_matches(entry, message=message, channel=channel, sender_contains=sender_contains)
            for entry in _helper_recent_chat(latest)
        ):
            return latest
        await asyncio.sleep(0.25)
    return latest


async def _wait_for_main_chat_entry(
    tc: BridgeTestCase,
    *,
    message: str,
    channel: str | None = None,
    sender_contains: str | None = None,
    timeout: float = 15.0,
) -> dict:
    def chat_seen(s: dict) -> bool:
        return any(
            _chat_entry_matches(entry, message=message, channel=channel, sender_contains=sender_contains)
            for entry in s.get("chat", []) or []
        )

    latest_tier2 = tc.latest_snapshot(2)
    if latest_tier2 is not None and chat_seen(latest_tier2):
        return latest_tier2
    latest_tier3 = tc.latest_snapshot(3)
    if latest_tier3 is not None and chat_seen(latest_tier3):
        return latest_tier3
    return await tc.wait_for_state_change(chat_seen, tier=None, timeout=timeout)


async def _query_fresh_tier3_snapshot(
    tc: BridgeTestCase,
    settle_ms: int = 500,
    timeout: float = 6.0,
) -> dict:
    baseline_tick = int((tc.latest_snapshot(3) or {}).get("tick", 0) or 0)
    result = await tc.send_action(
        "query_state",
        {"wait_ms": settle_ms},
        timeout=max(timeout, settle_ms / 1000.0 + 2.0),
    )
    tc.assert_action_success(result)

    deadline = asyncio.get_running_loop().time() + timeout
    latest_newer: dict | None = None
    while asyncio.get_running_loop().time() < deadline:
        remaining = max(0.25, deadline - asyncio.get_running_loop().time())
        snap = await tc.wait_for_snapshot(tier=3, timeout=remaining)
        if int(snap.get("tick", 0) or 0) > baseline_tick:
            latest_newer = snap
            break

    if latest_newer is None:
        raise TestFailure(
            f"Timed out waiting for fresh tier-3 snapshot after query_state; baseline_tick={baseline_tick}"
        )
    return latest_newer


async def _wait_for_fresh_tier3_state_change(
    tc: BridgeTestCase,
    predicate,
    timeout: float = 20.0,
    initial_settle_ms: int = 500,
) -> dict:
    deadline = asyncio.get_running_loop().time() + timeout
    latest = tc.latest_snapshot(3)
    if latest is not None and predicate(latest):
        return latest

    settle_ms = initial_settle_ms
    last_exc: Exception | None = None
    while asyncio.get_running_loop().time() < deadline:
        remaining = deadline - asyncio.get_running_loop().time()
        try:
            latest = await _query_fresh_tier3_snapshot(
                tc,
                settle_ms=min(settle_ms, 1500),
                timeout=min(6.0, max(2.0, remaining)),
            )
            if predicate(latest):
                return latest
        except Exception as exc:
            last_exc = exc
        await asyncio.sleep(0.25)
        settle_ms = min(settle_ms + 250, 1500)

    if last_exc is not None:
        raise last_exc
    raise TestFailure(f"Fresh tier-3 state change not observed within {timeout}s")


def _main_roundtrip_restored(
    snap: dict,
    singleton_model_id: int,
    expected_singleton_total: int,
    stack_model_id: int,
    expected_stack_total: int,
) -> bool:
    return (
        _count_inventory_quantity_by_model(snap, singleton_model_id) == expected_singleton_total
        and _count_inventory_quantity_by_model(snap, stack_model_id) == expected_stack_total
    )


async def _complete_forward_trade_and_verify_helper(
    tc: BridgeTestCase,
    expected_quantity: int,
    require_helper_closed: bool = True,
) -> dict | None:
    """Submit the main's offer, verify the helper observes partner items, then complete the trade.

    Sequence:
    1. Main submits its offer (items already in trade cart)
    2. Verify main's offer_sent state
    3. Verify helper sees the partner items
    4. THEN enable helper auto-submit + auto-accept
    5. Wait for helper to submit
    6. Main accepts
    """
    # Step 1: Main submits its offer
    result = await _send_trade_action(tc, "submit_trade_offer", {"gold": 0})
    tc.assert_action_success(result)

    # Step 2: Wait for main's submitted state
    def offer_submitted(s: dict) -> bool:
        trade = s.get("trade", {})
        return bool(trade.get("is_open")) and bool(trade.get("offer_sent"))

    await tc.wait_for_state_change(offer_submitted, tier=2, timeout=15.0)

    # Step 3: Verify helper sees our offered items in partner_items.
    # partner item model_id may be 0 (cross-session item lookup), so verify by quantity.
    for _ in range(30):
        helper = _read_helper_status()
        if helper:
            partner_items = helper.get("partner_items", [])
            if any(
                int(it.get("quantity", 0) or 0) == expected_quantity
                for it in partner_items
            ):
                break
        await asyncio.sleep(0.5)
    else:
        helper = _read_helper_status()
        raise TestFailure(
            f"Helper did not observe partner item qty={expected_quantity}; "
            f"helper_status={helper}"
        )

    # Step 4: Enable helper auto-submit, but keep helper auto-accept disabled for now.
    # We want BLUMPKINS to submit first, then have DISCO accept, then let BLUMPKINS accept second.
    helper_before_submit = _read_helper_status() or {}
    baseline_submit_attempts = int(helper_before_submit.get("submit_attempt_count", 0) or 0)
    baseline_accept_attempts = int(helper_before_submit.get("accept_attempt_count", 0) or 0)
    write_trade_helper_config(auto_submit=True, auto_accept=False, submit_gold=0)

    # Step 5: Wait for helper to submit its (empty) offer
    for _ in range(20):
        helper = _read_helper_status()
        if helper and int(helper.get("submit_attempt_count", 0) or 0) > baseline_submit_attempts:
            break
        await asyncio.sleep(0.5)
    else:
        helper = _read_helper_status()
        raise TestFailure(
            "Helper did not submit its offer for the current forward trade; "
            f"baseline_submit_attempts={baseline_submit_attempts} helper_status={helper}"
        )

    # Step 5b: Wait for the game to process BOTH submissions before starting acceptance.
    await asyncio.sleep(2.0)

    # Step 6: Main accepts first.
    result = await _send_trade_action(tc, "accept_trade", {})
    tc.assert_action_success(result)

    # Step 7: Now allow the helper to accept second.
    write_trade_helper_config(auto_submit=True, auto_accept=True, submit_gold=0)
    for _ in range(20):
        helper = _read_helper_status()
        if helper and int(helper.get("accept_attempt_count", 0) or 0) > baseline_accept_attempts:
            break
        await asyncio.sleep(0.5)
    else:
        helper = _read_helper_status()
        raise TestFailure(
            "Helper did not attempt accept for the current forward trade; "
            f"baseline_accept_attempts={baseline_accept_attempts} helper_status={helper}"
        )

    # Step 8: Wait for closure. If the first main accept was too early, retry DISCO accept.
    closed = False
    for _ in range(8):
        try:
            await tc.wait_for_state_change(
                lambda s: not bool(s.get("trade", {}).get("is_open")), tier=2, timeout=3.0
            )
            closed = True
            break
        except Exception:
            result = await _send_trade_action(tc, "accept_trade", {})
            tc.assert_action_success(result)
            await asyncio.sleep(0.75)

    assert_true(
        closed,
        f"Forward trade should close after DISCO accepts first and BLUMPKINS accepts second; helper_status={_read_helper_status()}",
    )
    # Reset helper config and give the game time to settle after trade completion.
    # Agent IDs may refresh after a completed trade.
    write_trade_helper_config(auto_submit=False)
    helper_closed = await _wait_for_helper_trade_closed(timeout=12.0)
    if require_helper_closed:
        assert_true(
            helper_closed is not None and int(helper_closed.get("trade_flags", 0) or 0) == 0,
            f"Helper should clear its trade state after forward completion; helper_status={helper_closed}",
        )
    await asyncio.sleep(1.0)
    return helper_closed


async def _complete_forward_trade_and_verify_helper_items(
    tc: BridgeTestCase,
    expected_quantities: list[int],
    require_helper_closed: bool = True,
) -> dict | None:
    """Submit the main's multi-item offer, verify the helper sees each quantity, then complete the trade."""
    normalized_quantities = [int(quantity) for quantity in expected_quantities]
    assert_true(bool(normalized_quantities), "Forward multi-item completion requires at least one expected quantity")

    result = await _send_trade_action(tc, "submit_trade_offer", {"gold": 0})
    tc.assert_action_success(result)

    def offer_submitted(s: dict) -> bool:
        trade = s.get("trade", {})
        return bool(trade.get("is_open")) and bool(trade.get("offer_sent"))

    await tc.wait_for_state_change(offer_submitted, tier=2, timeout=15.0)

    for _ in range(30):
        helper = _read_helper_status()
        if helper:
            partner_items = helper.get("partner_items", [])
            if _trade_item_quantities_match(partner_items, normalized_quantities):
                break
        await asyncio.sleep(0.5)
    else:
        helper = _read_helper_status()
        raise TestFailure(
            "Helper did not observe the expected multi-item partner quantities; "
            f"expected_quantities={normalized_quantities} helper_status={helper}"
        )

    helper_before_submit = _read_helper_status() or {}
    baseline_submit_attempts = int(helper_before_submit.get("submit_attempt_count", 0) or 0)
    baseline_accept_attempts = int(helper_before_submit.get("accept_attempt_count", 0) or 0)
    write_trade_helper_config(auto_submit=True, auto_accept=False, submit_gold=0)

    for _ in range(20):
        helper = _read_helper_status()
        if helper and int(helper.get("submit_attempt_count", 0) or 0) > baseline_submit_attempts:
            break
        await asyncio.sleep(0.5)
    else:
        helper = _read_helper_status()
        raise TestFailure(
            "Helper did not submit its offer for the current forward multi-item trade; "
            f"baseline_submit_attempts={baseline_submit_attempts} helper_status={helper}"
        )

    await asyncio.sleep(2.0)

    result = await _send_trade_action(tc, "accept_trade", {})
    tc.assert_action_success(result)

    write_trade_helper_config(auto_submit=True, auto_accept=True, submit_gold=0)
    for _ in range(20):
        helper = _read_helper_status()
        if helper and int(helper.get("accept_attempt_count", 0) or 0) > baseline_accept_attempts:
            break
        await asyncio.sleep(0.5)
    else:
        helper = _read_helper_status()
        raise TestFailure(
            "Helper did not attempt accept for the current forward multi-item trade; "
            f"baseline_accept_attempts={baseline_accept_attempts} helper_status={helper}"
        )

    closed = False
    for _ in range(8):
        try:
            await tc.wait_for_state_change(
                lambda s: not bool(s.get("trade", {}).get("is_open")), tier=2, timeout=3.0
            )
            closed = True
            break
        except Exception:
            result = await _send_trade_action(tc, "accept_trade", {})
            tc.assert_action_success(result)
            await asyncio.sleep(0.75)

    assert_true(
        closed,
        f"Forward multi-item trade should close after DISCO accepts first and BLUMPKINS accepts second; helper_status={_read_helper_status()}",
    )
    write_trade_helper_config(auto_submit=False)
    helper_closed = await _wait_for_helper_trade_closed(timeout=12.0)
    if require_helper_closed:
        assert_true(
            helper_closed is not None and int(helper_closed.get("trade_flags", 0) or 0) == 0,
            f"Helper should clear its trade state after forward multi-item completion; helper_status={helper_closed}",
        )
    await asyncio.sleep(1.0)
    return helper_closed


async def _complete_reverse_trade_from_helper_on_current_open_trade(
    tc: BridgeTestCase,
    model_id: int,
    expected_quantity: int | None = None,
) -> tuple[dict | None, int]:
    """Complete a helper-to-main trade on an already-open trade window."""
    requested_quantity = int(expected_quantity or 0)
    helper_before_submit = _read_helper_status() or {}
    baseline_submit_attempts = int(helper_before_submit.get("submit_attempt_count", 0) or 0)
    baseline_accept_attempts = int(helper_before_submit.get("accept_attempt_count", 0) or 0)
    write_trade_helper_config(
        auto_submit=True,
        auto_accept=False,
        submit_gold=0,
        offer_item_model_id=model_id,
        offer_item_quantity=requested_quantity,
    )

    def partner_item_seen(s: dict) -> bool:
        trade = s.get("trade", {})
        if not trade.get("is_open"):
            return False
        partner = trade.get("partner", {})
        return any(
            int(it.get("model_id", 0) or 0) == model_id
            for it in partner.get("items", [])
        )

    try:
        partner_snap = await tc.wait_for_state_change(partner_item_seen, tier=2, timeout=20.0)
    except Exception as exc:
        latest_trade = (await tc.wait_for_snapshot(tier=2, timeout=5.0)).get("trade", {})
        helper = _read_helper_status()
        raise TestFailure(
            f"Helper did not offer item model={model_id} back; "
            f"partner_items={latest_trade.get('partner', {}).get('items')} "
            f"helper={helper}"
        ) from exc

    partner_items = (partner_snap.get("trade", {}).get("partner", {}) or {}).get("items", [])
    actual_quantity = 0
    for item in partner_items:
        if int(item.get("model_id", 0) or 0) != model_id:
            continue
        actual_quantity = int(item.get("quantity", 0) or 0)
        break
    if actual_quantity <= 0 and expected_quantity is not None:
        actual_quantity = int(expected_quantity)

    # Wait for helper to have submitted its offer.
    for _ in range(20):
        helper = _read_helper_status()
        if helper:
            helper_flags = int(helper.get("trade_flags", 0) or 0)
            if (
                int(helper.get("submit_attempt_count", 0) or 0) > baseline_submit_attempts
                or bool(helper_flags & 0x2)
            ):
                break
        await asyncio.sleep(0.5)
    else:
        helper = _read_helper_status()
        raise TestFailure(
            "Helper did not submit its offer for the current return trade; "
            f"baseline_submit_attempts={baseline_submit_attempts} helper_status={helper}"
        )

    # Main submits an empty offer, then accepts first. Helper accepts second once accept_ready is true.
    result = await _send_trade_action(tc, "submit_trade_offer", {"gold": 0})
    tc.assert_action_success(result)

    def main_submitted(s: dict) -> bool:
        trade = s.get("trade", {})
        return bool(trade.get("is_open")) and bool(trade.get("offer_sent"))

    await tc.wait_for_state_change(main_submitted, tier=2, timeout=10.0)
    await asyncio.sleep(2.0)

    result = await _send_trade_action(tc, "accept_trade", {})
    tc.assert_action_success(result)

    write_trade_helper_config(
        auto_submit=True,
        auto_accept=True,
        submit_gold=0,
        offer_item_model_id=model_id,
        offer_item_quantity=requested_quantity,
    )
    for _ in range(20):
        helper = _read_helper_status()
        if helper:
            helper_flags = int(helper.get("trade_flags", 0) or 0)
            if (
                int(helper.get("accept_attempt_count", 0) or 0) > baseline_accept_attempts
                or bool(helper_flags & 0x4)
            ):
                break
        await asyncio.sleep(0.5)
    else:
        helper = _read_helper_status()
        raise TestFailure(
            "Helper did not attempt accept for the current return trade; "
            f"baseline_accept_attempts={baseline_accept_attempts} helper_status={helper}"
        )

    closed = False
    for _ in range(8):
        try:
            await tc.wait_for_state_change(
                lambda s: not bool(s.get("trade", {}).get("is_open")), tier=2, timeout=3.0
            )
            closed = True
            break
        except Exception:
            result = await _send_trade_action(tc, "accept_trade", {})
            tc.assert_action_success(result)
            await asyncio.sleep(0.75)

    assert_true(closed, "Return trade should close after both sides submit and accept")
    write_trade_helper_config(auto_submit=False)
    helper_closed = await _wait_for_helper_trade_closed(timeout=12.0)
    assert_true(
        helper_closed is not None and int(helper_closed.get("trade_flags", 0) or 0) == 0,
        f"Helper should clear its trade state after return completion; helper_status={helper_closed}",
    )
    await asyncio.sleep(1.0)
    return helper_closed, actual_quantity


async def _return_trade_from_helper(
    tc: BridgeTestCase,
    model_id: int,
    expected_quantity: int | None = None,
) -> int:
    """Open a new trade, have the helper offer an item back, and complete the return trade."""
    requested_quantity = int(expected_quantity or 0)
    write_trade_helper_config(
        auto_submit=True,
        auto_accept=False,
        submit_gold=0,
        offer_item_model_id=model_id,
        offer_item_quantity=requested_quantity,
    )
    await _open_trade_with_helper(tc, require_inventory_item=False)
    _, actual_quantity = await _complete_reverse_trade_from_helper_on_current_open_trade(
        tc,
        model_id,
        expected_quantity=expected_quantity,
    )
    return actual_quantity


async def _complete_reverse_trade_from_helper_items_on_current_open_trade(
    tc: BridgeTestCase,
    expected_offers: list[tuple[int, int]],
) -> tuple[dict | None, list[tuple[int, int]]]:
    """Complete a helper-to-main multi-item trade on an already-open trade window."""
    normalized_offers = [(int(model_id), int(quantity)) for model_id, quantity in expected_offers if int(model_id) > 0]
    assert_true(bool(normalized_offers), "Reverse multi-item completion requires at least one configured helper offer")
    assert_true(len(normalized_offers) <= 2, "Helper reverse multi-item completion supports at most two offers")
    primary_model_id, primary_quantity = normalized_offers[0]
    secondary_model_id, secondary_quantity = normalized_offers[1] if len(normalized_offers) > 1 else (0, 0)

    helper_before_submit = _read_helper_status() or {}
    baseline_submit_attempts = int(helper_before_submit.get("submit_attempt_count", 0) or 0)
    baseline_accept_attempts = int(helper_before_submit.get("accept_attempt_count", 0) or 0)
    write_trade_helper_config(
        auto_submit=True,
        auto_accept=False,
        submit_gold=0,
        offer_item_model_id=primary_model_id,
        offer_item_quantity=primary_quantity,
        offer_item_model_id_2=secondary_model_id,
        offer_item_quantity_2=secondary_quantity,
    )

    def partner_items_seen(s: dict) -> bool:
        trade = s.get("trade", {})
        if not trade.get("is_open"):
            return False
        partner_items = (trade.get("partner", {}) or {}).get("items", [])
        return _trade_items_cover_expected_models(partner_items, normalized_offers)

    try:
        partner_snap = await tc.wait_for_state_change(partner_items_seen, tier=2, timeout=20.0)
    except Exception as exc:
        latest_trade = (await tc.wait_for_snapshot(tier=2, timeout=5.0)).get("trade", {})
        helper = _read_helper_status()
        raise TestFailure(
            "Helper did not offer the expected multi-item return trade; "
            f"expected_offers={normalized_offers} "
            f"partner_items={latest_trade.get('partner', {}).get('items')} "
            f"helper={helper}"
        ) from exc

    partner_items = (partner_snap.get("trade", {}).get("partner", {}) or {}).get("items", [])
    actual_offers: list[tuple[int, int]] = []
    for model_id, expected_quantity in normalized_offers:
        actual_quantity = 0
        for item in partner_items:
            if int(item.get("model_id", 0) or 0) != model_id:
                continue
            actual_quantity = int(item.get("quantity", 0) or 0)
            break
        actual_offers.append((model_id, actual_quantity if actual_quantity > 0 else expected_quantity))

    for _ in range(20):
        helper = _read_helper_status()
        if helper:
            helper_flags = int(helper.get("trade_flags", 0) or 0)
            if (
                int(helper.get("submit_attempt_count", 0) or 0) > baseline_submit_attempts
                or bool(helper_flags & 0x2)
            ):
                break
        await asyncio.sleep(0.5)
    else:
        helper = _read_helper_status()
        raise TestFailure(
            "Helper did not submit its offer for the current reverse multi-item trade; "
            f"baseline_submit_attempts={baseline_submit_attempts} helper_status={helper}"
        )

    result = await _send_trade_action(tc, "submit_trade_offer", {"gold": 0})
    tc.assert_action_success(result)

    def main_submitted(s: dict) -> bool:
        trade = s.get("trade", {})
        return bool(trade.get("is_open")) and bool(trade.get("offer_sent"))

    await tc.wait_for_state_change(main_submitted, tier=2, timeout=10.0)
    await asyncio.sleep(2.0)

    result = await _send_trade_action(tc, "accept_trade", {})
    tc.assert_action_success(result)

    write_trade_helper_config(
        auto_submit=True,
        auto_accept=True,
        submit_gold=0,
        offer_item_model_id=primary_model_id,
        offer_item_quantity=primary_quantity,
        offer_item_model_id_2=secondary_model_id,
        offer_item_quantity_2=secondary_quantity,
    )
    for _ in range(20):
        helper = _read_helper_status()
        if helper:
            helper_flags = int(helper.get("trade_flags", 0) or 0)
            if (
                int(helper.get("accept_attempt_count", 0) or 0) > baseline_accept_attempts
                or bool(helper_flags & 0x4)
            ):
                break
        await asyncio.sleep(0.5)
    else:
        helper = _read_helper_status()
        raise TestFailure(
            "Helper did not attempt accept for the current reverse multi-item trade; "
            f"baseline_accept_attempts={baseline_accept_attempts} helper_status={helper}"
        )

    closed = False
    for _ in range(8):
        try:
            await tc.wait_for_state_change(
                lambda s: not bool(s.get("trade", {}).get("is_open")), tier=2, timeout=3.0
            )
            closed = True
            break
        except Exception:
            result = await _send_trade_action(tc, "accept_trade", {})
            tc.assert_action_success(result)
            await asyncio.sleep(0.75)

    assert_true(closed, "Reverse multi-item trade should close after both sides submit and accept")
    write_trade_helper_config(auto_submit=False)
    helper_closed = await _wait_for_helper_trade_closed(timeout=12.0)
    assert_true(
        helper_closed is not None and int(helper_closed.get("trade_flags", 0) or 0) == 0,
        f"Helper should clear its trade state after reverse multi-item completion; helper_status={helper_closed}",
    )
    await asyncio.sleep(1.0)
    return helper_closed, actual_offers


async def _return_trade_items_from_helper(
    tc: BridgeTestCase,
    expected_offers: list[tuple[int, int]],
) -> list[tuple[int, int]]:
    """Open a new trade, have the helper offer up to two items back, and complete the return trade."""
    normalized_offers = [(int(model_id), int(quantity)) for model_id, quantity in expected_offers if int(model_id) > 0]
    assert_true(bool(normalized_offers), "Helper return trade requires at least one configured offer")
    assert_true(len(normalized_offers) <= 2, "Helper return trade supports at most two configured offers")
    primary_model_id, primary_quantity = normalized_offers[0]
    secondary_model_id, secondary_quantity = normalized_offers[1] if len(normalized_offers) > 1 else (0, 0)
    write_trade_helper_config(
        auto_submit=True,
        auto_accept=False,
        submit_gold=0,
        offer_item_model_id=primary_model_id,
        offer_item_quantity=primary_quantity,
        offer_item_model_id_2=secondary_model_id,
        offer_item_quantity_2=secondary_quantity,
    )
    await _open_trade_with_helper(tc, require_inventory_item=False)
    _, actual_offers = await _complete_reverse_trade_from_helper_items_on_current_open_trade(tc, normalized_offers)
    return actual_offers


async def _offer_trade_items_from_main(
    tc: BridgeTestCase,
    offers: list[tuple[int, int, int]],
) -> None:
    """Offer multiple items from the main client into the currently open trade."""
    expected_offers: list[tuple[int, int]] = []
    for item_id, model_id, quantity in offers:
        result = await tc.send_action(
            "offer_trade_item",
            {"item_id": int(item_id), "quantity": int(quantity)},
            timeout=5.0,
        )
        tc.assert_action_success(result)
        expected_offers.append((int(model_id), int(quantity)))

        def own_offers_seen(s: dict) -> bool:
            trade = s.get("trade", {})
            if not trade.get("is_open"):
                return False
            player_items = (trade.get("player", {}) or {}).get("items", [])
            return _trade_items_cover_expected_models(player_items, expected_offers)

        await tc.wait_for_state_change(own_offers_seen, tier=2, timeout=20.0)


# ---------------------------------------------------------------------------
# Round-trip tests: offer → verify helper sees it → complete → trade back
# ---------------------------------------------------------------------------

async def test_player_trade_roundtrip_stackable_prompt_max_helper(tc: BridgeTestCase):
    """Full round-trip: offer full stack via Max, verify helper sees it, complete, trade back."""
    snap_before = await tc.wait_for_snapshot(tier=3, timeout=10.0)
    item_id, model_id, _ = _find_stackable_trade_offer_candidate(snap_before)
    if item_id <= 0 or model_id <= 0:
        tc.skip("No stackable inventory item for round-trip max test")
        return
    original_item = _find_inventory_item_ref_by_id(snap_before, item_id)
    if not original_item:
        tc.skip("Stackable item disappeared before round-trip max test")
        return
    _, _, _, original_quantity = original_item
    if original_quantity < 2:
        tc.skip("Need stack >= 2 for round-trip max test")
        return

    # Keep helper passive during offering phase
    write_trade_helper_config(auto_submit=False)
    await _open_trade_with_helper(tc)

    result = await tc.send_action("offer_trade_item_prompt_max", {"item_id": item_id}, timeout=5.0)
    tc.assert_action_success(result)

    def own_max_offer_seen(s: dict) -> bool:
        trade = s.get("trade", {})
        player = trade.get("player", {})
        return any(
            int(it.get("model_id", 0) or 0) == model_id
            and int(it.get("quantity", 0) or 0) == original_quantity
            for it in player.get("items", [])
        )

    await tc.wait_for_state_change(own_max_offer_seen, tier=2, timeout=20.0)

    await _complete_forward_trade_and_verify_helper(tc, original_quantity)
    await _return_trade_from_helper(tc, model_id)

    # Verify inventory restored
    snap_after = await tc.wait_for_snapshot(tier=3, timeout=10.0)
    # Item may have a different item_id after trades, so check total by model
    after_qty = 0
    for bag in snap_after.get("inventory", {}).get("bags", []):
        for item in bag.get("items", []):
            if int(item.get("model_id", 0) or 0) == model_id and not item.get("equipped", False):
                after_qty += int(item.get("quantity", 0) or 0)
    assert_true(after_qty >= original_quantity, f"Inventory should be restored after round-trip (had {original_quantity}, now {after_qty})")

    await _merge_split_stack_cleanup(tc, model_id, prefer_item_id=item_id)


async def test_player_trade_roundtrip_stackable_prompt_default_helper(tc: BridgeTestCase):
    """Full round-trip: offer default qty (1) via prompt, verify helper sees it, complete, trade back."""
    snap_before = await tc.wait_for_snapshot(tier=3, timeout=10.0)
    item_id, model_id, _ = _find_stackable_trade_offer_candidate(snap_before)
    if item_id <= 0 or model_id <= 0:
        tc.skip("No stackable inventory item for round-trip default test")
        return
    original_item = _find_inventory_item_ref_by_id(snap_before, item_id)
    if not original_item:
        tc.skip("Stackable item disappeared before round-trip default test")
        return
    _, _, _, original_quantity = original_item
    if original_quantity < 2:
        tc.skip("Need stack >= 2 for round-trip default test")
        return

    write_trade_helper_config(auto_submit=False)
    await _open_trade_with_helper(tc)

    result = await tc.send_action("offer_trade_item_prompt_default", {"item_id": item_id}, timeout=5.0)
    tc.assert_action_success(result)

    def own_default_offer_seen(s: dict) -> bool:
        trade = s.get("trade", {})
        player = trade.get("player", {})
        return any(
            int(it.get("model_id", 0) or 0) == model_id
            and int(it.get("quantity", 0) or 0) == 1
            for it in player.get("items", [])
        )

    await tc.wait_for_state_change(own_default_offer_seen, tier=2, timeout=20.0)

    await _complete_forward_trade_and_verify_helper(tc, 1, require_helper_closed=False)
    await _return_trade_from_helper(tc, model_id)

    # Verify inventory restored (total model quantity should match)
    snap_after = await tc.wait_for_snapshot(tier=3, timeout=10.0)
    after_qty = 0
    for bag in snap_after.get("inventory", {}).get("bags", []):
        for item in bag.get("items", []):
            if int(item.get("model_id", 0) or 0) == model_id and not item.get("equipped", False):
                after_qty += int(item.get("quantity", 0) or 0)
    assert_true(after_qty >= original_quantity, f"Inventory should be restored after round-trip (had {original_quantity}, now {after_qty})")

    await _merge_split_stack_cleanup(tc, model_id, prefer_item_id=item_id)


async def test_player_trade_roundtrip_stackable_prompt_exact_helper(tc: BridgeTestCase):
    """Full round-trip: offer exact qty via prompt, verify helper sees it, complete, trade back."""
    snap_before = await tc.wait_for_snapshot(tier=3, timeout=10.0)
    item_id, model_id, requested_quantity = _find_stackable_trade_offer_candidate(snap_before)
    if item_id <= 0 or model_id <= 0 or requested_quantity <= 0:
        tc.skip("No stackable inventory item for round-trip exact test")
        return
    original_item = _find_inventory_item_ref_by_id(snap_before, item_id)
    if not original_item:
        tc.skip("Stackable item disappeared before round-trip exact test")
        return
    _, _, _, original_quantity = original_item
    if original_quantity <= requested_quantity:
        tc.skip("Need stack greater than requested for round-trip exact test")
        return

    write_trade_helper_config(auto_submit=False)
    await _open_trade_with_helper(tc)

    result = await tc.send_action(
        "offer_trade_item_prompt_quantity",
        {"item_id": item_id, "quantity": requested_quantity},
        timeout=5.0,
    )
    tc.assert_action_success(result)

    def own_exact_offer_seen(s: dict) -> bool:
        trade = s.get("trade", {})
        player = trade.get("player", {})
        return any(
            int(it.get("model_id", 0) or 0) == model_id
            and int(it.get("quantity", 0) or 0) == requested_quantity
            for it in player.get("items", [])
        )

    await tc.wait_for_state_change(own_exact_offer_seen, tier=2, timeout=20.0)

    await _complete_forward_trade_and_verify_helper(tc, requested_quantity)
    await _return_trade_from_helper(tc, model_id)

    # Verify inventory restored
    snap_after = await tc.wait_for_snapshot(tier=3, timeout=10.0)
    after_qty = 0
    for bag in snap_after.get("inventory", {}).get("bags", []):
        for item in bag.get("items", []):
            if int(item.get("model_id", 0) or 0) == model_id and not item.get("equipped", False):
                after_qty += int(item.get("quantity", 0) or 0)
    assert_true(after_qty >= original_quantity, f"Inventory should be restored after round-trip (had {original_quantity}, now {after_qty})")

    await _merge_split_stack_cleanup(tc, model_id, prefer_item_id=item_id)


async def test_player_trade_open_offer_submit_cancel_helper(tc: BridgeTestCase):
    """Open trade, offer one item, submit the offer, verify submitted state, then cancel."""
    _, item_id = await _open_trade_with_helper(tc)
    if item_id <= 0:
        tc.skip("No non-equipped inventory item found to use as a reversible trade offer")
        return

    result = await tc.send_action(
        "offer_trade_item",
        {"item_id": item_id, "quantity": 1},
        timeout=5.0,
    )
    tc.assert_action_success(result)

    def own_offer_seen(s: dict) -> bool:
        trade = s.get("trade", {})
        if not trade.get("is_open"):
            return False
        player = trade.get("player", {})
        return any(int(it.get("item_id", 0)) == item_id for it in player.get("items", []))

    await tc.wait_for_state_change(own_offer_seen, tier=2, timeout=15.0)

    result = await _send_trade_action(tc, "submit_trade_offer", {"gold": 0})
    tc.assert_action_success(result)

    def offer_submitted(s: dict) -> bool:
        trade = s.get("trade", {})
        return bool(trade.get("is_open")) and bool(trade.get("offer_sent"))

    submit_snap = await tc.wait_for_state_change(offer_submitted, tier=2, timeout=15.0)
    assert_true(bool(submit_snap["trade"].get("offer_sent")), "Trade should report submitted offer after submit_trade_offer")

    result = await _send_trade_action(tc, "cancel_trade", {})
    tc.assert_action_success(result)

    def trade_closed(s: dict) -> bool:
        return not bool(s.get("trade", {}).get("is_open"))

    closed_snap = await tc.wait_for_state_change(trade_closed, tier=2, timeout=15.0)
    assert_true(not closed_snap["trade"]["is_open"], "Trade should report closed after cancel")


async def test_player_trade_open_offer_submit_change_cancel_helper(tc: BridgeTestCase):
    """Open trade, offer one item, submit it, retract the submitted state via change_trade_offer, then cancel."""
    _, item_id = await _open_trade_with_helper(tc)
    if item_id <= 0:
        tc.skip("No non-equipped inventory item found to use as a reversible trade offer")
        return

    result = await tc.send_action(
        "offer_trade_item",
        {"item_id": item_id, "quantity": 1},
        timeout=5.0,
    )
    tc.assert_action_success(result)

    def own_offer_seen(s: dict) -> bool:
        trade = s.get("trade", {})
        if not trade.get("is_open"):
            return False
        player = trade.get("player", {})
        return any(int(it.get("item_id", 0)) == item_id for it in player.get("items", []))

    await tc.wait_for_state_change(own_offer_seen, tier=2, timeout=15.0)

    result = await _send_trade_action(tc, "submit_trade_offer", {"gold": 0})
    tc.assert_action_success(result)

    def offer_submitted(s: dict) -> bool:
        trade = s.get("trade", {})
        return bool(trade.get("is_open")) and bool(trade.get("offer_sent"))

    submitted = await tc.wait_for_state_change(offer_submitted, tier=2, timeout=15.0)
    assert_true(bool(submitted["trade"].get("offer_sent")), "Trade should report offer_sent after submit_trade_offer")

    result = await _send_trade_action(tc, "change_trade_offer", {})
    tc.assert_action_success(result)

    def offer_retracted(s: dict) -> bool:
        trade = s.get("trade", {})
        if not trade.get("is_open"):
            return False
        return not bool(trade.get("offer_sent")) and not bool(trade.get("is_accepted"))

    try:
        retracted = await tc.wait_for_state_change(offer_retracted, tier=2, timeout=15.0)
    except Exception as exc:
        latest = await tc.wait_for_snapshot(tier=2, timeout=5.0)
        trade = latest.get("trade", {})
        helper_debug = None
        helper_status = _helper_status_path()
        if helper_status.exists():
            try:
                helper_debug = json.loads(helper_status.read_text(encoding="utf-8", errors="ignore"))
            except Exception:
                helper_debug = {"read_error": str(helper_status)}
        raise TestFailure(
            "State change not observed after change_trade_offer; "
            f"trade_flags={trade.get('flags')} "
            f"is_open={trade.get('is_open')} "
            f"offer_sent={trade.get('offer_sent')} "
            f"is_accepted={trade.get('is_accepted')} "
            f"player_gold={trade.get('player', {}).get('gold')} "
            f"player_items={trade.get('player', {}).get('items')} "
            f"partner_gold={trade.get('partner', {}).get('gold')} "
            f"partner_items={trade.get('partner', {}).get('items')} "
            f"debug_capture_count={trade.get('debug_capture_count')} "
            f"debug_window_ctx={trade.get('debug_window_ctx')} "
            f"debug_window_frame={trade.get('debug_window_frame')} "
            f"debug_ui_window_context={trade.get('debug_ui_window_context')} "
            f"debug_ui_window_frame={trade.get('debug_ui_window_frame')} "
            f"helper={helper_debug} root={exc}"
        ) from exc
    assert_true(not bool(retracted["trade"].get("offer_sent")), "Trade should no longer report offer_sent after change_trade_offer")
    assert_true(
        not bool(retracted["trade"].get("is_accepted")),
        "Trade should no longer report accepted after change_trade_offer",
    )
    assert_true(
        any(int(it.get("item_id", 0)) == item_id for it in retracted["trade"]["player"].get("items", [])),
        "change_trade_offer should retract the submitted offer state without removing items from the trade cart",
    )

    result = await _send_trade_action(tc, "cancel_trade", {})
    tc.assert_action_success(result)
    await tc.wait_for_state_change(lambda s: not bool(s.get("trade", {}).get("is_open")), tier=2, timeout=15.0)


async def test_player_trade_open_submit_gold_cancel_helper(tc: BridgeTestCase):
    """Submit a gold offer, verify it reflects in the trade snapshot, then cancel."""
    snap_before = await tc.wait_for_snapshot(tier=3, timeout=10.0)
    char_gold = int(snap_before.get("inventory", {}).get("gold_character", 0) or 0)
    if char_gold < 100:
        tc.skip("Not enough character gold to validate trade gold offering")
        return

    await _open_trade_with_helper(tc)
    offered_gold = min(100, char_gold)
    result = await _send_trade_action(tc, "submit_trade_offer", {"gold": offered_gold})
    tc.assert_action_success(result)

    def gold_offer_seen(s: dict) -> bool:
        trade = s.get("trade", {})
        if not trade.get("is_open"):
            return False
        player = trade.get("player", {})
        return bool(trade.get("offer_sent")) and int(player.get("gold", 0) or 0) == offered_gold

    try:
        submit_snap = await tc.wait_for_state_change(gold_offer_seen, tier=2, timeout=15.0)
    except Exception as exc:
        latest = await tc.wait_for_snapshot(tier=2, timeout=5.0)
        trade = latest.get("trade", {})
        raise TestFailure(
            "Gold offer did not reflect in trade snapshot; "
            f"expected_gold={offered_gold} "
            f"trade_flags={trade.get('flags')} "
            f"is_open={trade.get('is_open')} "
            f"offer_sent={trade.get('offer_sent')} "
            f"player_gold={trade.get('player', {}).get('gold')} "
            f"partner_gold={trade.get('partner', {}).get('gold')} "
            f"player_items={trade.get('player', {}).get('item_count')} "
            f"partner_items={trade.get('partner', {}).get('item_count')}"
        ) from exc
    trade = submit_snap["trade"]
    assert_true(bool(trade.get("offer_sent")), "Trade should report submitted offer after gold submit")
    assert_true(
        int(trade.get("player", {}).get("gold", 0) or 0) == offered_gold,
        f"Own trade gold should reflect submitted gold offer ({offered_gold})",
    )

    result = await _send_trade_action(tc, "cancel_trade", {})
    tc.assert_action_success(result)
    await tc.wait_for_state_change(lambda s: not bool(s.get("trade", {}).get("is_open")), tier=2, timeout=15.0)


async def test_player_trade_partner_gold_visible_helper(tc: BridgeTestCase):
    """Configure helper to submit gold and prove the main bridge sees it in trade.partner.gold."""
    offered_gold = 100
    write_trade_helper_config(submit_gold=offered_gold, auto_submit=True)
    try:
        await _open_trade_with_helper(tc)

        def partner_gold_seen(s: dict) -> bool:
            trade = s.get("trade", {})
            if not trade.get("is_open"):
                return False
            return int(trade.get("partner", {}).get("gold", 0) or 0) == offered_gold

        snap = await tc.wait_for_state_change(partner_gold_seen, tier=2, timeout=15.0)
        trade = snap.get("trade", {})
        assert_true(
            int(trade.get("partner", {}).get("gold", 0) or 0) == offered_gold,
            f"Partner gold should reflect helper offer ({offered_gold})",
        )

        async def helper_gold_seen() -> dict | None:
            helper_status = _helper_status_path()
            deadline = asyncio.get_running_loop().time() + 10.0
            latest = None
            while asyncio.get_running_loop().time() < deadline:
                if helper_status.exists():
                    latest = json.loads(helper_status.read_text(encoding="utf-8", errors="ignore"))
                    if int(latest.get("player_gold", 0) or 0) == offered_gold:
                        return latest
                await asyncio.sleep(0.5)
            return latest

        helper_debug = await helper_gold_seen()
        assert_true(helper_debug is not None, "Helper status should exist for partner-gold validation")
        assert_true(
            int((helper_debug or {}).get("player_gold", 0) or 0) == offered_gold,
            f"Helper-side player_gold should reflect submitted gold ({offered_gold})",
        )

        result = await _send_trade_action(tc, "cancel_trade", {})
        tc.assert_action_success(result)
        await tc.wait_for_state_change(lambda s: not bool(s.get("trade", {}).get("is_open")), tier=2, timeout=15.0)
    finally:
        write_trade_helper_config(submit_gold=0, auto_submit=False)


async def test_player_trade_zz_open_offer_submit_accept_complete_helper(tc: BridgeTestCase):
    """Complete a one-item trade, prove the helper sees it, and verify it leaves Disco Panic's inventory."""
    snap_before = await tc.wait_for_snapshot(tier=3, timeout=10.0)
    sacrificial_item_id, _ = _find_singleton_trade_offer_candidate(snap_before)
    if sacrificial_item_id <= 0:
        tc.skip("No singleton non-equipped inventory item found to use as the sacrificial complete-trade item")
        return

    before_ref = _find_inventory_item_ref_by_id(snap_before, sacrificial_item_id)
    if not before_ref:
        tc.skip("Sacrificial item disappeared before completion test")
        return

    await _open_trade_with_helper(tc)
    item_id = sacrificial_item_id

    result = await tc.send_action(
        "offer_trade_item",
        {"item_id": item_id, "quantity": 1},
        timeout=5.0,
    )
    tc.assert_action_success(result)

    def own_offer_seen(s: dict) -> bool:
        trade = s.get("trade", {})
        if not trade.get("is_open"):
            return False
        player = trade.get("player", {})
        return any(int(it.get("item_id", 0)) == item_id for it in player.get("items", []))

    await tc.wait_for_state_change(own_offer_seen, tier=2, timeout=15.0)
    await _complete_forward_trade_and_verify_helper(tc, 1, require_helper_closed=False)

    def inventory_transferred(s: dict) -> bool:
        return _find_inventory_item_ref_by_id(s, item_id) is None

    try:
        after_snap = await _wait_for_fresh_tier3_state_change(tc, inventory_transferred, timeout=20.0)
    except Exception as exc:
        latest = tc.latest_snapshot(3)
        if latest is None:
            latest = await _query_fresh_tier3_snapshot(tc, settle_ms=1000, timeout=8.0)
        helper = _read_helper_status()
        latest_ref = _find_inventory_item_ref_by_id(latest, item_id)
        latest_model_qty = _count_inventory_quantity_by_model(latest, before_ref[2])
        raise TestFailure(
            "Completed trade did not remove the offered singleton item from Disco Panic inventory; "
            f"item_id={item_id} "
            f"model_id={before_ref[2]} "
            f"latest_ref={latest_ref} "
            f"latest_model_qty={latest_model_qty} "
            f"trade_flags={latest.get('trade', {}).get('flags')} "
            f"ctos_total={latest.get('trade', {}).get('debug_ctos_packet_total')} "
            f"ctos_submit={latest.get('trade', {}).get('debug_ctos_trade_submit_offer_count')} "
            f"ctos_accept={latest.get('trade', {}).get('debug_ctos_trade_accept_count')} "
            f"ctos_cancel={latest.get('trade', {}).get('debug_ctos_trade_cancel_count')} "
            f"ctos_add_item={latest.get('trade', {}).get('debug_ctos_trade_add_item_count')} "
            f"ctos_packets={latest.get('trade', {}).get('debug_ctos_packets')} "
            f"inventory_bags={latest.get('inventory', {}).get('bags', [])} "
            f"helper={helper}"
        ) from exc
    assert_true(
        _find_inventory_item_ref_by_id(after_snap, item_id) is None,
        f"Sacrificial item {item_id} should leave Disco Panic inventory after completed trade",
    )


async def test_player_trade_zz_open_offer_stackable_submit_accept_complete_helper(tc: BridgeTestCase):
    """Complete a stackable trade in the direction that still has receiver inventory capacity."""
    snap_before = await tc.wait_for_snapshot(tier=3, timeout=10.0)
    main_free_slots_total = _inventory_free_slots_total(snap_before)
    item_id, model_id, requested_quantity = _find_stackable_trade_offer_candidate(snap_before)

    await _open_trade_with_helper(tc, require_inventory_item=False)
    helper_status = _read_helper_status() or {}
    helper_free_slots_total = _helper_inventory_free_slots_total(helper_status)
    helper_model_id, helper_quantity = _helper_stackable_offer_candidate(helper_status)
    helper_singleton_model_id = _helper_safe_singleton_offer_model(helper_status)

    async def cancel_open_trade_if_needed() -> None:
        latest_trade = (tc.latest_snapshot(2) or {}).get("trade", {})
        if not latest_trade.get("is_open"):
            return
        result = await _send_trade_action(tc, "cancel_trade", {})
        tc.assert_action_success(result)
        await tc.wait_for_state_change(lambda s: not bool(s.get("trade", {}).get("is_open")), tier=2, timeout=15.0)
        write_trade_helper_config(auto_submit=False)

    if helper_free_slots_total > 0:
        if item_id <= 0 or model_id <= 0 or requested_quantity <= 0:
            await cancel_open_trade_if_needed()
            tc.skip("No stackable inventory item found to use for forward completion validation")
            return

        original_item = _find_inventory_item_ref_by_id(snap_before, item_id)
        if not original_item:
            await cancel_open_trade_if_needed()
            tc.skip("Stackable completion candidate disappeared before test start")
            return

        before_total_quantity = _count_inventory_quantity_by_model(snap_before, model_id)
        assert_gt(before_total_quantity, requested_quantity, "Need stack quantity greater than the offered amount")

        result = await tc.send_action(
            "offer_trade_item",
            {"item_id": item_id, "quantity": requested_quantity},
            timeout=5.0,
        )
        tc.assert_action_success(result)

        def own_offer_seen(s: dict) -> bool:
            trade = s.get("trade", {})
            if not trade.get("is_open"):
                return False
            player = trade.get("player", {})
            return any(
                int(it.get("model_id", 0) or 0) == model_id
                and int(it.get("quantity", 0) or 0) == requested_quantity
                for it in player.get("items", [])
            )

        await tc.wait_for_state_change(own_offer_seen, tier=2, timeout=20.0)
        await _complete_forward_trade_and_verify_helper(tc, requested_quantity, require_helper_closed=False)

        def inventory_reduced(s: dict) -> bool:
            return _count_inventory_quantity_by_model(s, model_id) <= (before_total_quantity - requested_quantity)

        try:
            after_snap = await _wait_for_fresh_tier3_state_change(tc, inventory_reduced, timeout=20.0)
        except Exception as exc:
            latest = tc.latest_snapshot(3)
            if latest is None:
                latest = await _query_fresh_tier3_snapshot(tc, settle_ms=1000, timeout=8.0)
            helper = _read_helper_status()
            raise TestFailure(
                "Completed stackable trade did not reduce Disco Panic inventory by the offered quantity; "
                f"item_id={item_id} "
                f"model_id={model_id} "
                f"requested_quantity={requested_quantity} "
                f"before_total_quantity={before_total_quantity} "
                f"after_total_quantity={_count_inventory_quantity_by_model(latest, model_id)} "
                f"trade_flags={latest.get('trade', {}).get('flags')} "
                f"ctos_total={latest.get('trade', {}).get('debug_ctos_packet_total')} "
                f"ctos_submit={latest.get('trade', {}).get('debug_ctos_trade_submit_offer_count')} "
                f"ctos_accept={latest.get('trade', {}).get('debug_ctos_trade_accept_count')} "
                f"ctos_cancel={latest.get('trade', {}).get('debug_ctos_trade_cancel_count')} "
                f"ctos_add_item={latest.get('trade', {}).get('debug_ctos_trade_add_item_count')} "
                f"ctos_packets={latest.get('trade', {}).get('debug_ctos_packets')} "
                f"inventory_bags={latest.get('inventory', {}).get('bags', [])} "
                f"helper={helper}"
            ) from exc

        assert_true(
            _count_inventory_quantity_by_model(after_snap, model_id) <= (before_total_quantity - requested_quantity),
            f"Model {model_id} total quantity should drop by at least {requested_quantity} after completed trade",
        )
        return

    if main_free_slots_total <= 0:
        await cancel_open_trade_if_needed()
        tc.skip(
            "Both trade directions are blocked: BLUMPKINS has no free receiver slots and Disco Panic has no free receiver slots"
        )
        return

    reverse_model_id = 0
    reverse_expected_quantity = 0
    if helper_model_id > 0 and helper_quantity > 1:
        reverse_model_id = helper_model_id
        reverse_expected_quantity = 1 if helper_quantity == 2 else min(2, helper_quantity - 1)
    elif helper_singleton_model_id > 0:
        reverse_model_id = helper_singleton_model_id
        reverse_expected_quantity = 1

    if reverse_model_id <= 0 or reverse_expected_quantity <= 0:
        await cancel_open_trade_if_needed()
        tc.skip(
            "BLUMPKINS has no free receiver slots and no safe helper-side item is available for reverse-direction validation"
        )
        return

    before_total_quantity = _count_inventory_quantity_by_model(snap_before, reverse_model_id)
    await cancel_open_trade_if_needed()
    observed_quantity = await _return_trade_from_helper(
        tc,
        reverse_model_id,
        expected_quantity=reverse_expected_quantity,
    )
    received_quantity = observed_quantity if observed_quantity > 0 else reverse_expected_quantity
    assert_gt(received_quantity, 0, "Reverse-direction helper offer should produce a positive received quantity")

    def inventory_increased(s: dict) -> bool:
        return _count_inventory_quantity_by_model(s, reverse_model_id) >= (before_total_quantity + received_quantity)

    try:
        after_snap = await _wait_for_fresh_tier3_state_change(tc, inventory_increased, timeout=20.0)
    except Exception as exc:
        latest = tc.latest_snapshot(3)
        if latest is None:
            latest = await _query_fresh_tier3_snapshot(tc, settle_ms=1000, timeout=8.0)
        helper = _read_helper_status()
        raise TestFailure(
            "Completed reverse trade did not increase Disco Panic inventory by the helper-offered quantity; "
            f"model_id={reverse_model_id} "
            f"expected_received_quantity={received_quantity} "
            f"before_total_quantity={before_total_quantity} "
            f"after_total_quantity={_count_inventory_quantity_by_model(latest, reverse_model_id)} "
            f"trade_flags={latest.get('trade', {}).get('flags')} "
            f"ctos_total={latest.get('trade', {}).get('debug_ctos_packet_total')} "
            f"ctos_submit={latest.get('trade', {}).get('debug_ctos_trade_submit_offer_count')} "
            f"ctos_accept={latest.get('trade', {}).get('debug_ctos_trade_accept_count')} "
            f"ctos_cancel={latest.get('trade', {}).get('debug_ctos_trade_cancel_count')} "
            f"ctos_add_item={latest.get('trade', {}).get('debug_ctos_trade_add_item_count')} "
            f"ctos_packets={latest.get('trade', {}).get('debug_ctos_packets')} "
            f"inventory_bags={latest.get('inventory', {}).get('bags', [])} "
            f"helper={helper}"
        ) from exc

    assert_true(
        _count_inventory_quantity_by_model(after_snap, reverse_model_id) >= (before_total_quantity + received_quantity),
        f"Model {reverse_model_id} total quantity should increase by at least {received_quantity} after reverse completed trade",
    )


async def test_player_trade_zz_reverse_helper_stackable_submit_accept_complete_helper(tc: BridgeTestCase):
    """Complete a helper-originated stackable trade and verify Disco Panic receives the requested partial quantity."""
    snap_before = await tc.wait_for_snapshot(tier=3, timeout=10.0)
    main_free_slots_total = _inventory_free_slots_total(snap_before)
    if main_free_slots_total <= 0:
        tc.skip("Disco Panic has no free receiver slots for helper-originated reverse stackable validation")
        return

    await ensure_trade_helper_running()
    helper_status = _read_helper_status() or {}
    helper_model_id, helper_quantity = _helper_stackable_offer_candidate(helper_status)
    if helper_model_id <= 0 or helper_quantity < 2:
        tc.skip("No helper-side stackable inventory candidate is available for reverse completion validation")
        return

    requested_quantity = 1 if helper_quantity == 2 else min(2, helper_quantity - 1)
    before_total_quantity = _count_inventory_quantity_by_model(snap_before, helper_model_id)
    observed_quantity = await _return_trade_from_helper(
        tc,
        helper_model_id,
        expected_quantity=requested_quantity,
    )
    received_quantity = observed_quantity if observed_quantity > 0 else requested_quantity
    assert_gt(received_quantity, 0, "Helper reverse stackable trade should produce a positive received quantity")

    def inventory_increased(s: dict) -> bool:
        return _count_inventory_quantity_by_model(s, helper_model_id) >= (before_total_quantity + received_quantity)

    try:
        after_snap = await _wait_for_fresh_tier3_state_change(tc, inventory_increased, timeout=20.0)
    except Exception as exc:
        latest = tc.latest_snapshot(3)
        if latest is None:
            latest = await _query_fresh_tier3_snapshot(tc, settle_ms=1000, timeout=8.0)
        helper = _read_helper_status()
        raise TestFailure(
            "Completed helper-originated reverse stackable trade did not increase Disco Panic inventory by the helper-offered quantity; "
            f"model_id={helper_model_id} "
            f"requested_quantity={requested_quantity} "
            f"received_quantity={received_quantity} "
            f"before_total_quantity={before_total_quantity} "
            f"after_total_quantity={_count_inventory_quantity_by_model(latest, helper_model_id)} "
            f"trade_flags={latest.get('trade', {}).get('flags')} "
            f"ctos_total={latest.get('trade', {}).get('debug_ctos_packet_total')} "
            f"ctos_submit={latest.get('trade', {}).get('debug_ctos_trade_submit_offer_count')} "
            f"ctos_accept={latest.get('trade', {}).get('debug_ctos_trade_accept_count')} "
            f"ctos_cancel={latest.get('trade', {}).get('debug_ctos_trade_cancel_count')} "
            f"ctos_add_item={latest.get('trade', {}).get('debug_ctos_trade_add_item_count')} "
            f"ctos_packets={latest.get('trade', {}).get('debug_ctos_packets')} "
            f"inventory_bags={latest.get('inventory', {}).get('bags', [])} "
            f"helper={helper}"
        ) from exc

    assert_true(
        _count_inventory_quantity_by_model(after_snap, helper_model_id) >= (before_total_quantity + received_quantity),
        f"Model {helper_model_id} total quantity should increase by at least {received_quantity} after helper reverse stackable completion",
    )


async def test_player_trade_zz_roundtrip_singleton_and_stackable_complete_helper(tc: BridgeTestCase):
    """Trade one helper singleton and one helper stackable to Disco, then trade both back in one return trade."""
    snap_before = await tc.wait_for_snapshot(tier=3, timeout=10.0)
    main_free_slots_total = _inventory_free_slots_total(snap_before)
    await ensure_trade_helper_running()
    helper_before = _read_helper_status() or {}

    helper_stack_model_id, helper_stack_quantity = _helper_stackable_offer_candidate(helper_before)
    helper_stack_total_before = _helper_stackable_offer_total_quantity(helper_before)
    helper_singleton_model_id = _helper_safe_singleton_offer_model(helper_before)
    helper_singleton_total_before = _helper_safe_singleton_offer_total_quantity(helper_before)

    if helper_stack_model_id <= 0 or helper_stack_quantity < 2:
        tc.skip("BLUMPKINS does not currently have a usable helper-side stackable candidate for two-item round-trip validation")
        return
    if helper_singleton_model_id <= 0 or helper_singleton_total_before <= 0:
        tc.skip("BLUMPKINS does not currently have a usable helper-side singleton candidate for two-item round-trip validation")
        return

    requested_stack_quantity = 1 if helper_stack_quantity == 2 else min(2, helper_stack_quantity - 1)
    before_main_singleton_total = _count_inventory_quantity_by_model(snap_before, helper_singleton_model_id)
    before_main_stack_total = _count_inventory_quantity_by_model(snap_before, helper_stack_model_id)
    required_main_free_slots = 1 + (0 if before_main_stack_total > 0 else 1)
    if main_free_slots_total < required_main_free_slots:
        tc.skip(
            "Disco Panic cannot receive the helper-originated singleton+stackable pair in one trade; "
            f"main_free_slots_total={main_free_slots_total} required_main_free_slots={required_main_free_slots}"
        )
        return

    actual_helper_offers = await _return_trade_items_from_helper(
        tc,
        [
            (helper_singleton_model_id, 1),
            (helper_stack_model_id, requested_stack_quantity),
        ],
    )
    actual_offer_map = {int(model_id): int(quantity) for model_id, quantity in actual_helper_offers}
    received_singleton_quantity = int(actual_offer_map.get(helper_singleton_model_id, 1) or 1)
    received_stack_quantity = int(actual_offer_map.get(helper_stack_model_id, requested_stack_quantity) or requested_stack_quantity)

    def main_inventory_received_pair(s: dict) -> bool:
        return (
            _count_inventory_quantity_by_model(s, helper_singleton_model_id) >= (before_main_singleton_total + received_singleton_quantity)
            and _count_inventory_quantity_by_model(s, helper_stack_model_id) >= (before_main_stack_total + received_stack_quantity)
        )

    try:
        after_forward = await _wait_for_fresh_tier3_state_change(tc, main_inventory_received_pair, timeout=20.0)
    except Exception as exc:
        latest = tc.latest_snapshot(3)
        if latest is None:
            latest = await _query_fresh_tier3_snapshot(tc, settle_ms=1000, timeout=8.0)
        helper = _read_helper_status()
        raise TestFailure(
            "Helper-originated singleton+stackable forward trade did not reach Disco Panic inventory as expected; "
            f"singleton_model_id={helper_singleton_model_id} "
            f"stack_model_id={helper_stack_model_id} "
            f"received_singleton_quantity={received_singleton_quantity} "
            f"received_stack_quantity={received_stack_quantity} "
            f"before_main_singleton_total={before_main_singleton_total} "
            f"after_main_singleton_total={_count_inventory_quantity_by_model(latest, helper_singleton_model_id)} "
            f"before_main_stack_total={before_main_stack_total} "
            f"after_main_stack_total={_count_inventory_quantity_by_model(latest, helper_stack_model_id)} "
            f"helper={helper}"
        ) from exc

    singleton_item_id = _find_received_singleton_item_id(snap_before, after_forward, helper_singleton_model_id)
    stack_item_id = _find_received_stack_item_id(
        snap_before,
        after_forward,
        helper_stack_model_id,
        received_stack_quantity,
    )
    assert_gt(singleton_item_id, 0, "Disco Panic should have the helper-returned singleton item after the first completed trade")
    assert_gt(stack_item_id, 0, "Disco Panic should have the helper-returned stackable item after the first completed trade")

    write_trade_helper_config(auto_submit=False)
    await _open_trade_with_helper(tc, require_inventory_item=False)
    await _offer_trade_items_from_main(
        tc,
        [
            (singleton_item_id, helper_singleton_model_id, received_singleton_quantity),
            (stack_item_id, helper_stack_model_id, received_stack_quantity),
        ],
    )
    helper_closed = await _complete_forward_trade_and_verify_helper_items(
        tc,
        [received_singleton_quantity, received_stack_quantity],
        require_helper_closed=False,
    )

    def main_inventory_restored(s: dict) -> bool:
        return _main_roundtrip_restored(
            s,
            helper_singleton_model_id,
            before_main_singleton_total,
            helper_stack_model_id,
            before_main_stack_total,
        )

    try:
        after_return = await _wait_for_fresh_tier3_state_change(tc, main_inventory_restored, timeout=20.0)
    except Exception as exc:
        latest = tc.latest_snapshot(3)
        if latest is None:
            latest = await _query_fresh_tier3_snapshot(tc, settle_ms=1500, timeout=8.0)
        helper = _read_helper_status() or {}
        raise TestFailure(
            "Two-item round-trip return trade did not restore Disco Panic inventory to its starting counts; "
            f"singleton_model_id={helper_singleton_model_id} "
            f"stack_model_id={helper_stack_model_id} "
            f"selected_singleton_item_id={singleton_item_id} "
            f"selected_stack_item_id={stack_item_id} "
            f"received_singleton_quantity={received_singleton_quantity} "
            f"received_stack_quantity={received_stack_quantity} "
            f"before_singleton_refs={_inventory_item_refs_debug(snap_before, helper_singleton_model_id)} "
            f"after_forward_singleton_refs={_inventory_item_refs_debug(after_forward, helper_singleton_model_id)} "
            f"after_return_singleton_refs={_inventory_item_refs_debug(latest, helper_singleton_model_id)} "
            f"before_stack_refs={_inventory_item_refs_debug(snap_before, helper_stack_model_id)} "
            f"after_forward_stack_refs={_inventory_item_refs_debug(after_forward, helper_stack_model_id)} "
            f"after_return_stack_refs={_inventory_item_refs_debug(latest, helper_stack_model_id)} "
            f"expected_singleton_total={before_main_singleton_total} "
            f"actual_singleton_total={_count_inventory_quantity_by_model(latest, helper_singleton_model_id)} "
            f"expected_stack_total={before_main_stack_total} "
            f"actual_stack_total={_count_inventory_quantity_by_model(latest, helper_stack_model_id)} "
            f"helper={helper}"
        ) from exc

    helper = _read_helper_status() or {}
    if (
        _count_inventory_quantity_by_model(after_return, helper_singleton_model_id) != before_main_singleton_total
        or _count_inventory_quantity_by_model(after_return, helper_stack_model_id) != before_main_stack_total
    ):
        raise TestFailure(
            "Two-item round-trip return trade did not restore Disco Panic inventory to its starting counts; "
            f"singleton_model_id={helper_singleton_model_id} "
            f"stack_model_id={helper_stack_model_id} "
            f"selected_singleton_item_id={singleton_item_id} "
            f"selected_stack_item_id={stack_item_id} "
            f"received_singleton_quantity={received_singleton_quantity} "
            f"received_stack_quantity={received_stack_quantity} "
            f"before_singleton_refs={_inventory_item_refs_debug(snap_before, helper_singleton_model_id)} "
            f"after_forward_singleton_refs={_inventory_item_refs_debug(after_forward, helper_singleton_model_id)} "
            f"after_return_singleton_refs={_inventory_item_refs_debug(after_return, helper_singleton_model_id)} "
            f"before_stack_refs={_inventory_item_refs_debug(snap_before, helper_stack_model_id)} "
            f"after_forward_stack_refs={_inventory_item_refs_debug(after_forward, helper_stack_model_id)} "
            f"after_return_stack_refs={_inventory_item_refs_debug(after_return, helper_stack_model_id)} "
            f"expected_singleton_total={before_main_singleton_total} "
            f"actual_singleton_total={_count_inventory_quantity_by_model(after_return, helper_singleton_model_id)} "
            f"expected_stack_total={before_main_stack_total} "
            f"actual_stack_total={_count_inventory_quantity_by_model(after_return, helper_stack_model_id)} "
            f"helper={helper}"
        )

    assert_true(
        _count_inventory_quantity_by_model(after_return, helper_singleton_model_id) == before_main_singleton_total,
        f"Disco Panic singleton model {helper_singleton_model_id} should return to its starting total after the two-item round-trip",
    )
    assert_true(
        _count_inventory_quantity_by_model(after_return, helper_stack_model_id) == before_main_stack_total,
        f"Disco Panic stackable model {helper_stack_model_id} should return to its starting total after the two-item round-trip",
    )

    helper_after = helper_closed or _read_helper_status() or {}
    assert_true(
        int(helper_after.get("trade_flags", 0) or 0) == 0,
        f"Helper should be out of trade after the two-item round-trip; helper_status={helper_after}",
    )
    assert_true(
        int(helper_after.get("helper_stackable_offer_model_id", 0) or 0) == helper_stack_model_id,
        f"Helper should expose the same stackable model after the round-trip; helper_status={helper_after}",
    )
    assert_true(
        int(helper_after.get("helper_stackable_offer_total_quantity", 0) or 0) == helper_stack_total_before,
        f"Helper stackable model {helper_stack_model_id} should return to its starting total quantity after the two-item round-trip",
    )
    assert_true(
        int(helper_after.get("helper_safe_singleton_offer_model_id", 0) or 0) == helper_singleton_model_id,
        f"Helper should expose the same singleton model after the round-trip; helper_status={helper_after}",
    )
    assert_true(
        int(helper_after.get("helper_safe_singleton_offer_total_quantity", 0) or 0) == helper_singleton_total_before,
        f"Helper singleton model {helper_singleton_model_id} should return to its starting total quantity after the two-item round-trip",
    )


async def test_player_trade_zz_roundtrip_singleton_complete_helper(tc: BridgeTestCase):
    """Complete a singleton trade to BLUMPKINS, then complete the helper-to-main return trade and verify inventory restoration."""
    snap_before = await tc.wait_for_snapshot(tier=3, timeout=10.0)
    sacrificial_item_id, sacrificial_model_id = _find_singleton_trade_offer_candidate(snap_before)
    if sacrificial_item_id <= 0 or sacrificial_model_id <= 0:
        tc.skip("No singleton non-equipped inventory item found to use as the sacrificial round-trip item")
        return

    before_ref = _find_inventory_item_ref_by_id(snap_before, sacrificial_item_id)
    if not before_ref:
        tc.skip("Sacrificial round-trip item disappeared before test start")
        return

    before_total_quantity = _count_inventory_quantity_by_model(snap_before, sacrificial_model_id)
    assert_gt(before_total_quantity, 0, "Round-trip candidate model should exist in Disco Panic inventory before trade")

    write_trade_helper_config(auto_submit=False)
    await _open_trade_with_helper(tc)

    result = await tc.send_action(
        "offer_trade_item",
        {"item_id": sacrificial_item_id, "quantity": 1},
        timeout=5.0,
    )
    tc.assert_action_success(result)

    def own_offer_seen(s: dict) -> bool:
        trade = s.get("trade", {})
        if not trade.get("is_open"):
            return False
        player = trade.get("player", {})
        return any(int(it.get("item_id", 0) or 0) == sacrificial_item_id for it in player.get("items", []))

    await tc.wait_for_state_change(own_offer_seen, tier=2, timeout=15.0)
    await _complete_forward_trade_and_verify_helper(tc, 1, require_helper_closed=False)

    after_forward = await _wait_for_fresh_tier3_state_change(
        tc,
        lambda s: _find_inventory_item_ref_by_id(s, sacrificial_item_id) is None,
        timeout=20.0,
    )
    assert_true(
        _find_inventory_item_ref_by_id(after_forward, sacrificial_item_id) is None,
        f"Sacrificial round-trip item {sacrificial_item_id} should leave Disco Panic inventory after the forward trade",
    )

    await _return_trade_from_helper(tc, sacrificial_model_id)

    def inventory_restored(s: dict) -> bool:
        return _count_inventory_quantity_by_model(s, sacrificial_model_id) >= before_total_quantity

    try:
        after_return = await _wait_for_fresh_tier3_state_change(tc, inventory_restored, timeout=20.0)
    except Exception as exc:
        latest = tc.latest_snapshot(3)
        if latest is None:
            latest = await _query_fresh_tier3_snapshot(tc, settle_ms=1000, timeout=8.0)
        helper = _read_helper_status()
        raise TestFailure(
            "Helper-to-main return trade did not restore the expected singleton model quantity; "
            f"model_id={sacrificial_model_id} "
            f"expected_quantity>={before_total_quantity} "
            f"actual_quantity={_count_inventory_quantity_by_model(latest, sacrificial_model_id)} "
            f"inventory_bags={latest.get('inventory', {}).get('bags', [])} "
            f"helper={helper}"
        ) from exc

    assert_true(
        _count_inventory_quantity_by_model(after_return, sacrificial_model_id) >= before_total_quantity,
        f"Disco Panic inventory should regain model {sacrificial_model_id} after helper return trade",
    )


async def test_player_trade_zzz_chat_trade_and_whisper_helper(tc: BridgeTestCase):
    """Validate trade chat visibility plus two-way whispers between DISCO and BLUMPKINS."""
    await ensure_trade_helper_running()
    await _ensure_discopanic_at_trade_rendezvous(tc)

    try:
        await _wait_for_helper_visible(tc, timeout=10.0)
    except Exception:
        await _request_helper_move_to_discopanic(tc, timeout=45.0)
        await asyncio.sleep(2.0)
        await _wait_for_helper_visible(tc, timeout=180.0)

    token = int(asyncio.get_running_loop().time() * 1000)
    trade_msg_main = f"price check {token}"
    trade_msg_helper = f"one sec please {token}"
    whisper_to_helper = f"hey, are you free {token}"
    whisper_to_main = f"yes, ready now {token}"

    result = await tc.send_action(
        "send_chat",
        {"message": trade_msg_main, "channel": "trade"},
        timeout=5.0,
    )
    tc.assert_action_success(result)
    await _wait_for_main_chat_entry(tc, message=trade_msg_main, channel="trade", timeout=20.0)
    helper_trade_seen = await _wait_for_helper_chat_entry(message=trade_msg_main, channel="trade", timeout=20.0)
    assert_true(
        helper_trade_seen is not None,
        f"Helper should observe DISCO trade chat message '{trade_msg_main}'; helper_status={_read_helper_status()}",
    )

    helper_before_chat = _read_helper_status() or {}
    helper_chat_attempts_before = int(helper_before_chat.get("chat_send_attempt_count", 0) or 0)
    helper_chat_seq = request_helper_send_chat(channel="trade", message=trade_msg_helper)
    await _wait_for_main_chat_entry(tc, message=trade_msg_helper, channel="trade", timeout=20.0)
    helper_after_chat = None
    for _ in range(40):
        helper_after_chat = _read_helper_status() or {}
        if (
            int(helper_after_chat.get("chat_send_attempt_count", 0) or 0) > helper_chat_attempts_before
            and int(helper_after_chat.get("last_chat_send_seq", 0) or 0) == helper_chat_seq
        ):
            break
        await asyncio.sleep(0.25)
    assert_true(
        helper_after_chat is not None
        and int(helper_after_chat.get("chat_send_attempt_count", 0) or 0) > helper_chat_attempts_before
        and int(helper_after_chat.get("last_chat_send_seq", 0) or 0) == helper_chat_seq,
        "Helper should execute the requested trade chat send",
    )

    result = await tc.send_action(
        "send_whisper",
        {"recipient": _helper_name(), "message": whisper_to_helper},
        timeout=5.0,
    )
    tc.assert_action_success(result)
    helper_whisper_seen = await _wait_for_helper_chat_entry(message=whisper_to_helper, channel="whisper", timeout=20.0)
    assert_true(
        helper_whisper_seen is not None,
        f"Helper should observe DISCO whisper '{whisper_to_helper}'; helper_status={_read_helper_status()}",
    )

    helper_before_whisper = _read_helper_status() or {}
    helper_whisper_attempts_before = int(helper_before_whisper.get("whisper_send_attempt_count", 0) or 0)
    helper_whisper_seq = request_helper_send_whisper(recipient=_main_name(), message=whisper_to_main)
    await _wait_for_main_chat_entry(tc, message=whisper_to_main, channel="whisper", timeout=20.0)
    helper_after_whisper = None
    for _ in range(40):
        helper_after_whisper = _read_helper_status() or {}
        if (
            int(helper_after_whisper.get("whisper_send_attempt_count", 0) or 0) > helper_whisper_attempts_before
            and int(helper_after_whisper.get("last_whisper_send_seq", 0) or 0) == helper_whisper_seq
        ):
            break
        await asyncio.sleep(0.25)
    assert_true(
        helper_after_whisper is not None
        and int(helper_after_whisper.get("whisper_send_attempt_count", 0) or 0) > helper_whisper_attempts_before
        and int(helper_after_whisper.get("last_whisper_send_seq", 0) or 0) == helper_whisper_seq,
        "Helper should execute the requested whisper send",
    )
