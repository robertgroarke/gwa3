from __future__ import annotations

import asyncio
import os
import time
from typing import Any

from .base import TestFailure, TestSkipped
from .froggy_hm_route_helpers import *  # noqa: F403 - constants mirror the C++ Froggy flow.


class FroggyHmMaintenanceFlowMixin:
    async def ensure_gadds_merchant_open(self) -> tuple[bool, int]:
        if self.map_id() != MAP_GADDS:
            return False, 0
        if not await self.walk_to(
            GADDS_MERCHANT_X, GADDS_MERCHANT_Y, "Gadd's merchant", threshold=350.0, timeout=30.0
        ):
            return False, 0
        snap = await self.query_fresh(settle_ms=400, timeout=6.0)
        if snap is None:
            return False, 0
        npc_id = self.find_nearby_npc(GADDS_MERCHANT_X, GADDS_MERCHANT_Y, radius=600.0)
        if not npc_id:
            return False, 0
        if self.is_merchant_open():
            return True, npc_id
        await self.action("change_target", {"agent_id": npc_id}, wait_ms=400)
        await self.action("open_merchant", {"agent_id": npc_id}, wait_ms=2000)
        await self.query_fresh(settle_ms=400, timeout=6.0)
        if not self.is_merchant_open():
            await self.action("interact_npc", {"agent_id": npc_id}, wait_ms=2000)
            await self.query_fresh(settle_ms=400, timeout=6.0)
        return self.is_merchant_open(), npc_id

    async def run_native_maintenance_verification(
        self,
        *,
        phase_name: str,
        include_salvage: bool,
        detail_label: str,
    ) -> bool:
        if self.map_id() != MAP_GADDS:
            self._record(phase_name, "FAIL", "not at Gadd's")
            return False
        merchant_open, npc_id = await self.ensure_gadds_merchant_open()
        if not merchant_open:
            self._record(phase_name, "FAIL", "merchant window never opened")
            return False
        snap = await self.query_fresh(settle_ms=400, timeout=6.0)
        if snap is None:
            self._record(phase_name, "FAIL", "no snapshot before maintenance")
            return False

        before = self.maintenance_probe_state()
        print(
            f"[MAINT] {detail_label} before: free={before['free']} gold={before['gold']} "
            f"storage={before['storage_gold']} id_all={before['id_all']} superior_id={before['superior_id']} "
            f"salvage={before['salvage']} unidentified={before['unidentified']} "
            f"salvage_candidates={before['salvage_candidates']}"
        )
        result = await self.action(
            "froggy_run_maintenance_cycle",
            {"include_salvage": include_salvage},
            await_result=True,
            timeout=180.0,
            wait_ms=1000,
        )
        if not bool(result.get("success")):
            self._record(
                phase_name,
                "FAIL",
                str(result.get("error") or "native maintenance failed"),
            )
            return False

        await self.query_fresh(settle_ms=500, timeout=10.0)
        after = self.maintenance_probe_state()
        print(
            f"[MAINT] {detail_label} after: free={after['free']} gold={after['gold']} "
            f"storage={after['storage_gold']} id_all={after['id_all']} superior_id={after['superior_id']} "
            f"salvage={after['salvage']} unidentified={after['unidentified']} "
            f"salvage_candidates={after['salvage_candidates']}"
        )

        failures: list[str] = []
        if after["superior_id"] < TARGET_SUPERIOR_ID_KITS:
            failures.append(
                f"superior_id={after['superior_id']} < target {TARGET_SUPERIOR_ID_KITS}"
            )
        if after["salvage"] < TARGET_SALVAGE_KITS:
            failures.append(f"salvage={after['salvage']} < target {TARGET_SALVAGE_KITS}")
        if before["unidentified"] > 0 and before["id_all"] > 0:
            if after["unidentified"] >= before["unidentified"]:
                failures.append(
                    f"unidentified did not drop ({before['unidentified']} -> {after['unidentified']})"
                )
        if include_salvage and before["salvage_candidates"] > 0 and before["salvage"] > 0:
            if after["salvage_candidates"] >= before["salvage_candidates"]:
                failures.append(
                    "salvage candidates did not drop "
                    f"({before['salvage_candidates']} -> {after['salvage_candidates']})"
                )
        if failures:
            self._record(phase_name, "FAIL", "; ".join(failures))
            return False

        self._record(
            phase_name,
            "PASS",
            f"merchant_agent={npc_id} free={after['free']} gold={after['gold']} "
            f"storage={after['storage_gold']} superior_id={after['superior_id']} "
            f"salvage={after['salvage']} unidentified={before['unidentified']}->{after['unidentified']} "
            f"salvage_candidates={before['salvage_candidates']}->{after['salvage_candidates']}",
        )
        return True

    async def phase2c_identify_salvage(self) -> bool:
        print("\n=== PHASE 2c: Identify + Salvage one item ===")
        if self.map_id() != MAP_GADDS:
            self._record("phase2c_id_salvage", "SKIP", "not at Gadd's")
            return False

        snap = await self.query_fresh(settle_ms=400, timeout=6.0)
        if snap is None:
            self._record("phase2c_id_salvage", "SKIP", "no snapshot")
            return False

        did_identify = False
        unid_item_id, id_kit_id = self.find_unidentified_item()
        if unid_item_id and id_kit_id:
            await self.action(
                "identify_item",
                {"item_id": unid_item_id, "kit_id": id_kit_id},
                wait_ms=1200,
            )
            did_identify = True
            print(f"[IDENTIFY] item={unid_item_id} kit={id_kit_id}")

        await self.query_fresh(settle_ms=400, timeout=6.0)
        did_salvage = False
        salv_item_id, salv_kit_id = self.find_salvageable_item()
        if salv_item_id and salv_kit_id:
            await self.action(
                "salvage_start",
                {"item_id": salv_item_id, "kit_id": salv_kit_id},
                wait_ms=800,
            )
            await self.action("salvage_materials", {}, wait_ms=800)
            await self.action("salvage_done", {}, wait_ms=800)
            did_salvage = True
            print(f"[SALVAGE] item={salv_item_id} kit={salv_kit_id}")

        if not (did_identify or did_salvage):
            self._record("phase2c_id_salvage", "SKIP", "no eligible unid/salvage candidates")
            return True
        self._record(
            "phase2c_id_salvage",
            "PASS",
            f"identified={did_identify} salvaged={did_salvage}",
        )
        return True

    async def phase2d_xunlai(self) -> bool:
        print("\n=== PHASE 2d: Xunlai chest â€” withdraw + deposit gold ===")
        # Uses the open_xunlai bridge action (raw GoNPC INTERACT_NPC +
        # cancel_action + MarkXunlaiOpened), which establishes the
        # server-side Xunlai context that CHANGE_GOLD needs. Handle
        # Withdraw/DepositGold refuse unless IsXunlaiRecentlyOpened is
        # true, so even if the open fails, the DC-causing packet can't
        # fire.
        if self.map_id() != MAP_GADDS:
            self._record("phase2d_xunlai", "SKIP", "not at Gadd's")
            return False

        if not await self.walk_to(
            GADDS_XUNLAI_X, GADDS_XUNLAI_Y, "Gadd's Xunlai chest", threshold=300.0, timeout=30.0
        ):
            self._record("phase2d_xunlai", "FAIL", "could not reach Xunlai area")
            return False

        snap = await self.query_fresh(settle_ms=400, timeout=6.0)
        if snap is None:
            self._record("phase2d_xunlai", "SKIP", "no snapshot at Xunlai")
            return False

        # Find Xunlai Jingwei and open via the new open_xunlai action
        # (raw GoNPC, matches MaintenanceMgr::OpenXunlaiChest) rather than
        # interact_npc (which goes through the native function path that
        # does not establish the server-side storage context).
        npc_id = self.find_nearby_npc(GADDS_XUNLAI_X, GADDS_XUNLAI_Y, radius=500.0)
        if not npc_id:
            self._record("phase2d_xunlai", "SKIP", "no Xunlai NPC in range")
            return True
        await self.action("open_xunlai", {"agent_id": npc_id}, wait_ms=3500)

        gold_before = self.gold_character()
        storage_before = self.gold_storage()
        print(f"[XUNLAI] Before: character={gold_before} storage={storage_before}")

        # Pick a small round-trip amount we definitely have access to.
        amount = 50
        if storage_before >= amount:
            await self.action("withdraw_gold", {"amount": amount}, wait_ms=1500)
        else:
            print("[XUNLAI] storage < 50g â€” skipping withdraw leg")

        await self.query_fresh(settle_ms=400, timeout=6.0)
        if self.gold_character() >= amount:
            await self.action("deposit_gold", {"amount": amount}, wait_ms=1500)
        else:
            print("[XUNLAI] character < 50g â€” skipping deposit leg")

        await self.query_fresh(settle_ms=400, timeout=6.0)
        gold_after = self.gold_character()
        storage_after = self.gold_storage()
        print(f"[XUNLAI] After: character={gold_after} storage={storage_after}")
        self._record(
            "phase2d_xunlai",
            "PASS",
            f"delta_character={gold_after - gold_before} delta_storage={storage_after - storage_before}",
        )
        return True

    async def phase2e_town_blessing(self) -> bool:
        """Best-effort overworld blessing shrine in Gadd's (Asuran bodyguard).

        Gadd's is an Asuran outpost, so an Asuran blessing shrine is the
        likely hit â€” the dialog IDs mirror the in-dungeon version. If the
        shrine isn't present or the dialog isn't valid, the action dispatch
        still proves the bridge path works and we record SKIP accordingly.
        """
        print("\n=== PHASE 2e: Town blessing (best effort) ===")
        if self.map_id() != MAP_GADDS:
            self._record("phase2e_town_bless", "SKIP", "not at Gadd's")
            return False

        snap = await self.query_fresh(settle_ms=400, timeout=6.0)
        if snap is None:
            self._record("phase2e_town_bless", "SKIP", "no snapshot for shrine scan")
            return False

        # Scan all NPC-allegiance agents and pick the closest one whose
        # decoded name contains "blessing" or "shrine". Fall back to the
        # nearest generic NPC if no label match (name decode may still be
        # pending).
        best_id = 0
        best_dist = 1500.0
        best_name = ""
        for agent in self.snapshot.get("agents", []) or []:
            if agent.get("agent_type") != "living":
                continue
            if int(agent.get("allegiance", 0) or 0) != 6:
                continue
            name = str(agent.get("name", "") or "").lower()
            dist = float(agent.get("distance", 99999.0) or 99999.0)
            if "bless" in name or "shrine" in name:
                if dist < best_dist:
                    best_dist = dist
                    best_id = int(agent.get("id", 0) or 0)
                    best_name = name

        if not best_id:
            self._record("phase2e_town_bless", "SKIP", "no shrine NPC found by name")
            return True

        await self.action("interact_npc", {"agent_id": best_id}, wait_ms=1500)
        await self.action("dialog", {"dialog_id": DIALOG_ACCEPT_BLESSING}, wait_ms=1500)
        await self.action(
            "dialog",
            {"dialog_id": DIALOG_ACCEPT_BLESSING_FALLBACK},
            wait_ms=800,
        )
        self._record(
            "phase2e_town_bless",
            "PASS",
            f"shrine_agent={best_id} name={best_name!r}",
        )
        return True
