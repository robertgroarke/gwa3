#pragma once

#include <gwa3/game/Item.h>
#include <cstdint>

namespace GWA3::ItemMgr {

    bool Initialize();

    // Item manipulation
    void UseItem(uint32_t itemId);
    void EquipItem(uint32_t itemId);
    void DropItem(uint32_t itemId);
    void DestroyItem(uint32_t itemId);
    void MoveItem(uint32_t itemId, uint32_t bagId, uint32_t slot);
    void PickUpItem(uint32_t itemAgentId);
    void IdentifyItem(uint32_t itemId, uint32_t kitId);
    void SplitStack(uint32_t itemId, uint32_t quantity);

    // Salvage session
    void SalvageSessionOpen(uint32_t kitId, uint32_t itemId);
    void SalvageMaterials();
    void SalvageUpgrade();
    void SalvageSessionCancel();
    void SalvageSessionDone();

    // Merchant
    void BuyMaterials(uint32_t itemModelId, uint32_t quantity);
    void RequestQuote(uint32_t itemId);

    // Gold
    void ChangeGold(uint32_t charGold, uint32_t storageGold);
    void DropGold(uint32_t amount);

    // Item interaction (uses native ItemClick function when resolved)
    bool ClickItem(uint32_t itemId);

    // Data access
    Item* GetItemById(uint32_t itemId);
    Bag* GetBag(uint32_t bagIndex);
    Inventory* GetInventory();
    uint32_t GetGoldCharacter();
    uint32_t GetGoldStorage();

} // namespace GWA3::ItemMgr
