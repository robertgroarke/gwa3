#pragma once

#include <cstdint>

namespace GWA3 {
    struct Item;
}

namespace GWA3::MerchantMgr {

    bool Initialize();

    bool BuyMaterials(uint32_t modelId, uint32_t quantity);
    bool SellMaterialsToTrader(uint32_t itemId, uint32_t transactions = 1);
    void RequestQuote(uint32_t itemId);
    void TransactItems(uint32_t type, uint32_t quantity, uint32_t itemId);

    uint32_t GetMerchantItemCount();
    Item* GetMerchantItemByPosition(uint32_t itemPosition);
    Item* GetMerchantItemByModelId(uint32_t modelId);
    uint32_t GetMerchantItemIdByModelId(uint32_t modelId);

    bool BuyMerchantItemByPosition(uint32_t itemPosition, uint32_t quantity, uint32_t unitValue);
    bool BuyMerchantItem(uint32_t itemId, uint32_t quantity);
    bool SellInventoryItem(uint32_t itemId, uint32_t quantity = 0);

    bool RequestTraderQuoteByItemId(uint32_t itemId);

    bool RequestCrafterQuoteByItemId(uint32_t itemId);
    bool RequestCrafterQuoteByModelId(uint32_t modelId);
    bool RequestCrafterQuoteByPosition(uint32_t itemPosition);
    bool RequestCrafterQuoteByItemIdPacket(uint32_t itemId);
    bool RequestCrafterQuoteByPositionPacket(uint32_t itemPosition);

    bool CraftMerchantItem(uint32_t itemId, uint32_t quantity, uint32_t totalValue,
                           const uint32_t* materialModelIds, const uint32_t* materialQuantities,
                           uint32_t materialCount);
    bool CraftMerchantItemByModelId(uint32_t modelId, uint32_t quantity, uint32_t totalValue,
                                    const uint32_t* materialModelIds, const uint32_t* materialQuantities,
                                    uint32_t materialCount);
    bool CraftMerchantItemByPosition(uint32_t itemPosition, uint32_t quantity, uint32_t totalValue,
                                     const uint32_t* materialModelIds, const uint32_t* materialQuantities,
                                     uint32_t materialCount);
    bool CraftMerchantItemByItemIdPacket(uint32_t itemId, uint32_t quantity);
    bool CraftMerchantItemByPositionPacket(uint32_t itemPosition, uint32_t quantity);

} // namespace GWA3::MerchantMgr
