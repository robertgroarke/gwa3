#pragma once
// Crafting is implemented in TradeMgr — trade, merchant, and craft share UI state.
//
// Key functions in TradeMgr.h / TradeMgr.cpp:
//   CraftMerchantItem()                    — Native transaction call with materials
//   CraftMerchantItemByPositionPacket()    — Packet-level craft operation
//   RequestCrafterQuoteByPositionPacket()  — Quote request before crafting
//
// Quote responses are captured by TraderHook.h (core/).
// Craft commands execute on the game thread via GameThread::EnqueueRaw().
#include <gwa3/managers/TradeMgr.h>
