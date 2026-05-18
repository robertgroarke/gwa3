"""Player-trade open, offer, cancel, and removal bridge tests."""

from .player_trade_open_support import *

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
