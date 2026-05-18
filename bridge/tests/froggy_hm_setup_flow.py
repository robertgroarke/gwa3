from __future__ import annotations

import asyncio
import os
import time
from typing import Any

from .base import TestFailure, TestSkipped
from .froggy_hm_route_helpers import *  # noqa: F403 - constants mirror the C++ Froggy flow.


class FroggyHmSetupFlowMixin:
    async def phase1_travel_to_gadds(self) -> bool:
        print("\n=== PHASE 1: Travel to Gadd's Encampment ===")
        snap = await self.query_fresh(settle_ms=500, timeout=10.0)
        if snap is None:
            self._record("phase1_travel", "FAIL", "no initial snapshot")
            return False
        if self.map_id() == MAP_GADDS:
            self._record("phase1_travel", "PASS", "already at Gadd's")
            return True
        await self.action("travel", {"map_id": MAP_GADDS}, wait_ms=2000)
        arrived = await self.wait_for_map(MAP_GADDS, timeout=90.0)
        if arrived:
            await asyncio.sleep(5.0)  # world hydrate
            self._record("phase1_travel", "PASS")
            return True
        self._record("phase1_travel", "FAIL", "map never reached Gadd's")
        return False

    async def phase2_setup_heroes(self) -> bool:
        print("\n=== PHASE 2: Outpost setup (heroes + hard mode) ===")
        if self.map_id() != MAP_GADDS:
            self._record("phase2_setup", "SKIP", "not at Gadd's")
            return False

        # Read current hero state from the snapshot. Firing kick_hero for
        # a hero that is NOT in the party (or add_hero for one that
        # already is) lets the DLL send an invalid packet that the
        # server rejects with Code=007 â€” observed live after the phase
        # 2 mass kick/add cycle. We need to diff current vs desired and
        # only send the deltas.
        await self.query_fresh(settle_ms=400, timeout=6.0)
        current_heroes = [
            int(h.get("hero_id", 0) or 0)
            for h in self.snapshot.get("heroes", []) or []
            if int(h.get("hero_id", 0) or 0)
        ]
        print(f"[HEROES] current={current_heroes} desired={STANDARD_HEROES}")

        already_correct = current_heroes == STANDARD_HEROES
        if already_correct:
            print("[HEROES] party already matches Standard config; skipping kick/add")
            added = len(current_heroes)
        else:
            # Only kick heroes that are actually present AND not in the
            # desired set. Keep correct heroes in place.
            to_kick = [hid for hid in current_heroes if hid not in STANDARD_HEROES]
            to_add = [hid for hid in STANDARD_HEROES if hid not in current_heroes]
            print(f"[HEROES] to_kick={to_kick} to_add={to_add}")

            for hid in to_kick:
                await self.action("kick_hero", {"hero_id": hid}, wait_ms=400)
            if to_kick:
                await asyncio.sleep(1.5)

            for hid in to_add:
                await self.action("add_hero", {"hero_id": hid}, wait_ms=600)

            await asyncio.sleep(2.0)
            await self.query_fresh(settle_ms=400, timeout=6.0)
            added = len([h for h in self.snapshot.get("heroes", []) or [] if h.get("hero_id")])

        party_ok = self.party_size() >= 2
        if not party_ok:
            self._record("phase2_setup", "FAIL", f"party size {self.party_size()} after add_hero")
            return False

        # Guard behavior on all party heroes.
        for idx in range(1, added + 1):
            await self.action(
                "set_hero_behavior",
                {"hero_index": idx, "behavior": 1},
                wait_ms=200,
            )

        # Skip set_hard_mode if already enabled (check snapshot).
        already_hm = bool(self.snapshot.get("map", {}).get("hard_mode", False))
        if not already_hm:
            await self.action("set_hard_mode", {"enabled": True}, wait_ms=1000)

        self._record(
            "phase2_setup",
            "PASS",
            f"party_size={self.party_size()} already_correct={already_correct} hm_was_set={already_hm}",
        )
        return True

    async def phase2b_merchant(self) -> bool:
        print("\n=== PHASE 2b: Native pre-run maintenance verification ===")
        return await self.run_native_maintenance_verification(
            phase_name="phase2b_maintenance",
            include_salvage=True,
            detail_label="pre-run maintenance",
        )
        print("\n=== PHASE 2b: Merchant â€” read inventory/gold, buy + sell ===")
        # Gated: HandleMerchantBuy / HandleMerchantSell now refuse with
        # merchant_not_open_call_open_merchant_first when the merchant
        # isn't server-side-open (checked via
        # MerchantMgr::GetMerchantItemCount() > 0 â€” populated only after the
        # server replies with the merchant inventory). So even if the
        # test logic below has a bug, the DC-causing packet can't fire.
        if self.map_id() != MAP_GADDS:
            self._record("phase2b_merchant", "SKIP", "not at Gadd's")
            return False

        if not await self.walk_to(
            GADDS_MERCHANT_X, GADDS_MERCHANT_Y, "Gadd's merchant", threshold=350.0, timeout=30.0
        ):
            self._record("phase2b_merchant", "FAIL", "could not reach merchant area")
            return False

        snap = await self.query_fresh(settle_ms=400, timeout=6.0)
        if snap is None:
            self._record("phase2b_merchant", "SKIP", "no snapshot at merchant area")
            return False

        print(
            f"[INV] Gold character={self.gold_character()} storage={self.gold_storage()} "
            f"bags={[len(b.get('items', []) or []) for b in self.snapshot.get('inventory', {}).get('bags', [])]}"
        )

        npc_id = self.find_nearby_npc(GADDS_MERCHANT_X, GADDS_MERCHANT_Y, radius=600.0)
        if not npc_id:
            self._record("phase2b_merchant", "SKIP", "no merchant NPC found")
            return False

        # Try open_merchant first (GoNPC packet, confirmed working for
        # merchants in the conset bridge test); fall back to interact_npc.
        await self.action("change_target", {"agent_id": npc_id}, wait_ms=400)
        await self.action("open_merchant", {"agent_id": npc_id}, wait_ms=2000)
        snap = await self.query_fresh(settle_ms=400, timeout=6.0)
        if snap is None or not self.is_merchant_open():
            await self.action("interact_npc", {"agent_id": npc_id}, wait_ms=2000)
            await self.query_fresh(settle_ms=400, timeout=6.0)

        if not self.is_merchant_open():
            self._record("phase2b_merchant", "SKIP", "merchant window never opened")
            return True  # not a hard fail; merchant may not sell kits here

        items = self.merchant_items()
        print(f"[MERCHANT] {len(items)} items available")

        # BUY: pick a salvage kit or ID kit if the merchant sells one.
        # Use merchant_buy (native Transaction path) instead of
        # transact_items â€” the raw 0x4D packet crashes the client on some
        # merchant states (same family as the documented 0x39 INTERACT
        # crash in project memory).
        bought_any = False
        superior_before = self.count_superior_id_kits()
        salvage_before = self.count_salvage_kit_family()

        def merchant_item_id_for_model(model_id: int) -> int:
            for it in items:
                if int(it.get("model_id", 0) or 0) != model_id:
                    continue
                merch_item_id = int(it.get("item_id", 0) or 0)
                if merch_item_id > 0:
                    return merch_item_id
            return 0

        superior_merch_item_id = merchant_item_id_for_model(SUPERIOR_ID_KIT_MODEL)
        salvage_merch_item_id = 0
        for salvage_model in (
            CHEAP_SALVAGE_KIT_MODEL,
            RARE_SALVAGE_KIT_MODEL,
            ALT_SALVAGE_KIT_MODEL,
            EXPERT_SALVAGE_MODEL,
            SUPERIOR_SALVAGE_KIT_MODEL,
        ):
            salvage_merch_item_id = merchant_item_id_for_model(salvage_model)
            if salvage_merch_item_id > 0:
                break
        superior_needed = max(0, TARGET_SUPERIOR_ID_KITS - superior_before)
        salvage_needed = max(0, TARGET_SALVAGE_KITS - salvage_before)

        print(
            f"[MERCHANT] Kits before: superior_id={superior_before}/{TARGET_SUPERIOR_ID_KITS} "
            f"salvage={salvage_before}/{TARGET_SALVAGE_KITS}"
        )
        if superior_needed > 0 and superior_merch_item_id <= 0:
            print("[MERCHANT] Superior ID kits not sold here")
        for _ in range(superior_needed):
            if superior_merch_item_id <= 0:
                break
            await self.action(
                "merchant_buy",
                {"item_id": superior_merch_item_id, "quantity": 1},
                wait_ms=1500,
            )
            bought_any = True

        if salvage_needed > 0 and salvage_merch_item_id <= 0:
            print("[MERCHANT] Salvage kits not sold here")
        for _ in range(salvage_needed):
            if salvage_merch_item_id <= 0:
                break
            await self.action(
                "merchant_buy",
                {"item_id": salvage_merch_item_id, "quantity": 1},
                wait_ms=1500,
            )
            bought_any = True

        # SELL: sell one low-value stackable material if we have one.
        await self.query_fresh(settle_ms=400, timeout=6.0)
        superior_after = self.count_superior_id_kits()
        salvage_after = self.count_salvage_kit_family()
        print(
            f"[MERCHANT] Kits after: superior_id={superior_after}/{TARGET_SUPERIOR_ID_KITS} "
            f"salvage={salvage_after}/{TARGET_SALVAGE_KITS}"
        )
        sell_item_id = 0
        for item, _, _ in self.iter_inventory():
            if int(item.get("type", 0) or 0) != 11:  # 11 == material in GW item type enum
                continue
            if int(item.get("quantity", 0) or 0) < 2:
                continue
            sell_item_id = int(item.get("item_id", 0) or 0)
            if sell_item_id > 0:
                break
        if sell_item_id:
            await self.action(
                "merchant_sell",
                {"item_id": sell_item_id, "quantity": 1},
                wait_ms=1500,
            )
            print(f"[MERCHANT] Sold 1x item {sell_item_id}")
        else:
            print("[MERCHANT] No stackable material to sell â€” skipping sell leg")

        await self.action("cancel_action", {}, wait_ms=500)
        self._record(
            "phase2b_merchant",
            "PASS",
            f"gold_after={self.gold_character()} bought={bought_any} "
            f"superior_id={superior_after} salvage={salvage_after} sold={bool(sell_item_id)}",
        )
        return True
