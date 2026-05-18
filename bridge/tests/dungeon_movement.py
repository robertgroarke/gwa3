"""Shared movement helpers for dungeon bridge tests."""

import asyncio
import time


def distance_xy(ax: float, ay: float, bx: float, by: float) -> float:
    return ((ax - bx) ** 2 + (ay - by) ** 2) ** 0.5


async def wait_until_near(
    tc,
    x: float,
    y: float,
    threshold: float = 350.0,
    timeout: float = 20.0,
    tier: int = 1,
) -> bool:
    """Poll snapshots until the player is near a target coordinate."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        snap = await tc.wait_for_snapshot(tier=tier, timeout=3.0)
        me = snap.get("me", {})
        px = float(me.get("x", 0.0) or 0.0)
        py = float(me.get("y", 0.0) or 0.0)
        if distance_xy(px, py, x, y) <= threshold:
            return True
        await tc.ipc.send_action("move_to", {"x": x, "y": y}, "")
        await asyncio.sleep(1.0)
    return False


async def push_until_map_changes(
    tc,
    x: float,
    y: float,
    from_map: int,
    timeout: float = 40.0,
    tier: int = 1,
) -> bool:
    """Keep pushing a movement target until the map ID changes."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        await tc.send_action("move_to", {"x": x, "y": y})
        await asyncio.sleep(1.0)
        snap = await tc.wait_for_snapshot(tier=tier, timeout=2.0)
        if snap.get("map", {}).get("map_id") != from_map:
            return True
    return False


class DungeonMovementMixin:
    async def walk_to(
        self,
        x: float,
        y: float,
        label: str,
        threshold: float = 350.0,
        timeout: float = 30.0,
    ) -> bool:
        print(f"[MOVE] -> {label} ({x:.0f}, {y:.0f})")
        snap = await self.query_fresh(settle_ms=150, timeout=4.0)
        if snap is not None:
            px, py = self.pos()
            if distance_xy(px, py, x, y) <= threshold:
                print(f"[MOVE] Already at {label}")
                return True

        await self.action("move_to", {"x": x, "y": y}, wait_ms=100)
        last_issue = time.time()
        deadline = time.time() + timeout
        while time.time() < deadline:
            snap = await self.query_fresh(settle_ms=200, timeout=3.0)
            if snap is not None:
                px, py = self.pos()
                dist = distance_xy(px, py, x, y)
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
        print(f"[AGGRO-MOVE] -> {label} ({x:.0f}, {y:.0f}) fight_range={fight_range:.0f}")
        snap = await self.query_fresh(settle_ms=150, timeout=4.0)
        if snap is not None:
            px, py = self.pos()
            if distance_xy(px, py, x, y) <= threshold:
                print(f"[AGGRO-MOVE] Already at {label}")
                return True
        await self.action(
            "aggro_move_to",
            {"x": x, "y": y, "fight_range": fight_range},
            wait_ms=100,
        )
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
                        dist = distance_xy(px, py, x, y)
                        if dist <= threshold:
                            print(f"[AGGRO-MOVE] Arrived at {label} (dist={dist:.0f})")
                            return True
                await asyncio.sleep(2.0)
                continue
            px, py = self.pos()
            dist = distance_xy(px, py, x, y)
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
        deadline = time.time() + timeout
        while time.time() < deadline:
            await self.action("move_to", {"x": x, "y": y}, wait_ms=800)
            snap = await self.query_fresh(settle_ms=150, timeout=3.0)
            if snap is not None and self.map_id() != from_map:
                return True
        return False
