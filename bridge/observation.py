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
            loading = m.get("loading_state", "?")
            load_str = {0: "loading", 1: "loaded", 2: "disconnected"}.get(loading, f"state={loading}")
            vanquish = ""
            fk = m.get("foes_killed")
            ft = m.get("foes_to_kill")
            if fk is not None and ft is not None and (fk > 0 or ft > 0):
                vanquish = f" foes={fk}/{fk+ft}"
            lines.append(
                f"Map: id={m['map_id']} {load_str} "
                f"time={m.get('instance_time', 0)}ms{vanquish}"
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

        # Party with per-member status
        party = snap.get("party", {})
        if party:
            size = party.get("size", "?")
            dead = party.get("dead_count", 0)
            defeated = party.get("is_defeated", False)
            morale = party.get("morale", 0)
            morale_str = ""
            if morale < 0:
                morale_str = f" DP={morale}%"
            elif morale > 0:
                morale_str = f" morale=+{morale}%"
            lines.append(
                f"Party: size={size} dead={dead} defeated={defeated}{morale_str}"
            )
            members = party.get("members", [])
            if members:
                dead_members = [m for m in members if not m.get("is_alive", True)]
                if dead_members:
                    lines.append("  Dead members:")
                    for dm in dead_members:
                        role = "player" if dm.get("is_player") else "hero" if dm.get("is_hero") else "henchman"
                        lines.append(
                            f"    agent_id={dm.get('agent_id')} {profession_name(dm.get('primary', 0))} ({role})"
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
                    conditions = []
                    if foe.get("has_hex"): conditions.append("HEXED")
                    if foe.get("has_enchantment"): conditions.append("ENCHANTED")
                    cond_str = f" [{','.join(conditions)}]" if conditions else ""
                    name = foe.get("name", "")
                    name_str = f" \"{name}\"" if name else ""
                    lines.append(
                        f"    id={foe['id']}{name_str} dist={foe.get('distance', 0):.0f} "
                        f"hp={foe.get('hp', 0):.0%} energy={foe.get('energy', 0):.0%} "
                        f"lv{foe.get('level', 0)} {prof}/{sec}"
                        f"{cond_str}{casting}"
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

            # List chests / interactive gadgets
            gadgets = [a for a in agents if a.get("agent_type") == "gadget"]
            chests = [g for g in gadgets if g.get("is_chest")]
            if chests:
                lines.append("  Chests:")
                for ch in sorted(chests, key=lambda a: a.get("distance", 99999)):
                    lines.append(
                        f"    agent_id={ch['id']} dist={ch.get('distance', 0):.0f} "
                        f"gadget_id={ch.get('gadget_id', '?')}"
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

        # Merchant/trader window (from tier 2+)
        merchant = snap.get("merchant", {})
        if merchant.get("is_open"):
            lines.append(f"MERCHANT OPEN ({merchant.get('item_count', 0)} items):")
            merch_items = merchant.get("items", [])
            for mi in merch_items:
                mid = mi.get("model_id", 0)
                mname = item_name(mid) if mid else f"item#{mi.get('item_id', '?')}"
                mtype = item_type_name(mi.get("type", -1))
                val = mi.get("value", 0)
                qty = mi.get("quantity", 1)
                qty_str = f" x{qty}" if qty > 1 else ""
                lines.append(
                    f"  item_id={mi.get('item_id')} {mname}{qty_str} [{mtype}] "
                    f"value={val} model={mid}"
                )
            # Last quote info
            quote = merchant.get("last_quote")
            if quote:
                cost_id = quote.get("cost_item_id", 0)
                cost_val = quote.get("cost_value", 0)
                if cost_id == 0:
                    cost_str = f"{cost_val:,} gold"
                else:
                    cost_str = f"{cost_val}x {item_name(cost_id)} (model={cost_id})"
                lines.append(f"  Last quote: costs {cost_str}")

        # Inventory (from tier 3)
        inv = snap.get("inventory", {})
        if inv:
            lines.append(
                f"Gold: character={inv.get('gold_character', 0):,} storage={inv.get('gold_storage', 0):,}"
            )
            bags = inv.get("bags", [])
            total_items = sum(b.get("item_count", 0) for b in bags)
            free_slots = inv.get("free_slots_total", "?")
            lines.append(f"Backpack: {total_items} items, {free_slots} free slots across {len(bags)} bags")

        # Storage (from tier 3)
        storage = snap.get("storage", [])
        if storage:
            total_stored = sum(b.get("item_count", 0) for b in storage)
            lines.append(f"Xunlai Storage: {total_stored} items across {len(storage)} panes")

        # Title progression (from tier 3)
        titles = snap.get("titles", {})
        if titles:
            title_parts = []
            for name, data in titles.items():
                pts = data.get("current_points", 0)
                rank = data.get("current_rank", 0)
                nxt = data.get("points_needed_next", 0)
                title_parts.append(f"{name}={pts}pts(rank {rank}, next@{nxt})")
            lines.append("Titles: " + ", ".join(title_parts))

        # Chat log (from tier 2+)
        chat = snap.get("chat", [])
        if chat:
            lines.append(f"Chat ({len(chat)} new):")
            for msg in chat:
                ch = msg.get("channel", "?")
                sender = msg.get("sender", "")
                text = msg.get("message", "")
                lines.append(f"  [{ch}] {sender}: {text[:200]}")

        # Recent events
        events = self.get_recent_events(5)
        if events:
            lines.append("Recent events:")
            for ev in events:
                lines.append(f"  - {ev.get('event', 'unknown')}: {ev}")

        return "\n".join(lines)
