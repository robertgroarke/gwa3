"""Player-trade round-trip completion bridge tests."""

from .player_trade_return_support import *

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
