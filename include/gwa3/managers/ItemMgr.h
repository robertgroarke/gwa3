#pragma once

#include <gwa3/game/Item.h>
#include <cstdint>

namespace GWA3::ItemMgr {

    bool Initialize();

    // Item manipulation
    void UseItem(uint32_t itemId);
    void EquipItem(uint32_t itemId);
    void DropItem(uint32_t itemId);
    void MoveItem(uint32_t itemId, uint32_t bagId, uint32_t slot);
    void PickUpItem(uint32_t itemAgentId);
    void IdentifyItem(uint32_t itemId, uint32_t kitId);

    // Salvage session
    void SalvageSessionOpen(uint32_t kitId, uint32_t itemId);
    void SalvageMaterials();
    void SalvageSessionDone();

    // Merchant
    void BuyMaterials(uint32_t itemModelId, uint32_t quantity);
    void RequestQuote(uint32_t itemId);

    // Gold
    void ChangeGold(uint32_t charGold, uint32_t storageGold);
    void DropGold(uint32_t amount);

    // Xunlai storage state tracking. The CHANGE_GOLD packet and storage
    // MoveItem packets are only server-legal while the Xunlai chest is
    // "active" — i.e. shortly after sending the GoNPC INTERACT_NPC packet
    // to a Xunlai NPC. Firing those packets outside that window is
    // treated by the server as invalid and disconnects the client
    // (Code=007). These helpers track the last-opened tick so handlers
    // can refuse when the state is stale.
    void MarkXunlaiOpened();
    bool IsXunlaiRecentlyOpened();  // true if opened within the last 15s

    // Item interaction (uses native ItemClick function when resolved)
    bool ClickItem(uint32_t itemId);

    // Data access
    Item* GetItemById(uint32_t itemId);
    Item* FindItemByModelId(uint32_t modelId);
    Bag* GetBag(uint32_t bagIndex);
    Inventory* GetInventory();
    uint32_t GetGoldCharacter();
    uint32_t GetGoldStorage();

} // namespace GWA3::ItemMgr
