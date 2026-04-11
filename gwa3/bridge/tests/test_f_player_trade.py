"""Category F: Two-client player-trade bridge tests."""

from __future__ import annotations

import asyncio
import json

from .base import BridgeTestCase
from .helpers import TestFailure, assert_gt, assert_true
from .test_e_orchestrated import _wait_until_near
from .trade_harness import HELPER_NAME, ensure_trade_helper_running, write_trade_helper_config, _helper_status_path

MODEL_SALVAGE_KIT = 2992
TRADE_TEST_MAP = 650
TRADE_TEST_REGION = 4
TRADE_TEST_DISTRICT = 99
TRADE_TEST_LANGUAGE = 8


def _find_inventory_item_id_by_model(snap: dict, model_id: int) -> int:
    inv = snap.get("inventory", {})
    for bag in inv.get("bags", []):
        for item in bag.get("items", []):
            if item.get("model_id") == model_id and not item.get("equipped", False):
                return int(item.get("item_id", 0))
    return 0


def _find_trade_offer_candidate_item_id(snap: dict) -> int:
    salvage = _find_inventory_item_id_by_model(snap, MODEL_SALVAGE_KIT)
    if salvage > 0:
        return salvage
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
    for agent in snap.get("agents", []):
        if agent.get("agent_type") != "living":
            continue
        if agent.get("name") != HELPER_NAME:
            continue
        if not agent.get("is_alive", False):
            continue
        return agent
    return None


async def _wait_until_agent_within_range(
    tc: BridgeTestCase,
    agent_id: int,
    max_distance: float,
    timeout: float = 15.0,
) -> tuple[bool, dict]:
    deadline = asyncio.get_running_loop().time() + timeout
    next_reissue = 0.0
    last_seen: dict = {}
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
            now = asyncio.get_running_loop().time()
            if now >= next_reissue:
                last_move_exc = None
                for _ in range(2):
                    try:
                        result = await tc.send_action(
                            "move_to",
                            {"x": agent_x, "y": agent_y},
                            timeout=8.0,
                        )
                        tc.assert_action_success(result)
                        last_move_exc = None
                        break
                    except Exception as exc:
                        last_move_exc = exc
                        await asyncio.sleep(0.5)
                if last_move_exc:
                    raise last_move_exc
                next_reissue = now + 1.0
            break
        await asyncio.sleep(0.25)
    return False, last_seen


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


async def _wait_for_helper_visible(tc: BridgeTestCase, timeout: float = 90.0) -> dict:
    def helper_visible(snap: dict) -> bool:
        return _find_helper_agent(snap) is not None

    snap = await tc.wait_for_state_change(helper_visible, tier=2, timeout=timeout)
    helper = _find_helper_agent(snap)
    assert_true(helper is not None, "Helper should be visible in tier-2 snapshot")
    return helper


async def _open_trade_with_helper(tc: BridgeTestCase) -> tuple[int, int]:
    """Ensure helper is present, then open a trade and return (helper_id, salvage_item_id)."""
    await ensure_trade_helper_running()
    await _ensure_discopanic_at_trade_rendezvous(tc)
    try:
        helper = await _wait_for_helper_visible(tc, timeout=10.0)
    except Exception:
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
        last_move_exc = None
        for _ in range(2):
            try:
                result = await tc.send_action("move_to", {"x": helper_x, "y": helper_y}, timeout=8.0)
                tc.assert_action_success(result)
                last_move_exc = None
                break
            except Exception as exc:
                last_move_exc = exc
                await asyncio.sleep(0.5)
        if last_move_exc:
            raise last_move_exc
        reached = await _wait_until_near(tc, helper_x, helper_y, threshold=45.0, timeout=20.0)
        assert_true(reached, "Disco Panic should close to tight player-trade range")
    helper_in_range, helper_range_debug = await _wait_until_agent_within_range(tc, helper_id, max_distance=100.0, timeout=10.0)
    assert_true(
        helper_in_range,
        f"Helper should be within direct player-trade range before initiate_trade; last_seen={helper_range_debug}",
    )

    # Prefer a salvage kit, but fall back to any non-equipped inventory item for reversible trade tests.
    snap_before = await tc.wait_for_snapshot(tier=3, timeout=10.0)
    item_id = _find_trade_offer_candidate_item_id(snap_before)

    result = await tc.send_action("change_target", {"agent_id": helper_id}, timeout=5.0)
    tc.assert_action_success(result)

    def helper_targeted(s: dict) -> bool:
        me = s.get("me", {})
        return int(me.get("target_id", 0) or 0) == helper_id

    await tc.wait_for_state_change(helper_targeted, tier=1, timeout=10.0)
    await asyncio.sleep(0.5)

    open_snap = None
    last_open_exc: Exception | None = None
    for _attempt in range(2):
        result = await tc.send_action(
            "initiate_trade",
            {"agent_id": helper_id, "player_number": helper_player_number},
            timeout=5.0,
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
    await _open_trade_with_helper(tc)

    result = await tc.send_action("cancel_trade", {}, timeout=5.0)
    tc.assert_action_success(result)

    def trade_closed(s: dict) -> bool:
        return not bool(s.get("trade", {}).get("is_open"))

    closed_snap = await tc.wait_for_state_change(trade_closed, tier=2, timeout=15.0)
    assert_true(not closed_snap["trade"]["is_open"], "Trade should report closed after cancel")


async def test_player_trade_open_idle_cancel_helper(tc: BridgeTestCase):
    """Open a player trade, dwell briefly without offering anything, then cancel."""
    await _open_trade_with_helper(tc)
    await asyncio.sleep(2.0)

    # Prove the bridge still survives the open-trade dwell long enough to observe
    # another snapshot before we cancel.
    snap = await tc.wait_for_snapshot(tier=2, timeout=10.0)
    assert_true(bool(snap.get("trade", {}).get("is_open")), "Trade should still report open after dwell")

    result = await tc.send_action("cancel_trade", {}, timeout=5.0)
    tc.assert_action_success(result)

    def trade_closed(s: dict) -> bool:
        return not bool(s.get("trade", {}).get("is_open"))

    closed_snap = await tc.wait_for_state_change(trade_closed, tier=2, timeout=15.0)
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
    result = await tc.send_action("cancel_trade", {}, timeout=5.0)
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
        result = await tc.send_action("remove_trade_item", {"slot_or_item_id": remove_value}, timeout=5.0)
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

    result = await tc.send_action("cancel_trade", {}, timeout=5.0)
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

    result = await tc.send_action("cancel_trade", {}, timeout=5.0)
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

    result = await tc.send_action("cancel_trade", {}, timeout=5.0)
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

    result = await tc.send_action("cancel_trade", {}, timeout=5.0)
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

    result = await tc.send_action("cancel_trade", {}, timeout=5.0)
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


async def _complete_forward_trade_and_verify_helper(
    tc: BridgeTestCase,
    expected_quantity: int,
) -> None:
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
    result = await tc.send_action("submit_trade_offer", {"gold": 0}, timeout=5.0)
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

    # Step 4: NOW enable helper auto-submit and auto-accept.
    # We do this AFTER main has submitted so the helper doesn't race ahead.
    write_trade_helper_config(auto_submit=True, submit_gold=0)

    # Step 5: Wait for helper to submit its (empty) offer
    for _ in range(20):
        helper = _read_helper_status()
        if helper and int(helper.get("submit_attempt_count", 0) or 0) > 0:
            break
        await asyncio.sleep(0.5)

    # Step 5b: Wait for the game to process BOTH submissions before accepting.
    # The game server needs time to register both sides' submit packets.
    # Also verify the partner submitted from our side (is_accepted or offer state changes).
    await asyncio.sleep(2.0)

    # Step 6: Main accepts — retry until trade closes
    closed = False
    for _ in range(12):
        result = await tc.send_action("accept_trade", {}, timeout=5.0)
        tc.assert_action_success(result)
        try:
            await tc.wait_for_state_change(
                lambda s: not bool(s.get("trade", {}).get("is_open")), tier=2, timeout=4.0
            )
            closed = True
            break
        except Exception:
            await asyncio.sleep(1.5)

    assert_true(closed, "Forward trade should close after both sides submit and accept")
    # Reset helper config and give the game time to settle after trade completion.
    # Agent IDs may refresh after a completed trade.
    write_trade_helper_config(auto_submit=False)
    await asyncio.sleep(3.0)


async def _return_trade_from_helper(
    tc: BridgeTestCase,
    model_id: int,
) -> None:
    """Open a new trade, configure helper to offer the item back, complete the return trade.

    Sequence:
    1. Configure helper to offer item + auto-submit + auto-accept
    2. Open trade
    3. Wait for helper's item to appear as partner items
    4. Wait for helper to have submitted its offer
    5. Main submits empty offer
    6. Main accepts
    """
    # Step 1: Configure helper
    write_trade_helper_config(auto_submit=True, submit_gold=0, offer_item_model_id=model_id)

    # Step 2: Open trade
    await _open_trade_with_helper(tc)

    # Step 3: Wait for helper's item in partner view
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
        await tc.wait_for_state_change(partner_item_seen, tier=2, timeout=20.0)
    except Exception as exc:
        latest_trade = (await tc.wait_for_snapshot(tier=2, timeout=5.0)).get("trade", {})
        helper = _read_helper_status()
        raise TestFailure(
            f"Helper did not offer item model={model_id} back; "
            f"partner_items={latest_trade.get('partner', {}).get('items')} "
            f"helper={helper}"
        ) from exc

    # Step 4: Wait for helper to have submitted its offer
    for _ in range(20):
        helper = _read_helper_status()
        if helper and int(helper.get("submit_attempt_count", 0) or 0) > 0:
            break
        await asyncio.sleep(0.5)

    # Step 5: Main submits empty offer
    result = await tc.send_action("submit_trade_offer", {"gold": 0}, timeout=5.0)
    tc.assert_action_success(result)

    def main_submitted(s: dict) -> bool:
        trade = s.get("trade", {})
        return bool(trade.get("is_open")) and bool(trade.get("offer_sent"))

    await tc.wait_for_state_change(main_submitted, tier=2, timeout=10.0)

    # Step 6: Main accepts — retry until trade closes
    closed = False
    for _ in range(8):
        result = await tc.send_action("accept_trade", {}, timeout=5.0)
        tc.assert_action_success(result)
        try:
            await tc.wait_for_state_change(
                lambda s: not bool(s.get("trade", {}).get("is_open")), tier=2, timeout=3.0
            )
            closed = True
            break
        except Exception:
            await asyncio.sleep(0.75)

    assert_true(closed, "Return trade should close after both sides submit and accept")
    # Reset helper config
    write_trade_helper_config(auto_submit=False)
    await asyncio.sleep(1.0)


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

    await _complete_forward_trade_and_verify_helper(tc, 1)
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

    result = await tc.send_action("submit_trade_offer", {"gold": 0}, timeout=5.0)
    tc.assert_action_success(result)

    def offer_submitted(s: dict) -> bool:
        trade = s.get("trade", {})
        return bool(trade.get("is_open")) and bool(trade.get("offer_sent"))

    submit_snap = await tc.wait_for_state_change(offer_submitted, tier=2, timeout=15.0)
    assert_true(bool(submit_snap["trade"].get("offer_sent")), "Trade should report submitted offer after submit_trade_offer")

    result = await tc.send_action("cancel_trade", {}, timeout=5.0)
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

    result = await tc.send_action("submit_trade_offer", {"gold": 0}, timeout=5.0)
    tc.assert_action_success(result)

    def offer_submitted(s: dict) -> bool:
        trade = s.get("trade", {})
        return bool(trade.get("is_open")) and bool(trade.get("offer_sent"))

    submitted = await tc.wait_for_state_change(offer_submitted, tier=2, timeout=15.0)
    assert_true(bool(submitted["trade"].get("offer_sent")), "Trade should report offer_sent after submit_trade_offer")

    result = await tc.send_action("change_trade_offer", {}, timeout=5.0)
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

    result = await tc.send_action("cancel_trade", {}, timeout=5.0)
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
    result = await tc.send_action("submit_trade_offer", {"gold": offered_gold}, timeout=5.0)
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

    result = await tc.send_action("cancel_trade", {}, timeout=5.0)
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

        result = await tc.send_action("cancel_trade", {}, timeout=5.0)
        tc.assert_action_success(result)
        await tc.wait_for_state_change(lambda s: not bool(s.get("trade", {}).get("is_open")), tier=2, timeout=15.0)
    finally:
        write_trade_helper_config(submit_gold=0, auto_submit=False)


async def test_player_trade_zz_open_offer_submit_accept_complete_helper(tc: BridgeTestCase):
    """Complete a one-item trade and verify the offered item leaves Disco Panic's inventory."""
    snap_before = await tc.wait_for_snapshot(tier=3, timeout=10.0)
    sacrificial_item_id = _find_inventory_item_id_by_model(snap_before, MODEL_SALVAGE_KIT)
    before_count = sum(
        1
        for bag in snap_before.get("inventory", {}).get("bags", [])
        for item in bag.get("items", [])
        if item.get("model_id") == MODEL_SALVAGE_KIT and not item.get("equipped", False)
    )
    if sacrificial_item_id <= 0:
        tc.skip("No salvage kit found in inventory to use as the sacrificial complete-trade item")
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

    result = await tc.send_action("submit_trade_offer", {"gold": 0}, timeout=5.0)
    tc.assert_action_success(result)

    def offer_submitted(s: dict) -> bool:
        trade = s.get("trade", {})
        return bool(trade.get("is_open")) and bool(trade.get("offer_sent"))

    await tc.wait_for_state_change(offer_submitted, tier=2, timeout=15.0)

    helper_status = _helper_status_path()
    for _ in range(20):
        if helper_status.exists():
            try:
                helper_debug = json.loads(helper_status.read_text(encoding="utf-8", errors="ignore"))
            except Exception:
                helper_debug = None
            if helper_debug and int(helper_debug.get("submit_attempt_count", 0) or 0) > 0:
                break
        await asyncio.sleep(0.5)

    def trade_closed(s: dict) -> bool:
        return not bool(s.get("trade", {}).get("is_open"))

    closed = False
    for _ in range(8):
        result = await tc.send_action("accept_trade", {}, timeout=5.0)
        tc.assert_action_success(result)
        try:
            await tc.wait_for_state_change(trade_closed, tier=2, timeout=3.0)
            closed = True
            break
        except Exception:
            await asyncio.sleep(0.75)

    assert_true(closed, "Trade should close after submit/accept completion")

    def inventory_decremented(s: dict) -> bool:
        count = sum(
            1
            for bag in s.get("inventory", {}).get("bags", [])
            for item in bag.get("items", [])
            if item.get("model_id") == MODEL_SALVAGE_KIT and not item.get("equipped", False)
        )
        return count == max(0, before_count - 1)

    after_snap = await tc.wait_for_state_change(inventory_decremented, tier=3, timeout=20.0)
    after_count = sum(
        1
        for bag in after_snap.get("inventory", {}).get("bags", [])
        for item in bag.get("items", [])
        if item.get("model_id") == MODEL_SALVAGE_KIT and not item.get("equipped", False)
    )
    assert_true(after_count == max(0, before_count - 1), f"Salvage kit count should decrease after completed trade ({before_count} -> {after_count})")
