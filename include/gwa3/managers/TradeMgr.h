#pragma once

#include <cstdint>

namespace GWA3 {
    struct Item;
}

namespace GWA3::TradeMgr {

    bool Initialize();

    // Player-to-player trade
    void InitiateTrade(uint32_t agentId, uint32_t requestedPlayerNumber = 0);
    void InitiateTradeUiOnly(uint32_t agentId, uint32_t requestedPlayerNumber = 0);
    void OfferItem(uint32_t itemId, uint32_t quantity = 0, uint32_t attemptsRemaining = 6);
    void OfferItemPacket(uint32_t itemId, uint32_t quantity);
    void SubmitOffer(uint32_t gold = 0);
    void AcceptTrade();
    void CancelTrade(uint32_t actionRowIndex = 10, int32_t preferredChildIndex = -1, uint32_t transportMode = 0);
    void ChangeOffer();
    void RemoveItem(uint32_t itemId);

    // NPC merchant
    bool BuyMaterials(uint32_t modelId, uint32_t quantity);
    void RequestQuote(uint32_t itemId);
    void TransactItems(uint32_t type, uint32_t quantity, uint32_t itemId);
    uint32_t GetMerchantItemCount();
    Item* GetMerchantItemByPosition(uint32_t itemPosition);
    Item* GetMerchantItemByModelId(uint32_t modelId);
    uint32_t GetMerchantItemIdByModelId(uint32_t modelId);
    bool BuyMerchantItemByPosition(uint32_t itemPosition, uint32_t quantity, uint32_t unitValue);
    bool BuyMerchantItem(uint32_t itemId, uint32_t quantity);
    bool RequestTraderQuoteByItemId(uint32_t itemId);
    bool SellMaterialsToTrader(uint32_t itemId, uint32_t transactions = 1);

    // Trade quantity prompt
    uint32_t GetTradeQuantityPromptFrame();
    uint32_t GetTradeQuantityPromptChildCount();
    bool IsTradeQuantityPromptOpen();
    bool ConfirmTradeQuantityPromptValue(uint32_t quantity);
    bool ConfirmTradeQuantityPromptMax();
    bool OfferItemPromptDefault(uint32_t itemId);
    bool OfferItemPromptValue(uint32_t itemId, uint32_t quantity);
    bool OfferItemPromptMax(uint32_t itemId);

    // Trade UI state queries
    uint32_t GetTradeUiPlayerUpdatedCount();
    uint32_t GetTradeUiInitiateCount();
    uint32_t GetTradeUiLastInitiateWParam();
    uint32_t GetTradeUiSessionStartCount();
    uint32_t GetTradeUiSessionUpdatedCount();
    uint32_t GetTradeUiLastSessionStartState();
    uint32_t GetTradeUiLastSessionStartPlayerNumber();
    uint32_t GetPartyButtonCallbackHitCount();
    uint32_t GetPartyButtonCallbackLastThis();
    uint32_t GetPartyButtonCallbackLastArg();
    uint32_t GetTradeWindowCaptureCount();
    uint32_t GetTradeWindowContext();
    uint32_t GetTradeWindowFrame();
    uint32_t GetTradeWindowUiFrame();
    uint32_t GetTradeWindowUiState();
    uint32_t GetTradeWindowUiContext();

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
