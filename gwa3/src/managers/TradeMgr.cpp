#include <gwa3/managers/TradeMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>
#include <gwa3/core/Log.h>

namespace GWA3::TradeMgr {

static bool s_initialized = false;

bool Initialize() {
    if (s_initialized) return true;
    s_initialized = true;
    Log::Info("TradeMgr: Initialized");
    return true;
}

void InitiateTrade(uint32_t agentId)   { CtoS::TradePlayer(agentId); }
void CancelTrade()                      { CtoS::TradeCancel(); }
void AcceptTrade()                      { CtoS::TradeAccept(); }

void OfferItem(uint32_t itemId) {
    CtoS::SendPacket(2, Packets::TRADE_OFFER_ITEM, itemId);
}

void SubmitOffer() {
    CtoS::SendPacket(1, Packets::TRADE_SUBMIT_OFFER);
}

void ChangeOffer() {
    CtoS::SendPacket(1, Packets::TRADE_CHANGE_OFFER);
}

void RemoveItem(uint32_t itemId) {
    CtoS::SendPacket(2, Packets::TRADE_REMOVE_ITEM, itemId);
}

void BuyMaterials(uint32_t modelId, uint32_t quantity) {
    CtoS::SendPacket(3, Packets::BUY_MATERIALS, modelId, quantity);
}

void RequestQuote(uint32_t itemId) {
    CtoS::SendPacket(2, Packets::REQUEST_QUOTE, itemId);
}

void TransactItems(uint32_t type, uint32_t quantity, uint32_t itemId) {
    CtoS::SendPacket(4, Packets::TRANSACT_ITEMS, type, quantity, itemId);
}

} // namespace GWA3::TradeMgr
