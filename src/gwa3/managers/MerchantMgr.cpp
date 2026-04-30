#include <gwa3/managers/MerchantMgr.h>
#include <gwa3/core/CallbackRegistry.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/RenderHook.h>
#include <gwa3/core/TraderHook.h>
#include <gwa3/game/Item.h>
#include <gwa3/managers/ChatMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>

#include <Windows.h>
#include <cstring>

namespace GWA3::MerchantMgr {

static bool s_initialized = false;


static constexpr int kQuoteSlots = 4;
static constexpr int kQuoteSlotSize = 64;
static constexpr uint32_t kCrafterMaxMaterials = 8;
static constexpr int kCrafterScratchSlots = 4;
static constexpr size_t kCrafterQuoteScratchSize = 64;
static constexpr size_t kCrafterTransactScratchSize = 128;
static uintptr_t s_requestQuoteBase = 0;
static volatile LONG s_quoteSlotIndex = 0;
static volatile LONG s_crafterQuoteScratchIndex = 0;
static volatile LONG s_crafterTransactScratchIndex = 0;
static uintptr_t s_crafterQuoteScratch[kCrafterScratchSlots] = {};
static uintptr_t s_crafterTransactScratch[kCrafterScratchSlots] = {};

static GWA3::HookEntry s_merchantTransactUiEntry{nullptr};
static bool s_merchantTransactCallbackRegistered = false;

static constexpr uint32_t kUiSendMerchantRequestQuote = 0x30000006u;
static constexpr uint32_t kUiSendMerchantTransactItem = 0x30000007u;

struct CrafterQuoteTask {
    uint32_t item_id;
};

struct TraderQuoteTask {
    uint32_t item_id;
};

struct TraderTransactTask {
    uint32_t item_id;
    uint32_t cost;
};

struct TraderSellTask {
    uint32_t item_id;
    uint32_t value;
};

struct CrafterTransactionTask {
    uint32_t quantity;
    uint32_t item_id;
    uint32_t total_value;
    uint32_t material_count;
    uint32_t material_item_ids[kCrafterMaxMaterials];
    uint32_t material_quantities[kCrafterMaxMaterials];
};

struct MerchantTransactionInfo {
    uint32_t item_count;
    uint32_t* item_ids;
    uint32_t* item_quantities;
};

struct MerchantQuoteInfo {
    uint32_t unknown;
    uint32_t item_count;
    uint32_t* item_ids;
};

struct MerchantRequestQuoteMessage {
    uint32_t type;
    uint32_t unknown;
    MerchantQuoteInfo give;
    MerchantQuoteInfo recv;
};

struct MerchantTransactItemMessage {
    uint32_t type;
    uint32_t gold_give;
    MerchantTransactionInfo give;
    uint32_t gold_recv;
    MerchantTransactionInfo recv;
};

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

static bool ReadU32(uintptr_t address, uint32_t& value) {
    if (address <= 0x10000) return false;
    __try {
        value = *reinterpret_cast<uint32_t*>(address);
        return true;
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
    uint32_t merchantSize = 0;
    if (!ReadPtr(p2 + 0x24, merchantBase)) return false;
    if (!ReadU32(p2 + 0x28, merchantSize)) return false;

    base = merchantBase;
    size = merchantSize;
    return base > 0x10000 && size > 0 && size < 4096;
}

static bool GetGlobalItemArrayBaseAndSize(uintptr_t& base, uint32_t& size) {
    base = 0;
    size = 0;

    if (Offsets::BasePointer <= 0x10000) return false;

    uintptr_t p0 = 0;
    uintptr_t p1 = 0;
    uintptr_t p2 = 0;
    if (!ReadPtr(Offsets::BasePointer, p0)) return false;
    if (!ReadPtr(p0 + 0x18, p1)) return false;
    if (!ReadPtr(p1 + 0x40, p2)) return false;

    uintptr_t itemBase = 0;
    uint32_t itemSize = 0;
    if (!ReadPtr(p2 + 0xB8, itemBase)) return false;
    if (!ReadU32(p2 + 0xC0, itemSize)) return false;

    base = itemBase;
    size = itemSize;
    return base > 0x10000 && size > 0 && size < 8192;
}

static Item* FindInventoryItemByModelIdWithQuantity(uint32_t modelId, uint32_t minQuantity) {
    Inventory* inv = ItemMgr::GetInventory();
    if (!inv || modelId == 0 || minQuantity == 0) return nullptr;

    __try {
        for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
            Bag* bag = inv->bags[bagIdx];
            if (!bag || !bag->items.buffer) continue;
            for (uint32_t i = 0; i < bag->items.size; ++i) {
                Item* item = bag->items.buffer[i];
                if (!item || item->model_id != modelId) continue;
                const uint32_t available = item->quantity ? item->quantity : 1u;
                if (available >= minQuantity && item->item_id != 0) {
                    return item;
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }

    return nullptr;
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

static Item* ValidateMerchantItemPtr(Item* item, uint32_t expectedItemId) {
    if (!item) return nullptr;

    __try {
        // Virtual merchant entries are not bag-backed and should round-trip to the
        // same item id. Touch the core fields here so callers do not crash later
        // while logging or transacting against a stale pointer.
        const uint32_t actualItemId = item->item_id;
        const uint32_t modelId = item->model_id;
        const uint32_t quantity = item->quantity;
        const uint32_t value = item->value;
        const uintptr_t bagPtr = reinterpret_cast<uintptr_t>(item->bag);
        const uint32_t agentId = item->agent_id;
        (void)quantity;
        (void)value;

        if (actualItemId != expectedItemId) return nullptr;
        if (modelId == 0) return nullptr;
        if (bagPtr > 0 && bagPtr <= 0x10000) return nullptr;
        if (bagPtr != 0 && agentId != 0) return nullptr;
        return item;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

static bool EnsureRequestQuoteShellcode() {
    if (s_requestQuoteBase) return true;

    void* mem = VirtualAlloc(nullptr, kQuoteSlots * kQuoteSlotSize,
                             MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!mem) {
        Log::Error("MerchantMgr: VirtualAlloc failed for request-quote shellcode pool");
        return false;
    }

    s_requestQuoteBase = reinterpret_cast<uintptr_t>(mem);
    return true;
}

static Item* FindTraderVirtualItemByModelId(uint32_t modelId) {
    if (modelId == 0) return nullptr;

    uintptr_t itemBase = 0;
    uint32_t itemSize = 0;
    if (!GetGlobalItemArrayBaseAndSize(itemBase, itemSize)) return nullptr;

    __try {
        for (uint32_t id = 1; id < itemSize; ++id) {
            uintptr_t itemPtr = 0;
            if (!ReadPtr(itemBase + static_cast<uintptr_t>(id) * 4u, itemPtr)) continue;

            Item* item = reinterpret_cast<Item*>(itemPtr);
            if (!item) continue;
            if (item->bag != nullptr || item->agent_id != 0) continue;
            if (item->model_id == modelId && item->item_id != 0) {
                return item;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }

    return nullptr;
}

static uint32_t ResolveTraderMaterialItemId(uint32_t modelId, uint32_t pack, uint32_t packs) {
    uint32_t traderItemId = GetMerchantItemIdByModelId(modelId);
    if (traderItemId) {
        return traderItemId;
    }

    Item* virtualItem = FindTraderVirtualItemByModelId(modelId);
    traderItemId = virtualItem ? virtualItem->item_id : 0u;
    if (traderItemId) {
        Log::Info("MerchantMgr: BuyMaterials fallback-resolved virtual trader item model=%u item=%u pack=%u/%u",
                  modelId, traderItemId, pack, packs);
    }
    return traderItemId;
}

static void LogMerchantMaterialSnapshot(uint32_t targetModelId) {
    uintptr_t merchantBase = 0;
    uint32_t merchantSize = 0;
    if (!GetMerchantItemsBaseAndSize(merchantBase, merchantSize)) {
        Log::Warn("MerchantMgr: Merchant snapshot unavailable for target model=%u", targetModelId);
        return;
    }

    Log::Warn("MerchantMgr: Merchant snapshot targetModel=%u merchantItems=%u",
              targetModelId, merchantSize);
    __try {
        for (uint32_t i = 0; i < merchantSize && i < 24u; ++i) {
            const uint32_t itemId = *reinterpret_cast<uint32_t*>(merchantBase + i * 4);
            if (!itemId) continue;

            Item* item = ValidateMerchantItemPtr(GetMerchantItemPtrByItemId(itemId), itemId);
            if (!item) {
                Log::Warn("MerchantMgr:   merchant[%u] item=%u ptr=invalid", i, itemId);
                continue;
            }

            Log::Warn("MerchantMgr:   merchant[%u] item=%u model=%u qty=%u bag=%p agent=%u",
                      i,
                      item->item_id,
                      item->model_id,
                      item->quantity,
                      item->bag,
                      item->agent_id);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Warn("MerchantMgr: Merchant snapshot faulted for target model=%u", targetModelId);
    }
}

static void TransactionBuyNative(uint32_t quantity, uint32_t itemId, uint32_t totalValue) {
    if (!Offsets::Transaction || quantity == 0 || itemId == 0) return;
    uint32_t qty = quantity;
    uint32_t id = itemId;
    const uintptr_t fn = Offsets::Transaction;
    __asm {
        lea eax, qty
        push eax
        lea eax, id
        push eax
        push 1
        push 0
        push 0
        push 0
        push 0
        mov eax, totalValue
        push eax
        push 1
        mov eax, fn
        call eax
        add esp, 0x24
    }
}

static void TransactionSellNative(uint32_t quantity, uint32_t itemId, uint32_t totalValue) {
    if (!Offsets::Transaction || itemId == 0) return;
    uint32_t qty = quantity;
    uint32_t id = itemId;
    const uintptr_t fn = Offsets::Transaction;
    __asm {
        push 0
        push 0
        push 0
        mov eax, totalValue
        push eax
        cmp qty, 0
        jz sell_all
        lea eax, qty
        push eax
        jmp sell_qty_done
sell_all:
        push 0
sell_qty_done:
        lea eax, id
        push eax
        push 1
        push 0
        push 0x0B
        mov eax, fn
        call eax
        add esp, 0x24
    }
}

static void NextQuoteSlot(uintptr_t& scAddr, uintptr_t& itemAddr) {
    LONG idx = InterlockedIncrement(&s_quoteSlotIndex) % kQuoteSlots;
    scAddr = s_requestQuoteBase + idx * kQuoteSlotSize;
    itemAddr = scAddr + 48;
}

static uintptr_t AcquireScratchBlock(uintptr_t* slots, volatile LONG* index, size_t size) {
    if (!slots || !index || size == 0) return 0;
    const LONG slot = InterlockedIncrement(index) % kCrafterScratchSlots;
    uintptr_t& addr = slots[slot];
    if (addr) return addr;

    void* mem = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!mem) {
        Log::Warn("MerchantMgr: scratch allocation failed for %zu bytes", size);
        return 0;
    }
    addr = reinterpret_cast<uintptr_t>(mem);
    return addr;
}

static void __cdecl RequestCrafterQuoteDirectInvoker(void* storage) {
    auto* task = reinterpret_cast<CrafterQuoteTask*>(storage);
    if (!task || !Offsets::RequestQuote || task->item_id == 0) return;

    const uintptr_t scratch = AcquireScratchBlock(s_crafterQuoteScratch, &s_crafterQuoteScratchIndex, kCrafterQuoteScratchSize);
    if (scratch < 0x10000) return;

    uint32_t* itemIdPtr = reinterpret_cast<uint32_t*>(scratch);
    *itemIdPtr = task->item_id;
    const uintptr_t fn = Offsets::RequestQuote;
    __asm {
        mov eax, itemIdPtr
        push eax
        push 1
        push 0
        push 0
        push 0
        push 0
        push 0
        push 3
        xor ecx, ecx
        mov edx, 2
        mov eax, fn
        call eax
        add esp, 0x20
    }
}

static void __cdecl RequestCrafterQuoteInvoker(void* storage) {
    auto* task = reinterpret_cast<CrafterQuoteTask*>(storage);
    if (!task || task->item_id == 0) return;

    // Always use the direct function call for crafter quotes.
    // UIMessage kSendMerchantRequestQuote is a proven no-op in crafter context —
    // it queues successfully but produces no outbound CtoS packet and no server response.
    // The direct RequestQuoteFunction call matches GWA2 CommandRequestCraftQuote behavior.
    if (Offsets::RequestQuote > 0x10000) {
        Log::Info("MerchantMgr: RequestCrafterQuote using direct function call (item=%u)", task->item_id);
        RequestCrafterQuoteDirectInvoker(storage);
        return;
    }

    Log::Warn("MerchantMgr: RequestCrafterQuote has no viable path (RequestQuote offset unresolved)");
}

// UIMessage callback for kUiSendMerchantTransactItem (0x30000007).
// GWCA invented this UIMessage ID — the game has no native handler for it.
// We register our own handler that bridges to the native Transaction function,
// matching how GWCA/GWToolbox makes this work.
static void __cdecl OnMerchantTransactCall(MerchantTransactItemMessage* msg) {
    const uintptr_t fn = Offsets::Transaction;
    uint32_t txType = msg->type;
    uint32_t goldGive = msg->gold_give;
    uint32_t goldRecv = msg->gold_recv;
    uint32_t giveCount = msg->give.item_count;
    uint32_t* giveIds = msg->give.item_ids;
    uint32_t* giveQtys = msg->give.item_quantities;
    uint32_t recvCount = msg->recv.item_count;
    uint32_t* recvIds = msg->recv.item_ids;
    uint32_t* recvQtys = msg->recv.item_quantities;

    __asm {
        mov eax, recvQtys
        push eax
        mov eax, recvIds
        push eax
        push recvCount
        push goldRecv
        mov eax, giveQtys
        push eax
        mov eax, giveIds
        push eax
        push giveCount
        push goldGive
        push txType
        mov eax, fn
        call eax
        add esp, 0x24
    }
}

static void OnMerchantTransactUIMessage(HookStatus*, uint32_t msgId, void* wParam, void*) {
    if (!wParam || !Offsets::Transaction) return;
    auto* msg = reinterpret_cast<MerchantTransactItemMessage*>(wParam);
    Log::Info("MerchantMgr: OnMerchantTransact UIMsg=0x%08X type=%u goldGive=%u giveCount=%u recvCount=%u",
              msgId, msg->type, msg->gold_give, msg->give.item_count, msg->recv.item_count);
    OnMerchantTransactCall(msg);
    Log::Info("MerchantMgr: OnMerchantTransact Transaction returned");
}

static void EnsureMerchantTransactCallback() {
    if (s_merchantTransactCallbackRegistered) return;
    CallbackRegistry::RegisterUIMessageCallback(
        &s_merchantTransactUiEntry, kUiSendMerchantTransactItem,
        OnMerchantTransactUIMessage, 0x1);
    s_merchantTransactCallbackRegistered = true;
    Log::Info("MerchantMgr: Registered UIMessage callback for kUiSendMerchantTransactItem (0x%08X)",
              kUiSendMerchantTransactItem);
}

static void __cdecl CraftMerchantItemDirectInvoker(void* storage) {
    auto* task = reinterpret_cast<CrafterTransactionTask*>(storage);
    if (!task || !Offsets::Transaction || task->quantity == 0 || task->item_id == 0 || task->material_count == 0) {
        return;
    }

    const uintptr_t scratch = AcquireScratchBlock(s_crafterTransactScratch, &s_crafterTransactScratchIndex, kCrafterTransactScratchSize);
    if (scratch < 0x10000) return;

    auto* scratchU32 = reinterpret_cast<uint32_t*>(scratch);
    uint32_t* materialItemIds = scratchU32;
    uint32_t* materialQuantities = materialItemIds + kCrafterMaxMaterials;
    uint32_t* recvItemIds = materialQuantities + kCrafterMaxMaterials;
    uint32_t* recvQuantities = recvItemIds + 1;

    memcpy(materialItemIds, task->material_item_ids, sizeof(task->material_item_ids));
    memcpy(materialQuantities, task->material_quantities, sizeof(task->material_quantities));
    recvItemIds[0] = task->item_id;
    recvQuantities[0] = task->quantity;

    uint32_t totalValue = task->total_value;
    uint32_t giveCount = task->material_count;
    const uintptr_t fn = Offsets::Transaction;
    __asm {
        mov eax, recvQuantities
        push eax
        mov eax, recvItemIds
        push eax
        push 1
        push 0
        mov eax, materialQuantities
        push eax
        mov eax, materialItemIds
        push eax
        mov eax, giveCount
        push eax
        mov eax, totalValue
        push eax
        push 3
        mov eax, fn
        call eax
        add esp, 0x24
    }
}

static void __cdecl CraftMerchantItemInvoker(void* storage) {
    auto* task = reinterpret_cast<CrafterTransactionTask*>(storage);
    if (!task || task->quantity == 0 || task->item_id == 0 || task->material_count == 0) {
        return;
    }

    const uintptr_t scratch = AcquireScratchBlock(s_crafterTransactScratch, &s_crafterTransactScratchIndex, kCrafterTransactScratchSize);
    if (scratch < 0x10000) return;

    auto* packet = reinterpret_cast<MerchantTransactItemMessage*>(scratch);
    auto* scratchU32 = reinterpret_cast<uint32_t*>(scratch + sizeof(*packet));
    uint32_t* materialItemIds = scratchU32;
    uint32_t* materialQuantities = materialItemIds + kCrafterMaxMaterials;
    uint32_t* recvItemIds = materialQuantities + kCrafterMaxMaterials;
    uint32_t* recvQuantities = recvItemIds + 1;

    memset(packet, 0, sizeof(*packet));
    memcpy(materialItemIds, task->material_item_ids, sizeof(task->material_item_ids));
    memcpy(materialQuantities, task->material_quantities, sizeof(task->material_quantities));
    recvItemIds[0] = task->item_id;
    recvQuantities[0] = task->quantity;

    packet->type = 3u;
    packet->gold_give = task->total_value;
    packet->gold_recv = 0u;
    packet->give.item_count = task->material_count;
    packet->give.item_ids = materialItemIds;
    packet->give.item_quantities = materialQuantities;
    packet->recv.item_count = 1u;
    packet->recv.item_ids = recvItemIds;
    packet->recv.item_quantities = recvQuantities;

    Log::Info("MerchantMgr: CraftMerchantItem via UIMessage (item=%u qty=%u gold=%u mats=%u matIds=[%u,%u] matQtys=[%u,%u] recvId=%u recvQty=%u UIMsg=0x%08X)",
              task->item_id, task->quantity, task->total_value, task->material_count,
              materialItemIds[0], materialItemIds[1], materialQuantities[0], materialQuantities[1],
              recvItemIds[0], recvQuantities[0], kUiSendMerchantTransactItem);

    if (Offsets::UIMessage > 0x10000) {
        UIMgr::SendUIMessageAsm(kUiSendMerchantTransactItem, packet, nullptr);
        Log::Info("MerchantMgr: CraftMerchantItem UIMessage dispatched");
        return;
    }

    // Fallback to direct function call if UIMessage not available
    Log::Info("MerchantMgr: CraftMerchantItem falling back to direct function call");
    CraftMerchantItemDirectInvoker(storage);
}

bool Initialize() {
    if (s_initialized) return true;
    EnsureMerchantTransactCallback();
    s_initialized = true;
    Log::Info("MerchantMgr: Initialized");
    return true;
}


static void __cdecl RequestTraderQuoteInvoker(void* storage) {
    auto* task = reinterpret_cast<TraderQuoteTask*>(storage);
    if (!task || !Offsets::RequestQuote || task->item_id == 0) return;

    using RequestQuoteNative = void (__cdecl*)(uint32_t, uint32_t, MerchantQuoteInfo, MerchantQuoteInfo);
    auto fn = reinterpret_cast<RequestQuoteNative>(Offsets::RequestQuote);

    uint32_t recvItemId = task->item_id;
    MerchantQuoteInfo give = {};
    MerchantQuoteInfo recv = {};
    recv.item_count = 1;
    recv.item_ids = &recvItemId;
    fn(0x0Cu, 0u, give, recv);
}

static void __cdecl RequestTraderSellQuoteInvoker(void* storage) {
    auto* task = reinterpret_cast<TraderQuoteTask*>(storage);
    if (!task || !Offsets::RequestQuote || task->item_id == 0) return;

    using RequestQuoteNative = void (__cdecl*)(uint32_t, uint32_t, MerchantQuoteInfo, MerchantQuoteInfo);
    auto fn = reinterpret_cast<RequestQuoteNative>(Offsets::RequestQuote);

    uint32_t giveItemId = task->item_id;
    MerchantQuoteInfo give = {};
    MerchantQuoteInfo recv = {};
    give.item_count = 1;
    give.item_ids = &giveItemId;
    fn(0x0Du, 0u, give, recv);
}

static void __cdecl TransactionTraderBuyInvoker(void* storage) {
    auto* task = reinterpret_cast<TraderTransactTask*>(storage);
    if (!task || !Offsets::Transaction || task->item_id == 0 || task->cost == 0) return;

    using TransactionNative = void (__cdecl*)(uint32_t, uint32_t, MerchantTransactionInfo, uint32_t, MerchantTransactionInfo);
    auto fn = reinterpret_cast<TransactionNative>(Offsets::Transaction);

    uint32_t recvItemId = task->item_id;
    uint32_t recvQty = 1;
    MerchantTransactionInfo give = {};
    MerchantTransactionInfo recv = {};
    recv.item_count = 1;
    recv.item_ids = &recvItemId;
    recv.item_quantities = &recvQty;
    fn(0x0Cu, task->cost, give, 0u, recv);
}

static void __cdecl TransactionTraderSellInvoker(void* storage) {
    auto* task = reinterpret_cast<TraderSellTask*>(storage);
    if (!task || !Offsets::Transaction || task->item_id == 0 || task->value == 0) return;

    using TransactionNative = void (__cdecl*)(uint32_t, uint32_t, MerchantTransactionInfo, uint32_t, MerchantTransactionInfo);
    auto fn = reinterpret_cast<TransactionNative>(Offsets::Transaction);

    uint32_t giveItemId = task->item_id;
    uint32_t giveQty = 1;
    MerchantTransactionInfo give = {};
    MerchantTransactionInfo recv = {};
    give.item_count = 1;
    give.item_ids = &giveItemId;
    give.item_quantities = &giveQty;
    fn(0x0Du, 0u, give, task->value, recv);
}


bool BuyMaterials(uint32_t modelId, uint32_t quantity) {
    if (modelId == 0 || quantity == 0) return false;
    if (!Offsets::RequestQuote || !Offsets::Transaction) return false;
    if (!TraderHook::IsInitialized() && !TraderHook::Initialize()) return false;
    if (!GameThread::IsInitialized() || GameThread::IsOnGameThread()) {
        Log::Warn("MerchantMgr: BuyMaterials requires caller off the game thread model=%u qty=%u gt_init=%u on_gt=%u",
                  modelId, quantity, GameThread::IsInitialized() ? 1u : 0u, GameThread::IsOnGameThread() ? 1u : 0u);
        return false;
    }

    const uint32_t merchantCount = GetMerchantItemCount();
    if (merchantCount == 0) {
        Log::Warn("MerchantMgr: BuyMaterials called with no merchant inventory open model=%u qty=%u", modelId, quantity);
        return false;
    }

    const uint32_t packs = (quantity + 9u) / 10u;
    uint32_t boughtPacks = 0;
    uint32_t traderItemId = 0;
    for (uint32_t pack = 0; pack < packs; ++pack) {
        if (!traderItemId) {
            traderItemId = ResolveTraderMaterialItemId(modelId, pack + 1u, packs);
        }
        if (!traderItemId) {
            Log::Warn("MerchantMgr: BuyMaterials could not resolve merchant item for model=%u pack=%u/%u",
                      modelId, pack + 1u, packs);
            if (pack == 0) {
                LogMerchantMaterialSnapshot(modelId);
            }
            break;
        }

        TraderHook::Reset();
        const TraderQuoteTask quoteTask{traderItemId};
        GameThread::EnqueueRaw(&RequestTraderQuoteInvoker, &quoteTask, sizeof(quoteTask));

        const DWORD quoteStart = GetTickCount();
        while ((GetTickCount() - quoteStart) < 5000u) {
            const uint32_t quotedCost = TraderHook::GetCostValue();
            if (quotedCost > 0 && quotedCost < 100000u) {
                break;
            }
            Sleep(25);
        }

        const uint32_t quotedCost = TraderHook::GetCostValue();
        const uint32_t quotedItemId = TraderHook::GetCostItemId();
        if (quotedCost == 0 || quotedCost >= 100000u || quotedItemId == 0) {
            traderItemId = 0;
            Log::Warn("MerchantMgr: BuyMaterials quote failed model=%u pack=%u/%u quotedItem=%u quotedCost=%u",
                      modelId, pack + 1u, packs, quotedItemId, quotedCost);
            break;
        }

        const uint32_t goldBefore = ItemMgr::GetGoldCharacter();
        const TraderTransactTask txTask{quotedItemId, quotedCost};
        GameThread::EnqueueRaw(&TransactionTraderBuyInvoker, &txTask, sizeof(txTask));

        const DWORD buyStart = GetTickCount();
        bool buyObserved = false;
        while ((GetTickCount() - buyStart) < 5000u) {
            if (ItemMgr::GetGoldCharacter() < goldBefore) {
                buyObserved = true;
                break;
            }
            Sleep(25);
        }
        if (!buyObserved) {
            traderItemId = 0;
            Log::Warn("MerchantMgr: BuyMaterials transact did not change gold model=%u pack=%u/%u item=%u cost=%u goldBefore=%u goldAfter=%u",
                      modelId, pack + 1u, packs, quotedItemId, quotedCost,
                      goldBefore, ItemMgr::GetGoldCharacter());
            break;
        }

        ++boughtPacks;
        Sleep(ChatMgr::GetPing() + 250);
    }

    Log::Info("MerchantMgr: BuyMaterials native flow model=%u requestedQty=%u requestedPacks=%u boughtPacks=%u",
              modelId, quantity, packs, boughtPacks);
    return boughtPacks > 0;
}

bool SellMaterialsToTrader(uint32_t itemId, uint32_t transactions) {
    if (itemId == 0 || transactions == 0) return false;
    if (!Offsets::RequestQuote || !Offsets::Transaction) return false;
    if (!TraderHook::IsInitialized() && !TraderHook::Initialize()) return false;
    if (!GameThread::IsInitialized() || GameThread::IsOnGameThread()) {
        Log::Warn("MerchantMgr: SellMaterialsToTrader requires caller off the game thread item=%u tx=%u gt_init=%u on_gt=%u",
                  itemId, transactions, GameThread::IsInitialized() ? 1u : 0u, GameThread::IsOnGameThread() ? 1u : 0u);
        return false;
    }
    if (GetMerchantItemCount() == 0) {
        Log::Warn("MerchantMgr: SellMaterialsToTrader called with no trader inventory open item=%u tx=%u",
                  itemId, transactions);
        return false;
    }

    Item* inventoryItem = ItemMgr::GetItemById(itemId);
    const uint32_t modelId = inventoryItem ? inventoryItem->model_id : 0u;
    if (!inventoryItem || modelId == 0) {
        Log::Warn("MerchantMgr: SellMaterialsToTrader could not resolve inventory item=%u model=%u",
                  itemId, modelId);
        return false;
    }

    uint32_t soldCount = 0;
    for (uint32_t tx = 0; tx < transactions; ++tx) {
        TraderHook::Reset();
        const uint32_t initialQuoteId = TraderHook::GetQuoteId();
        const TraderQuoteTask quoteTask{itemId};
        GameThread::EnqueueRaw(&RequestTraderSellQuoteInvoker, &quoteTask, sizeof(quoteTask));

        const DWORD quoteStart = GetTickCount();
        bool quoteObserved = false;
        while ((GetTickCount() - quoteStart) < 5000u) {
            if (TraderHook::GetQuoteId() != initialQuoteId) {
                quoteObserved = true;
                break;
            }
            Sleep(25);
        }

        const uint32_t quotedValue = TraderHook::GetCostValue();
        const uint32_t quotedItemId = TraderHook::GetCostItemId();
        if (!quoteObserved || quotedValue == 0 || quotedValue >= 100000u) {
            Log::Warn("MerchantMgr: SellMaterialsToTrader quote failed inventory=%u model=%u tx=%u/%u quoteObserved=%u quotedItem=%u quotedValue=%u",
                      itemId, modelId, tx + 1u, transactions, quoteObserved ? 1u : 0u, quotedItemId, quotedValue);
            LogMerchantMaterialSnapshot(modelId);
            break;
        }

        const uint32_t goldBefore = ItemMgr::GetGoldCharacter();
        const TraderSellTask sellTask{itemId, quotedValue};
        GameThread::EnqueueRaw(&TransactionTraderSellInvoker, &sellTask, sizeof(sellTask));

        const DWORD sellStart = GetTickCount();
        bool sellObserved = false;
        while ((GetTickCount() - sellStart) < 5000u) {
            if (ItemMgr::GetGoldCharacter() > goldBefore) {
                sellObserved = true;
                break;
            }
            Sleep(25);
        }
        if (!sellObserved) {
            Log::Warn("MerchantMgr: SellMaterialsToTrader transact did not change gold inventory=%u model=%u tx=%u/%u quotedItem=%u value=%u goldBefore=%u goldAfter=%u",
                      itemId, modelId, tx + 1u, transactions, quotedItemId, quotedValue,
                      goldBefore, ItemMgr::GetGoldCharacter());
            break;
        }

        ++soldCount;
        Sleep(ChatMgr::GetPing() + 250);
    }

    Log::Info("MerchantMgr: SellMaterialsToTrader native flow item=%u requestedTx=%u soldTx=%u",
              itemId, transactions, soldCount);
    return soldCount > 0;
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

Item* GetMerchantItemByPosition(uint32_t itemPosition) {
    uintptr_t merchantBase = 0;
    uint32_t merchantSize = 0;
    if (!GetMerchantItemsBaseAndSize(merchantBase, merchantSize)) return nullptr;
    if (itemPosition == 0 || itemPosition > merchantSize) return nullptr;

    uint32_t itemId = 0;
    if (!ReadU32(merchantBase + 4 * (itemPosition - 1), itemId)) return nullptr;
    return itemId ? ValidateMerchantItemPtr(GetMerchantItemPtrByItemId(itemId), itemId) : nullptr;
}

Item* GetMerchantItemByModelId(uint32_t modelId) {
    uintptr_t merchantBase = 0;
    uint32_t merchantSize = 0;
    if (!GetMerchantItemsBaseAndSize(merchantBase, merchantSize)) return nullptr;

    __try {
        for (uint32_t i = 0; i < merchantSize; ++i) {
            const uint32_t itemId = *reinterpret_cast<uint32_t*>(merchantBase + i * 4);
            if (!itemId) continue;

            Item* item = ValidateMerchantItemPtr(GetMerchantItemPtrByItemId(itemId), itemId);
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

bool BuyMerchantItemByPosition(uint32_t itemPosition, uint32_t quantity, uint32_t unitValue) {
    Item* item = GetMerchantItemByPosition(itemPosition);
    if (!item || quantity == 0 || unitValue == 0) return false;
    const uint32_t itemId = item->item_id;
    if (!itemId || !Offsets::Transaction) return false;

    const uint32_t totalValue = unitValue * quantity;
    GameThread::Enqueue([itemId, quantity, totalValue]() {
        TransactionBuyNative(quantity, itemId, totalValue);
    });
    return true;
}

bool BuyMerchantItem(uint32_t itemId, uint32_t quantity) {
    Item* item = GetMerchantItemPtrByItemId(itemId);
    if (!item || quantity == 0) return false;

    // Merchant item->value is the resale value. The native merchant buy path
    // expects the purchase total, which is 2x the resale value for the normal
    // merchant stock observed through the bridge.
    const uint32_t unitValue = item->value * 2;
    if (unitValue == 0 || !Offsets::Transaction) return false;

    const uint32_t totalValue = unitValue * quantity;
    GameThread::Enqueue([itemId, quantity, totalValue]() {
        TransactionBuyNative(quantity, itemId, totalValue);
    });
    return true;
}

bool SellInventoryItem(uint32_t itemId, uint32_t quantity) {
    Item* item = ItemMgr::GetItemById(itemId);
    if (!item || !Offsets::Transaction) return false;

    const uint32_t qty = quantity ? quantity : item->quantity;
    const uint32_t totalValue = item->value * qty;
    GameThread::Enqueue([itemId, qty, totalValue]() {
        TransactionSellNative(qty, itemId, totalValue);
    });
    return true;
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

bool RequestCrafterQuoteByItemId(uint32_t itemId) {
    if (!Offsets::RequestQuote || itemId == 0 || !GameThread::IsInitialized()) return false;
    if (!TraderHook::IsInitialized() && !TraderHook::Initialize()) return false;

    TraderHook::Reset();
    const CrafterQuoteTask task{itemId};
    GameThread::EnqueueRaw(&RequestCrafterQuoteInvoker, &task, sizeof(task));
    return true;
}

bool RequestCrafterQuoteByModelId(uint32_t modelId) {
    const uint32_t itemId = GetMerchantItemIdByModelId(modelId);
    return itemId != 0 && RequestCrafterQuoteByItemId(itemId);
}

bool RequestCrafterQuoteByPosition(uint32_t itemPosition) {
    Item* item = GetMerchantItemByPosition(itemPosition);
    return item && item->item_id != 0 && RequestCrafterQuoteByItemId(item->item_id);
}

bool RequestCrafterQuoteByItemIdPacket(uint32_t itemId) {
    if (itemId == 0) return false;
    if (!TraderHook::IsInitialized() && !TraderHook::Initialize()) return false;
    TraderHook::Reset();
    CtoS::SendPacket(2, Packets::REQUEST_QUOTE, itemId);
    return true;
}

bool RequestCrafterQuoteByPositionPacket(uint32_t itemPosition) {
    Item* item = GetMerchantItemByPosition(itemPosition);
    return item && item->item_id != 0 && RequestCrafterQuoteByItemIdPacket(item->item_id);
}

bool CraftMerchantItem(uint32_t itemId, uint32_t quantity, uint32_t totalValue,
                       const uint32_t* materialModelIds, const uint32_t* materialQuantities,
                       uint32_t materialCount) {
    if (!Offsets::Transaction || !GameThread::IsInitialized() || itemId == 0 || quantity == 0 || totalValue == 0) {
        Log::Warn("MerchantMgr: CraftMerchantItem early reject Transaction=0x%08X GameThread=%u item=%u qty=%u gold=%u",
                  static_cast<unsigned>(Offsets::Transaction), GameThread::IsInitialized() ? 1u : 0u,
                  itemId, quantity, totalValue);
        return false;
    }
    if (!materialModelIds || !materialQuantities || materialCount == 0 || materialCount > kCrafterMaxMaterials) {
        Log::Warn("MerchantMgr: CraftMerchantItem material args invalid mats=%p qtys=%p count=%u",
                  materialModelIds, materialQuantities, materialCount);
        return false;
    }

    CrafterTransactionTask task{};
    task.quantity = quantity;
    // Direct TransactItem crafter probes succeeded with the merchant item id in
    // recv.item_ids, matching the receive-array shape used by trader/material flows.
    task.item_id = itemId;
    task.total_value = totalValue;
    task.material_count = materialCount;

    for (uint32_t i = 0; i < materialCount; ++i) {
        const uint32_t modelId = materialModelIds[i];
        const uint32_t needed = materialQuantities[i] * quantity;
        if (modelId == 0 || needed == 0) return false;

        Item* item = FindInventoryItemByModelIdWithQuantity(modelId, needed);
        if (!item) {
            Log::Warn("MerchantMgr: CraftMerchantItem missing material model=%u qty=%u", modelId, needed);
            return false;
        }
        task.material_item_ids[i] = item->item_id;
        task.material_quantities[i] = needed;
    }

    Log::Info("MerchantMgr: CraftMerchantItem dispatching via GameThread::EnqueueRaw (item=%u qty=%u gold=%u mats=%u matItemIds=[%u,%u])",
              task.item_id, task.quantity, task.total_value, task.material_count,
              task.material_item_ids[0], task.material_item_ids[1]);
    GameThread::EnqueueRaw(&CraftMerchantItemInvoker, &task, sizeof(task));
    return true;
}

bool CraftMerchantItemByModelId(uint32_t modelId, uint32_t quantity, uint32_t totalValue,
                                const uint32_t* materialModelIds, const uint32_t* materialQuantities,
                                uint32_t materialCount) {
    const uint32_t itemId = GetMerchantItemIdByModelId(modelId);
    return itemId != 0 && CraftMerchantItem(itemId, quantity, totalValue,
                                            materialModelIds, materialQuantities, materialCount);
}

bool CraftMerchantItemByPosition(uint32_t itemPosition, uint32_t quantity, uint32_t totalValue,
                                 const uint32_t* materialModelIds, const uint32_t* materialQuantities,
                                 uint32_t materialCount) {
    Item* item = GetMerchantItemByPosition(itemPosition);
    return item && item->item_id != 0 && CraftMerchantItem(item->item_id, quantity, totalValue,
                                                           materialModelIds, materialQuantities, materialCount);
}

bool CraftMerchantItemByItemIdPacket(uint32_t itemId, uint32_t quantity) {
    if (itemId == 0 || quantity == 0) return false;
    CtoS::SendPacket(4, Packets::TRANSACT_ITEMS, 3u, quantity, itemId);
    return true;
}

bool CraftMerchantItemByPositionPacket(uint32_t itemPosition, uint32_t quantity) {
    Item* item = GetMerchantItemByPosition(itemPosition);
    return item && item->item_id != 0 && CraftMerchantItemByItemIdPacket(item->item_id, quantity);
}

} // namespace GWA3::MerchantMgr
