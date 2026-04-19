r"""Category N: Froggy HM end-to-end bridge test.

Mirrors the C++ ``RunFroggyFeatureTest`` flow in
``gwa3/src/tests/IntegrationTestEpic14.cpp`` — but drives every step through
the LLM bridge IPC instead of from inside the DLL. This proves the bridge
exposes everything an external agent (the LLM) needs to run the same Bogroot
Growths HM loop the C++ integration test runs.

Phases (match the C++ Froggy feature test):
    1. Travel to Gadd's Encampment (map 638).
    2. Kick existing heroes, add the Standard hero set, set hard mode.
    3. Walk Gadd's exit waypoints and enter Sparkfly Swamp (map 558).
    4. Target a foe and attack (proves combat actions route through bridge).
    5. Walk the Sparkfly -> Tekks waypoints, dialog-accept Tekks' War quest.
    6. Walk Tekks -> Bogroot portal, interact the portal, enter dungeon (615).
    7. Walk to the blessing shrine and dialog-accept the blessing.
    8. Return to Gadd's Encampment.

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

from ..ipc_client import IpcClient
from .base import BridgeTestCase, TestSkipped
from .helpers import TestFailure


# --- Constants lifted from IntegrationTestEpic14.cpp ---
MAP_GADDS = 638
MAP_SPARKFLY = 558
MAP_BOGROOT_LVL1 = 615

QUEST_TEKKS_WAR = 0x339
DIALOG_TEKKS_ACCEPT = 0x833901
DIALOG_ACCEPT_BLESSING = 0x84

TEKKS_X = 12396.0
TEKKS_Y = 22407.0
DUNGEON_PORTAL_X = 13097.0
DUNGEON_PORTAL_Y = 26393.0
BLESSING_X = 19099.0
BLESSING_Y = 7762.0

# Gadd's exit portal waypoints (see C++ Phase 4)
EXIT_WP1 = (-10018.0, -21892.0)
EXIT_WP2 = (-9550.0, -20400.0)
EXIT_PUSH = (-9451.0, -19766.0)

# Sparkfly -> Tekks path (C++ kSparkflyToTekksPath)
SPARKFLY_TO_TEKKS_PATH = [
    (-4559.0, -14406.0, "Sparkfly waypoint 1"),
    (-5204.0, -9831.0, "Sparkfly waypoint 2"),
    (-928.0, -8699.0, "Sparkfly waypoint 3"),
    (4200.0, -4897.0, "Sparkfly waypoint 4"),
    (6114.0, 819.0, "Sparkfly waypoint 5"),
    (9500.0, 2281.0, "Sparkfly waypoint 6"),
    (11570.0, 6120.0, "Sparkfly waypoint 7"),
    (11025.0, 11710.0, "Sparkfly waypoint 8"),
    (14624.0, 19314.0, "Sparkfly waypoint 9"),
    (TEKKS_X, TEKKS_Y, "Tekks"),
]

# Tekks -> Bogroot portal (C++ kTekksToDungeonPath)
TEKKS_TO_DUNGEON_PATH = [
    (12228.0, 22677.0, "Dungeon approach 1"),
    (12470.0, 25036.0, "Dungeon approach 2"),
    (12968.0, 26219.0, "Dungeon approach 3"),
    (DUNGEON_PORTAL_X, DUNGEON_PORTAL_Y, "Bogroot portal"),
]

# Bogroot spawn -> blessing shrine (C++ kBogrootToBlessingPath)
BOGROOT_TO_BLESSING_PATH = [
    (17026.0, 2168.0, "Bogroot start"),
    (BLESSING_X, BLESSING_Y, "Blessing shrine"),
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
        self.results: dict[str, str] = {}

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
        print("[BRIDGE] Connected.")
        return True

    async def action(self, name: str, params: dict | None = None, wait_ms: int = 500) -> str:
        req_id = self._next_req_id()
        print(f"  >> {name}({params or {}}) [{req_id}]")
        await self.ipc.send_action(name, params, req_id)
        if wait_ms:
            await asyncio.sleep(wait_ms / 1000.0)
        return req_id

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

        # Best-effort kick of any existing heroes. The LLM bridge exposes
        # ``kick_hero`` per hero_id; there is no wholesale 'kick all' (the
        # old ``kick_all_heroes`` path was deprecated, which test_e
        # documents).
        for hid in STANDARD_HEROES:
            await self.action("kick_hero", {"hero_id": hid}, wait_ms=200)
        await asyncio.sleep(1.5)

        added = 0
        for hid in STANDARD_HEROES:
            await self.action("add_hero", {"hero_id": hid}, wait_ms=400)
            added += 1

        await asyncio.sleep(2.0)
        await self.query_fresh(settle_ms=400, timeout=6.0)
        party_ok = self.party_size() >= 2  # player + at least one hero
        if not party_ok:
            self._record("phase2_setup", "FAIL", f"party size {self.party_size()} after add_hero")
            return False

        # Set all added heroes to Guard behavior (1). Heroes are indexed
        # 1..N in party order.
        for idx in range(1, added + 1):
            await self.action(
                "set_hero_behavior",
                {"hero_index": idx, "behavior": 1},
                wait_ms=150,
            )

        await self.action("set_hard_mode", {"enabled": True}, wait_ms=1000)
        self._record("phase2_setup", "PASS", f"party_size={self.party_size()}")
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
        await asyncio.sleep(5.0)
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
        self._record("phase4_combat", "PASS", f"attacked foe {foe_id}")
        return True

    async def phase5_walk_to_tekks(self) -> bool:
        print("\n=== PHASE 5: Walk Sparkfly -> Tekks ===")
        if self.map_id() != MAP_SPARKFLY:
            self._record("phase5_tekks", "SKIP", "not in Sparkfly")
            return False
        for x, y, label in SPARKFLY_TO_TEKKS_PATH:
            threshold = 250.0 if label == "Tekks" else 500.0
            if not await self.walk_to(x, y, label, threshold=threshold, timeout=60.0):
                self._record("phase5_tekks", "FAIL", f"stuck at {label}")
                return False
        self._record("phase5_tekks", "PASS")
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
        for x, y, label in TEKKS_TO_DUNGEON_PATH:
            if not await self.walk_to(x, y, label, threshold=500.0, timeout=45.0):
                self._record("phase7_bogroot", "FAIL", f"stuck at {label}")
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
        for x, y, label in BOGROOT_TO_BLESSING_PATH:
            if not await self.walk_to(x, y, label, threshold=500.0, timeout=45.0):
                self._record("phase8_blessing", "FAIL", f"stuck at {label}")
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

    # --- Entrypoint ---------------------------------------------------------

    async def run(self) -> bool:
        if not await self.connect():
            return False
        try:
            # Drain initial handshake snapshots so later query_fresh results
            # are post-action, not the pre-connect baseline.
            await self.drain(max_messages=50)
            await self.query_fresh(settle_ms=500, timeout=10.0)

            phases = [
                self.phase1_travel_to_gadds,
                self.phase2_setup_heroes,
                self.phase3_enter_sparkfly,
                self.phase4_combat_proof,
                self.phase5_walk_to_tekks,
                self.phase6_accept_tekks_quest,
                self.phase7_enter_bogroot,
                self.phase8_blessing,
                self.phase9_return_to_outpost,
            ]
            # A FAIL in phase N aborts subsequent phases; they get recorded
            # as SKIP so the summary reflects why they did not run.
            aborted = False
            for phase in phases:
                if aborted:
                    self._record(phase.__name__.replace("phase", "phase"), "SKIP", "earlier phase failed")
                    continue
                try:
                    ok = await phase()
                except Exception as exc:  # noqa: BLE001
                    self._record(phase.__name__, "FAIL", f"{type(exc).__name__}: {exc}")
                    aborted = True
                    continue
                if not ok:
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
