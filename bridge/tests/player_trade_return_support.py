"""Player-trade reverse and item return helpers."""

from .player_trade_roundtrip_support import *

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



__all__ = [name for name in list(globals()) if not name.startswith('__')]
