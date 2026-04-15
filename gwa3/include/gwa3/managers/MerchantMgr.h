#pragma once
// Merchant functionality is part of TradeMgr — trade, merchant, and craft are one subsystem.
//
// Key merchant functions in TradeMgr.h / TradeMgr.cpp:
//   GetMerchantItemCount/ByPosition/ByModelId() — Merchant inventory access
//   BuyMerchantItemByPosition/ByModelId()       — Purchase from NPC merchant
//   SellMerchantItem()                          — Sell to NPC merchant
//   RequestTraderQuoteByItemId/ByModelId()      — Rare material trader quotes
//   BuyMaterials()                              — Material trader bulk buy
//   RequestQuote() / TransactItems()            — Low-level trader operations
#include <gwa3/managers/TradeMgr.h>
