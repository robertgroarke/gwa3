"""Player-trade round-trip completion bridge tests."""

from .player_trade_return_support import *

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
