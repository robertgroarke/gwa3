#include <gwa3/managers/TradeMgr.h>
#include <gwa3/core/Log.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/RenderHook.h>
#include <gwa3/core/TraderHook.h>
#include <gwa3/game/Item.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>

#include <Windows.h>
#include <cstring>

namespace GWA3::TradeMgr {

static bool s_initialized = false;
static constexpr int kQuoteSlots = 4;
static constexpr int kQuoteSlotSize = 64;
static uintptr_t s_requestQuoteBase = 0;
static volatile LONG s_quoteSlotIndex = 0;

static bool ReadPtr(uintptr_t address, uintptr_t& value) {
    if (address <= 0x10000) return false;
    __try {
        value = *reinterpret_cast<uintptr_t*>(address);
        return value > 0x10000;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        value = 0;
        return false;
    }
}

static bool GetMerchantItemsBaseAndSize(uintptr_t& base, uint32_t& size) {
    base = 0;
    size = 0;

    if (Offsets::BasePointer <= 0x10000) return false;

    uintptr_t p0 = 0;
    uintptr_t p1 = 0;
    uintptr_t p2 = 0;
    if (!ReadPtr(Offsets::BasePointer, p0)) return false;
    if (!ReadPtr(p0 + 0x18, p1)) return false;
    if (!ReadPtr(p1 + 0x2C, p2)) return false;

    uintptr_t merchantBase = 0;
    uintptr_t merchantSize = 0;
    if (!ReadPtr(p2 + 0x24, merchantBase)) return false;
    if (!ReadPtr(p2 + 0x28, merchantSize)) return false;

    base = merchantBase;
    size = static_cast<uint32_t>(merchantSize);
    return base > 0x10000 && size > 0 && size < 4096;
}

static Item* GetMerchantItemPtrByItemId(uint32_t itemId) {
    if (Offsets::BasePointer <= 0x10000 || itemId == 0) return nullptr;

    uintptr_t p0 = 0;
    uintptr_t p1 = 0;
    uintptr_t p2 = 0;
    uintptr_t p3 = 0;
    if (!ReadPtr(Offsets::BasePointer, p0)) return nullptr;
    if (!ReadPtr(p0 + 0x18, p1)) return nullptr;
    if (!ReadPtr(p1 + 0x40, p2)) return nullptr;
    if (!ReadPtr(p2 + 0xB8, p3)) return nullptr;

    uintptr_t itemPtr = 0;
    if (!ReadPtr(p3 + static_cast<uintptr_t>(itemId) * 4, itemPtr)) return nullptr;
    return reinterpret_cast<Item*>(itemPtr);
}

static bool EnsureRequestQuoteShellcode() {
    if (s_requestQuoteBase) return true;

    void* mem = VirtualAlloc(nullptr, kQuoteSlots * kQuoteSlotSize,
                             MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!mem) {
        Log::Error("TradeMgr: VirtualAlloc failed for request-quote shellcode pool");
        return false;
    }

    s_requestQuoteBase = reinterpret_cast<uintptr_t>(mem);
    return true;
}

static void NextQuoteSlot(uintptr_t& scAddr, uintptr_t& itemAddr) {
    LONG idx = InterlockedIncrement(&s_quoteSlotIndex) % kQuoteSlots;
    scAddr = s_requestQuoteBase + idx * kQuoteSlotSize;
    itemAddr = scAddr + 48;
}

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

uint32_t GetMerchantItemCount() {
    uintptr_t base = 0;
    uint32_t size = 0;
    if (!GetMerchantItemsBaseAndSize(base, size)) return 0;
    return size;
}

Item* GetMerchantItemByModelId(uint32_t modelId) {
    uintptr_t merchantBase = 0;
    uint32_t merchantSize = 0;
    if (!GetMerchantItemsBaseAndSize(merchantBase, merchantSize)) return nullptr;

    __try {
        for (uint32_t i = 0; i < merchantSize; ++i) {
            const uint32_t itemId = *reinterpret_cast<uint32_t*>(merchantBase + i * 4);
            if (!itemId) continue;

            Item* item = GetMerchantItemPtrByItemId(itemId);
            if (!item) continue;
            if (item->model_id == modelId && item->bag == nullptr && item->agent_id == 0) {
                return item;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }

    return nullptr;
}

uint32_t GetMerchantItemIdByModelId(uint32_t modelId) {
    Item* item = GetMerchantItemByModelId(modelId);
    return item ? item->item_id : 0;
}

bool RequestTraderQuoteByItemId(uint32_t itemId) {
    if (!Offsets::RequestQuote || itemId == 0) return false;
    if (!RenderHook::IsInitialized() || !EnsureRequestQuoteShellcode()) return false;

    TraderHook::Reset();

    uintptr_t scAddr = 0;
    uintptr_t itemAddr = 0;
    NextQuoteSlot(scAddr, itemAddr);

    memcpy(reinterpret_cast<void*>(itemAddr), &itemId, sizeof(itemId));

    auto* sc = reinterpret_cast<uint8_t*>(scAddr);
    size_t i = 0;
    auto emit8 = [&](uint8_t v) { sc[i++] = v; };
    auto emit32 = [&](uint32_t v) { memcpy(sc + i, &v, sizeof(v)); i += 4; };

    emit8(0xB8); emit32(static_cast<uint32_t>(itemAddr)); // mov eax, item_ptr
    emit8(0x50);                                                     // push eax
    emit8(0x6A); emit8(0x01);                                        // push 1
    emit8(0x6A); emit8(0x00);                                        // push 0
    emit8(0x6A); emit8(0x00);                                        // push 0
    emit8(0x6A); emit8(0x00);                                        // push 0
    emit8(0x6A); emit8(0x00);                                        // push 0
    emit8(0x6A); emit8(0x00);                                        // push 0
    emit8(0x6A); emit8(0x0C);                                        // push 0xC (TraderBuy quote)
    emit8(0x31); emit8(0xC9);                                        // xor ecx, ecx
    emit8(0xBA); emit32(0x00000002);                                 // mov edx, 2
    emit8(0xB8); emit32(static_cast<uint32_t>(Offsets::RequestQuote)); // mov eax, RequestQuote
    emit8(0xFF); emit8(0xD0);                                        // call eax
    emit8(0x83); emit8(0xC4); emit8(0x20);                           // add esp, 0x20
    emit8(0xC3);                                                     // ret

    FlushInstructionCache(GetCurrentProcess(), sc, static_cast<DWORD>(i));
    return RenderHook::EnqueueCommand(scAddr);
}

bool RequestTraderQuoteByModelId(uint32_t modelId) {
    const uint32_t itemId = GetMerchantItemIdByModelId(modelId);
    return itemId != 0 && RequestTraderQuoteByItemId(itemId);
}

} // namespace GWA3::TradeMgr
