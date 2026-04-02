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
    void SubmitOffer();
    void AcceptTrade();
    void CancelTrade();
    void ChangeOffer();
    void RemoveItem(uint32_t itemId);

    // NPC merchant
    void BuyMaterials(uint32_t modelId, uint32_t quantity);
    void RequestQuote(uint32_t itemId);
    void TransactItems(uint32_t type, uint32_t quantity, uint32_t itemId);
    uint32_t GetMerchantItemCount();
    Item* GetMerchantItemByModelId(uint32_t modelId);
    uint32_t GetMerchantItemIdByModelId(uint32_t modelId);
    bool RequestTraderQuoteByItemId(uint32_t itemId);
    bool RequestTraderQuoteByModelId(uint32_t modelId);

} // namespace GWA3::TradeMgr
