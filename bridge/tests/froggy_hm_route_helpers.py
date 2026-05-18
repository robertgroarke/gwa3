"""Waypoint-route helpers for the Froggy HM bridge test."""

import asyncio
import time

from ..farming_knowledge import DUNGEONS, MAP_NAMES
from ..gamedata import ITEM_NAMES


def _map_id(name: str) -> int:
    for map_id, map_name in MAP_NAMES.items():
        if map_name == name:
            return map_id
    raise KeyError(f"unknown map name: {name}")


def _item_model_id(name: str) -> int:
    for model_id, item_name in ITEM_NAMES.items():
        if item_name == name:
            return model_id
    raise KeyError(f"unknown item name: {name}")


_BOGROOT_GROWTHS = DUNGEONS["Bogroot Growths"]

MAP_GADDS = _BOGROOT_GROWTHS["entry_outpost_map_id"]
MAP_SPARKFLY = _map_id("Sparkfly Swamp")
MAP_BOGROOT_LVL1 = _BOGROOT_GROWTHS["level_map_ids"][0]

CHEAP_ID_KIT_MODEL = _item_model_id("Identification Kit")
SUPERIOR_ID_KIT_MODEL = _item_model_id("Superior Identification Kit")
ALT_ID_KIT_MODEL = 235
CHEAP_SALVAGE_KIT_MODEL = _item_model_id("Salvage Kit")
EXPERT_SALVAGE_MODEL = _item_model_id("Expert Salvage Kit")
RARE_SALVAGE_KIT_MODEL = _item_model_id("Salvage Kit 2")
SUPERIOR_SALVAGE_KIT_MODEL = _item_model_id("Superior Salvage Kit")
ALT_SALVAGE_KIT_MODEL = 243
TARGET_SUPERIOR_ID_KITS = 3
TARGET_SALVAGE_KITS = 10
MAINTENANCE_KIT_MODELS = {
    CHEAP_ID_KIT_MODEL,
    SUPERIOR_ID_KIT_MODEL,
    ALT_ID_KIT_MODEL,
    CHEAP_SALVAGE_KIT_MODEL,
    EXPERT_SALVAGE_MODEL,
    RARE_SALVAGE_KIT_MODEL,
    SUPERIOR_SALVAGE_KIT_MODEL,
    ALT_SALVAGE_KIT_MODEL,
}

QUEST_TEKKS_WAR = 0x339
DIALOG_TEKKS_ACCEPT = 0x833901
DIALOG_TEKKS_REWARD = 0x833907
DIALOG_TEKKS_DUNGEON_ENTRY = 0x833905
DIALOG_ACCEPT_BLESSING = 0x84
DIALOG_ACCEPT_BLESSING_FALLBACK = 0x85

GADDS_MERCHANT_X = -8374.0
GADDS_MERCHANT_Y = -22491.0
GADDS_XUNLAI_X = -10481.0
GADDS_XUNLAI_Y = -22787.0

TEKKS_X = 12396.0
TEKKS_Y = 22407.0
TEKKS_STAGE_X = 12061.0
TEKKS_STAGE_Y = 22485.0
TEKKS_SEARCH_X = 12396.0
TEKKS_SEARCH_Y = 22407.0
DUNGEON_STAGE_X = 12228.0
DUNGEON_STAGE_Y = 22677.0
SPARKFLY_DUNGEON_SIDE_THRESHOLD = 12000.0
DUNGEON_PORTAL_X = 13097.0
DUNGEON_PORTAL_Y = 26393.0
BLESSING_X = 19099.0
BLESSING_Y = 7762.0

EXIT_WP1 = (-10018.0, -21892.0)
EXIT_WP2 = (-9550.0, -20400.0)
EXIT_PUSH = (-9451.0, -19766.0)

SPARKFLY_TO_TEKKS_PATH = [
    (-4559.0, -14406.0, 1300.0, "Sparkfly waypoint 1"),
    (-5204.0, -9831.0, 1300.0, "Sparkfly waypoint 2"),
    (-928.0, -8699.0, 1300.0, "Sparkfly waypoint 3"),
    (4200.0, -4897.0, 1500.0, "Sparkfly waypoint 4"),
    (6114.0, 819.0, 1300.0, "Sparkfly waypoint 5"),
    (9500.0, 2281.0, 1300.0, "Sparkfly waypoint 6"),
    (11570.0, 6120.0, 1200.0, "Sparkfly waypoint 7"),
    (11025.0, 11710.0, 900.0, "Sparkfly waypoint 8"),
    (14624.0, 19314.0, 600.0, "Sparkfly waypoint 9"),
    (TEKKS_X, TEKKS_Y, 0.0, "Tekks"),
]

TEKKS_TO_DUNGEON_PATH = [
    (12228.0, 22677.0, 0.0, "Dungeon approach 1"),
    (12470.0, 25036.0, 0.0, "Dungeon approach 2"),
    (12968.0, 26219.0, 0.0, "Dungeon approach 3"),
    (DUNGEON_PORTAL_X, DUNGEON_PORTAL_Y, 0.0, "Bogroot portal"),
]

BOGROOT_TO_BLESSING_PATH = [
    (17026.0, 2168.0, 1300.0, "Bogroot start"),
    (BLESSING_X, BLESSING_Y, 900.0, "Blessing shrine"),
]

STANDARD_HEROES = [25, 14, 21, 4, 24, 15, 1]


class FroggyHmRouteMixin:
    async def _walk_waypoint_route(
        self,
        waypoints: list,
        *,
        route_label: str,
        final_threshold: float = 250.0,
        interior_threshold: float = 500.0,
        per_wp_timeout: float = 300.0,
        wipe_recovery_timeout: float = 180.0,
    ) -> tuple[bool, str]:
        if not waypoints:
            return True, "empty route"

        idx = 0
        visited_after_wipe = 0
        while idx < len(waypoints):
            wp = waypoints[idx]
            x, y, fight_range, label = float(wp[0]), float(wp[1]), float(wp[2]), str(wp[3])
            is_last = idx == len(waypoints) - 1
            threshold = final_threshold if is_last else interior_threshold

            print(f"[ROUTE {route_label}] waypoint {idx + 1}/{len(waypoints)}: {label} "
                  f"fight_range={fight_range:.0f}")

            if fight_range > 0.0:
                arrived = await self.aggro_walk_to(
                    x, y, label,
                    fight_range=fight_range,
                    threshold=threshold,
                    timeout=per_wp_timeout,
                )
            else:
                arrived = await self.walk_to(
                    x, y, label, threshold=threshold, timeout=per_wp_timeout
                )

            await self.query_fresh(settle_ms=200, timeout=4.0)
            if self.is_party_wiped():
                print(f"[ROUTE {route_label}] WIPE detected at {label} - "
                      f"player_hp={self.player_hp_fraction():.2f} party_hp="
                      f"{self.party_hp_fraction():.2f}. Waiting for rez...")
                await self.action("cancel_action", {}, wait_ms=500)
                if not await self._wait_for_party_rez(timeout=wipe_recovery_timeout):
                    return False, f"wipe at {label}, rez timeout"
                new_idx = self.nearest_waypoint_index(waypoints)
                print(f"[ROUTE {route_label}] Rezzed. Resuming at nearest "
                      f"waypoint {new_idx + 1}/{len(waypoints)} ({waypoints[new_idx][3]})")
                idx = new_idx
                visited_after_wipe += 1
                if visited_after_wipe > 3:
                    return False, f"too many wipes at {label}"
                continue

            if not arrived:
                return False, f"stuck at {label}"

            idx += 1

        return True, "ok"

    async def _wait_for_party_rez(self, timeout: float) -> bool:
        deadline = time.time() + timeout
        while time.time() < deadline:
            await self.query_fresh(settle_ms=300, timeout=5.0)
            if self.party_hp_fraction() > 0.5:
                return True
            await asyncio.sleep(3.0)
        return False

    async def _run_direct_tekks_debug_path(self, reason_label: str) -> tuple[bool, str]:
        print(f"[ROUTE Sparkfly->Tekks] Falling back to native-style Tekks recovery from {reason_label}")
        await self.query_fresh(settle_ms=200, timeout=4.0)
        px, py = self.pos()
        dist_stage = ((px - TEKKS_STAGE_X) ** 2 + (py - TEKKS_STAGE_Y) ** 2) ** 0.5
        dist_dungeon = ((px - DUNGEON_STAGE_X) ** 2 + (py - DUNGEON_STAGE_Y) ** 2) ** 0.5
        near_dungeon_side = (
            dist_stage <= SPARKFLY_DUNGEON_SIDE_THRESHOLD
            or dist_dungeon <= SPARKFLY_DUNGEON_SIDE_THRESHOLD
        )

        if near_dungeon_side:
            print("[ROUTE Sparkfly->Tekks] Near dungeon side detected; using short Tekks approach")
            stage_reached = await self.walk_to(
                TEKKS_STAGE_X, TEKKS_STAGE_Y, "Tekks stage", threshold=700.0, timeout=90.0
            )
            search_reached = await self.walk_to(
                TEKKS_SEARCH_X, TEKKS_SEARCH_Y, "Tekks search", threshold=700.0, timeout=90.0
            )
            await self.query_fresh(settle_ms=200, timeout=4.0)
            px, py = self.pos()
            dist_stage = ((px - TEKKS_STAGE_X) ** 2 + (py - TEKKS_STAGE_Y) ** 2) ** 0.5
            dist_search = ((px - TEKKS_SEARCH_X) ** 2 + (py - TEKKS_SEARCH_Y) ** 2) ** 0.5
            if stage_reached or search_reached or dist_stage <= 900.0 or dist_search <= 900.0:
                return True, "native short Tekks approach"
            return False, "native short Tekks approach failed"

        print("[ROUTE Sparkfly->Tekks] South-side fallback detected; rerunning late aggro route")
        late_path = SPARKFLY_TO_TEKKS_PATH[2:9]
        ok, detail = await self._walk_waypoint_route(
            late_path,
            route_label="Sparkfly->Tekks fallback aggro",
            final_threshold=600.0,
            interior_threshold=700.0,
            per_wp_timeout=300.0,
        )
        if not ok:
            return False, f"fallback aggro route: {detail}"

        stage_reached = await self.walk_to(
            TEKKS_STAGE_X, TEKKS_STAGE_Y, "Tekks stage", threshold=900.0, timeout=90.0
        )
        search_reached = await self.walk_to(
            TEKKS_SEARCH_X, TEKKS_SEARCH_Y, "Tekks search", threshold=900.0, timeout=90.0
        )
        await self.query_fresh(settle_ms=200, timeout=4.0)
        px, py = self.pos()
        dist_stage = ((px - TEKKS_STAGE_X) ** 2 + (py - TEKKS_STAGE_Y) ** 2) ** 0.5
        dist_search = ((px - TEKKS_SEARCH_X) ** 2 + (py - TEKKS_SEARCH_Y) ** 2) ** 0.5
        if stage_reached or search_reached or dist_stage <= 1100.0 or dist_search <= 1100.0:
            return True, "native full aggro Tekks fallback"
        return False, "native full aggro Tekks fallback failed"
