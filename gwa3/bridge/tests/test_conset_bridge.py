"""Test the full conset crafting cycle via the LLM bridge IPC commands.

This script drives the same sequence as IntegrationTestSession::TestConsetCraftCycle
but through the named pipe bridge, proving the LLM agent has all the commands
needed to execute a complete conset buy+craft cycle.

Sequence:
1. Travel to Embark Beach (map 857)
2. Walk to Xunlai Chest, withdraw gold to 100k
3. Walk to material trader, open merchant
4. Buy materials (Iron, Dust, Bone, Feather) via trader_buy
5. Walk to each crafter (Eyja, Kwat, Alcus), craft consumables via craft_item

Usage:
    python -m bridge.tests.test_conset_bridge

Requires gwa3.dll injected in LLM mode (--llm flag) with a character
already logged in (char select handled by DLL bootstrap).
"""

import asyncio
import sys
import time

sys.path.insert(0, ".")
from bridge.ipc_client import IpcClient


from bridge import farming_knowledge as fk

# --- Constants ---
MAP_EMBARK_BEACH = 857

# Consumable model IDs — these are the seeds; everything else (recipe,
# crafter, materials, coords, gold cost) is resolved via get_recipe so the
# test doubles as a validation of the farming_knowledge tables that Gemma
# will be using at runtime.
MODEL_GRAIL = 24861
MODEL_ESSENCE = 24859
MODEL_ARMOR = 24860

# Build RECIPES + CRAFTERS from farming_knowledge.get_recipe so any drift
# between the test's expectations and what Gemma will see via the tool
# surfaces as an immediate failure rather than a silent divergence.
def _load_recipes_from_knowledge():
    recipes: dict[str, tuple[int, list[int], list[int]]] = {}
    crafters: dict[str, tuple[str, float, float]] = {}
    for model_id in (MODEL_GRAIL, MODEL_ESSENCE, MODEL_ARMOR):
        res = fk.get_recipe(model_id)
        assert res.get("success"), f"get_recipe({model_id}) failed: {res}"
        r = res["recipe"]
        name = r["name"]
        mat_ids = [m["model_id"] for m in r["materials"]]
        mat_qtys = [m["quantity"] for m in r["materials"]]
        recipes[name] = (model_id, mat_ids, mat_qtys)
        crafters[name] = (r["crafter_name"], r["crafter_x"], r["crafter_y"])
    return recipes, crafters

RECIPES, CRAFTERS = _load_recipes_from_knowledge()

# Xunlai + material trader coords come from get_outpost_info(857) — same
# reasoning: lookup failure means Gemma wouldn't be able to farm here.
def _load_embark_coords():
    res = fk.get_outpost_info(MAP_EMBARK_BEACH)
    assert res.get("success"), f"get_outpost_info(857) failed: {res}"
    info = res["info"]
    xunlai = info["xunlai_chest"]
    trader = info["material_trader"]
    return (int(xunlai["x"]), int(xunlai["y"]),
            int(trader["x"]), int(trader["y"]))

XUNLAI_X, XUNLAI_Y, MATERIAL_TRADER_X, MATERIAL_TRADER_Y = _load_embark_coords()

# Material model IDs — kept as aliases for readability in buy-plan output.
# Sourced from the recipe materials so any rename in farming_knowledge
# propagates without a second edit.
MAT_IRON = RECIPES["Grail of Might"][1][0]      # first mat of Grail
MAT_DUST = RECIPES["Grail of Might"][1][1]      # second mat of Grail
MAT_BONE = RECIPES["Armor of Salvation"][1][1]  # second mat of Armor
MAT_FEATHER = RECIPES["Essence of Celerity"][1][0]  # first mat of Essence

TARGET_GOLD = 100000
# Gold cost per craft — pull from the first recipe (all three share 250g)
CRAFT_COST = fk.get_recipe(MODEL_GRAIL)["recipe"]["gold_cost"]


class ConsetBridgeTest:
    def __init__(self, pipe_name: str = r"\\.\pipe\gwa3_llm_biscuit"):
        self.ipc = IpcClient(pipe_name=pipe_name)
        self.snapshot = {}
        self._req_counter = 0

    def _next_req_id(self) -> str:
        self._req_counter += 1
        return f"conset-{self._req_counter}"

    async def connect(self):
        print("[BRIDGE] Connecting to gwa3 named pipe...")
        ok = await self.ipc.connect(timeout=30.0)
        if not ok:
            print("[BRIDGE] ERROR: Could not connect to pipe")
            return False
        print("[BRIDGE] Connected!")
        return True

    async def read_until_snapshot(self, timeout: float = 15.0, min_tier: int = 2) -> dict | None:
        """Read messages until we get a snapshot of at least the given tier."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            msg = await self.ipc.read_message()
            if msg is None:
                return None
            if msg.get("type") == "snapshot":
                tier = msg.get("tier", 1)
                self.snapshot = msg
                if tier >= min_tier:
                    return msg
            # action_result, event, heartbeat — skip
        return None

    async def drain_pipe(self, count: int = 50):
        """Drain up to `count` buffered messages from the pipe.
        Uses peek-style reads via `has_pending` would be ideal but not available.
        Instead, read N messages expecting them to be already queued."""
        drained = 0
        # Read a limited number of messages with short wait
        for _ in range(count):
            try:
                # Use asyncio.wait_for but wrap each call carefully
                msg = await asyncio.wait_for(
                    asyncio.shield(self.ipc.read_message()), timeout=0.05
                )
                if msg is None:
                    break
                if msg.get("type") == "snapshot":
                    self.snapshot = msg
                drained += 1
            except asyncio.TimeoutError:
                break
            except Exception:
                break
        return drained

    async def query_fresh_state(self, timeout: float = 5.0, settle_ms: int = 300) -> dict | None:
        """Request fresh tier-3 snapshot, return it. Drains any queued messages
        BEFORE requesting so we don't return a stale snapshot that was already
        in flight when prior actions ran.

        Flow:
          1. Drain the pipe of any buffered snapshots (stale data from before).
          2. Send query_state with server-side settle delay (lets pending game
             ops like buys/crafts complete before the snapshot is built).
          3. Read until we get a tier-3 snapshot — this one was produced AFTER
             the settle delay, so it reflects post-action state.
        """
        drained = await self.drain_pipe(count=100)
        if drained > 0:
            print(f"[QUERY] Drained {drained} stale messages before query")
        await self.action("query_state", {"wait_ms": settle_ms}, wait_ms=settle_ms + 200)
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                msg = await self.ipc.read_message()
            except Exception:
                return None
            if msg is None:
                return None
            if msg.get("type") == "snapshot":
                self.snapshot = msg
                if msg.get("tier", 0) == 3:
                    return msg
        return None

    async def action(self, name: str, params: dict | None = None, wait_ms: int = 500) -> bool:
        """Send an action and wait briefly."""
        req_id = self._next_req_id()
        print(f"  >> {name}({params or {}}) [{req_id}]")
        await self.ipc.send_action(name, params, req_id)
        await asyncio.sleep(wait_ms / 1000.0)
        return True

    async def action_and_wait_snapshot(self, name: str, params: dict, wait_ms: int = 2000) -> dict | None:
        """Send an action, then read until we get a fresh snapshot."""
        await self.action(name, params, wait_ms=100)
        await asyncio.sleep(wait_ms / 1000.0)
        return await self.read_until_snapshot(timeout=10.0)

    def get_gold(self) -> int:
        inv = self.snapshot.get("inventory", {})
        return inv.get("gold_character", 0)

    def get_storage_gold(self) -> int:
        inv = self.snapshot.get("inventory", {})
        return inv.get("gold_storage", 0)

    def get_map_id(self) -> int:
        return self.snapshot.get("map", {}).get("map_id", 0)

    def get_pos(self) -> tuple[float, float]:
        me = self.snapshot.get("me", {})
        return me.get("x", 0), me.get("y", 0)

    def is_merchant_open(self) -> bool:
        return self.snapshot.get("merchant", {}).get("is_open", False)

    def get_merchant_items(self) -> list:
        return self.snapshot.get("merchant", {}).get("items", [])

    def count_material(self, model_id: int) -> int:
        """Count total quantity of a material model in inventory."""
        total = 0
        for bag in self.snapshot.get("inventory", {}).get("bags", []):
            for item in bag.get("items", []):
                if item.get("model_id") == model_id:
                    total += item.get("quantity", 1)
        return total

    def count_consumable(self, model_id: int) -> int:
        """Count consumable items by model ID."""
        return self.count_material(model_id)  # same logic

    def find_nearby_npcs(self, max_count: int = 5,
                         target_x: float | None = None,
                         target_y: float | None = None) -> list[int]:
        """Find nearby NPC agents (allegiance=6), sorted by distance.
        If target_x/target_y are given, sort by distance to that point instead
        of distance to player (matches the C++ test's ConsetFindNearestNPC)."""
        agents = self.snapshot.get("agents", [])
        npcs = []
        for a in agents:
            if a.get("agent_type") != "living":
                continue
            if a.get("allegiance") != 6:
                continue
            if target_x is not None and target_y is not None:
                ax, ay = a.get("x", 0), a.get("y", 0)
                dist_sq = (ax - target_x) ** 2 + (ay - target_y) ** 2
                # Cap at 500 units^2 like ConsetFindNearestNPC
                if dist_sq > 500 * 500:
                    continue
                npcs.append((dist_sq, a.get("id")))
            else:
                npcs.append((a.get("distance", 9999), a.get("id")))
        npcs.sort()
        return [npc_id for _, npc_id in npcs[:max_count]]

    def find_nearest_npc(self, target_x: float | None = None,
                         target_y: float | None = None) -> int | None:
        """Find nearest NPC (allegiance=6). Matches target coords if given."""
        npcs = self.find_nearby_npcs(1, target_x, target_y)
        return npcs[0] if npcs else None

    async def move_to_and_wait(self, x: float, y: float, label: str, timeout: float = 45.0):
        """Move to coordinates and wait until close enough.

        Uses query_fresh_state for position checks — that path drains the pipe
        and requests a fresh server-built tier-3 snapshot, which is the only
        reliable way to get current position after the pipe has accumulated
        many seconds of stale messages during buys/crafts.
        """
        print(f"[MOVE] Moving to {label} ({x}, {y})...")

        # Check if already at target before issuing a move
        snap = await self.query_fresh_state(timeout=3.0, settle_ms=100)
        if snap:
            px, py = self.get_pos()
            dist = ((px - x) ** 2 + (py - y) ** 2) ** 0.5
            if dist < 250:
                print(f"[MOVE] Already at {label} (dist={dist:.0f})")
                return True

        await self.action("move_to", {"x": x, "y": y}, wait_ms=100)
        last_move = time.time()

        deadline = time.time() + timeout
        while time.time() < deadline:
            # Fresh-state poll: forces a new server-side snapshot so position
            # reflects the current game state, not stale pipe buffer.
            snap = await self.query_fresh_state(timeout=2.0, settle_ms=200)
            if snap:
                px, py = self.get_pos()
                dist = ((px - x) ** 2 + (py - y) ** 2) ** 0.5
                if dist < 250:
                    print(f"[MOVE] Arrived at {label} (dist={dist:.0f})")
                    return True

            # Re-issue move every 2 seconds (keeps path fresh, re-trigger
            # MovePlayerNear in case prior thread exited)
            if time.time() - last_move >= 2.0:
                await self.ipc.send_action("move_to", {"x": x, "y": y}, self._next_req_id())
                last_move = time.time()

        print(f"[MOVE] TIMEOUT reaching {label}")
        return False

    async def open_merchant_npc(self, agent_id: int, label: str) -> bool:
        """Open a merchant NPC dialog and wait for items."""
        for attempt in range(3):
            # First target the NPC, wait for movement to settle
            await self.action("change_target", {"agent_id": agent_id}, wait_ms=500)
            print(f"[NPC] Opening {label} (agent={agent_id}, attempt {attempt+1})...")
            # Try open_merchant (GoNPC packet) first — this is the proven approach
            # from the C++ test (ConsetOpenNPCDialog).
            await self.action("open_merchant", {"agent_id": agent_id}, wait_ms=2000)
            # Query fresh state to check if merchant actually opened
            snap = await self.query_fresh_state(timeout=3.0, settle_ms=500)
            if snap:
                merchant = snap.get("merchant", {})
                items = merchant.get("items", [])
                item_count = merchant.get("item_count", 0)
                if merchant.get("is_open") and items:
                    print(f"[NPC] {label} open: {len(items)} items")
                    return True
                elif merchant.get("is_open") and item_count > 0:
                    print(f"[NPC] {label} has item_count={item_count} but items list empty, accepting")
                    return True
            # Fallback to interact_npc on next attempt
            if attempt < 2:
                await self.action("interact_npc", {"agent_id": agent_id}, wait_ms=2000)
                snap = await self.query_fresh_state(timeout=3.0, settle_ms=500)
                if snap:
                    merchant = snap.get("merchant", {})
                    items = merchant.get("items", [])
                    if merchant.get("is_open") and items:
                        print(f"[NPC] {label} open via interact_npc: {len(items)} items")
                        return True
        print(f"[NPC] FAILED to open {label} after 3 attempts")
        return False

    async def run(self):
        if not await self.connect():
            return False

        # Get initial snapshot
        snap = await self.read_until_snapshot()
        if not snap:
            print("[ERROR] No initial snapshot")
            return False

        # Wait for game to settle after bootstrap, then get fresh state
        print("[START] Waiting for game to settle...")
        await asyncio.sleep(3)
        snap = await self.query_fresh_state(timeout=10.0)
        if snap:
            print(f"[START] Got tier-{snap.get('tier')} snapshot")
        print(f"[START] Map={self.get_map_id()} Gold={self.get_gold()} Storage={self.get_storage_gold()}")
        print(f"[START] Pos={self.get_pos()}")

        # Surface what Gemma would see if she queried the knowledge tools.
        # This doubles as an assertion that the lookups cover the farming
        # path: if a recipe or crafter disappears from the knowledge tables,
        # the test run makes it immediately visible rather than surfacing as
        # a craft failure 90 seconds in.
        print(f"[KNOWLEDGE] Xunlai=({XUNLAI_X},{XUNLAI_Y}) "
              f"Material Trader=({MATERIAL_TRADER_X},{MATERIAL_TRADER_Y}) "
              f"(via get_outpost_info({MAP_EMBARK_BEACH}))")
        for recipe_name, (model_id, mat_ids, mat_qtys) in RECIPES.items():
            crafter_name, cx, cy = CRAFTERS[recipe_name]
            mat_summary = ", ".join(
                f"{q}x {fk.get_material_info(mid).get('name', mid)}"
                for mid, q in zip(mat_ids, mat_qtys))
            print(f"[KNOWLEDGE] {recipe_name} model={model_id} "
                  f"crafter={crafter_name}@({cx:.0f},{cy:.0f}) "
                  f"mats=[{mat_summary}] cost={CRAFT_COST}g/craft")

        # 1. Travel to Embark Beach if not there
        if self.get_map_id() != MAP_EMBARK_BEACH:
            print("[TRAVEL] Traveling to Embark Beach...")
            await self.action("travel", {"map_id": MAP_EMBARK_BEACH}, wait_ms=5000)
            for _ in range(60):
                snap = await self.read_until_snapshot(timeout=2.0)
                if snap and self.get_map_id() == MAP_EMBARK_BEACH:
                    break
            if self.get_map_id() != MAP_EMBARK_BEACH:
                print("[ERROR] Failed to travel to Embark Beach")
                return False
            await asyncio.sleep(3)  # let world settle
            await self.read_until_snapshot()

        # 2. Withdraw gold
        gold = self.get_gold()
        storage_gold = self.get_storage_gold()
        print(f"[GOLD] Character={gold} Storage={storage_gold} Target={TARGET_GOLD}")
        if gold < TARGET_GOLD and storage_gold > 0:
            await self.move_to_and_wait(XUNLAI_X, XUNLAI_Y, "Xunlai Chest")
            # Find and interact with Xunlai NPC (closest to target coords)
            snap = await self.read_until_snapshot()
            npc_id = self.find_nearest_npc(XUNLAI_X, XUNLAI_Y)
            if npc_id:
                await self.action("interact_npc", {"agent_id": npc_id}, wait_ms=2000)
            withdraw = min(TARGET_GOLD - gold, storage_gold)
            print(f"[GOLD] Withdrawing {withdraw}...")
            await self.action("withdraw_gold", {"amount": withdraw}, wait_ms=1000)
            # Query fresh state to get updated gold
            await self.query_fresh_state(timeout=5.0)
            print(f"[GOLD] After withdraw: Character={self.get_gold()} Storage={self.get_storage_gold()}")

        # 3. Open material trader and buy materials
        num_consets = 5
        await self.move_to_and_wait(MATERIAL_TRADER_X, MATERIAL_TRADER_Y, "Material Trader")
        snap = await self.read_until_snapshot(min_tier=2, timeout=5.0)
        # Try multiple nearby NPCs near target coords, not player
        npc_ids = self.find_nearby_npcs(5, MATERIAL_TRADER_X, MATERIAL_TRADER_Y)
        if not npc_ids:
            print("[ERROR] No NPCs found near material trader")
            return False
        merchant_opened = False
        for npc_id in npc_ids:
            print(f"[NPC] Trying NPC {npc_id} for material trader...")
            if await self.open_merchant_npc(npc_id, "Material Trader"):
                merchant_opened = True
                break
        if not merchant_opened:
            print("[ERROR] Failed to open material trader after trying all nearby NPCs")
            return False

        # 4. Buy materials via trader_buy
        materials_needed = {
            MAT_IRON: num_consets * 100,
            MAT_DUST: num_consets * 100,
            MAT_BONE: num_consets * 50,
            MAT_FEATHER: num_consets * 50,
        }
        merchant_items = self.get_merchant_items()
        print(f"[BUY] Merchant has {len(merchant_items)} items. Planning {num_consets} consets.")
        for it in merchant_items:
            print(f"  item_id={it.get('item_id')} model={it.get('model_id')} type={it.get('type')} qty={it.get('quantity')} val={it.get('value')}")

        # Query fresh state for inventory counts
        await self.query_fresh_state(timeout=5.0)

        for mat_model, needed in materials_needed.items():
            have = self.count_material(mat_model)
            if have >= needed:
                print(f"[BUY] Material {mat_model}: have={have} need={needed} — skip")
                continue
            missing = needed - have
            packs = (missing + 9) // 10
            print(f"[BUY] Material {mat_model}: have={have} need={needed} missing={missing} packs={packs}")

            # Use trader_buy with model_id — the C++ side resolves the virtual
            # item ID automatically via FindVirtualItemByModel.
            # trader_buy is synchronous on the DLL side (quote+transact completes
            # before returning), so wait just enough for the game to process.
            for p in range(packs):
                await self.action("trader_buy", {"model_id": mat_model}, wait_ms=200)
                if (p + 1) % 20 == 0:
                    print(f"[BUY] Pausing after {p+1} packs...")
                    await asyncio.sleep(2)

            # Use query_state with long settle to let all pending buys complete
            snap = await self.query_fresh_state(timeout=8.0, settle_ms=1000)
            final = self.count_material(mat_model)
            print(f"[BUY] Material {mat_model}: final count={final}")

        # 5. Craft consumables
        snap = await self.query_fresh_state(timeout=5.0)
        print(f"[CRAFT] Starting crafting phase. Gold={self.get_gold()}")

        results = {}
        for recipe_name, (model_id, mat_ids, mat_qtys) in RECIPES.items():
            crafter_name, cx, cy = CRAFTERS[recipe_name]
            print(f"\n[CRAFT] === {recipe_name} at {crafter_name} ===")

            # Precheck materials: if the trader was out-of-stock we never bought
            # some models. Crafting without enough would fail on the server side
            # after we spent the travel time — detect and skip upfront.
            max_craftable = 999
            for mat_id, per_craft in zip(mat_ids, mat_qtys):
                have = self.count_material(mat_id)
                affordable = have // per_craft
                if affordable < max_craftable:
                    max_craftable = affordable
            if max_craftable <= 0:
                missing_mats = [mid for mid, per in zip(mat_ids, mat_qtys)
                                if self.count_material(mid) < per]
                print(f"[CRAFT] Skipping {recipe_name} — insufficient materials "
                      f"(missing models: {missing_mats})")
                results[recipe_name] = 0
                continue
            if max_craftable < num_consets:
                print(f"[CRAFT] Only enough materials for {max_craftable}x "
                      f"{recipe_name} (wanted {num_consets})")

            await self.move_to_and_wait(cx, cy, crafter_name)
            snap = await self.read_until_snapshot()

            # Find crafter NPCs near target coords, try each until merchant opens
            npc_ids = self.find_nearby_npcs(3, cx, cy)
            if not npc_ids:
                print(f"[CRAFT] No NPCs near {crafter_name}")
                results[recipe_name] = 0
                continue

            merchant_opened = False
            for npc_id in npc_ids:
                if await self.open_merchant_npc(npc_id, crafter_name):
                    merchant_opened = True
                    break
            if not merchant_opened:
                print(f"[CRAFT] Failed to open {crafter_name}")
                results[recipe_name] = 0
                continue

            # Use model_id directly for crafting — the CraftMerchantItemByModelId
            # function looks up the merchant item by model internally.
            # We pass item_id=0 as a placeholder; the model_id is what matters.
            craft_item_id = 0  # will be resolved by model_id in the craft command

            # Craft in batches of up to 5
            # Use query_state to get fresh inventory count before crafting
            await self.query_fresh_state(timeout=3.0)
            before = self.count_consumable(model_id)
            print(f"[CRAFT] {recipe_name} starting count: {before}")
            remaining = min(num_consets, max_craftable)
            crafted = 0
            while remaining > 0:
                batch = min(remaining, 5)
                gold_cost = CRAFT_COST * batch
                print(f"[CRAFT] Crafting {batch}x {recipe_name} (gold={self.get_gold()})...")
                await self.action("craft_item", {
                    "item_id": craft_item_id,
                    "quantity": batch,
                    "gold": gold_cost,
                    "model_id": model_id,
                    "material_model_ids": mat_ids,
                    "material_quantities": mat_qtys,
                }, wait_ms=2000)
                # Query fresh state to see if craft succeeded
                await self.query_fresh_state(timeout=3.0)
                after = self.count_consumable(model_id)
                batch_crafted = after - before - crafted
                if batch_crafted > 0:
                    crafted += batch_crafted
                    remaining -= batch_crafted
                    print(f"[CRAFT] Crafted {batch_crafted} (total={crafted}/{num_consets})")
                else:
                    print(f"[CRAFT] Batch failed — stopping")
                    break

            results[recipe_name] = crafted

        # Summary
        print("\n" + "=" * 60)
        print("[RESULT] Conset Crafting Complete!")
        total_gold = self.get_gold()
        for name, count in results.items():
            print(f"  {name}: {count}")
        complete_sets = min(results.values()) if results else 0
        print(f"  Complete sets: {complete_sets}")
        print(f"  Gold remaining: {total_gold}")
        print("=" * 60)

        return complete_sets > 0


async def main():
    test = ConsetBridgeTest()
    try:
        success = await test.run()
        print(f"\n{'PASS' if success else 'FAIL'}")
        return 0 if success else 1
    except Exception as e:
        print(f"\n[ERROR] {e}")
        import traceback
        traceback.print_exc()
        return 1
    finally:
        test.ipc.disconnect()


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
