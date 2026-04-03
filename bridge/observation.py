"""Observation window manager — maintains a sliding window of recent game state snapshots."""

from collections import deque


class ObservationWindow:
    """Maintains a sliding window of the most recent game state snapshots."""

    def __init__(self, max_size: int = 5):
        self.max_size = max_size
        self._snapshots: deque[dict] = deque(maxlen=max_size)
        self._events: deque[dict] = deque(maxlen=50)

    def add_snapshot(self, snapshot: dict):
        """Add a snapshot, keeping only the latest per tier."""
        self._snapshots.append(snapshot)

    def add_event(self, event: dict):
        """Add a discrete game event."""
        self._events.append(event)

    @property
    def latest(self) -> dict | None:
        """Return the most recent snapshot."""
        return self._snapshots[-1] if self._snapshots else None

    @property
    def latest_full(self) -> dict | None:
        """Return the most recent tier 3 (full) snapshot."""
        for snap in reversed(self._snapshots):
            if snap.get("tier") == 3:
                return snap
        return None

    def get_recent_events(self, count: int = 10) -> list[dict]:
        """Return the most recent events."""
        return list(self._events)[-count:]

    def drain_events(self) -> list[dict]:
        """Return and clear all pending events."""
        events = list(self._events)
        self._events.clear()
        return events

    def build_context_summary(self) -> str:
        """Build a compact text summary of the current game state for the LLM prompt."""
        snap = self.latest
        if not snap:
            return "No game state available yet."

        lines = []

        # Player state
        me = snap.get("me", {})
        if me.get("agent_id"):
            hp_pct = me.get("hp", 0)
            energy_pct = me.get("energy", 0)
            lines.append(
                f"Player: agent_id={me['agent_id']} pos=({me.get('x', 0):.0f}, {me.get('y', 0):.0f}) "
                f"HP={hp_pct:.0%} Energy={energy_pct:.0%} "
                f"target={me.get('target_id', 0)} "
                f"{'CASTING' if me.get('is_casting') else 'MOVING' if me.get('is_moving') else 'idle'}"
            )

        # Map
        m = snap.get("map", {})
        if m.get("map_id"):
            lines.append(
                f"Map: id={m['map_id']} loaded={m.get('is_loaded')} "
                f"time={m.get('instance_time', 0)}ms"
            )

        # Skillbar
        skills = snap.get("skillbar", [])
        if skills:
            sk_parts = []
            for sk in skills:
                rech = sk.get("recharge", 0)
                status = f"R{rech}" if rech > 0 else "ready"
                sk_parts.append(f"[{sk['slot']}:{sk.get('skill_id', 0)}={status}]")
            lines.append("Skills: " + " ".join(sk_parts))

        # Party
        party = snap.get("party", {})
        if party:
            lines.append(
                f"Party: defeated={party.get('is_defeated', False)}"
            )

        # Nearby agents (from tier 2+)
        agents = snap.get("agents", [])
        if agents:
            foes = [a for a in agents if a.get("allegiance") == 3 and a.get("is_alive", True)]
            allies = [a for a in agents if a.get("allegiance") == 1]
            items = [a for a in agents if a.get("agent_type") == "item"]
            lines.append(
                f"Nearby: {len(foes)} foes, {len(allies)} allies, {len(items)} items "
                f"(total {len(agents)} agents)"
            )
            if foes:
                lines.append("  Foes:")
                for foe in sorted(foes, key=lambda a: a.get("distance", 99999)):
                    casting = ""
                    if foe.get("is_casting"):
                        sk_id = foe.get("casting_skill_id", 0)
                        sk_type = foe.get("casting_skill_type", "?")
                        casting = f" CASTING skill {sk_id} (type={sk_type})"
                    lines.append(
                        f"    id={foe['id']} dist={foe.get('distance', 0):.0f} "
                        f"hp={foe.get('hp', 0):.0%} energy={foe.get('energy', 0):.0%} "
                        f"lv{foe.get('level', 0)} prof={foe.get('primary', 0)}/{foe.get('secondary', 0)}"
                        f"{casting}"
                    )

        # Inventory (from tier 3)
        inv = snap.get("inventory", {})
        if inv:
            lines.append(
                f"Gold: char={inv.get('gold_character', 0)} storage={inv.get('gold_storage', 0)}"
            )

        # Recent events
        events = self.get_recent_events(5)
        if events:
            lines.append("Recent events:")
            for ev in events:
                lines.append(f"  - {ev.get('event', 'unknown')}: {ev}")

        return "\n".join(lines)
