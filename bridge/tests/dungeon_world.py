"""Shared world snapshot helpers for dungeon bridge tests."""


class DungeonWorldMixin:
    def map_id(self) -> int:
        return int(self.snapshot.get("map", {}).get("map_id", 0) or 0)

    def pos(self) -> tuple[float, float]:
        me = self.snapshot.get("me", {})
        return float(me.get("x", 0.0) or 0.0), float(me.get("y", 0.0) or 0.0)

    def party_size(self) -> int:
        return int(self.snapshot.get("party", {}).get("size", 0) or 0)

    def player_hp_fraction(self) -> float:
        me = self.snapshot.get("me", {})
        return float(me.get("hp", 0.0) or 0.0)

    def party_hp_fraction(self) -> float:
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
        if not self.is_player_dead():
            return False
        heroes = self.snapshot.get("heroes", []) or []
        if not heroes:
            return self.is_player_dead()
        down = sum(1 for h in heroes if float(h.get("hp", 0.0) or 0.0) <= 0.001)
        return down >= (len(heroes) * 3) // 4

    def nearest_waypoint_index(self, waypoints: list) -> int:
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

    def find_dropped_items(self, max_distance: float = 2000.0) -> list[int]:
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
