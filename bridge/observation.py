"""Observation window manager — maintains a sliding window of recent game state snapshots."""

from collections import deque
from .gamedata import (
    skill_name,
    item_name,
    profession_name,
    skill_type_name,
    item_type_name,
)


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

        # Skillbar — show human-readable skill names
        skills = snap.get("skillbar", [])
        if skills:
            sk_parts = []
            for sk in skills:
                sid = sk.get("skill_id", 0)
                rech = sk.get("recharge", 0)
                name = skill_name(sid) if sid else "Empty"
                cost = sk.get("energy_cost", "?")
                stype = skill_type_name(sk.get("type", -1)) if "type" in sk else ""
                status = f"R{rech}" if rech > 0 else "ready"
                sk_parts.append(f"[{sk['slot']}:{name}({sid}) {stype} {cost}e {status}]")
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

            # List each foe with details
            if foes:
                lines.append("  Foes:")
                for foe in sorted(foes, key=lambda a: a.get("distance", 99999)):
                    prof = profession_name(foe.get("primary", 0))
                    sec = profession_name(foe.get("secondary", 0))
                    casting = ""
                    if foe.get("is_casting"):
                        sk_id = foe.get("casting_skill_id", 0)
                        sk_nm = skill_name(sk_id)
                        sk_tp = skill_type_name(foe.get("casting_skill_type", -1))
                        cast_time = foe.get("casting_skill_activation", 0)
                        casting = f" CASTING {sk_nm}({sk_id}) [{sk_tp}, {cast_time:.1f}s]"
                    lines.append(
                        f"    id={foe['id']} dist={foe.get('distance', 0):.0f} "
                        f"hp={foe.get('hp', 0):.0%} energy={foe.get('energy', 0):.0%} "
                        f"lv{foe.get('level', 0)} {prof}/{sec}"
                        f"{casting}"
                    )

            # List ground items with details
            if items:
                lines.append("  Items on ground:")
                for it in sorted(items, key=lambda a: a.get("distance", 99999)):
                    model_id = it.get("model_id", 0)
                    iname = item_name(model_id) if model_id else f"item_id={it.get('item_id', '?')}"
                    itype = item_type_name(it.get("item_type", -1)) if "item_type" in it else ""
                    qty = it.get("quantity", 1)
                    owner = it.get("owner", 0)
                    qty_str = f" x{qty}" if qty > 1 else ""
                    owner_str = f" (yours)" if owner == snap.get("me", {}).get("agent_id", -1) else ""
                    lines.append(
                        f"    agent_id={it['id']} dist={it.get('distance', 0):.0f} "
                        f"{iname}{qty_str} [{itype}] model={model_id}{owner_str}"
                    )

        # Hero skillbars and casting (from tier 2+)
        heroes = snap.get("heroes", [])
        if heroes:
            lines.append(f"Heroes ({len(heroes)}):")
            for hero in heroes:
                prof = profession_name(hero.get("primary", 0))
                casting = ""
                if hero.get("is_casting"):
                    sk_id = hero.get("casting_skill_id", 0)
                    casting = f" CASTING {skill_name(sk_id)}({sk_id})"
                hero_skills = hero.get("skillbar", [])
                sk_summary = " ".join(
                    f"[{s.get('slot')}:{skill_name(s.get('skill_id',0))}={'R'+str(s['recharge']) if s.get('recharge',0)>0 else 'rdy'}]"
                    for s in hero_skills if s.get("skill_id", 0)
                )
                lines.append(
                    f"  agent={hero.get('agent_id')} {prof} "
                    f"hp={hero.get('hp', 0):.0%} energy={hero.get('energy', 0):.0%}"
                    f"{casting}"
                )
                if sk_summary:
                    lines.append(f"    {sk_summary}")

        # Dialog state (from tier 2+)
        dialog = snap.get("dialog", {})
        if dialog.get("is_open"):
            lines.append("DIALOG OPEN:")
            body = dialog.get("body_raw", "")
            if body:
                lines.append(f"  NPC says: {body[:300]}")
            buttons = dialog.get("buttons", [])
            if buttons:
                lines.append("  Options:")
                for btn in buttons:
                    label = btn.get("label", "???")
                    did = btn.get("dialog_id", 0)
                    lines.append(f"    [{did}] {label}")

        # Inventory (from tier 3)
        inv = snap.get("inventory", {})
        if inv:
            lines.append(
                f"Gold: char={inv.get('gold_character', 0)} storage={inv.get('gold_storage', 0)}"
            )
            bags = inv.get("bags", [])
            total_items = sum(b.get("item_count", 0) for b in bags)
            lines.append(f"Backpack: {total_items} items across {len(bags)} bags")

        # Storage (from tier 3)
        storage = snap.get("storage", [])
        if storage:
            total_stored = sum(b.get("item_count", 0) for b in storage)
            lines.append(f"Xunlai Storage: {total_stored} items across {len(storage)} panes")

        # Recent events
        events = self.get_recent_events(5)
        if events:
            lines.append("Recent events:")
            for ev in events:
                lines.append(f"  - {ev.get('event', 'unknown')}: {ev}")

        return "\n".join(lines)
