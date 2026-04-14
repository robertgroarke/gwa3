#pragma once

#include <cstdint>

namespace GWA3 {
    struct Item;
}

namespace GWA3::TradeMgr {

    bool Initialize();

    // Player-to-player trade
    void InitiateTrade(uint32_t agentId);
    void OfferItem(uint32_t itemId);
    void SubmitOffer(uint32_t gold = 0);
    void AcceptTrade();
    void CancelTrade();
    void ChangeOffer();
    void RemoveItem(uint32_t itemId);

    // NPC merchant
    void BuyMaterials(uint32_t modelId, uint32_t quantity);
    void RequestQuote(uint32_t itemId);
    void TransactItems(uint32_t type, uint32_t quantity, uint32_t itemId);
    uint32_t GetMerchantItemCount();
    Item* GetMerchantItemByPosition(uint32_t itemPosition);
    Item* GetMerchantItemByModelId(uint32_t modelId);
    uint32_t GetMerchantItemIdByModelId(uint32_t modelId);
    bool BuyMerchantItemByPosition(uint32_t itemPosition, uint32_t quantity, uint32_t unitValue);
    bool BuyMerchantItemByModelId(uint32_t modelId, uint32_t quantity);
    bool SellMerchantItem(uint32_t itemId, uint32_t quantity, uint32_t totalValue);
    bool RequestTraderQuoteByItemId(uint32_t itemId);
    bool RequestTraderQuoteByModelId(uint32_t modelId);

    // Trade quantity prompt
    uint32_t GetTradeQuantityPromptFrame();
    uint32_t GetTradeQuantityPromptChildCount();
    bool IsTradeQuantityPromptOpen();
    bool ConfirmTradeQuantityPromptValue(uint32_t quantity);
    bool ConfirmTradeQuantityPromptMax();
    bool OfferItemPromptMax(uint32_t itemId);

    // Trade UI state queries
    uint32_t GetTradeUiPlayerUpdatedCount();
    uint32_t GetTradeUiSessionStartCount();
    uint32_t GetTradeUiSessionUpdatedCount();
    uint32_t GetTradeUiLastSessionStartState();
    uint32_t GetTradeUiLastSessionStartPlayerNumber();

    // Sell inventory item to NPC merchant
    bool SellInventoryItem(uint32_t itemId, uint32_t quantity = 0);

    // Crafter packet-level operations
    bool RequestCrafterQuoteByPositionPacket(uint32_t itemPosition);
    bool CraftMerchantItemByPositionPacket(uint32_t itemPosition, uint32_t quantity);

    // Crafter via native Transaction function (UIMessage or direct call)
    bool CraftMerchantItem(uint32_t itemId, uint32_t quantity, uint32_t totalValue,
                           const uint32_t* materialModelIds, const uint32_t* materialQuantities,
                           uint32_t materialCount);
    bool CraftMerchantItemByModelId(uint32_t modelId, uint32_t quantity, uint32_t totalValue,
                                    const uint32_t* materialModelIds, const uint32_t* materialQuantities,
                                    uint32_t materialCount);

} // namespace GWA3::TradeMgr
