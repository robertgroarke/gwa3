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


# --- Constants ---
MAP_EMBARK_BEACH = 857

# NPC coordinates in Embark Beach
XUNLAI_X, XUNLAI_Y = 2283, -2134
MATERIAL_TRADER_X, MATERIAL_TRADER_Y = 2933, -2236
EYJA_X, EYJA_Y = 3336, 627       # Grail of Might crafter
KWAT_X, KWAT_Y = 3596, 107       # Essence of Celerity crafter
ALCUS_X, ALCUS_Y = 3704, -163    # Armor of Salvation crafter

# Material model IDs
MAT_IRON = 948
MAT_DUST = 929
MAT_BONE = 921
MAT_FEATHER = 933

# Consumable model IDs
MODEL_GRAIL = 24861
MODEL_ESSENCE = 24859
MODEL_ARMOR = 24860

# Recipes: (model_id, material_model_ids, material_quantities)
RECIPES = {
    "Grail of Might": (MODEL_GRAIL, [MAT_IRON, MAT_DUST], [50, 50]),
    "Essence of Celerity": (MODEL_ESSENCE, [MAT_FEATHER, MAT_DUST], [50, 50]),
    "Armor of Salvation": (MODEL_ARMOR, [MAT_IRON, MAT_BONE], [50, 50]),
}

CRAFTERS = {
    "Grail of Might": ("Eyja", EYJA_X, EYJA_Y),
    "Essence of Celerity": ("Kwat", KWAT_X, KWAT_Y),
    "Armor of Salvation": ("Alcus", ALCUS_X, ALCUS_Y),
}

TARGET_GOLD = 100000
CRAFT_COST = 250  # gold per craft


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

    def find_nearest_npc(self) -> int | None:
        """Find nearest NPC agent (allegiance=6) from snapshot."""
        agents = self.snapshot.get("agents", [])
        best_id = None
        best_dist = 9999
        for a in agents:
            if a.get("agent_type") != "living":
                continue
            if a.get("allegiance") != 6:  # NPC allegiance
                continue
            dist = a.get("distance", 9999)
            if dist < best_dist:
                best_dist = dist
                best_id = a.get("id")
        return best_id

    async def move_to_and_wait(self, x: float, y: float, label: str, timeout: float = 45.0):
        """Move to coordinates and wait until close enough. Re-issues move every 2s."""
        print(f"[MOVE] Moving to {label} ({x}, {y})...")
        await self.action("move_to", {"x": x, "y": y}, wait_ms=100)
        last_move = time.time()

        deadline = time.time() + timeout
        while time.time() < deadline:
            # Read any available message (snapshot, action_result, etc.)
            msg = await asyncio.wait_for(self.ipc.read_message(), timeout=2.0) if self.ipc.connected else None
            if msg and msg.get("type") == "snapshot":
                self.snapshot = msg
                px, py = self.get_pos()
                dist = ((px - x) ** 2 + (py - y) ** 2) ** 0.5
                if dist < 250:
                    print(f"[MOVE] Arrived at {label} (dist={dist:.0f})")
                    return True

            # Re-issue move every 2 seconds
            if time.time() - last_move >= 2.0:
                await self.ipc.send_action("move_to", {"x": x, "y": y}, self._next_req_id())
                last_move = time.time()

        print(f"[MOVE] TIMEOUT reaching {label}")
        return False

    async def open_merchant_npc(self, agent_id: int, label: str) -> bool:
        """Open a merchant NPC dialog and wait for items."""
        print(f"[NPC] Opening {label} (agent={agent_id})...")
        await self.action("open_merchant", {"agent_id": agent_id}, wait_ms=2000)
        snap = await self.read_until_snapshot(timeout=5.0)
        if snap and self.is_merchant_open():
            items = self.get_merchant_items()
            print(f"[NPC] {label} open: {len(items)} items")
            return True
        # Retry with interact_npc
        print(f"[NPC] Retrying {label} with interact_npc...")
        await self.action("interact_npc", {"agent_id": agent_id}, wait_ms=2000)
        snap = await self.read_until_snapshot(timeout=5.0)
        if snap and self.is_merchant_open():
            items = self.get_merchant_items()
            print(f"[NPC] {label} open on retry: {len(items)} items")
            return True
        print(f"[NPC] FAILED to open {label}")
        return False

    async def run(self):
        if not await self.connect():
            return False

        # Get initial snapshot
        snap = await self.read_until_snapshot()
        if not snap:
            print("[ERROR] No initial snapshot")
            return False

        # Wait for game to settle after bootstrap, then get tier-3 snapshot
        print("[START] Waiting for game to settle...")
        await asyncio.sleep(3)
        snap = await self.read_until_snapshot(min_tier=3, timeout=30.0)
        if snap:
            print(f"[START] Got tier-{snap.get('tier')} snapshot")
        print(f"[START] Map={self.get_map_id()} Gold={self.get_gold()} Storage={self.get_storage_gold()}")
        print(f"[START] Pos={self.get_pos()}")

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
            # Find and interact with Xunlai NPC
            snap = await self.read_until_snapshot()
            # Use open_merchant to interact with chest
            # The Xunlai doesn't need merchant — just interact + ChangeGold
            npc_id = self.find_nearest_npc()
            if npc_id:
                await self.action("interact_npc", {"agent_id": npc_id}, wait_ms=2000)
            withdraw = min(TARGET_GOLD - gold, storage_gold)
            print(f"[GOLD] Withdrawing {withdraw}...")
            await self.action("withdraw_gold", {"amount": withdraw}, wait_ms=500)
            snap = await self.read_until_snapshot()
            print(f"[GOLD] After withdraw: Character={self.get_gold()} Storage={self.get_storage_gold()}")

        # 3. Open material trader
        await self.move_to_and_wait(MATERIAL_TRADER_X, MATERIAL_TRADER_Y, "Material Trader")
        snap = await self.read_until_snapshot()
        # Find material trader NPC
        npc_id = self.find_nearest_npc()
        if not npc_id:
            print("[ERROR] No NPC found near material trader")
            return False
        if not await self.open_merchant_npc(npc_id, "Material Trader"):
            return False

        # 4. Buy materials
        # Calculate how many consets to craft based on budget
        gold = self.get_gold()
        materials_needed = {MAT_IRON: 0, MAT_DUST: 0, MAT_BONE: 0, MAT_FEATHER: 0}

        # For simplicity, buy 7 consets worth
        num_consets = 7
        materials_needed[MAT_IRON] = num_consets * 100  # 50 per Grail + 50 per Armor
        materials_needed[MAT_DUST] = num_consets * 100  # 50 per Grail + 50 per Essence
        materials_needed[MAT_BONE] = num_consets * 50   # 50 per Armor
        materials_needed[MAT_FEATHER] = num_consets * 50 # 50 per Essence

        merchant_items = self.get_merchant_items()
        print(f"[BUY] Merchant has {len(merchant_items)} items. Planning {num_consets} consets.")

        for mat_model, needed in materials_needed.items():
            have = self.count_material(mat_model)
            if have >= needed:
                print(f"[BUY] Material {mat_model}: have={have} need={needed} — skip")
                continue
            missing = needed - have
            packs = (missing + 9) // 10
            print(f"[BUY] Material {mat_model}: have={have} need={needed} missing={missing} packs={packs}")

            # Find this material's item_id in the merchant list
            item_id = None
            for item in merchant_items:
                if item.get("model_id") == mat_model:
                    item_id = item.get("item_id")
                    break
            if not item_id:
                print(f"[BUY] WARNING: Material {mat_model} not in merchant list — using virtual item scan")
                # The trader_buy command handles virtual items by ID
                # We need the virtual item ID — fall back to buying by the merchant item
                continue

            for p in range(packs):
                await self.action("trader_buy", {"item_id": item_id}, wait_ms=1500)
                if (p + 1) % 20 == 0:
                    print(f"[BUY] Pausing after {p+1} packs...")
                    await asyncio.sleep(2)
                    snap = await self.read_until_snapshot()

            snap = await self.read_until_snapshot()
            final = self.count_material(mat_model)
            print(f"[BUY] Material {mat_model}: final count={final}")

        # 5. Craft consumables
        snap = await self.read_until_snapshot()
        print(f"[CRAFT] Starting crafting phase. Gold={self.get_gold()}")

        results = {}
        for recipe_name, (model_id, mat_ids, mat_qtys) in RECIPES.items():
            crafter_name, cx, cy = CRAFTERS[recipe_name]
            print(f"\n[CRAFT] === {recipe_name} at {crafter_name} ===")

            await self.move_to_and_wait(cx, cy, crafter_name)
            snap = await self.read_until_snapshot()

            # Find crafter NPC
            npc_id = self.find_nearest_npc()
            if not npc_id:
                print(f"[CRAFT] No NPC near {crafter_name}")
                results[recipe_name] = 0
                continue

            if not await self.open_merchant_npc(npc_id, crafter_name):
                results[recipe_name] = 0
                continue

            # Find the item_id for this consumable in the crafter's list
            merchant_items = self.get_merchant_items()
            craft_item_id = None
            for item in merchant_items:
                if item.get("model_id") == model_id:
                    craft_item_id = item.get("item_id")
                    break
            if not craft_item_id:
                print(f"[CRAFT] {recipe_name} (model={model_id}) not found in crafter list")
                results[recipe_name] = 0
                continue

            # Craft in batches of up to 5
            before = self.count_consumable(model_id)
            remaining = num_consets
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
                snap = await self.read_until_snapshot()
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
