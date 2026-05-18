"""Inventory snapshot helpers for the Froggy HM bridge test."""

from .froggy_hm_route_helpers import (
    ALT_ID_KIT_MODEL,
    ALT_SALVAGE_KIT_MODEL,
    CHEAP_ID_KIT_MODEL,
    CHEAP_SALVAGE_KIT_MODEL,
    EXPERT_SALVAGE_MODEL,
    MAINTENANCE_KIT_MODELS,
    RARE_SALVAGE_KIT_MODEL,
    SUPERIOR_ID_KIT_MODEL,
    SUPERIOR_SALVAGE_KIT_MODEL,
)


class FroggyHmInventoryMixin:
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
            + self.count_inventory_model(SUPERIOR_SALVAGE_KIT_MODEL)
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
            SUPERIOR_SALVAGE_KIT_MODEL,
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
            if not bool(item.get("is_identified", True)):
                continue
            if int(item.get("quantity", 1) or 1) > 1:
                continue
            return item_id, kit_id
        return 0, kit_id
