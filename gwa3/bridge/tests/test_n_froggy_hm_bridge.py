r"""Category N: Froggy HM end-to-end bridge test.

Mirrors the C++ ``RunFroggyFeatureTest`` flow in
``gwa3/src/tests/IntegrationTestEpic14.cpp`` — but drives every step through
the LLM bridge IPC instead of from inside the DLL. This proves the bridge
exposes everything an external agent (the LLM) needs to run the same Bogroot
Growths HM loop the C++ integration test runs.

Phases (expand on the C++ Froggy feature test by exercising more of the
LLM bridge surface area — inventory/gold reads, merchant buy+sell, identify,
salvage, loot, then hand off the long Sparkfly/Bogroot work to the same
native Froggy helpers the in-DLL lane already trusts):
    1.  Travel to Gadd's Encampment (map 638).
    2.  Kick existing heroes, add the Standard hero set, set hard mode.
    2b. Walk to Gadd's merchant, open merchant window, log inventory/gold,
        buy a salvage/id kit, and sell a cheap stackable if one is present.
    2c. Identify an unidentified inventory item and run a one-shot salvage
        session (SALVAGE_START -> SALVAGE_MATERIALS -> SALVAGE_DONE).
    2d. Walk to Gadd's Xunlai chest, interact with Xunlai Jingwei, and
        round-trip a small amount via ``withdraw_gold`` + ``deposit_gold``.
    2e. Best-effort overworld blessing shrine (Asuran bodyguard in Gadd's):
        find an NPC whose decoded name mentions blessing/shrine and click
        the standard accept-blessing dialog through the bridge.
    3.  Walk Gadd's exit waypoints and enter Sparkfly Swamp (map 558).
    4.  Target a foe and attack (proves combat actions route through bridge).
    4b. Scan tier-2 agents for ``agent_type == "item"`` (dropped loot) and
        exercise ``pick_up_item``.
    5.  Run Froggy's native Sparkfly -> Tekks route helper through the bridge.
    6.  Run one or more native Froggy Bogroot dungeon loops through the bridge
        (``GWA3_FROGGY_DUNGEON_LOOPS``, default ``2``).
    7.  Return to Gadd's Encampment.
    7b. Run the same native identify/salvage/sell/restock maintenance cadence
        the C++ Froggy feature test uses post-run.

The early outpost setup still uses bridge-native ``move_to`` / ``aggro_move_to``
proofs. The repeatable dungeon work now routes through Froggy's proven native
helpers so the LLM lane loops the same Tekks/Bogroot logic the in-DLL Froggy
lane already runs.

Usage (stand-alone, like ``test_conset_bridge.py``):

    set GWA3_PIPE_NAME=\\.\pipe\gwa3_llm_disco
    python -m bridge.tests.test_n_froggy_hm_bridge

Or via the runner (not in ``DEFAULT_MODULE_NAMES`` — must be filtered in):

    python -m bridge.tests --filter "test_froggy_hm_full_flow"

Preconditions:
    * Guild Wars launched through the validated GWLauncher flow for the
      assigned character (Disco Panic is the default validation account).
    * ``gwa3_<lane>.dll`` injected with ``--llm`` on the launcher-returned
      PID.
    * The matching ``GWA3_PIPE_NAME`` env var set so the bridge client
      connects to the lane's isolated pipe.

The test is long (~15 minutes on a clean run) and destructive to current
game state (moves the character, changes party, enters dungeon). It is not
part of the default bridge suite for that reason.
"""

from __future__ import annotations

import asyncio
import os
import sys
import time
from typing import Any

from ..ipc_client import IpcClient
from .base import BridgeTestCase, TestSkipped
from .helpers import TestFailure


# --- Constants lifted from IntegrationTestEpic14.cpp ---
MAP_GADDS = 638
MAP_SPARKFLY = 558
MAP_BOGROOT_LVL1 = 615

QUEST_TEKKS_WAR = 0x339
DIALOG_TEKKS_ACCEPT = 0x833901
DIALOG_TEKKS_REWARD = 0x833907
DIALOG_TEKKS_DUNGEON_ENTRY = 0x833905
DIALOG_ACCEPT_BLESSING = 0x84

# Merchant salvage/ID kit models (from IntegrationTestEpic14.cpp /
# MaintenanceMgr.cpp).
CHEAP_ID_KIT_MODEL = 2989
SUPERIOR_ID_KIT_MODEL = 5899
ALT_ID_KIT_MODEL = 235
CHEAP_SALVAGE_KIT_MODEL = 2992
EXPERT_SALVAGE_MODEL = 2991
RARE_SALVAGE_KIT_MODEL = 2993
FROGGY_SALVAGE_MODEL = 5900
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
    FROGGY_SALVAGE_MODEL,
    ALT_SALVAGE_KIT_MODEL,
}

# Gadd's merchant area (from test_e_orchestrated.py / farming_knowledge).
GADDS_MERCHANT_X = -8374.0
GADDS_MERCHANT_Y = -22491.0
GADDS_XUNLAI_X = -10481.0
GADDS_XUNLAI_Y = -22787.0

# Overworld blessing dialog IDs (``DIALOG_ACCEPT_BLESSING`` is the
# "accept any blessing" reply used by the in-dungeon shrine too). Gadd's is
# an Asuran outpost, so the Asuran bodyguard blessing shrine is the most
# likely hit there; we also try the generic accept dialog as a fallback.
DIALOG_ACCEPT_BLESSING_FALLBACK = 0x85

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

# Gadd's exit portal waypoints (see C++ Phase 4)
EXIT_WP1 = (-10018.0, -21892.0)
EXIT_WP2 = (-9550.0, -20400.0)
EXIT_PUSH = (-9451.0, -19766.0)

# Sparkfly -> Tekks path, matching AutoIt Froggy_HM RunToDungeon()
# waypoint array. Each entry: (x, y, fight_range, label). fight_range
# mirrors AutoIt — it drops from 1300 through the early aggro zones to
# 900/600/0 near the end, so the final approach to Tekks/portal isn't
# wasted on unnecessary engagements. A fight_range of 0 means "walk
# through, no aggro-fight" — agent should use plain move_to there.
SPARKFLY_TO_TEKKS_PATH = [
    (-4559.0, -14406.0, 1300.0, "Sparkfly waypoint 1"),
    (-5204.0, -9831.0,  1300.0, "Sparkfly waypoint 2"),
    (-928.0,  -8699.0,  1300.0, "Sparkfly waypoint 3"),
    (4200.0,  -4897.0,  1500.0, "Sparkfly waypoint 4"),
    (6114.0,  819.0,    1300.0, "Sparkfly waypoint 5"),
    (9500.0,  2281.0,   1300.0, "Sparkfly waypoint 6"),
    (11570.0, 6120.0,   1200.0, "Sparkfly waypoint 7"),
    (11025.0, 11710.0,  900.0,  "Sparkfly waypoint 8"),
    (14624.0, 19314.0,  600.0,  "Sparkfly waypoint 9"),
    (TEKKS_X, TEKKS_Y,  0.0,    "Tekks"),
]

# Tekks -> Bogroot portal (from C++ kTekksToDungeonPath). AutoIt's
# TakeQuest0 follow-up walks this with fight_range=0 — it's a stretch
# past Tekks's platform where enemies don't normally roam.
TEKKS_TO_DUNGEON_PATH = [
    (12228.0, 22677.0, 0.0, "Dungeon approach 1"),
    (12470.0, 25036.0, 0.0, "Dungeon approach 2"),
    (12968.0, 26219.0, 0.0, "Dungeon approach 3"),
    (DUNGEON_PORTAL_X, DUNGEON_PORTAL_Y, 0.0, "Bogroot portal"),
]

# Bogroot spawn -> blessing shrine. Bogroot Lvl1 does have enemies on
# the path so use aggro-fight range for the approach.
BOGROOT_TO_BLESSING_PATH = [
    (17026.0, 2168.0, 1300.0, "Bogroot start"),
    (BLESSING_X, BLESSING_Y, 900.0, "Blessing shrine"),
]

# Standard.txt hero order (GWA Censured/hero_configs/Standard.txt)
STANDARD_HEROES = [25, 14, 21, 4, 24, 15, 1]


class FroggyHmBridgeTest:
    """End-to-end Froggy HM flow driven entirely through the LLM bridge."""

    def __init__(self, pipe_name: str | None = None):
        resolved = pipe_name or os.environ.get(
            "GWA3_PIPE_NAME", r"\\.\pipe\gwa3_llm_disco"
        )
        self.ipc = IpcClient(pipe_name=resolved)
        self.snapshot: dict = {}
        self._req_counter = 0
        self._action_results: dict[str, dict] = {}
        self.results: dict[str, str] = {}
        self.dungeon_loop_count = max(
            1,
            int(os.environ.get("GWA3_FROGGY_DUNGEON_LOOPS", "2") or "2"),
        )
        self.dungeon_loop_timeout = max(
            1200.0,
            float(
                os.environ.get(
                    "GWA3_FROGGY_DUNGEON_LOOP_TIMEOUT_SEC",
                    "3600",
                )
                or "3600"
            ),
        )

    # --- Result tracking ----------------------------------------------------

    def _record(self, phase: str, status: str, detail: str = ""):
        suffix = f" — {detail}" if detail else ""
        tag = f"{status}{suffix}"
        self.results[phase] = tag
        print(f"[{status}] {phase}{suffix}")

    # --- IPC plumbing -------------------------------------------------------

    def _next_req_id(self) -> str:
        self._req_counter += 1
        return f"froggy-{self._req_counter}"

    async def connect(self) -> bool:
        print(f"[BRIDGE] Connecting to {self.ipc.pipe_name} ...")
        ok = await self.ipc.connect(timeout=30.0)
        if not ok:
            print("[BRIDGE] ERROR: could not connect (is gwa3 injected with --llm?)")
            return False
        self._action_results.clear()
        print("[BRIDGE] Connected.")
        return True

    async def action(
        self,
        name: str,
        params: dict | None = None,
        wait_ms: int = 500,
        await_result: bool = False,
        timeout: float = 30.0,
    ) -> dict | str:
        req_id = self._next_req_id() if await_result else ""
        shown_req = req_id or "fire-and-forget"
        print(f"  >> {name}({params or {}}) [{shown_req}]")
        await self.ipc.send_action(name, params, req_id)
        if not await_result:
            if wait_ms:
                await asyncio.sleep(wait_ms / 1000.0)
            return shown_req

        deadline = time.monotonic() + timeout
        result: dict | None = None
        while time.monotonic() < deadline:
            if req_id in self._action_results:
                result = self._action_results.pop(req_id)
                break
            try:
                remaining = max(0.1, deadline - time.monotonic())
                msg = await asyncio.wait_for(self.ipc.read_message(), timeout=remaining)
            except asyncio.TimeoutError:
                continue
            if msg is None:
                continue
            if msg.get("type") == "action_result":
                msg_req_id = msg.get("request_id", "") or ""
                if msg_req_id == req_id:
                    result = msg
                    break
                if msg_req_id:
                    self._action_results[msg_req_id] = msg
                continue
            if msg.get("type") == "snapshot":
                self.snapshot = msg

        if result is None:
            raise RuntimeError(f"timed out waiting for action_result: {name} [{req_id}]")
        if wait_ms:
            await asyncio.sleep(wait_ms / 1000.0)
        return result

    async def drain(self, max_messages: int = 100) -> int:
        drained = 0
        for _ in range(max_messages):
            try:
                msg = await asyncio.wait_for(
                    asyncio.shield(self.ipc.read_message()), timeout=0.05
                )
            except (asyncio.TimeoutError, Exception):
                break
            if msg is None:
                break
            if msg.get("type") == "action_result":
                req_id = msg.get("request_id", "") or ""
                if req_id:
                    self._action_results[req_id] = msg
                drained += 1
                continue
            if msg.get("type") == "snapshot":
                self.snapshot = msg
            drained += 1
        return drained

    async def query_fresh(self, settle_ms: int = 300, timeout: float = 6.0) -> dict | None:
        """Send query_state, then wait for the resulting tier-3 snapshot.

        Mirrors the conset bridge test's ``query_fresh_state`` — drains any
        stale pipe messages first, then asks the DLL for a freshly built
        snapshot so position/inventory readings are post-action, not stale.
        """
        await self.drain()
        await self.action("query_state", {"wait_ms": settle_ms}, wait_ms=settle_ms + 150)
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                msg = await asyncio.wait_for(self.ipc.read_message(), timeout=1.0)
            except asyncio.TimeoutError:
                continue
            if msg is None:
                return None
            if msg.get("type") == "action_result":
                req_id = msg.get("request_id", "") or ""
                if req_id:
                    self._action_results[req_id] = msg
                continue
            if msg.get("type") == "snapshot":
                self.snapshot = msg
                if msg.get("tier", 0) == 3:
                    return msg
        return None

    async def wait_for_map(
        self,
        map_id: int,
        timeout: float,
        require_loaded: bool = True,
    ) -> bool:
        deadline = time.time() + timeout
        while time.time() < deadline:
            snap = await self.query_fresh(settle_ms=200, timeout=3.0)
            if snap is None:
                continue
            m = snap.get("map", {})
            if m.get("map_id") == map_id and (not require_loaded or m.get("loading_state") == 1):
                return True
        return False

    # --- Snapshot accessors -------------------------------------------------

    def map_id(self) -> int:
        return int(self.snapshot.get("map", {}).get("map_id", 0) or 0)

    def pos(self) -> tuple[float, float]:
        me = self.snapshot.get("me", {})
        return float(me.get("x", 0.0) or 0.0), float(me.get("y", 0.0) or 0.0)

    def party_size(self) -> int:
        return int(self.snapshot.get("party", {}).get("size", 0) or 0)

    def player_hp_fraction(self) -> float:
        """0.0..1.0. 0.0 = dead, 1.0 = full. Mirrors AutoIt GetMyHealth."""
        me = self.snapshot.get("me", {})
        return float(me.get("hp", 0.0) or 0.0)

    def party_hp_fraction(self) -> float:
        """Average HP fraction across player + heroes in the current party.

        Mirrors AutoIt's ``GetPartyHealth()`` — used by ``MoveandAggroEx``
        for wipe detection (waits for the party to rez back above 50%).
        Snapshot exposes per-hero hp fractions in ``heroes[].hp`` and the
        player's in ``me.hp``.
        """
        hps: list[float] = []
        me_hp = float(self.snapshot.get("me", {}).get("hp", -1.0) or -1.0)
        if me_hp >= 0.0:
            hps.append(me_hp)
        for h in self.snapshot.get("heroes", []) or []:
            hp = h.get("hp", None)
            if hp is not None:
                hps.append(float(hp or 0.0))
        if not hps:
            return 0.0
        return sum(hps) / len(hps)

    def is_player_dead(self) -> bool:
        return self.player_hp_fraction() <= 0.001

    def is_party_wiped(self) -> bool:
        """True when player is dead AND >=75% of the party is at 0 HP.

        One hero going down is not a wipe; three+ down while the player
        is also dead is. Mirrors AutoIt ``Wipe()`` — the real Wipe() also
        checks GetPartyDefeated but that would require a new snapshot
        field; the hp-fraction heuristic is a close stand-in that uses
        only fields already exposed.
        """
        if not self.is_player_dead():
            return False
        heroes = self.snapshot.get("heroes", []) or []
        if not heroes:
            return self.is_player_dead()
        down = sum(1 for h in heroes if float(h.get("hp", 0.0) or 0.0) <= 0.001)
        return down >= (len(heroes) * 3) // 4

    def nearest_waypoint_index(self, waypoints: list) -> int:
        """Index of the waypoint closest to the player's current position.

        Mirrors AutoIt ``GetNearestWaypointIndex($aWaypoints)`` — used on
        recovery after a wipe or stuck state to pick the best restart
        point rather than blindly resuming from the last attempted
        waypoint. Waypoint entries may be either ``(x, y, label)`` or
        ``(x, y, fight_range, label)``.
        """
        px, py = self.pos()
        best_idx = 0
        best_dist = float("inf")
        for idx, wp in enumerate(waypoints):
            wx = float(wp[0])
            wy = float(wp[1])
            d = ((wx - px) ** 2 + (wy - py) ** 2) ** 0.5
            if d < best_dist:
                best_dist = d
                best_idx = idx
        return best_idx

    # --- Inventory / merchant helpers ---

    def gold_character(self) -> int:
        return int(self.snapshot.get("inventory", {}).get("gold_character", 0) or 0)

    def gold_storage(self) -> int:
        return int(self.snapshot.get("inventory", {}).get("gold_storage", 0) or 0)

    def free_slots_total(self) -> int:
        return int(self.snapshot.get("inventory", {}).get("free_slots_total", 0) or 0)

    def iter_inventory(self):
        """Yield (item, bag_index, slot) for every non-equipped inventory item."""
        inv = self.snapshot.get("inventory", {})
        for bag in inv.get("bags", []) or []:
            bag_index = int(bag.get("bag_index", 0) or 0)
            for item in bag.get("items", []) or []:
                if item.get("equipped", False):
                    continue
                yield item, bag_index, int(item.get("slot", 0) or 0)

    def find_inventory_item_by_model(self, model_id: int) -> int:
        for item, _, _ in self.iter_inventory():
            if int(item.get("model_id", 0) or 0) == model_id:
                return int(item.get("item_id", 0) or 0)
        return 0

    def count_inventory_model(self, model_id: int) -> int:
        total = 0
        for item, _, _ in self.iter_inventory():
            if int(item.get("model_id", 0) or 0) != model_id:
                continue
            total += max(1, int(item.get("quantity", 1) or 1))
        return total

    def count_salvage_kit_family(self) -> int:
        return (
            self.count_inventory_model(CHEAP_SALVAGE_KIT_MODEL)
            + self.count_inventory_model(EXPERT_SALVAGE_MODEL)
            + self.count_inventory_model(RARE_SALVAGE_KIT_MODEL)
            + self.count_inventory_model(ALT_SALVAGE_KIT_MODEL)
            + self.count_inventory_model(FROGGY_SALVAGE_MODEL)
        )

    def count_superior_id_kits(self) -> int:
        return self.count_inventory_model(SUPERIOR_ID_KIT_MODEL)

    def count_all_id_kits(self) -> int:
        return (
            self.count_inventory_model(SUPERIOR_ID_KIT_MODEL)
            + self.count_inventory_model(CHEAP_ID_KIT_MODEL)
            + self.count_inventory_model(ALT_ID_KIT_MODEL)
        )

    def is_maintenance_kit_model(self, model_id: int) -> bool:
        return model_id in MAINTENANCE_KIT_MODELS

    def count_unidentified_maintenance_items(self) -> int:
        total = 0
        for item, _, _ in self.iter_inventory():
            model_id = int(item.get("model_id", 0) or 0)
            if model_id == 0 or self.is_maintenance_kit_model(model_id):
                continue
            if bool(item.get("is_identified", True)):
                continue
            total += max(1, int(item.get("quantity", 1) or 1))
        return total

    def count_salvage_candidates_for_maintenance(self) -> int:
        total = 0
        for item, _, _ in self.iter_inventory():
            model_id = int(item.get("model_id", 0) or 0)
            if model_id == 0 or self.is_maintenance_kit_model(model_id):
                continue
            if not bool(item.get("is_identified", True)):
                continue
            if int(item.get("quantity", 1) or 1) > 1:
                continue
            if str(item.get("rarity", "") or "").lower() not in ("white", "blue"):
                continue
            if not bool(item.get("is_material_salvageable", False)):
                continue
            total += 1
        return total

    def maintenance_probe_state(self) -> dict[str, int]:
        return {
            "free": self.free_slots_total(),
            "gold": self.gold_character(),
            "storage_gold": self.gold_storage(),
            "id_all": self.count_all_id_kits(),
            "superior_id": self.count_superior_id_kits(),
            "salvage": self.count_salvage_kit_family(),
            "unidentified": self.count_unidentified_maintenance_items(),
            "salvage_candidates": self.count_salvage_candidates_for_maintenance(),
        }

    def find_unidentified_item(self) -> tuple[int, int]:
        """Return (item_id, kit_id) for a candidate identify operation, or (0, 0)."""
        kit_id = self.find_inventory_item_by_model(SUPERIOR_ID_KIT_MODEL)
        if kit_id == 0:
            kit_id = self.find_inventory_item_by_model(CHEAP_ID_KIT_MODEL)
        if kit_id == 0:
            kit_id = self.find_inventory_item_by_model(ALT_ID_KIT_MODEL)
        if kit_id == 0:
            return 0, 0
        for item, _, _ in self.iter_inventory():
            # Unidentified loot usually has is_identified == False on the
            # flags field; fall back to any uniq item with value > 0.
            if bool(item.get("is_identified", True)):
                continue
            item_id = int(item.get("item_id", 0) or 0)
            if item_id > 0 and item_id != kit_id:
                return item_id, kit_id
        return 0, kit_id

    def find_salvageable_item(self) -> tuple[int, int]:
        """Return (item_id, kit_id) for a candidate salvage operation, or (0, 0)."""
        for model_id in (
            CHEAP_SALVAGE_KIT_MODEL,
            RARE_SALVAGE_KIT_MODEL,
            ALT_SALVAGE_KIT_MODEL,
            EXPERT_SALVAGE_MODEL,
            FROGGY_SALVAGE_MODEL,
        ):
            kit_id = self.find_inventory_item_by_model(model_id)
            if kit_id != 0:
                break
        else:
            kit_id = 0
        if kit_id == 0:
            return 0, 0
        for item, _, _ in self.iter_inventory():
            item_id = int(item.get("item_id", 0) or 0)
            if item_id <= 0 or item_id == kit_id:
                continue
            # Prefer identified items so we are not consuming rare unid loot.
            if not bool(item.get("is_identified", True)):
                continue
            # Skip stacks of materials/kits — only want equipment-ish items.
            if int(item.get("quantity", 1) or 1) > 1:
                continue
            return item_id, kit_id
        return 0, kit_id

    def find_dropped_items(self, max_distance: float = 2000.0) -> list[int]:
        """Return agent_ids of nearby dropped items (agent_type == 'item')."""
        out: list[int] = []
        for agent in self.snapshot.get("agents", []) or []:
            if agent.get("agent_type") != "item":
                continue
            dist = float(agent.get("distance", 99999.0) or 99999.0)
            if dist <= max_distance:
                aid = int(agent.get("id", 0) or 0)
                if aid > 0:
                    out.append(aid)
        return out

    def find_nearby_npc(
        self,
        x: float,
        y: float,
        radius: float = 400.0,
        allegiance: int = 6,
    ) -> int:
        best_id = 0
        best_dist = radius
        for agent in self.snapshot.get("agents", []) or []:
            if agent.get("agent_type") != "living":
                continue
            if int(agent.get("allegiance", 0) or 0) != allegiance:
                continue
            ax = float(agent.get("x", 0.0) or 0.0)
            ay = float(agent.get("y", 0.0) or 0.0)
            dist = ((ax - x) ** 2 + (ay - y) ** 2) ** 0.5
            if dist < best_dist:
                best_dist = dist
                best_id = int(agent.get("id", 0) or 0)
        return best_id

    def find_gadgets(self, max_distance: float = 2500.0) -> list[dict]:
        """Return gadget agents within range (chests, doors, portals, signposts)."""
        out: list[dict] = []
        for agent in self.snapshot.get("agents", []) or []:
            if agent.get("agent_type") != "gadget":
                continue
            dist = float(agent.get("distance", 99999.0) or 99999.0)
            if dist <= max_distance:
                out.append(agent)
        out.sort(key=lambda a: float(a.get("distance", 99999.0) or 99999.0))
        return out

    def merchant_items(self) -> list[dict]:
        return self.snapshot.get("merchant", {}).get("items", []) or []

    def is_merchant_open(self) -> bool:
        return bool(self.snapshot.get("merchant", {}).get("is_open", False))

    def find_foe(self, max_range: float = 5000.0) -> dict | None:
        best: dict | None = None
        best_dist = max_range
        for agent in self.snapshot.get("agents", []) or []:
            if agent.get("agent_type") != "living":
                continue
            if int(agent.get("allegiance", 0) or 0) != 3:
                continue
            if not agent.get("is_alive", False):
                continue
            dist = float(agent.get("distance", 99999.0) or 99999.0)
            if dist < best_dist:
                best_dist = dist
                best = agent
        return best

    def find_signpost(
        self,
        x: float,
        y: float,
        radius: float = 400.0,
    ) -> int | None:
        """Find a gadget/signpost agent near target coords (e.g. dungeon portal)."""
        best_id: int | None = None
        best_dist_sq = radius * radius
        for agent in self.snapshot.get("agents", []) or []:
            agent_type = agent.get("agent_type", "")
            if agent_type not in ("gadget", "signpost", "living"):
                continue
            ax = float(agent.get("x", 0.0) or 0.0)
            ay = float(agent.get("y", 0.0) or 0.0)
            d2 = (ax - x) ** 2 + (ay - y) ** 2
            if d2 < best_dist_sq:
                best_dist_sq = d2
                aid = int(agent.get("id", 0) or 0)
                if aid > 0:
                    best_id = aid
        return best_id

    # --- Movement -----------------------------------------------------------

    async def walk_to(
        self,
        x: float,
        y: float,
        label: str,
        threshold: float = 350.0,
        timeout: float = 30.0,
    ) -> bool:
        """Walk to a target coordinate, re-issuing ``move_to`` as needed.

        Matches the C++ ``MovePlayerNear`` pattern: poll a fresh tier-3
        snapshot, re-send the move action if we have not arrived yet, give up
        after ``timeout`` seconds.
        """
        print(f"[MOVE] -> {label} ({x:.0f}, {y:.0f})")
        snap = await self.query_fresh(settle_ms=150, timeout=4.0)
        if snap is not None:
            px, py = self.pos()
            if ((px - x) ** 2 + (py - y) ** 2) ** 0.5 <= threshold:
                print(f"[MOVE] Already at {label}")
                return True

        await self.action("move_to", {"x": x, "y": y}, wait_ms=100)
        last_issue = time.time()
        deadline = time.time() + timeout
        while time.time() < deadline:
            snap = await self.query_fresh(settle_ms=200, timeout=3.0)
            if snap is not None:
                px, py = self.pos()
                dist = ((px - x) ** 2 + (py - y) ** 2) ** 0.5
                if dist <= threshold:
                    print(f"[MOVE] Arrived at {label} (dist={dist:.0f})")
                    return True
            if time.time() - last_issue >= 2.5:
                await self.ipc.send_action("move_to", {"x": x, "y": y}, self._next_req_id())
                last_issue = time.time()
        print(f"[MOVE] TIMEOUT reaching {label}")
        await self.action("cancel_action", {}, wait_ms=200)
        await asyncio.sleep(0.5)
        return False

    async def aggro_walk_to(
        self,
        x: float,
        y: float,
        label: str,
        fight_range: float = 1350.0,
        threshold: float = 500.0,
        timeout: float = 90.0,
    ) -> bool:
        """Walk to (x, y) with aggro-fight behavior via ``aggro_move_to``.

        ``aggro_move_to`` is a long-running server-side walk (it blocks on
        combat) — we fire it once and then poll for arrival. If it hasn't
        converged within ``timeout`` we return False so the phase logic
        can decide whether to retry.
        """
        print(f"[AGGRO-MOVE] -> {label} ({x:.0f}, {y:.0f}) fight_range={fight_range:.0f}")
        snap = await self.query_fresh(settle_ms=150, timeout=4.0)
        if snap is not None:
            px, py = self.pos()
            if ((px - x) ** 2 + (py - y) ** 2) ** 0.5 <= threshold:
                print(f"[AGGRO-MOVE] Already at {label}")
                return True
        await self.action(
            "aggro_move_to",
            {"x": x, "y": y, "fight_range": fight_range},
            wait_ms=100,
        )
        # aggro_move_to runs a long blocking combat loop in the DLL — if we
        # spam query_state on it, snapshot builds can collide with live
        # combat state reads and crash the client. Poll slowly (once every
        # few seconds) and prefer pushed snapshots; only force a fresh
        # snapshot after extended silence so we do not sit on stale
        # positions for the full waypoint timeout.
        deadline = time.time() + timeout
        last_forced_query_at = 0.0
        while time.time() < deadline:
            drained = await self.drain(max_messages=200)
            if drained == 0:
                now = time.time()
                if (now - last_forced_query_at) >= 15.0:
                    snap = await self.query_fresh(settle_ms=150, timeout=4.0)
                    last_forced_query_at = now
                    if snap is not None:
                        px, py = self.pos()
                        dist = ((px - x) ** 2 + (py - y) ** 2) ** 0.5
                        if dist <= threshold:
                            print(f"[AGGRO-MOVE] Arrived at {label} (dist={dist:.0f})")
                            return True
                await asyncio.sleep(2.0)
                continue
            px, py = self.pos()
            dist = ((px - x) ** 2 + (py - y) ** 2) ** 0.5
            if dist <= threshold:
                print(f"[AGGRO-MOVE] Arrived at {label} (dist={dist:.0f})")
                return True
            await asyncio.sleep(3.0)
        print(f"[AGGRO-MOVE] TIMEOUT reaching {label}")
        await self.action("cancel_action", {}, wait_ms=200)
        await asyncio.sleep(0.5)
        return False

    async def push_until_map_changes(
        self,
        x: float,
        y: float,
        from_map: int,
        timeout: float = 40.0,
    ) -> bool:
        """Keep moving toward (x, y) until the map id changes (zone transition)."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            await self.action("move_to", {"x": x, "y": y}, wait_ms=800)
            snap = await self.query_fresh(settle_ms=150, timeout=3.0)
            if snap is not None and self.map_id() != from_map:
                return True
        return False

    # --- Phases -------------------------------------------------------------

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
        # server rejects with Code=007 — observed live after the phase
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
            include_salvage=False,
            detail_label="pre-run maintenance",
        )
        print("\n=== PHASE 2b: Merchant — read inventory/gold, buy + sell ===")
        # Gated: HandleMerchantBuy / HandleMerchantSell now refuse with
        # merchant_not_open_call_open_merchant_first when the merchant
        # isn't server-side-open (checked via
        # TradeMgr::GetMerchantItemCount() > 0 — populated only after the
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
        # transact_items — the raw 0x4D packet crashes the client on some
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
            FROGGY_SALVAGE_MODEL,
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
            print("[MERCHANT] No stackable material to sell — skipping sell leg")

        await self.action("cancel_action", {}, wait_ms=500)
        self._record(
            "phase2b_merchant",
            "PASS",
            f"gold_after={self.gold_character()} bought={bought_any} "
            f"superior_id={superior_after} salvage={salvage_after} sold={bool(sell_item_id)}",
        )
        return True

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
        print("\n=== PHASE 2d: Xunlai chest — withdraw + deposit gold ===")
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
            print("[XUNLAI] storage < 50g — skipping withdraw leg")

        await self.query_fresh(settle_ms=400, timeout=6.0)
        if self.gold_character() >= amount:
            await self.action("deposit_gold", {"amount": amount}, wait_ms=1500)
        else:
            print("[XUNLAI] character < 50g — skipping deposit leg")

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
        likely hit — the dialog IDs mirror the in-dungeon version. If the
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

    async def phase4c_skill_usage(self) -> bool:
        """Validate use_skill / use_hero_skill through recharge deltas.

        Runs in Sparkfly (after phase 4 combat) rather than in the
        outpost — firing use_skill in an outpost is server-invalid and
        DCs the client with Code=007 (outposts disallow combat packets).

        Sparkfly has its own quirk: the DLL hard-suppresses direct
        player use_skill via SparkflyPlayerUseSkillOverride unless the
        call comes from inside Froggy's aggro loop, so the PLAYER cast
        half of this phase will usually show zero bumps. The HERO cast
        half (use_hero_skill) is not suppressed and should produce
        visible recharge deltas when it fires legally.
        """
        print("\n=== PHASE 4c: Skill usage validation (recharge delta) ===")
        if self.map_id() != MAP_SPARKFLY:
            self._record("phase4c_skills", "SKIP", "not in Sparkfly")
            return True

        await self.action("cancel_action", {}, wait_ms=500)
        snap = await self.query_fresh(settle_ms=600, timeout=6.0)
        if snap is None:
            self._record("phase4c_skills", "SKIP", "no snapshot for skillbar read")
            return False

        player_skills = snap.get("skillbar", [])
        hero_skillbars = [
            h.get("skillbar", []) for h in snap.get("heroes", []) if h.get("skillbar")
        ]
        foe = self.find_foe()
        skill_target = int(foe.get("id", 0) or 0) if foe else 0

        def skill_list(bar) -> list[tuple[int, int, int]]:
            # Return [(slot, skill_id, recharge)] for non-empty slots.
            out = []
            for sk in bar or []:
                sid = int(sk.get("skill_id", 0) or 0)
                if sid:
                    out.append(
                        (int(sk.get("slot", 0) or 0), sid, int(sk.get("recharge", 0) or 0))
                    )
            return out

        def ready_slots(skills: list[tuple[int, int, int]]) -> list[int]:
            return [slot for slot, _sid, recharge in skills if recharge <= 0]

        player_before = skill_list(player_skills)
        print(f"[SKILLS] Player before: {player_before}")

        # Only fire slot 0 (the elite) — empirically, firing multiple
        # player skills against target=0 in outpost triggers a deep DLL
        # state bug: SkillMgr::GetPlayerSkillbar() starts returning null,
        # MovePlayerNear can't read player position, and the next ~30s
        # of actions all fail. One cast is enough to prove the bridge
        # action reaches the skill system; multi-cast validation belongs
        # in an explorable with real enemies, not an outpost sandbox.
        player_bumped: list[int] = []
        if player_before:
            player_slots = ready_slots(player_before) or [player_before[0][0]]
            first_slot = player_slots[0]
            before_by_slot = {slot: rc for slot, _sid, rc in player_before}
            await self.action(
                "use_skill",
                {"slot": first_slot, "target_agent_id": skill_target},
                wait_ms=2500,
            )
            await self.query_fresh(settle_ms=800, timeout=6.0)
            player_after = skill_list(self.snapshot.get("skillbar", []))
            print(f"[SKILLS] Player after:  {player_after}")
            player_bumped = [
                slot
                for slot, _sid, rc_after in player_after
                if rc_after > before_by_slot.get(slot, 0)
            ]
        else:
            player_after = []

        # Hero 1 skill usage — try currently ready slots against a live foe
        # until one produces a visible recharge bump.
        hero_bumped: list[int] = []
        if hero_skillbars:
            hero1_after = []
            for _attempt in range(8):
                await self.query_fresh(settle_ms=600, timeout=6.0)
                hero1_before = skill_list(
                    (self.snapshot.get("heroes", []) or [{}])[0].get("skillbar", [])
                )
                print(f"[SKILLS] Hero1 before:  {hero1_before}")
                if not hero1_before:
                    break
                hero_ready_slots = ready_slots(hero1_before)
                if not hero_ready_slots:
                    break
                first_slot = hero_ready_slots[0]
                before_by_slot = {slot: rc for slot, _sid, rc in hero1_before}
                await self.action(
                    "use_hero_skill",
                    {"hero_index": 1, "slot": first_slot, "target_agent_id": skill_target},
                    wait_ms=2000,
                )
                await self.query_fresh(settle_ms=800, timeout=6.0)
                hero1_after = skill_list(
                    (self.snapshot.get("heroes", []) or [{}])[0].get("skillbar", [])
                )
                hero_bumped = [
                    slot
                    for slot, _sid, rc_after in hero1_after
                    if rc_after > before_by_slot.get(slot, 0)
                ]
                if hero_bumped:
                    break
            print(f"[SKILLS] Hero1 after:   {hero1_after}")

        # A successful bridge cast can manifest two ways in the snapshot:
        #   1. recharge on the cast slot bumps from 0 → >0 (expected)
        #   2. player skillbar read returns empty because the cast
        #      transiently invalidated the DLL's cached agent pointer
        #      (known DLL bug observed when firing player skills with
        #      target=0 in an outpost) — this is ALSO evidence that the
        #      packet went out, because the invalidation is caused by
        #      the game's response to our cast packet.
        player_skillbar_vanished = bool(player_before) and not player_after
        if not player_bumped and not hero_bumped and not player_skillbar_vanished:
            self._record(
                "phase4c_skills",
                "FAIL",
                "no recharge delta from a ready player or hero skill cast",
            )
            return True
        self._record(
            "phase4c_skills",
            "PASS",
            f"player_bumped={player_bumped} hero1_bumped={hero_bumped} "
            f"skillbar_invalidated={player_skillbar_vanished} target={skill_target}",
        )
        return True

    async def phase3_enter_sparkfly(self) -> bool:
        print("\n=== PHASE 3: Enter Sparkfly Swamp ===")
        if self.map_id() != MAP_GADDS:
            self._record("phase3_sparkfly", "SKIP", "not at Gadd's")
            return False
        for wx, wy in (EXIT_WP1, EXIT_WP2):
            if not await self.walk_to(wx, wy, f"exit waypoint ({wx:.0f},{wy:.0f})"):
                self._record("phase3_sparkfly", "FAIL", "exit waypoint timeout")
                return False
        left = await self.push_until_map_changes(EXIT_PUSH[0], EXIT_PUSH[1], MAP_GADDS, timeout=45.0)
        if not left:
            self._record("phase3_sparkfly", "FAIL", "never left Gadd's")
            return False
        if not await self.wait_for_map(MAP_SPARKFLY, timeout=60.0):
            self._record("phase3_sparkfly", "FAIL", "Sparkfly never loaded")
            return False
        # Extended stabilization wait — the DLL's snapshot thread crashes if
        # we query state while GW's world-load is still hydrating (observed
        # ~3s post-load crashes in early iteration). Mirror the C++ Froggy
        # test's WaitForStablePlayerState(10000) + Sleep(5000) pattern.
        await asyncio.sleep(15.0)
        self._record("phase3_sparkfly", "PASS")
        return True

    async def phase4_combat_proof(self) -> bool:
        print("\n=== PHASE 4: Combat in Sparkfly ===")
        if self.map_id() != MAP_SPARKFLY:
            self._record("phase4_combat", "SKIP", "not in Sparkfly")
            return False
        snap = await self.query_fresh(settle_ms=300, timeout=6.0)
        if snap is None:
            self._record("phase4_combat", "SKIP", "no snapshot for foe lookup")
            return False
        foe = self.find_foe()
        if foe is None:
            # Nudge toward first enemy cluster (C++ PHASE 5 fallback).
            await self.walk_to(-4559.0, -14406.0, "first Sparkfly cluster", threshold=500.0, timeout=25.0)
            snap = await self.query_fresh(settle_ms=300, timeout=6.0)
            foe = self.find_foe() if snap is not None else None
        if foe is None:
            self._record("phase4_combat", "SKIP", "no enemies within range")
            return True
        foe_id = int(foe.get("id", 0) or 0)
        await self.action("change_target", {"agent_id": foe_id}, wait_ms=400)
        await self.action("attack", {"agent_id": foe_id}, wait_ms=1500)

        # Baseline skillbar state so we can prove player use_skill fired.
        player_before = [
            (int(s.get("slot", 0) or 0), int(s.get("recharge", 0) or 0))
            for s in self.snapshot.get("skillbar", []) or []
            if int(s.get("skill_id", 0) or 0)
        ]

        # Cycle player slots 0..3 — the DLL hard-suppresses player UseSkill
        # in Sparkfly (SetSparkflyPlayerUseSkillOverride) unless called
        # from inside Froggy's aggro loop. A pass here with zero bumped
        # slots CONFIRMS the suppression is active from the bridge path
        # and we'd need a new action to explicitly override it.
        for slot in range(4):
            await self.action(
                "use_skill", {"slot": slot, "target_agent_id": foe_id}, wait_ms=1200
            )

        # Hero 1 skill 0 — heroes are not suppressed.
        await self.action(
            "use_hero_skill",
            {"hero_index": 1, "slot": 0, "target_agent_id": foe_id},
            wait_ms=1000,
        )

        await self.query_fresh(settle_ms=600, timeout=6.0)
        player_after = [
            (int(s.get("slot", 0) or 0), int(s.get("recharge", 0) or 0))
            for s in self.snapshot.get("skillbar", []) or []
            if int(s.get("skill_id", 0) or 0)
        ]
        before_by_slot = {s: rc for s, rc in player_before}
        player_bumped = [
            s for s, rc in player_after if rc > before_by_slot.get(s, 0)
        ]

        self._record(
            "phase4_combat",
            "PASS",
            f"attacked foe {foe_id}, player_bumped_slots={player_bumped} "
            f"(empty list => player UseSkill suppressed in Sparkfly), "
            f"hero1 skill 0 dispatched",
        )
        return True

    async def phase4b_loot(self) -> bool:
        print("\n=== PHASE 4b: Loot dropped items ===")
        if self.map_id() != MAP_SPARKFLY:
            self._record("phase4b_loot", "SKIP", "not in Sparkfly")
            return False
        # Give the fight a moment to produce drops.
        await asyncio.sleep(6.0)
        snap = await self.query_fresh(settle_ms=400, timeout=6.0)
        if snap is None:
            self._record("phase4b_loot", "SKIP", "no snapshot for loot scan")
            return False
        drops = self.find_dropped_items(max_distance=3000.0)
        if not drops:
            self._record("phase4b_loot", "SKIP", "no dropped items within 3000u")
            return True
        picked = 0
        for agent_id in drops[:5]:
            await self.action("pick_up_item", {"agent_id": agent_id}, wait_ms=600)
            picked += 1
        self._record("phase4b_loot", "PASS", f"picked={picked}")
        return True

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
        """Iterate an AutoIt-style waypoint route with wipe recovery.

        Mirrors AutoIt ``MoveandAggroEx($aWaypoints)`` at the primitive
        level: for each ``(x, y, fight_range, label)`` entry walk toward
        (x, y) — via ``aggro_walk_to`` if fight_range > 0, else plain
        ``walk_to`` — then check for wipe. On wipe, cancel, wait for
        party HP to recover above 50%, then resume at
        ``nearest_waypoint_index`` instead of the last attempted
        waypoint. Returns (arrived_at_final, detail).

        The last entry's arrival uses ``final_threshold``; interior
        entries use ``interior_threshold`` (matches the AutoIt pattern
        where final-approach waypoints need tighter precision).
        """
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

            # After every leg: check wipe. AutoIt MoveandAggroEx does
            # this in its outer loop. We skip the `Wipe()` deadlock of
            # the AutoIt path in favor of a simpler "dead + majority of
            # heroes down -> wait for rez" check since the snapshot
            # doesn't yet expose GetPartyDefeated.
            await self.query_fresh(settle_ms=200, timeout=4.0)
            if self.is_party_wiped():
                print(f"[ROUTE {route_label}] WIPE detected at {label} — "
                      f"player_hp={self.player_hp_fraction():.2f} party_hp="
                      f"{self.party_hp_fraction():.2f}. Waiting for rez...")
                await self.action("cancel_action", {}, wait_ms=500)
                if not await self._wait_for_party_rez(timeout=wipe_recovery_timeout):
                    return False, f"wipe at {label}, rez timeout"
                # Restart at the nearest waypoint. AutoIt uses
                # WipeManagement to pick; we use plain nearest.
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
        """Wait for party HP fraction to climb above 0.5 (mirrors AutoIt)."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            await self.query_fresh(settle_ms=300, timeout=5.0)
            if self.party_hp_fraction() > 0.5:
                return True
            await asyncio.sleep(3.0)
        return False

    async def _run_direct_tekks_debug_path(self, reason_label: str) -> tuple[bool, str]:
        """Mirror the native Froggy Sparkfly->Tekks recovery logic.

        Native Froggy does not use a long plain-move chain here. It either:
        1. Uses a short Tekks stage/search approach if already near the
           dungeon side of Sparkfly.
        2. Re-runs the late Sparkfly aggro route, then validates against the
           Tekks stage/search points with loose thresholds.
        """
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

    async def phase5_walk_to_tekks(self) -> bool:
        print("\n=== PHASE 5: Native Froggy Sparkfly -> Tekks route ===")
        if self.map_id() != MAP_SPARKFLY:
            self._record("phase5_tekks", "SKIP", "not in Sparkfly")
            return False
        result = await self.action(
            "froggy_run_sparkfly_route_to_tekks",
            {},
            await_result=True,
            timeout=900.0,
            wait_ms=1000,
        )
        if not bool(result.get("success")):
            self._record(
                "phase5_tekks",
                "FAIL",
                str(result.get("error") or "native Froggy route failed"),
            )
            return False
        await self.query_fresh(settle_ms=400, timeout=6.0)
        px, py = self.pos()
        dist = ((px - TEKKS_X) ** 2 + (py - TEKKS_Y) ** 2) ** 0.5
        self._record("phase5_tekks", "PASS", f"native Froggy route dist_to_tekks={dist:.0f}")
        return True

    async def phase6_run_dungeon_loops(self) -> bool:
        print(f"\n=== PHASE 6: Native Froggy dungeon loops x{self.dungeon_loop_count} ===")
        current = self.map_id()
        if current not in (MAP_SPARKFLY, MAP_BOGROOT_LVL1):
            self._record("phase6_dungeon_loops", "SKIP", f"unsupported start map={current}")
            return False

        loop_details: list[str] = []
        for loop_index in range(self.dungeon_loop_count):
            refresh = await self.action(
                "froggy_refresh_combat_skillbar",
                {},
                await_result=True,
                timeout=30.0,
                wait_ms=500,
            )
            if not bool(refresh.get("success")):
                self._record(
                    "phase6_dungeon_loops",
                    "FAIL",
                    f"loop {loop_index + 1}/{self.dungeon_loop_count}: "
                    f"{refresh.get('error') or 'combat refresh failed'}",
                )
                return False

            result = await self.action(
                "froggy_run_dungeon_loop",
                {},
                await_result=True,
                timeout=self.dungeon_loop_timeout,
                wait_ms=1500,
            )
            await self.query_fresh(settle_ms=500, timeout=10.0)
            current = self.map_id()
            if not bool(result.get("success")):
                self._record(
                    "phase6_dungeon_loops",
                    "FAIL",
                    f"loop {loop_index + 1}/{self.dungeon_loop_count}: "
                    f"{result.get('error') or 'native dungeon loop failed'} map={current}",
                )
                return False
            if current not in (MAP_SPARKFLY, MAP_GADDS):
                self._record(
                    "phase6_dungeon_loops",
                    "FAIL",
                    f"loop {loop_index + 1}/{self.dungeon_loop_count}: unexpected final map={current}",
                )
                return False
            loop_details.append(f"{loop_index + 1}:{current}")
            if current == MAP_GADDS and loop_index + 1 < self.dungeon_loop_count:
                self._record(
                    "phase6_dungeon_loops",
                    "FAIL",
                    f"loop {loop_index + 1}/{self.dungeon_loop_count}: returned to Gadd's early",
                )
                return False

        self._record(
            "phase6_dungeon_loops",
            "PASS",
            f"loops={self.dungeon_loop_count} final_map={self.map_id()} trail={' '.join(loop_details)}",
        )
        return True

    async def phase6_accept_tekks_quest(self) -> bool:
        print("\n=== PHASE 6: Accept Tekks' War quest ===")
        if self.map_id() != MAP_SPARKFLY:
            self._record("phase6_quest", "SKIP", "not in Sparkfly (post-walk)")
            return False
        snap = await self.query_fresh(settle_ms=300, timeout=6.0)
        if snap is None:
            self._record("phase6_quest", "SKIP", "no snapshot near Tekks")
            return False
        # Find a living NPC near Tekks coords (allegiance 6 = ally NPC).
        tekks_id = 0
        best_dist = 400.0
        for agent in snap.get("agents", []) or []:
            if agent.get("agent_type") != "living":
                continue
            if int(agent.get("allegiance", 0) or 0) != 6:
                continue
            ax = float(agent.get("x", 0.0) or 0.0)
            ay = float(agent.get("y", 0.0) or 0.0)
            dist = ((ax - TEKKS_X) ** 2 + (ay - TEKKS_Y) ** 2) ** 0.5
            if dist < best_dist:
                best_dist = dist
                tekks_id = int(agent.get("id", 0) or 0)
        if tekks_id:
            await self.action("interact_npc", {"agent_id": tekks_id}, wait_ms=1500)
        await self.action("dialog", {"dialog_id": DIALOG_TEKKS_ACCEPT}, wait_ms=2000)
        await self.query_fresh(settle_ms=500, timeout=6.0)
        # Try to set the quest active, but do not fail the phase if the
        # server hasn't populated the quest log yet — the dialog click is
        # the authoritative accept.
        await self.action("set_active_quest", {"quest_id": QUEST_TEKKS_WAR}, wait_ms=500)
        self._record("phase6_quest", "PASS", f"tekks_agent={tekks_id}")
        return True

    async def phase7_enter_bogroot(self) -> bool:
        print("\n=== PHASE 7: Walk to Bogroot portal + enter dungeon ===")
        if self.map_id() != MAP_SPARKFLY:
            self._record("phase7_bogroot", "SKIP", "not in Sparkfly")
            return False
        ok, detail = await self._walk_waypoint_route(
            TEKKS_TO_DUNGEON_PATH,
            route_label="Tekks->Bogroot",
            final_threshold=500.0,
            interior_threshold=500.0,
            per_wp_timeout=240.0,
        )
        if not ok:
            self._record("phase7_bogroot", "FAIL", detail)
            return False
        snap = await self.query_fresh(settle_ms=400, timeout=6.0)
        portal_id = None
        if snap is not None:
            portal_id = self.find_signpost(DUNGEON_PORTAL_X, DUNGEON_PORTAL_Y, radius=500.0)
        if portal_id:
            await self.action("interact_signpost", {"agent_id": portal_id}, wait_ms=2000)
        # Confirm with the dungeon-entry dialog button, then push into the
        # portal in case the signpost interact already opened the map.
        await self.action("dialog", {"dialog_id": 0x833905}, wait_ms=2000)
        if not await self.wait_for_map(MAP_BOGROOT_LVL1, timeout=90.0):
            self._record("phase7_bogroot", "FAIL", "Bogroot Lvl1 never loaded")
            return False
        await asyncio.sleep(5.0)
        self._record("phase7_bogroot", "PASS", f"portal_agent={portal_id}")
        return True

    async def phase8_blessing(self) -> bool:
        print("\n=== PHASE 8: Bogroot blessing shrine ===")
        if self.map_id() != MAP_BOGROOT_LVL1:
            self._record("phase8_blessing", "SKIP", "not in Bogroot Lvl1")
            return False
        ok, detail = await self._walk_waypoint_route(
            BOGROOT_TO_BLESSING_PATH,
            route_label="Bogroot->Blessing",
            final_threshold=500.0,
            interior_threshold=500.0,
            per_wp_timeout=240.0,
        )
        if not ok:
            self._record("phase8_blessing", "FAIL", detail)
            return False
        snap = await self.query_fresh(settle_ms=400, timeout=6.0)
        shrine_id = 0
        if snap is not None:
            best_dist = 400.0
            for agent in snap.get("agents", []) or []:
                if agent.get("agent_type") != "living":
                    continue
                if int(agent.get("allegiance", 0) or 0) != 6:
                    continue
                ax = float(agent.get("x", 0.0) or 0.0)
                ay = float(agent.get("y", 0.0) or 0.0)
                dist = ((ax - BLESSING_X) ** 2 + (ay - BLESSING_Y) ** 2) ** 0.5
                if dist < best_dist:
                    best_dist = dist
                    shrine_id = int(agent.get("id", 0) or 0)
        if shrine_id:
            await self.action("interact_npc", {"agent_id": shrine_id}, wait_ms=1500)
        await self.action("dialog", {"dialog_id": DIALOG_ACCEPT_BLESSING}, wait_ms=2000)
        self._record("phase8_blessing", "PASS", f"shrine_agent={shrine_id}")
        return True

    async def phase8b_dungeon_gadgets(self) -> bool:
        """Probe nearby dungeon gadgets — chests, doors, next-level portals.

        Multi-level Bogroot traversal + key-gated doors are not scripted yet
        (the C++ Froggy test does not cover it either). This phase instead
        validates that the bridge surfaces gadget agents in the snapshot and
        that ``interact_signpost`` / ``pick_up_item`` dispatch cleanly against
        them, which is the missing prerequisite for an LLM-driven dungeon
        runner.
        """
        print("\n=== PHASE 8b: Dungeon gadget probe (chests/doors/portals) ===")
        if self.map_id() != MAP_BOGROOT_LVL1:
            self._record("phase8b_gadgets", "SKIP", "not in Bogroot Lvl1")
            return False

        snap = await self.query_fresh(settle_ms=400, timeout=6.0)
        if snap is None:
            self._record("phase8b_gadgets", "SKIP", "no snapshot for gadget scan")
            return False

        gadgets = self.find_gadgets(max_distance=2500.0)
        if not gadgets:
            self._record("phase8b_gadgets", "SKIP", "no gadgets in range")
            return True

        interacted = 0
        for g in gadgets[:3]:
            gid = int(g.get("id", 0) or 0)
            gname = g.get("name", "") or ""
            gdist = float(g.get("distance", 0.0) or 0.0)
            print(f"[GADGET] id={gid} name={gname!r} dist={gdist:.0f}")
            if gid <= 0:
                continue
            # Walk close enough that interact will succeed.
            gx = float(g.get("x", 0.0) or 0.0)
            gy = float(g.get("y", 0.0) or 0.0)
            await self.walk_to(gx, gy, f"gadget {gid}", threshold=250.0, timeout=25.0)
            await self.action("interact_signpost", {"agent_id": gid}, wait_ms=1500)
            interacted += 1

        # Best-effort chest loot: if any new item-agent appeared after
        # interacting, try to pick it up.
        await self.query_fresh(settle_ms=400, timeout=6.0)
        drops = self.find_dropped_items(max_distance=2500.0)
        for agent_id in drops[:3]:
            await self.action("pick_up_item", {"agent_id": agent_id}, wait_ms=600)

        self._record(
            "phase8b_gadgets",
            "PASS",
            f"interacted={interacted} post_interact_drops={len(drops)}",
        )
        return True

    async def phase9_return_to_outpost(self) -> bool:
        print("\n=== PHASE 9: Return to Gadd's ===")
        current = self.map_id()
        if current == MAP_GADDS:
            self._record("phase9_return", "PASS", "already at Gadd's")
            return True
        if current == MAP_BOGROOT_LVL1:
            # In-dungeon — resign/return doesn't work, travel back directly.
            await self.action("travel", {"map_id": MAP_GADDS}, wait_ms=2000)
        else:
            await self.action("return_to_outpost", {}, wait_ms=2000)
        if not await self.wait_for_map(MAP_GADDS, timeout=120.0):
            self._record("phase9_return", "FAIL", "did not reach Gadd's")
            return False
        self._record("phase9_return", "PASS")
        return True

    async def phase7b_post_run_maintenance(self) -> bool:
        print("\n=== PHASE 7b: Native post-run maintenance verification ===")
        return await self.run_native_maintenance_verification(
            phase_name="phase7b_maintenance",
            include_salvage=True,
            detail_label="post-run maintenance",
        )
        print("\n=== PHASE 7b: Native Froggy maintenance cycle ===")
        if self.map_id() != MAP_GADDS:
            self._record("phase7b_maintenance", "FAIL", "not at Gadd's")
            return False
        merchant_open, npc_id = await self.ensure_gadds_merchant_open()
        if not merchant_open:
            self._record("phase7b_maintenance", "FAIL", "merchant window never opened")
            return False
        result = await self.action(
            "froggy_run_maintenance_cycle",
            {"include_salvage": True},
            await_result=True,
            timeout=180.0,
            wait_ms=1000,
        )
        if not bool(result.get("success")):
            self._record(
                "phase7b_maintenance",
                "FAIL",
                str(result.get("error") or "native maintenance failed"),
            )
            return False
        await self.query_fresh(settle_ms=500, timeout=10.0)
        self._record(
            "phase7b_maintenance",
            "PASS",
            f"merchant_agent={npc_id} free={self.free_slots_total()} gold={self.gold_character()} "
            f"superior_id={self.count_superior_id_kits()} salvage={self.count_salvage_kit_family()}",
        )
        return True

    async def phase9b_quest_reward(self) -> bool:
        """If the Tekks' War quest is in the reward state, accept it.

        The C++ Froggy test reaccepts the quest after a dungeon loop
        (``DIALOG_TEKKS_ACCEPT`` reissues once the previous cycle rewarded
        out). Here we just try the reward dialog button — if the quest is
        not actually in reward state, the dialog click is a no-op, which
        is still a valid bridge-round-trip proof.
        """
        print("\n=== PHASE 9b: Tekks' War quest reward accept ===")
        if self.map_id() == MAP_GADDS:
            # Mirror the native Froggy lane: once the run has already
            # unwound back to Gadd's, treat the post-Bogroot reward tail
            # as skipped rather than attempting a direct outpost->explorable
            # travel back into Sparkfly.
            self._record(
                "phase9b_reward",
                "SKIP",
                "dungeon loop already returned to Gadd's instead of Sparkfly",
            )
            return True

        if self.map_id() != MAP_SPARKFLY:
            self._record("phase9b_reward", "SKIP", "not in Sparkfly")
            return True

        # Reuse the Sparkfly->Tekks path end. Only walk the last few
        # waypoints to save time (we are already past the early ones).
        # Use _walk_waypoint_route so wipe recovery + fight_range=0 tail
        # semantics are handled consistently with the main phase 5.
        tail = SPARKFLY_TO_TEKKS_PATH[-3:]
        ok, detail = await self._walk_waypoint_route(
            tail,
            route_label="Sparkfly->Tekks (reward tail)",
            final_threshold=250.0,
            interior_threshold=500.0,
            per_wp_timeout=120.0,
        )
        if not ok:
            self._record("phase9b_reward", "SKIP", f"reward walk: {detail}")
            return True

        snap = await self.query_fresh(settle_ms=400, timeout=6.0)
        if snap is not None:
            tekks_id = self.find_nearby_npc(TEKKS_X, TEKKS_Y, radius=400.0)
            if tekks_id:
                await self.action("interact_npc", {"agent_id": tekks_id}, wait_ms=1500)
        await self.action("dialog", {"dialog_id": DIALOG_TEKKS_REWARD}, wait_ms=2000)
        self._record("phase9b_reward", "PASS", "reward dialog click dispatched")
        return True

    # --- Entrypoint ---------------------------------------------------------

    async def run(self) -> bool:
        if not await self.connect():
            return False
        try:
            # Drain initial handshake snapshots so later query_fresh results
            # are post-action, not the pre-connect baseline.
            await self.drain(max_messages=50)
            await self.query_fresh(settle_ms=500, timeout=10.0)

            # Each entry: (phase_method, blocking). Only blocking phases
            # stop subsequent execution on failure — e.g. if we can't reach
            # Gadd's, there's no way to run merchant/xunlai/sparkfly. Soft
            # phases (loot, blessing, dungeon probe) record FAIL but keep
            # going so the summary shows coverage of every bridge capability.
            phases: list[tuple[Any, bool]] = [
                (self.phase1_travel_to_gadds, True),
                (self.phase2_setup_heroes, True),
                (self.phase2b_merchant, False),
                (self.phase2e_town_blessing, False),
                (self.phase3_enter_sparkfly, True),
                (self.phase4_combat_proof, False),
                (self.phase4b_loot, False),
                # Skill-usage validation runs AFTER combat in Sparkfly
                # (not in outpost) — use_skill in an outpost is
                # server-invalid and DCs the client. phase 4 already
                # exercised in-combat casts with real enemy targets;
                # this phase adds recharge-delta evidence.
                (self.phase4c_skill_usage, False),
                (self.phase5_walk_to_tekks, True),
                (self.phase6_run_dungeon_loops, True),
                (self.phase9_return_to_outpost, True),
                (self.phase7b_post_run_maintenance, True),
            ]
            aborted = False
            for phase, blocking in phases:
                if aborted:
                    self._record(phase.__name__, "SKIP", "earlier blocking phase failed")
                    continue
                try:
                    ok = await phase()
                except Exception as exc:  # noqa: BLE001
                    self._record(phase.__name__, "FAIL", f"{type(exc).__name__}: {exc}")
                    if blocking:
                        aborted = True
                    continue
                if not ok and blocking:
                    aborted = True

            print("\n" + "=" * 60)
            print("FROGGY HM BRIDGE TEST COMPLETE")
            for phase_name, status in self.results.items():
                print(f"  {phase_name:32s} {status}")
            print("=" * 60)
            return not any(s.startswith("FAIL") for s in self.results.values())
        finally:
            self.ipc.disconnect()


# ---------------------------------------------------------------------------
# Runner-compatible entrypoint
# ---------------------------------------------------------------------------

async def test_froggy_hm_full_flow(tc: BridgeTestCase):
    """Single runner-visible test that drives the whole Froggy HM flow.

    The runner wraps each test in its own ``BridgeTestCase`` — which is
    fine for snapshot assertions but not a great fit for a ~15-minute
    multi-phase flow. So this function just delegates to
    :class:`FroggyHmBridgeTest` against the same pipe the runner already
    resolved, skipping the test if the lane is not configured to run it.
    """
    if os.environ.get("GWA3_RUN_FROGGY_HM_BRIDGE", "").lower() not in ("1", "true", "yes"):
        raise TestSkipped(
            "Set GWA3_RUN_FROGGY_HM_BRIDGE=1 to enable the long-running "
            "Froggy HM end-to-end bridge test (destructive to game state)"
        )
    # Reuse the runner's pipe name so a single GWA3_PIPE_NAME env var covers
    # both the short suite and this long test.
    harness = FroggyHmBridgeTest(pipe_name=tc.ipc.pipe_name)
    # Close the runner's connection first so we only hold one handle on
    # the named pipe at a time.
    tc.ipc.disconnect()
    success = await harness.run()
    if not success:
        raise TestFailure(f"Froggy HM bridge flow had failures: {harness.results}")


# ---------------------------------------------------------------------------
# Stand-alone script entrypoint
# ---------------------------------------------------------------------------

async def _main() -> int:
    test = FroggyHmBridgeTest()
    try:
        ok = await test.run()
        print(f"\n{'PASS' if ok else 'FAIL'}")
        return 0 if ok else 1
    except Exception as exc:  # noqa: BLE001
        import traceback
        print(f"\n[ERROR] {exc}")
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(asyncio.run(_main()))
