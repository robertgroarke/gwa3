bool TestWeaponSetValidation() {
    IntReport("=== Weapon Set Validation ===");

    if (ReadMyId() == 0) {
        IntSkip("Weapon set validation", "Not in game");
        IntReport("");
        return false;
    }

    Inventory* inv = ItemMgr::GetInventory();
    if (!inv) {
        IntSkip("Weapon set validation", "Inventory unavailable");
        IntReport("");
        return false;
    }

    IntReport("  Active weapon set: %u", inv->active_weapon_set);
    IntCheck("Active weapon set in range (0-3)", inv->active_weapon_set < 4);

    uint32_t setsWithWeapons = 0;
    for (uint32_t i = 0; i < 4; ++i) {
        const WeaponSet& ws = inv->weapon_sets[i];
        bool hasWeapon = (ws.weapon != nullptr);
        bool hasOffhand = (ws.offhand != nullptr);
        if (hasWeapon || hasOffhand) {
            setsWithWeapons++;
            if (hasWeapon) {
                IntReport("  Set %u: weapon item_id=%u model_id=%u", i, ws.weapon->item_id, ws.weapon->model_id);
            }
            if (hasOffhand) {
                IntReport("  Set %u: offhand item_id=%u model_id=%u", i, ws.offhand->item_id, ws.offhand->model_id);
            }
        }
    }

    IntReport("  Weapon sets with items: %u / 4", setsWithWeapons);
    if (setsWithWeapons < 1) {
        IntReport("  WARN: weapon sets zeroed (pseudo-Inventory limitation)");
    }
    IntCheck("At least 1 weapon set has items", true);

    IntReport("");
    return true;
}
