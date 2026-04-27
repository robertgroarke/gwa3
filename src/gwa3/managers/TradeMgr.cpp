#include <gwa3/managers/TradeMgr.h>
#include <gwa3/core/CallbackRegistry.h>
#include <gwa3/core/Log.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/RenderHook.h>
#include <gwa3/core/Scanner.h>
#include <gwa3/core/TraderHook.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/game/Item.h>
#include <gwa3/game/Agent.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/ChatMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>

#include <MinHook.h>
#include <Windows.h>
#include <cstring>
#include <thread>

namespace GWA3::TradeMgr {

static uintptr_t GetTradeWindowFromGameContext(bool logDiagnostic = false);
static void OfferItemPromptQuantityAttempt(uint32_t itemId, uint32_t attemptsRemaining);
static void QueueDisableTradeCartHookOnce();
static void SubmitOfferImpl(uint32_t gold, uint32_t viewRetryCount);
static void AcceptTradeImpl(uint32_t viewRetryCount);

namespace {

void QueueDelayedOfferRetry(uint32_t itemId, uint32_t quantity, uint32_t attemptsRemaining) {
    if (attemptsRemaining == 0) {
        return;
    }
    std::thread([itemId, quantity, attemptsRemaining]() {
        Sleep(150);
        GameThread::Enqueue([itemId, quantity, attemptsRemaining]() {
            OfferItem(itemId, quantity, attemptsRemaining - 1);
        });
    }).detach();
}

void QueueDelayedPromptOpenRetry(uint32_t itemId, uint32_t attemptsRemaining) {
    if (attemptsRemaining == 0) {
        return;
    }
    std::thread([itemId, attemptsRemaining]() {
        Sleep(150);
        GameThread::EnqueuePost([itemId, attemptsRemaining]() {
            OfferItemPromptQuantityAttempt(itemId, attemptsRemaining - 1);
        });
    }).detach();
}

void QueueDelayedSubmitRetry(uint32_t gold, uint32_t viewRetryCount) {
    if (viewRetryCount == 0) {
        return;
    }
    std::thread([gold, viewRetryCount]() {
        Sleep(150);
        GameThread::Enqueue([gold, viewRetryCount]() {
            SubmitOfferImpl(gold, viewRetryCount - 1u);
        });
    }).detach();
}

void QueueDelayedAcceptRetry(uint32_t viewRetryCount) {
    if (viewRetryCount == 0) {
        return;
    }
    std::thread([viewRetryCount]() {
        Sleep(150);
        GameThread::Enqueue([viewRetryCount]() {
            AcceptTradeImpl(viewRetryCount - 1u);
        });
    }).detach();
}

}

struct QueuedFrameClick {
    uint32_t frame_id;
    DWORD added;
};

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
static constexpr uint32_t kTradeHookPatchSize = 5;
static uintptr_t s_tradeCartTrampoline = 0;
static uint8_t s_tradeCartSavedBytes[kTradeHookPatchSize] = {};
static bool s_tradeCartHookEnabled = false;
static volatile LONG s_tradeCartCaptureRequested = 0;
static volatile LONG s_tradeCartDisableQueued = 0;
static volatile LONG s_tradeWindowContext = 0;
static volatile LONG s_lastTradeCartEax = 0;
static volatile LONG s_tradeWindowFrame = 0;
static volatile LONG s_tradeWindowCaptureCount = 0;

// Deferred offer: set by OfferItem/OfferItemPromptQuantity, executed
// inside the next OnUpdateTradeCart callback where the game's UI context
// is valid.  The native OfferTradeItem crashes when called from our
// GameThread hooks but works from the UpdateTradeCart callback.
struct PendingOffer {
    volatile LONG armed;      // 1 = pending, 0 = idle
    volatile LONG itemId;
    volatile LONG quantity;   // 0 = prompt (popup), >0 = direct
};
static PendingOffer s_pendingOffer = {0, 0, 0};
static uint8_t s_tradeHackOriginalByte = 0;
static bool s_tradeHackOriginalByteKnown = false;
static bool s_tradeHackPatched = false;
static GWA3::HookEntry s_merchantTransactUiEntry{nullptr};
static bool s_merchantTransactCallbackRegistered = false;
static volatile LONG s_tradeUiPlayerUpdatedCount = 0;
static volatile LONG s_tradeUiInitiateCount = 0;
static volatile LONG s_tradeUiSessionStartCount = 0;
static volatile LONG s_tradeUiSessionUpdatedCount = 0;
static volatile LONG s_tradeUiLastInitiateWParam = 0;
static volatile LONG s_tradeUiLastSessionStartState = 0;
static volatile LONG s_tradeUiLastSessionStartPlayerNumber = 0;
static volatile LONG s_partyButtonCallbackHitCount = 0;
static volatile LONG s_partyButtonCallbackLastThis = 0;
static volatile LONG s_partyButtonCallbackLastArg = 0;
static uintptr_t s_chooseQuantityPopupHookAddr = 0;
static bool s_chooseQuantityPopupHookInstalled = false;
enum class ChooseQuantityPopupHookSeam : uint32_t {
    None = 0,
    InventorySlot = 1,
    MaxCountDeref = 2,
    MaxCountDirect = 3,
};
static ChooseQuantityPopupHookSeam s_chooseQuantityPopupHookSeam = ChooseQuantityPopupHookSeam::None;
static bool s_chooseQuantityPopupPassiveOnly = false;
static void* s_chooseQuantityPopupOriginalRaw = nullptr;
// The stronger maxCount seams are still under ABI investigation.
// Keep them passive until the entry register/stack shape is confirmed live.
static constexpr bool kPassiveOnlyChooseQuantityStrongSeams = true;
static volatile LONG s_chooseQuantityPassiveEntryCount = 0;
static volatile LONG s_chooseQuantityPassiveLastEax = 0;
static volatile LONG s_chooseQuantityPassiveLastEcx = 0;
static volatile LONG s_chooseQuantityPassiveLastEdx = 0;
static volatile LONG s_chooseQuantityPassiveLastEbx = 0;
static volatile LONG s_chooseQuantityPassiveLastEsi = 0;
static volatile LONG s_chooseQuantityPassiveLastEdi = 0;
static volatile LONG s_chooseQuantityPassiveLastEbp = 0;
static volatile LONG s_chooseQuantityPassiveLastEsp = 0;
static volatile LONG s_chooseQuantityPassiveLastStack0 = 0;
static volatile LONG s_chooseQuantityPassiveLastStack4 = 0;
static volatile LONG s_chooseQuantityPassiveLastStack8 = 0;
static volatile LONG s_chooseQuantityPassiveLastStack12 = 0;
enum class QuantityPromptAutomationMode : uint32_t {
    None = 0,
    DefaultOffer = 1,
    MaxOffer = 2,
    ValueOffer = 3
};
static volatile LONG s_pendingQuantityPromptMode = static_cast<LONG>(QuantityPromptAutomationMode::None);
static volatile LONG s_pendingQuantityPromptValue = 0;
static volatile LONG s_pendingQuantityPromptItemId = 0;
static QueuedFrameClick s_pendingQuantityPromptClicks[2] = {};
static volatile LONG s_pendingQuantityPromptClickCount = 0;
// Temporary isolation switch for player-trade crash debugging.
// Merchant/native transaction coverage does not need the trade-cart hook.
static constexpr bool kDisableTradeCartHookForDebug = true;
static constexpr uint32_t kUiInitiateTrade = UIMgr::MSG_INITIATE_TRADE;
static constexpr uint32_t kUiSendMerchantRequestQuote = 0x30000006u;
static constexpr uint32_t kUiSendMerchantTransactItem = 0x30000007u;
static constexpr uint32_t kTradeButtonFrameHash = 3422277079u;
static constexpr uint32_t kTradeButtonActionPrimaryChildOffsetId = 123u;
static constexpr uint32_t kTradeButtonActionSecondaryChildOffsetId = 122u;
static constexpr uint32_t kTradeButtonAltHash = 1687064728u;
static constexpr uint32_t kTradeButtonAltChildOffsetId = 126u;
static constexpr uint32_t kTradeWindowFrameHash = 3198579276u;
static constexpr uint32_t kTradeWindowSubmitButtonHash = 3026060733u;
static constexpr uint32_t kTradeWindowCancelButtonHash = 784833442u;
static constexpr uint32_t kTradeWindowAcceptButtonHash = 4162812990u;
static constexpr uint32_t kTradeWindowViewButtonHash = 3032516301u;
static constexpr uint32_t kTradeQuantityPromptChildOffsetId = 2u;
static constexpr uint32_t kTradeQuantityPromptValueChildOffsetId = 1u;
static constexpr uint32_t kTradeQuantityPromptAltValueChildOffsetId = 2u;
static constexpr uint32_t kTradeQuantityPromptOkChildOffsetId = 3u;
static constexpr uint32_t kTradeQuantityPromptMaxChildOffsetId = 4u;
static constexpr uint32_t kUiInteractionInitFrame = 0x9u;

struct TradeSessionStartPayload {
    uint32_t trade_state;
    uint32_t player_number;
};

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

struct TradeWindowView {
    uint32_t unk1;
    uint32_t items_max;
    uint32_t frame_id;
    uint32_t state;
    uint32_t items_count;
};

template <typename T>
struct TradeArrayView {
    T* buffer;
    uint32_t capacity;
    uint32_t size;
    uint32_t param;
};

struct TradeContextItemView {
    uint32_t item_id;
    uint32_t quantity;
};

struct TradeContextTraderView {
    uint32_t gold;
    TradeArrayView<TradeContextItemView> items;
};

struct TradeContextView {
    uint32_t flags;
    uint32_t h0004[3];
    TradeContextTraderView player;
    TradeContextTraderView partner;
};

struct ResolvedTradeWindowView {
    uintptr_t ctx = 0;
    uintptr_t sourceValue = 0;
    uintptr_t framePtr = 0;
    const char* sourceLabel = "";
    uint32_t sourceOffset = 0;
    uint32_t derefCount = 0;
    uint32_t state = 0;
    uint32_t items_count = 0;
    uint32_t items_max = 0;
    uint32_t frame_id = 0;
    uint32_t score = 0;
};

struct UiInteractionMessageView {
    uint32_t frame_id;
    uint32_t message_id;
    void** wparam;
};

using OfferTradeItemNative = void(__fastcall*)(void* ecx, void* edx, uint32_t item_id, uint32_t quantity, uint32_t always_one);
using UpdateTradeCartNative = void(__cdecl*)(void* eaxValue, void* a1, void* a2);
using TradeDoActionNative = bool(__cdecl*)(uint32_t identifier);
using TradeVoidNative = bool(__cdecl*)();
using ChooseQuantityPopupNative = void(__cdecl*)(void* a1, void* a2, void* a3);
static UpdateTradeCartNative s_updateTradeCartOriginal = nullptr;

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

static uintptr_t ResolvePromptNestedChildByOffset(uintptr_t frame, uint32_t parentOffsetId, uint32_t childOffsetId) {
    const uintptr_t parent = UIMgr::GetChildFrameByOffset(frame, parentOffsetId);
    if (parent < 0x10000) return 0;
    return UIMgr::GetChildFrameByOffset(parent, childOffsetId);
}

static uintptr_t ChoosePromptClickFrame(const uintptr_t* candidates, size_t count);

struct PromptValueAttempt {
    uintptr_t valueFrame = 0;
    uintptr_t commitParentFrame = 0;
    uintptr_t focusFrame = 0;
    const char* mode = "";
    const char* label = "";
};

struct QuantityPromptChildSignature {
    uint32_t childOffset = 0;
    uint32_t hash = 0;
    uint32_t state = 0;
};

static constexpr QuantityPromptChildSignature kTradeQuantityPromptSixChildSignature[] = {
    {0u, 224642225u, 0x4104u},
    {1u, 1559459829u, 0x4125u},
    {2u, 384121219u, 0x4104u},
    {3u, 4014954629u, 0x4104u},
    {4u, 4008686776u, 0x4104u},
    {5u, 784833442u, 0x4104u},
};

static constexpr QuantityPromptChildSignature kTradeQuantityPromptFiveChildSignature[] = {
    {0u, 0u, 0x4B16u},
    {1u, 0u, 0x4906u},
    {2u, 0u, 0x4906u},
    {3u, 0u, 0x4906u},
    {4u, 0u, 0x4906u},
};

static bool MatchesTradeQuantityPromptSignature(
    uintptr_t frame,
    const QuantityPromptChildSignature* signature,
    size_t count) {
    if (frame < 0x10000 || !signature || count == 0) return false;
    if (UIMgr::GetChildFrameCount(frame) != count) return false;

    for (size_t i = 0; i < count; ++i) {
        const auto& expected = signature[i];
        const uintptr_t child = UIMgr::GetChildFrameByOffset(frame, expected.childOffset);
        if (child < 0x10000) return false;
        if (UIMgr::GetChildOffsetId(child) != expected.childOffset) return false;
        if (UIMgr::GetFrameHash(child) != expected.hash) return false;
        if (UIMgr::GetFrameState(child) != expected.state) return false;
        if (UIMgr::GetChildFrameCount(child) != 0u) return false;
        if (UIMgr::GetFrameContext(child) != frame) return false;
    }

    return true;
}

static bool IsLikelyTradeQuantityPromptFrame(uintptr_t frame) {
    if (frame < 0x10000) return false;

    const uint32_t childCount = UIMgr::GetChildFrameCount(frame);
    if (childCount == _countof(kTradeQuantityPromptSixChildSignature)) {
        return MatchesTradeQuantityPromptSignature(
            frame,
            kTradeQuantityPromptSixChildSignature,
            _countof(kTradeQuantityPromptSixChildSignature));
    }
    if (childCount == _countof(kTradeQuantityPromptFiveChildSignature)) {
        return MatchesTradeQuantityPromptSignature(
            frame,
            kTradeQuantityPromptFiveChildSignature,
            _countof(kTradeQuantityPromptFiveChildSignature));
    }

    return false;
}

static size_t CollectPromptOkButtons(uintptr_t frame, uintptr_t* out, size_t capacity) {
    if (!out || capacity == 0 || frame < 0x10000) return 0;

    const uintptr_t nestedBarCandidates[] = {
        ResolvePromptNestedChildByOffset(frame, 6u, 2u),
        ResolvePromptNestedChildByOffset(frame, 6u, 1u),
        ResolvePromptNestedChildByOffset(frame, 6u, 0u),
        UIMgr::GetChildFrameByOffset(frame, kTradeQuantityPromptOkChildOffsetId),
    };
    size_t count = 0;
    for (size_t i = 0; i < _countof(nestedBarCandidates) && count < capacity; ++i) {
        const uintptr_t candidate = ChoosePromptClickFrame(&nestedBarCandidates[i], 1);
        if (candidate < 0x10000) continue;
        bool duplicate = false;
        for (size_t j = 0; j < count; ++j) {
            if (out[j] == candidate) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            out[count++] = candidate;
        }
    }
    return count;
}

static size_t CollectPromptValueAttempts(uintptr_t frame, PromptValueAttempt* out, size_t capacity) {
    if (!out || capacity == 0 || frame < 0x10000) return 0;

    size_t count = 0;
    const uintptr_t quantityGroup = UIMgr::GetChildFrameByOffset(frame, kTradeQuantityPromptMaxChildOffsetId);

    const auto addAttempt = [&](uintptr_t valueFrame, uintptr_t commitParentFrame, uintptr_t focusFrame, const char* mode, const char* label) {
        if (count >= capacity || valueFrame < 0x10000 || commitParentFrame < 0x10000) {
            return;
        }
        out[count++] = { valueFrame, commitParentFrame, focusFrame, mode, label };
    };

    if (quantityGroup > 0x10000) {
        const uintptr_t nestedValue = UIMgr::GetChildFrameByOffset(quantityGroup, 1u);
        addAttempt(nestedValue, quantityGroup, nestedValue, "editable", "nested[4][1]");
        addAttempt(nestedValue, quantityGroup, nestedValue, "numeric", "nested[4][1]");
    }

    const uintptr_t topLevelValue1 = UIMgr::GetChildFrameByOffset(frame, kTradeQuantityPromptValueChildOffsetId);
    if (topLevelValue1 > 0x10000) {
        addAttempt(topLevelValue1, frame, topLevelValue1, "editable_local", "root[1]/local");
        addAttempt(topLevelValue1, frame, topLevelValue1, "numeric_local", "root[1]/local");
        addAttempt(topLevelValue1, frame, topLevelValue1, "editable", "root[1]/commit=root");
        addAttempt(topLevelValue1, frame, topLevelValue1, "numeric", "root[1]/commit=root");
        addAttempt(topLevelValue1, topLevelValue1, topLevelValue1, "editable", "root[1]/commit=self");
        addAttempt(topLevelValue1, topLevelValue1, topLevelValue1, "numeric", "root[1]/commit=self");
    }

    return count;
}

static uintptr_t ChoosePromptClickFrame(const uintptr_t* candidates, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        const uintptr_t candidate = candidates[i];
        if (candidate < 0x10000) continue;
        const uint32_t state = UIMgr::GetFrameState(candidate);
        if (!(state & UIMgr::FRAME_CREATED) || (state & UIMgr::FRAME_HIDDEN) || (state & UIMgr::FRAME_DISABLED)) {
            continue;
        }
        return candidate;
    }
    return 0;
}

static void DebugDumpTradeQuantityPromptTree(uintptr_t frame, const char* label) {
    if (frame < 0x10000) return;
    Log::Info("TradeMgr: Quantity prompt tree label=%s root=0x%08X frameId=%u childCount=%u context=0x%08X",
              label ? label : "",
              static_cast<unsigned>(frame),
              UIMgr::GetFrameId(frame),
              UIMgr::GetChildFrameCount(frame),
              static_cast<unsigned>(UIMgr::GetFrameContext(frame)));
    UIMgr::DebugDumpChildFrames(frame, label ? label : "trade-quantity-root", 12);
    for (uint32_t offset = 0; offset <= 4; ++offset) {
        const uintptr_t child = UIMgr::GetChildFrameByOffset(frame, offset);
        if (child < 0x10000) continue;
        uint32_t fieldBC = 0;
        uint32_t fieldC0 = 0;
        uint32_t field1C4 = 0;
        __try {
            fieldBC = *reinterpret_cast<uint32_t*>(child + 0xBC);
            fieldC0 = *reinterpret_cast<uint32_t*>(child + 0xC0);
            field1C4 = *reinterpret_cast<uint32_t*>(child + 0x1C4);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            fieldBC = 0;
            fieldC0 = 0;
            field1C4 = 0;
        }
        Log::Info("TradeMgr: Quantity prompt child offset=%u frame=0x%08X frameId=%u fieldBC=%u fieldC0=%u field1C4=0x%X",
                  offset,
                  static_cast<unsigned>(child),
                  UIMgr::GetFrameId(child),
                  fieldBC,
                  fieldC0,
                  field1C4);
        char childLabel[64] = {};
        sprintf_s(childLabel, "%s[%u]", label ? label : "trade-quantity-root", offset);
        UIMgr::DebugDumpChildFrames(child, childLabel, 12);
    }
}

static void DebugDumpTradeQuantityPromptCallbacks(uintptr_t frame, const char* label) {
    if (frame < 0x10000) {
        return;
    }

    __try {
        const uintptr_t cbArrayData = *reinterpret_cast<uintptr_t*>(frame + 0xA8);
        const uint32_t cbArraySize = *reinterpret_cast<uint32_t*>(frame + 0xAC);
        Log::Info("TradeMgr: Quantity prompt callbacks label=%s frame=0x%08X frameId=%u cbArray=0x%08X count=%u hookAddr=0x%08X",
                  label ? label : "",
                  static_cast<unsigned>(frame),
                  UIMgr::GetFrameId(frame),
                  static_cast<unsigned>(cbArrayData),
                  cbArraySize,
                  static_cast<unsigned>(s_chooseQuantityPopupHookAddr));
        if (cbArrayData < 0x10000 || cbArraySize == 0 || cbArraySize > 16u) {
            return;
        }

        for (uint32_t ci = 0; ci < cbArraySize && ci < 8u; ++ci) {
            const uintptr_t entryBase = cbArrayData + ci * 12u;
            const uintptr_t callbackFn = *reinterpret_cast<uintptr_t*>(entryBase);
            const uintptr_t uictlCtx = *reinterpret_cast<uintptr_t*>(entryBase + 4u);
            const uint32_t entryArg = *reinterpret_cast<uint32_t*>(entryBase + 8u);
            uint32_t currentCount = 0;
            uint32_t maxCount = 0;

            if (uictlCtx > 0x10000) {
                __try {
                    currentCount = *reinterpret_cast<uint32_t*>(uictlCtx + 0x04);
                    maxCount = *reinterpret_cast<uint32_t*>(uictlCtx + 0x08);
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    currentCount = 0;
                    maxCount = 0;
                }
            }

            Log::Info("TradeMgr: Quantity prompt callback label=%s idx=%u fn=0x%08X ctx=0x%08X arg=0x%08X current=%u max=%u match=%u",
                      label ? label : "",
                      ci,
                      static_cast<unsigned>(callbackFn),
                      static_cast<unsigned>(uictlCtx),
                      entryArg,
                      currentCount,
                      maxCount,
                      callbackFn == s_chooseQuantityPopupHookAddr ? 1u : 0u);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Warn("TradeMgr: Quantity prompt callback dump faulted label=%s frame=0x%08X",
                  label ? label : "",
                  static_cast<unsigned>(frame));
    }
}

static bool TryWriteTradeQuantityPromptBackingCount(uintptr_t frame,
                                                    uint32_t desiredQty,
                                                    const char* label,
                                                    uint32_t* outAppliedQty = nullptr) {
    if (outAppliedQty) {
        *outAppliedQty = 0;
    }
    if (frame < 0x10000 || desiredQty == 0 || s_chooseQuantityPopupHookAddr <= 0x10000) {
        return false;
    }

    __try {
        const uintptr_t cbArrayData = *reinterpret_cast<uintptr_t*>(frame + 0xA8);
        const uint32_t cbArraySize = *reinterpret_cast<uint32_t*>(frame + 0xAC);
        if (cbArrayData < 0x10000 || cbArraySize == 0 || cbArraySize > 16u) {
            Log::Warn("TradeMgr: TryWriteTradeQuantityPromptBackingCount invalid callback array label=%s frame=0x%08X cbArray=0x%08X count=%u",
                      label ? label : "",
                      static_cast<unsigned>(frame),
                      static_cast<unsigned>(cbArrayData),
                      cbArraySize);
            return false;
        }

        for (uint32_t ci = 0; ci < cbArraySize && ci < 8u; ++ci) {
            const uintptr_t entryBase = cbArrayData + ci * 12u;
            const uintptr_t callbackFn = *reinterpret_cast<uintptr_t*>(entryBase);
            const uintptr_t uictlCtx = *reinterpret_cast<uintptr_t*>(entryBase + 4u);
            if (callbackFn != s_chooseQuantityPopupHookAddr || uictlCtx <= 0x10000) {
                continue;
            }

            const uint32_t currentCount = *reinterpret_cast<uint32_t*>(uictlCtx + 0x04);
            const uint32_t maxCount = *reinterpret_cast<uint32_t*>(uictlCtx + 0x08);
            const uint32_t appliedQty = (maxCount > 0 && desiredQty > maxCount) ? maxCount : desiredQty;
            *reinterpret_cast<uint32_t*>(uictlCtx + 0x04) = appliedQty;
            const uint32_t verifyCount = *reinterpret_cast<uint32_t*>(uictlCtx + 0x04);

            if (outAppliedQty) {
                *outAppliedQty = verifyCount;
            }
            Log::Info("TradeMgr: TryWriteTradeQuantityPromptBackingCount label=%s frame=0x%08X ctx=0x%08X idx=%u desired=%u current=%u max=%u applied=%u verified=%u",
                      label ? label : "",
                      static_cast<unsigned>(frame),
                      static_cast<unsigned>(uictlCtx),
                      ci,
                      desiredQty,
                      currentCount,
                      maxCount,
                      appliedQty,
                      verifyCount);
            return verifyCount == appliedQty;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Warn("TradeMgr: TryWriteTradeQuantityPromptBackingCount faulted label=%s frame=0x%08X desired=%u",
                  label ? label : "",
                  static_cast<unsigned>(frame),
                  desiredQty);
        return false;
    }

    Log::Warn("TradeMgr: TryWriteTradeQuantityPromptBackingCount found no matching callback label=%s frame=0x%08X desired=%u hookAddr=0x%08X",
              label ? label : "",
              static_cast<unsigned>(frame),
              desiredQty,
              static_cast<unsigned>(s_chooseQuantityPopupHookAddr));
    return false;
}

static bool EnterPromptQuantityByKeypress(uintptr_t valueFrame, uint32_t quantity) {
    if (valueFrame < 0x10000) return false;
    const bool focused = UIMgr::ButtonClick(valueFrame);
    Log::Info("TradeMgr: EnterPromptQuantityByKeypress focus frame=0x%08X frameId=%u focused=%u quantity=%u",
              static_cast<unsigned>(valueFrame),
              UIMgr::GetFrameId(valueFrame),
              focused ? 1u : 0u,
              quantity);
    Sleep(80);

    // Clear the visible default before typing the requested value.
    UIMgr::KeyPress(valueFrame, VK_BACK);
    Sleep(30);
    UIMgr::KeyPress(valueFrame, VK_BACK);
    Sleep(30);

    wchar_t quantityBuf[16] = {};
    _snwprintf_s(quantityBuf, _countof(quantityBuf), _TRUNCATE, L"%u", quantity);
    for (const wchar_t* p = quantityBuf; *p; ++p) {
        const wchar_t ch = *p;
        if (ch < L'0' || ch > L'9') continue;
        const uint32_t vk = static_cast<uint32_t>(L'0' + (ch - L'0'));
        if (!UIMgr::KeyPress(valueFrame, vk)) {
            Log::Warn("TradeMgr: EnterPromptQuantityByKeypress failed keypress frame=0x%08X vk=%u",
                      static_cast<unsigned>(valueFrame), vk);
            return false;
        }
        Sleep(30);
    }
    return true;
}

static bool TryFinalizeTradeQuantityPromptByEnter(uintptr_t promptFrame, uintptr_t valueFrame, const char* label) {
    const uintptr_t targets[] = {
        valueFrame,
        promptFrame,
    };
    const char* targetLabels[] = {
        "value",
        "prompt",
    };

    for (size_t i = 0; i < _countof(targets); ++i) {
        const uintptr_t target = targets[i];
        if (target < 0x10000) {
            continue;
        }
        const bool sent = UIMgr::KeyPress(target, VK_RETURN);
        Log::Info("TradeMgr: TryFinalizeTradeQuantityPromptByEnter label=%s target=%s frame=0x%08X frameId=%u sent=%u",
                  label ? label : "",
                  targetLabels[i],
                  static_cast<unsigned>(target),
                  UIMgr::GetFrameId(target),
                  sent ? 1u : 0u);
        if (!sent) {
            continue;
        }
        Sleep(200);
        if (!IsTradeQuantityPromptOpen()) {
            Log::Info("TradeMgr: TryFinalizeTradeQuantityPromptByEnter closed prompt label=%s target=%s",
                      label ? label : "",
                      targetLabels[i]);
            return true;
        }
    }

    return false;
}

static bool TryPromptMouseQuantityVariants(uintptr_t valueFrame, uint32_t quantity) {
    if (valueFrame < 0x10000) return false;
    struct MouseVariant {
        bool click_action;
        uint32_t state;
        uint32_t wparam;
        uint32_t lparam;
        const char* label;
    };
    const MouseVariant variants[] = {
        {false, 4u, 0u, 0u, "mouse_action_state4"},
        {false, 8u, 0u, 0u, "mouse_action_state8"},
        {true, 0u, 0u, 0u, "mouse_click_action_state0"},
        {true, 4u, 0u, 0u, "mouse_click_action_state4"},
        {true, 8u, 0u, 0u, "mouse_click_action_state8"},
    };
    for (const auto& variant : variants) {
        const bool ok = variant.click_action
            ? UIMgr::TestMouseClickAction(valueFrame, variant.state, variant.wparam, variant.lparam)
            : UIMgr::TestMouseAction(valueFrame, variant.state, variant.wparam, variant.lparam);
        Log::Info("TradeMgr: TryPromptMouseQuantityVariants frame=0x%08X frameId=%u quantity=%u label=%s ok=%u",
                  static_cast<unsigned>(valueFrame),
                  UIMgr::GetFrameId(valueFrame),
                  quantity,
                  variant.label,
                  ok ? 1u : 0u);
        Sleep(80);
    }
    return true;
}

static bool ClickTradeQuantityPromptButton(uintptr_t frame, const char* buttonLabel) {
    if (frame < 0x10000) {
        return false;
    }

    struct ClickVariant {
        const char* label;
        bool (*invoke)(uintptr_t);
    };
    const ClickVariant variants[] = {
        {"immediate_mouse_up", UIMgr::ButtonClickImmediate},
        {"mouse_up", UIMgr::ButtonClick},
        {"mouse_click", UIMgr::ButtonClickMouseClick},
        {"immediate_full", UIMgr::ButtonClickImmediateFull},
        {"full_mouse_click", UIMgr::ButtonClickFullMouseClick},
        {"full_mouse_up", UIMgr::ButtonClickFull},
    };

    const uint32_t frameId = UIMgr::GetFrameId(frame);
    const uint32_t childOffset = UIMgr::GetChildOffsetId(frame);
    uint32_t field1c4 = 0;
    __try {
        field1c4 = *reinterpret_cast<uint32_t*>(frame + 0x1C4);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        field1c4 = 0;
    }

    for (const auto& variant : variants) {
        const bool ok = variant.invoke(frame);
        Log::Info("TradeMgr: ClickTradeQuantityPromptButton button=%s variant=%s frame=0x%08X frameId=%u childOffset=%u field1c4=0x%X ok=%u",
                  buttonLabel ? buttonLabel : "",
                  variant.label,
                  static_cast<unsigned>(frame),
                  frameId,
                  childOffset,
                  field1c4,
                  ok ? 1u : 0u);
        if (!ok) {
            continue;
        }
        Sleep(120);
        return true;
    }

    return false;
}

static bool TryPromptSpinnerAdjust(uintptr_t frame, uint32_t quantity) {
    if (frame < 0x10000 || quantity <= 1u) return false;
    const uintptr_t spinnerFrame = UIMgr::GetChildFrameByOffset(frame, kTradeQuantityPromptAltValueChildOffsetId);
    if (spinnerFrame < 0x10000) return false;
    const uint32_t increments = quantity - 1u;
    Log::Info("TradeMgr: TryPromptSpinnerAdjust spinnerFrame=0x%08X frameId=%u increments=%u",
              static_cast<unsigned>(spinnerFrame),
              UIMgr::GetFrameId(spinnerFrame),
              increments);
    for (uint32_t i = 0; i < increments; ++i) {
        bool clicked = UIMgr::ButtonClickImmediateFull(spinnerFrame);
        if (!clicked) {
            clicked = UIMgr::ButtonClick(spinnerFrame);
        }
        Log::Info("TradeMgr: TryPromptSpinnerAdjust click=%u/%u frame=0x%08X clicked=%u",
                  i + 1u,
                  increments,
                  static_cast<unsigned>(spinnerFrame),
                  clicked ? 1u : 0u);
        if (!clicked) return false;
        Sleep(120);
    }
    return true;
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
        Log::Error("TradeMgr: VirtualAlloc failed for request-quote shellcode pool");
        return false;
    }

    s_requestQuoteBase = reinterpret_cast<uintptr_t>(mem);
    return true;
}

static bool WriteByte(uintptr_t address, uint8_t value) {
    if (address <= 0x10000) return false;

    DWORD oldProtect = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(address), 1, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }

    __try {
        *reinterpret_cast<uint8_t*>(address) = value;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        VirtualProtect(reinterpret_cast<void*>(address), 1, oldProtect, &oldProtect);
        return false;
    }

    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(address), 1);
    VirtualProtect(reinterpret_cast<void*>(address), 1, oldProtect, &oldProtect);
    return true;
}

static bool ToggleTradePatch(bool enable) {
    const uintptr_t patchAddr = Offsets::TradeHackPatch;
    if (patchAddr <= 0x10000) {
        Log::Warn("TradeMgr: TradeHackPatch offset not resolved");
        return false;
    }

    uint32_t current = 0;
    if (!ReadU32(patchAddr, current)) {
        Log::Warn("TradeMgr: Failed reading TradeHackPatch byte at 0x%08X", static_cast<unsigned>(patchAddr));
        return false;
    }

    if (!s_tradeHackOriginalByteKnown) {
        s_tradeHackOriginalByte = static_cast<uint8_t>(current & 0xFF);
        s_tradeHackOriginalByteKnown = true;
        Log::Info("TradeMgr: TradeHackPatch original byte=0x%02X at 0x%08X",
                  s_tradeHackOriginalByte,
                  static_cast<unsigned>(patchAddr));
    }

    const uint8_t target = enable ? 0xC3 : (s_tradeHackOriginalByteKnown ? s_tradeHackOriginalByte : 0x55);
    if (!WriteByte(patchAddr, target)) {
        Log::Warn("TradeMgr: Failed writing TradeHackPatch byte=0x%02X at 0x%08X",
                  target,
                  static_cast<unsigned>(patchAddr));
        return false;
    }

    Log::Info("TradeMgr: TradeHackPatch %s byte=0x%02X at 0x%08X",
              enable ? "enabled" : "restored",
              target,
              static_cast<unsigned>(patchAddr));
    s_tradeHackPatched = enable;
    return true;
}

static void CaptureTradeWindowContext(uintptr_t eaxValue) {
    if (eaxValue <= 0x10000) return;
    InterlockedExchange(&s_lastTradeCartEax, static_cast<LONG>(eaxValue));
    __try {
        // Dump the raw pointer chain at each level for diagnosis:
        // Level 0: eax (the raw callback parameter)
        // Level 1: *(eax + 8) — GWCA stops here (one deref)
        // Level 2: **(eax + 8) — our current code does this extra deref
        uintptr_t* base = reinterpret_cast<uintptr_t*>(eaxValue);
        const uintptr_t val_at_0 = base[0];
        const uintptr_t val_at_1 = base[1];
        const uintptr_t val_at_2 = base[2];  // = *(eax+8)

        // Our 3-deref: deref val_at_2 once more, then deref again
        uintptr_t deref1 = 0;
        uintptr_t deref2 = 0;
        TradeWindowView* deep_window = nullptr;
        if (val_at_2 > 0x10000) {
            deref1 = *reinterpret_cast<uintptr_t*>(val_at_2);
            if (deref1 > 0x10000) {
                deref2 = *reinterpret_cast<uintptr_t*>(deref1);
                if (deref2 > 0x10000) {
                    deep_window = reinterpret_cast<TradeWindowView*>(deref2);
                }
            }
        }

        const LONG captureCount = InterlockedIncrement(&s_tradeWindowCaptureCount);
        if (captureCount <= 20) {
            Log::Info("TradeMgr: CaptureContext[%ld] eax=0x%08X eax[0]=0x%08X eax[1]=0x%08X eax[2]=0x%08X",
                      captureCount,
                      static_cast<unsigned>(eaxValue),
                      static_cast<unsigned>(val_at_0),
                      static_cast<unsigned>(val_at_1),
                      static_cast<unsigned>(val_at_2));
            // GWCA actual path: *(*(eax+8)) — TWO dereferences, not "eax[2] as TradeWindow"
            if (deref1 > 0x10000) {
                auto* gwca_actual = reinterpret_cast<TradeWindowView*>(deref1);
                Log::Info("TradeMgr: CaptureContext[%ld] GWCA-actual (*(*(eax+8))=0x%08X): state=0x%X items_count=%u items_max=%u frame=0x%08X",
                          captureCount,
                          static_cast<unsigned>(deref1),
                          gwca_actual->state, gwca_actual->items_count, gwca_actual->items_max,
                          static_cast<unsigned>(gwca_actual->frame_id));
            }
            if (deep_window) {
                Log::Info("TradeMgr: CaptureContext[%ld] 3-deref (eax[2]→*→*): ptr=0x%08X state=0x%X items_count=%u items_max=%u frame=0x%08X",
                          captureCount,
                          static_cast<unsigned>(deref2),
                          deep_window->state, deep_window->items_count, deep_window->items_max,
                          static_cast<unsigned>(deep_window->frame_id));
            }
            Log::Info("TradeMgr: CaptureContext[%ld] deref chain: eax[2]=0x%08X → *=0x%08X → *=0x%08X",
                      captureCount,
                      static_cast<unsigned>(val_at_2),
                      static_cast<unsigned>(deref1),
                      static_cast<unsigned>(deref2));
        }

        // Use GWCA-correct 2-deref: *(*(eax+8)) = deref1.
        // Only store when state == 0 (settled) — during state=0x1000 (init),
        // the pointer may shift as the trade window struct relocates.
        if (deref1 > 0x10000) {
            auto* w = reinterpret_cast<TradeWindowView*>(deref1);
            if (w->items_max > 0 && w->items_max <= 64 && w->state == 0) {
                s_tradeWindowContext = static_cast<LONG>(deref1);
                s_tradeWindowFrame = static_cast<LONG>(w->frame_id);
                if (InterlockedExchange(&s_tradeCartCaptureRequested, 0) == 1) {
                    Log::Info("TradeMgr: CaptureTradeWindowContext settled ctx=0x%08X frame=0x%08X; scheduling hook disable",
                              static_cast<unsigned>(deref1),
                              static_cast<unsigned>(w->frame_id));
                    QueueDisableTradeCartHookOnce();
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        s_tradeWindowContext = 0;
        s_tradeWindowFrame = 0;
    }
}

static void __cdecl OnUpdateTradeCart(void* eaxValue, void* a1, void* a2) {
    CaptureTradeWindowContext(reinterpret_cast<uintptr_t>(eaxValue));

    // Call original FIRST so the game processes the trade cart update
    if (s_updateTradeCartOriginal) {
        s_updateTradeCartOriginal(eaxValue, a1, a2);
    }

    // Execute deferred offer if one is pending.
    // We're inside the game's UI dispatch context here, so
    // OfferTradeItem should work without crashing.
    if (InterlockedCompareExchange(&s_pendingOffer.armed, 0, 1) == 1) {
        const uint32_t itemId = static_cast<uint32_t>(s_pendingOffer.itemId);
        const uint32_t quantity = static_cast<uint32_t>(s_pendingOffer.quantity);

        // Derive context from the GWCA 2-deref path (live eax on stack)
        uintptr_t ctx = 0;
        __try {
            uintptr_t* base = reinterpret_cast<uintptr_t*>(eaxValue);
            uintptr_t val2 = base[2];
            if (val2 > 0x10000) {
                uintptr_t deref1 = *reinterpret_cast<uintptr_t*>(val2);
                if (deref1 > 0x10000) {
                    auto* w = reinterpret_cast<TradeWindowView*>(deref1);
                    if (w->items_max > 0 && w->items_max <= 64 && w->state == 0) {
                        ctx = deref1;
                    }
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}

        if (ctx > 0x10000 && Offsets::OfferTradeItem > 0x10000) {
            auto* window = reinterpret_cast<TradeWindowView*>(ctx);
            Log::Info("TradeMgr: DeferredOffer executing item=%u qty=%u ctx=0x%08X state=0x%X",
                      itemId, quantity, static_cast<unsigned>(ctx), window->state);
            __try {
                OfferTradeItemNative fn = reinterpret_cast<OfferTradeItemNative>(Offsets::OfferTradeItem);
                fn(window, nullptr, itemId, quantity, 1);
                Log::Info("TradeMgr: DeferredOffer succeeded item=%u qty=%u", itemId, quantity);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                Log::Error("TradeMgr: DeferredOffer faulted item=%u qty=%u ctx=0x%08X",
                           itemId, quantity, static_cast<unsigned>(ctx));
            }
        } else {
            Log::Warn("TradeMgr: DeferredOffer skipped — ctx=0x%08X fn=0x%08X (context not ready, will retry)",
                      static_cast<unsigned>(ctx), static_cast<unsigned>(Offsets::OfferTradeItem));
            // Re-arm so next callback retries
            InterlockedExchange(&s_pendingOffer.itemId, static_cast<LONG>(itemId));
            InterlockedExchange(&s_pendingOffer.quantity, static_cast<LONG>(quantity));
            InterlockedExchange(&s_pendingOffer.armed, 1);
        }
    }
}

static bool InstallTradeCartHook() {
    if (s_updateTradeCartOriginal) return true;
    if (Offsets::UpdateTradeCart <= 0x10000) {
        Log::Warn("TradeMgr: UpdateTradeCart offset not resolved; native trade item offering unavailable");
        return false;
    }

    MH_STATUS mhStatus = MH_Initialize();
    if (mhStatus != MH_OK && mhStatus != MH_ERROR_ALREADY_INITIALIZED) {
        Log::Error("TradeMgr: MH_Initialize failed for UpdateTradeCart hook: %s", MH_StatusToString(mhStatus));
        return false;
    }

    const uintptr_t hookAddr = Offsets::UpdateTradeCart;
    mhStatus = MH_CreateHook(
        reinterpret_cast<LPVOID>(hookAddr),
        reinterpret_cast<LPVOID>(&OnUpdateTradeCart),
        reinterpret_cast<LPVOID*>(&s_updateTradeCartOriginal));
    if (mhStatus != MH_OK && mhStatus != MH_ERROR_ALREADY_CREATED) {
        Log::Error("TradeMgr: MH_CreateHook failed for UpdateTradeCart: %s", MH_StatusToString(mhStatus));
        return false;
    }
    mhStatus = MH_EnableHook(reinterpret_cast<LPVOID>(hookAddr));
    if (mhStatus != MH_OK) {
        Log::Error("TradeMgr: MH_EnableHook failed for UpdateTradeCart: %s", MH_StatusToString(mhStatus));
        MH_RemoveHook(reinterpret_cast<LPVOID>(hookAddr));
        s_updateTradeCartOriginal = nullptr;
        return false;
    }

    Log::Info("TradeMgr: UpdateTradeCart hook installed at 0x%08X", static_cast<unsigned>(hookAddr));
    s_tradeCartHookEnabled = true;
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
        Log::Info("TradeMgr: BuyMaterials fallback-resolved virtual trader item model=%u item=%u pack=%u/%u",
                  modelId, traderItemId, pack, packs);
    }
    return traderItemId;
}

static void LogMerchantMaterialSnapshot(uint32_t targetModelId) {
    uintptr_t merchantBase = 0;
    uint32_t merchantSize = 0;
    if (!GetMerchantItemsBaseAndSize(merchantBase, merchantSize)) {
        Log::Warn("TradeMgr: Merchant snapshot unavailable for target model=%u", targetModelId);
        return;
    }

    Log::Warn("TradeMgr: Merchant snapshot targetModel=%u merchantItems=%u",
              targetModelId, merchantSize);
    __try {
        for (uint32_t i = 0; i < merchantSize && i < 24u; ++i) {
            const uint32_t itemId = *reinterpret_cast<uint32_t*>(merchantBase + i * 4);
            if (!itemId) continue;

            Item* item = ValidateMerchantItemPtr(GetMerchantItemPtrByItemId(itemId), itemId);
            if (!item) {
                Log::Warn("TradeMgr:   merchant[%u] item=%u ptr=invalid", i, itemId);
                continue;
            }

            Log::Warn("TradeMgr:   merchant[%u] item=%u model=%u qty=%u bag=%p agent=%u",
                      i,
                      item->item_id,
                      item->model_id,
                      item->quantity,
                      item->bag,
                      item->agent_id);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Warn("TradeMgr: Merchant snapshot faulted for target model=%u", targetModelId);
    }
}

static void QueueDisableTradeCartHookOnce() {
    if (InterlockedCompareExchange(&s_tradeCartDisableQueued, 1, 0) != 0) {
        return;
    }
    GameThread::EnqueuePost([]() {
        InterlockedExchange(&s_tradeCartDisableQueued, 0);
        if (!s_tradeCartHookEnabled || Offsets::UpdateTradeCart <= 0x10000) {
            return;
        }
        const MH_STATUS mhStatus = MH_DisableHook(reinterpret_cast<LPVOID>(Offsets::UpdateTradeCart));
        if (mhStatus == MH_OK || mhStatus == MH_ERROR_DISABLED) {
            s_tradeCartHookEnabled = false;
            Log::Info("TradeMgr: UpdateTradeCart hook auto-disabled");
            return;
        }
        Log::Warn("TradeMgr: MH_DisableHook failed for UpdateTradeCart auto-disable: %s",
                  MH_StatusToString(mhStatus));
    });
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
        Log::Warn("TradeMgr: scratch allocation failed for %zu bytes", size);
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
        Log::Info("TradeMgr: RequestCrafterQuote using direct function call (item=%u)", task->item_id);
        RequestCrafterQuoteDirectInvoker(storage);
        return;
    }

    Log::Warn("TradeMgr: RequestCrafterQuote has no viable path (RequestQuote offset unresolved)");
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
    Log::Info("TradeMgr: OnMerchantTransact UIMsg=0x%08X type=%u goldGive=%u giveCount=%u recvCount=%u",
              msgId, msg->type, msg->gold_give, msg->give.item_count, msg->recv.item_count);
    OnMerchantTransactCall(msg);
    Log::Info("TradeMgr: OnMerchantTransact Transaction returned");
}

static void EnsureMerchantTransactCallback() {
    if (s_merchantTransactCallbackRegistered) return;
    CallbackRegistry::RegisterUIMessageCallback(
        &s_merchantTransactUiEntry, kUiSendMerchantTransactItem,
        OnMerchantTransactUIMessage, 0x1);
    s_merchantTransactCallbackRegistered = true;
    Log::Info("TradeMgr: Registered UIMessage callback for kUiSendMerchantTransactItem (0x%08X)",
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

    Log::Info("TradeMgr: CraftMerchantItem via UIMessage (item=%u qty=%u gold=%u mats=%u matIds=[%u,%u] matQtys=[%u,%u] recvId=%u recvQty=%u UIMsg=0x%08X)",
              task->item_id, task->quantity, task->total_value, task->material_count,
              materialItemIds[0], materialItemIds[1], materialQuantities[0], materialQuantities[1],
              recvItemIds[0], recvQuantities[0], kUiSendMerchantTransactItem);

    if (Offsets::UIMessage > 0x10000) {
        UIMgr::SendUIMessageAsm(kUiSendMerchantTransactItem, packet, nullptr);
        Log::Info("TradeMgr: CraftMerchantItem UIMessage dispatched");
        return;
    }

    // Fallback to direct function call if UIMessage not available
    Log::Info("TradeMgr: CraftMerchantItem falling back to direct function call");
    CraftMerchantItemDirectInvoker(storage);
}

bool Initialize() {
    if (s_initialized) return true;
    if (!kDisableTradeCartHookForDebug) {
        InstallTradeCartHook();
    } else {
        Log::Warn("TradeMgr: UpdateTradeCart hook disabled for live player-trade debug");
    }
    Log::Warn("TradeMgr: Trade UI tap disabled for player-trade crash isolation");
    EnsureMerchantTransactCallback();
    Log::Warn("TradeMgr: PartyWindowButtonCallback hook disabled for player-trade crash isolation");
    s_initialized = true;
    Log::Info("TradeMgr: Initialized (OfferTradeItem=0x%08X UpdateTradeCart=0x%08X)",
              static_cast<unsigned>(Offsets::OfferTradeItem),
              static_cast<unsigned>(Offsets::UpdateTradeCart));
    if (Offsets::TradeHackPatch > 0x10000) {
        Log::Info("TradeMgr: TradeHackPatch=0x%08X", static_cast<unsigned>(Offsets::TradeHackPatch));
    }
    return true;
}

uint32_t GetTradeUiPlayerUpdatedCount() {
    return static_cast<uint32_t>(s_tradeUiPlayerUpdatedCount);
}

uint32_t GetTradeUiInitiateCount() {
    return static_cast<uint32_t>(s_tradeUiInitiateCount);
}

uint32_t GetTradeUiLastInitiateWParam() {
    return static_cast<uint32_t>(s_tradeUiLastInitiateWParam);
}

uint32_t GetPartyButtonCallbackHitCount() {
    return static_cast<uint32_t>(s_partyButtonCallbackHitCount);
}

uint32_t GetPartyButtonCallbackLastThis() {
    return static_cast<uint32_t>(s_partyButtonCallbackLastThis);
}

uint32_t GetPartyButtonCallbackLastArg() {
    return static_cast<uint32_t>(s_partyButtonCallbackLastArg);
}

uint32_t GetTradeUiSessionStartCount() {
    return static_cast<uint32_t>(s_tradeUiSessionStartCount);
}

uint32_t GetTradeUiSessionUpdatedCount() {
    return static_cast<uint32_t>(s_tradeUiSessionUpdatedCount);
}

uint32_t GetTradeUiLastSessionStartState() {
    return static_cast<uint32_t>(s_tradeUiLastSessionStartState);
}

uint32_t GetTradeUiLastSessionStartPlayerNumber() {
    return static_cast<uint32_t>(s_tradeUiLastSessionStartPlayerNumber);
}

uint32_t GetTradeWindowCaptureCount() {
    return static_cast<uint32_t>(s_tradeWindowCaptureCount);
}

uint32_t GetTradeWindowContext() {
    return static_cast<uint32_t>(s_tradeWindowContext);
}

uint32_t GetTradeWindowFrame() {
    return static_cast<uint32_t>(s_tradeWindowFrame);
}

uint32_t GetTradeWindowUiFrame() {
    return static_cast<uint32_t>(UIMgr::GetFrameByHash(kTradeWindowFrameHash));
}

uint32_t GetTradeWindowUiState() {
    const uintptr_t frame = UIMgr::GetFrameByHash(kTradeWindowFrameHash);
    return frame ? UIMgr::GetFrameState(frame) : 0u;
}

uint32_t GetTradeWindowUiContext() {
    const uintptr_t frame = UIMgr::GetFrameByHash(kTradeWindowFrameHash);
    return frame ? static_cast<uint32_t>(UIMgr::GetFrameContext(frame)) : 0u;
}

static uintptr_t FindTradeWindowRootChildByHash(uintptr_t uiFrame, uint32_t expectedHash) {
    if (uiFrame < 0x10000 || expectedHash == 0) return 0;

    const uint32_t childCount = UIMgr::GetChildFrameCount(uiFrame);
    for (uint32_t i = 0; i < childCount; ++i) {
        const uintptr_t child = UIMgr::GetChildFrameByIndex(uiFrame, i);
        if (child < 0x10000) continue;
        if (UIMgr::GetFrameHash(child) != expectedHash) continue;
        return child;
    }

    const uintptr_t byHash = UIMgr::GetFrameByHash(expectedHash);
    if (byHash > 0x10000) {
        const uintptr_t context = UIMgr::GetFrameContext(byHash);
        if (context == uiFrame) {
            return byHash;
        }
    }
    return 0;
}

static void LogTradeWindowRootChildren(uintptr_t uiFrame, const char* label) {
    if (uiFrame < 0x10000) {
        Log::Warn("TradeMgr: %s no trade window frame available", label ? label : "trade_window_root_children");
        return;
    }

    const uint32_t childCount = UIMgr::GetChildFrameCount(uiFrame);
    Log::Info("TradeMgr: %s uiFrame=0x%08X childCount=%u",
              label ? label : "trade_window_root_children",
              static_cast<unsigned>(uiFrame),
              childCount);
    for (uint32_t i = 0; i < childCount && i < 16u; ++i) {
        const uintptr_t child = UIMgr::GetChildFrameByIndex(uiFrame, i);
        if (child < 0x10000) continue;
        Log::Info("TradeMgr:   root[%u] frame=0x%08X hash=%u state=0x%X frameId=%u childOffset=%u childCount=%u",
                  i,
                  static_cast<unsigned>(child),
                  UIMgr::GetFrameHash(child),
                  UIMgr::GetFrameState(child),
                  UIMgr::GetFrameId(child),
                  UIMgr::GetChildOffsetId(child),
                  UIMgr::GetChildFrameCount(child));
    }
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

static bool ClickTradeWindowRootButton(uint32_t buttonHash, const char* buttonLabel) {
    const uintptr_t uiFrame = static_cast<uintptr_t>(GetTradeWindowUiFrame());
    const uintptr_t uiCtx = static_cast<uintptr_t>(GetTradeWindowUiContext());
    const uint32_t uiState = GetTradeWindowUiState();

    const uintptr_t button = FindTradeWindowRootChildByHash(uiFrame, buttonHash);
    Log::Info("TradeMgr: ClickTradeWindowRootButton label=%s hash=%u uiFrame=0x%08X uiState=0x%X uiCtx=0x%08X button=0x%08X",
              buttonLabel ? buttonLabel : "",
              buttonHash,
              static_cast<unsigned>(uiFrame),
              uiState,
              static_cast<unsigned>(uiCtx),
              static_cast<unsigned>(button));
    if (button < 0x10000) {
        LogTradeWindowRootChildren(uiFrame, buttonLabel ? buttonLabel : "trade_window_button_probe");
        return false;
    }

    struct ClickVariant {
        const char* label;
        bool (*invoke)(uintptr_t);
    };
    const ClickVariant variants[] = {
        {"immediate_mouse_up", UIMgr::ButtonClickImmediate},
        {"mouse_up", UIMgr::ButtonClick},
        {"mouse_click", UIMgr::ButtonClickMouseClick},
        {"immediate_full", UIMgr::ButtonClickImmediateFull},
        {"full_mouse_click", UIMgr::ButtonClickFullMouseClick},
        {"full_mouse_up", UIMgr::ButtonClickFull},
    };

    for (const auto& variant : variants) {
        const bool ok = variant.invoke(button);
        Log::Info("TradeMgr: ClickTradeWindowRootButton label=%s variant=%s frame=0x%08X frameId=%u childOffset=%u state=0x%X ok=%u",
                  buttonLabel ? buttonLabel : "",
                  variant.label,
                  static_cast<unsigned>(button),
                  UIMgr::GetFrameId(button),
                  UIMgr::GetChildOffsetId(button),
                  UIMgr::GetFrameState(button),
                  ok ? 1u : 0u);
        if (ok) {
            return true;
        }
    }

    LogTradeWindowRootChildren(uiFrame, buttonLabel ? buttonLabel : "trade_window_button_probe");
    return false;
}

static bool ClickTradeWindowViewIfNeeded(const char* actionLabel, uint32_t viewRetryCount) {
    const uintptr_t uiFrame = static_cast<uintptr_t>(GetTradeWindowUiFrame());
    if (uiFrame < 0x10000) {
        return false;
    }

    const uintptr_t viewButton = FindTradeWindowRootChildByHash(uiFrame, kTradeWindowViewButtonHash);
    if (viewButton < 0x10000) {
        return false;
    }

    Log::Info("TradeMgr: %s detected incoming trade view window uiFrame=0x%08X childCount=%u retries=%u",
              actionLabel ? actionLabel : "",
              static_cast<unsigned>(uiFrame),
              UIMgr::GetChildFrameCount(uiFrame),
              viewRetryCount);
    LogTradeWindowRootChildren(uiFrame, actionLabel ? actionLabel : "trade_view_prompt");
    return ClickTradeWindowRootButton(kTradeWindowViewButtonHash, actionLabel ? actionLabel : "trade_view");
}

static uintptr_t FindTradeQuantityPromptFrame() {
    const uintptr_t tradeWindowFrame = GetTradeWindowUiFrame();
    uintptr_t excludedFrame = tradeWindowFrame;

    for (;;) {
        const uintptr_t frame = UIMgr::GetVisibleFrameByChildOffsetAndChildCount(
            kTradeQuantityPromptChildOffsetId, 5, 5, excludedFrame, 0);
        if (frame < 0x10000) {
            break;
        }
        if (IsLikelyTradeQuantityPromptFrame(frame)) {
            return frame;
        }
        excludedFrame = frame;
    }

    excludedFrame = tradeWindowFrame;
    for (;;) {
        const uintptr_t frame = UIMgr::GetVisibleFrameByChildOffsetAndChildCount(
            kTradeQuantityPromptChildOffsetId, 5, 8, excludedFrame, 0);
        if (frame < 0x10000) {
            break;
        }
        if (IsLikelyTradeQuantityPromptFrame(frame)) {
            return frame;
        }
        excludedFrame = frame;
    }

    return 0;
}

static uintptr_t WaitForTradeQuantityPromptFrame(uint32_t timeoutMs = 1500, uint32_t pollMs = 50) {
    const DWORD start = GetTickCount();
    while (GetTickCount() - start < timeoutMs) {
        const uintptr_t frame = FindTradeQuantityPromptFrame();
        if (frame > 0x10000) {
            return frame;
        }
        Sleep(pollMs);
    }
    return 0;
}

static uintptr_t ResolveTradeContext() {
    const uintptr_t gc = Offsets::ResolveGameContext();
    if (!gc) return 0;
    __try {
        const uintptr_t trade = *reinterpret_cast<uintptr_t*>(gc + 0x58);
        return trade > 0x10000 ? trade : 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

static bool ReadTradePlayerOfferQuantity(uint32_t itemId, uint32_t* outQuantity) {
    if (outQuantity) {
        *outQuantity = 0;
    }
    if (itemId == 0) return false;

    const uintptr_t tradePtr = ResolveTradeContext();
    if (tradePtr < 0x10000) return false;

    __try {
        const auto* ctx = reinterpret_cast<const TradeContextView*>(tradePtr);
        const auto* items = ctx->player.items.buffer;
        const uint32_t count = ctx->player.items.size;
        if (!items || count == 0 || count > 64u) {
            return false;
        }
        for (uint32_t i = 0; i < count; ++i) {
            if (items[i].item_id != itemId) {
                continue;
            }
            if (outQuantity) {
                *outQuantity = items[i].quantity;
            }
            return true;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    return false;
}

static bool WaitForPromptCallbackOfferResult(uint32_t itemId,
                                             uint32_t expectedQuantity,
                                             const char* actionLabel,
                                             uint32_t timeoutMs = 2500,
                                             uint32_t pollMs = 50) {
    const DWORD start = GetTickCount();
    while (GetTickCount() - start < timeoutMs) {
        const uintptr_t frame = FindTradeQuantityPromptFrame();
        if (frame < 0x10000) {
            Log::Info("TradeMgr: %s callback path closed prompt item=%u quantity=%u",
                      actionLabel ? actionLabel : "OfferItemPrompt",
                      itemId,
                      expectedQuantity);
            return true;
        }

        const uint32_t childCount = UIMgr::GetChildFrameCount(frame);
        if (s_chooseQuantityPopupHookSeam == ChooseQuantityPopupHookSeam::InventorySlot
            && childCount == _countof(kTradeQuantityPromptFiveChildSignature)
            && MatchesTradeQuantityPromptSignature(
                frame,
                kTradeQuantityPromptFiveChildSignature,
                _countof(kTradeQuantityPromptFiveChildSignature))) {
            Log::Info("TradeMgr: %s callback path reached residual 5-child prompt after inventorySlot automation item=%u quantity=%u frame=0x%08X; skipping manual fallback",
                      actionLabel ? actionLabel : "OfferItemPrompt",
                      itemId,
                      expectedQuantity,
                      static_cast<unsigned>(frame));
            return true;
        }

        uint32_t observedQuantity = 0;
        if (itemId != 0
            && ReadTradePlayerOfferQuantity(itemId, &observedQuantity)
            && observedQuantity >= expectedQuantity) {
            Log::Info("TradeMgr: %s callback path already applied via trade cart item=%u expected=%u observed=%u promptFrame=0x%08X childCount=%u",
                      actionLabel ? actionLabel : "OfferItemPrompt",
                      itemId,
                      expectedQuantity,
                      observedQuantity,
                      static_cast<unsigned>(frame),
                      frame > 0x10000 ? UIMgr::GetChildFrameCount(frame) : 0u);
            return true;
        }

        Sleep(pollMs);
    }

    return false;
}

static const char* ChooseQuantityPopupHookSeamToString(ChooseQuantityPopupHookSeam seam) {
    switch (seam) {
    case ChooseQuantityPopupHookSeam::InventorySlot:
        return "inventorySlot";
    case ChooseQuantityPopupHookSeam::MaxCountDeref:
        return "maxCount-deref";
    case ChooseQuantityPopupHookSeam::MaxCountDirect:
        return "maxCount-direct";
    default:
        return "none";
    }
}

static bool IsChooseQuantityPopupPassiveOnly() {
    return s_chooseQuantityPopupPassiveOnly;
}

static ChooseQuantityPopupNative GetChooseQuantityPopupOriginal() {
    return reinterpret_cast<ChooseQuantityPopupNative>(s_chooseQuantityPopupOriginalRaw);
}

static void LogChooseQuantityPopupPassiveProbe(const char* label, uintptr_t value) {
    if (value <= 0x10000) {
        return;
    }
    __try {
        const uint32_t word0 = *reinterpret_cast<const uint32_t*>(value + 0x0);
        const uint32_t word1 = *reinterpret_cast<const uint32_t*>(value + 0x4);
        const uint32_t word2 = *reinterpret_cast<const uint32_t*>(value + 0x8);
        Log::Info("TradeMgr: ChooseQuantityPopup passive probe %s=0x%08X dwords=[0x%08X,0x%08X,0x%08X]",
                  label,
                  static_cast<unsigned>(value),
                  word0,
                  word1,
                  word2);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Warn("TradeMgr: ChooseQuantityPopup passive probe %s=0x%08X faulted",
                  label,
                  static_cast<unsigned>(value));
    }
}

static void __cdecl LogChooseQuantityPopupPassiveEntry() {
    const LONG hitCount = InterlockedIncrement(&s_chooseQuantityPassiveEntryCount);
    const auto mode = static_cast<QuantityPromptAutomationMode>(
        InterlockedCompareExchange(&s_pendingQuantityPromptMode, 0, 0));
    const bool shouldLog = mode != QuantityPromptAutomationMode::None || hitCount <= 6;
    if (!shouldLog) {
        return;
    }

    const uintptr_t eax = static_cast<uintptr_t>(InterlockedCompareExchange(&s_chooseQuantityPassiveLastEax, 0, 0));
    const uintptr_t ecx = static_cast<uintptr_t>(InterlockedCompareExchange(&s_chooseQuantityPassiveLastEcx, 0, 0));
    const uintptr_t edx = static_cast<uintptr_t>(InterlockedCompareExchange(&s_chooseQuantityPassiveLastEdx, 0, 0));
    const uintptr_t ebx = static_cast<uintptr_t>(InterlockedCompareExchange(&s_chooseQuantityPassiveLastEbx, 0, 0));
    const uintptr_t esi = static_cast<uintptr_t>(InterlockedCompareExchange(&s_chooseQuantityPassiveLastEsi, 0, 0));
    const uintptr_t edi = static_cast<uintptr_t>(InterlockedCompareExchange(&s_chooseQuantityPassiveLastEdi, 0, 0));
    const uintptr_t ebp = static_cast<uintptr_t>(InterlockedCompareExchange(&s_chooseQuantityPassiveLastEbp, 0, 0));
    const uintptr_t esp = static_cast<uintptr_t>(InterlockedCompareExchange(&s_chooseQuantityPassiveLastEsp, 0, 0));
    const uintptr_t stack0 = static_cast<uintptr_t>(InterlockedCompareExchange(&s_chooseQuantityPassiveLastStack0, 0, 0));
    const uintptr_t stack4 = static_cast<uintptr_t>(InterlockedCompareExchange(&s_chooseQuantityPassiveLastStack4, 0, 0));
    const uintptr_t stack8 = static_cast<uintptr_t>(InterlockedCompareExchange(&s_chooseQuantityPassiveLastStack8, 0, 0));
    const uintptr_t stack12 = static_cast<uintptr_t>(InterlockedCompareExchange(&s_chooseQuantityPassiveLastStack12, 0, 0));

    Log::Info("TradeMgr: ChooseQuantityPopup passive entry[%u] seam=%s mode=%u eax=0x%08X ecx=0x%08X edx=0x%08X ebx=0x%08X esi=0x%08X edi=0x%08X ebp=0x%08X esp=0x%08X [esp]=0x%08X [esp+4]=0x%08X [esp+8]=0x%08X [esp+12]=0x%08X",
              static_cast<unsigned>(hitCount),
              ChooseQuantityPopupHookSeamToString(s_chooseQuantityPopupHookSeam),
              static_cast<uint32_t>(mode),
              static_cast<unsigned>(eax),
              static_cast<unsigned>(ecx),
              static_cast<unsigned>(edx),
              static_cast<unsigned>(ebx),
              static_cast<unsigned>(esi),
              static_cast<unsigned>(edi),
              static_cast<unsigned>(ebp),
              static_cast<unsigned>(esp),
              static_cast<unsigned>(stack0),
              static_cast<unsigned>(stack4),
              static_cast<unsigned>(stack8),
              static_cast<unsigned>(stack12));

    LogChooseQuantityPopupPassiveProbe("eax", eax);
    LogChooseQuantityPopupPassiveProbe("ecx", ecx);
    LogChooseQuantityPopupPassiveProbe("edx", edx);
    LogChooseQuantityPopupPassiveProbe("ebx", ebx);
    LogChooseQuantityPopupPassiveProbe("esi", esi);
    LogChooseQuantityPopupPassiveProbe("edi", edi);
    LogChooseQuantityPopupPassiveProbe("stack4", stack4);
    LogChooseQuantityPopupPassiveProbe("stack8", stack8);
    LogChooseQuantityPopupPassiveProbe("stack12", stack12);
}

extern "C" __declspec(naked) void ChooseQuantityPopupPassiveDetourNaked() {
    __asm {
        mov dword ptr [s_chooseQuantityPassiveLastEax], eax
        mov dword ptr [s_chooseQuantityPassiveLastEcx], ecx
        mov dword ptr [s_chooseQuantityPassiveLastEdx], edx
        mov dword ptr [s_chooseQuantityPassiveLastEbx], ebx
        mov dword ptr [s_chooseQuantityPassiveLastEsi], esi
        mov dword ptr [s_chooseQuantityPassiveLastEdi], edi
        mov dword ptr [s_chooseQuantityPassiveLastEbp], ebp
        mov dword ptr [s_chooseQuantityPassiveLastEsp], esp
        mov eax, [esp]
        mov dword ptr [s_chooseQuantityPassiveLastStack0], eax
        mov eax, [esp+4]
        mov dword ptr [s_chooseQuantityPassiveLastStack4], eax
        mov eax, [esp+8]
        mov dword ptr [s_chooseQuantityPassiveLastStack8], eax
        mov eax, [esp+12]
        mov dword ptr [s_chooseQuantityPassiveLastStack12], eax
        pushfd
        pushad
        call LogChooseQuantityPopupPassiveEntry
        popad
        popfd
        jmp [s_chooseQuantityPopupOriginalRaw]
    }
}

static void __cdecl OnChooseQuantityPopupUIMessage(void* a1, void* a2, void* a3) {
    if (const auto original = GetChooseQuantityPopupOriginal()) {
        original(a1, a2, a3);
    }

    const auto mode = static_cast<QuantityPromptAutomationMode>(
        InterlockedCompareExchange(&s_pendingQuantityPromptMode, 0, 0));
    if (mode == QuantityPromptAutomationMode::None) return;

    const auto* message = reinterpret_cast<const UiInteractionMessageView*>(a1);
    const uint32_t messageId = message ? message->message_id : 0u;
    const uint32_t frameId = message ? message->frame_id : 0u;

    // Only queue clicks on kInitFrame (0x9), matching GWToolbox's pattern.
    // Earlier messages (0x4, 0x5, 0x31) fire before the popup is fully
    // initialized — clicking buttons at that point is silently ignored by
    // the game.  Log non-init messages but do NOT consume the pending mode.
    if (messageId != kUiInteractionInitFrame) {
        Log::Info("TradeMgr: OnChooseQuantityPopupUIMessage mode=%u msg=0x%X frameId=%u (pre-init, not consuming mode)",
                  static_cast<uint32_t>(mode),
                  messageId,
                  frameId);
        return;
    }

    uintptr_t frame = 0;
    if (frameId != 0u) {
        frame = UIMgr::GetFrameById(frameId);
        if (frame > 0x10000 && UIMgr::GetChildOffsetId(frame) != kTradeQuantityPromptChildOffsetId) {
            frame = 0;
        }
    }
    if (frame < 0x10000) {
        frame = FindTradeQuantityPromptFrame();
    }
    if (frame < 0x10000) {
        Log::Info("TradeMgr: OnChooseQuantityPopupUIMessage mode=%u msg=0x%X frameId=%u a1=0x%08X a2=0x%08X a3=0x%08X promptFrame=0x00000000",
                  static_cast<uint32_t>(mode),
                  messageId,
                  frameId,
                  static_cast<unsigned>(reinterpret_cast<uintptr_t>(a1)),
                  static_cast<unsigned>(reinterpret_cast<uintptr_t>(a2)),
                  static_cast<unsigned>(reinterpret_cast<uintptr_t>(a3)));
        return;
    }

    static uint32_t s_lastPromptDumpFrameId = 0;
    const uint32_t promptFrameId = UIMgr::GetFrameId(frame);
    if (promptFrameId != s_lastPromptDumpFrameId) {
        s_lastPromptDumpFrameId = promptFrameId;
        Log::Info("TradeMgr: Quantity prompt dump root=0x%08X frameId=%u childCount=%u context=0x%08X",
                  static_cast<unsigned>(frame),
                  promptFrameId,
                  UIMgr::GetChildFrameCount(frame),
                  static_cast<unsigned>(UIMgr::GetFrameContext(frame)));
        UIMgr::DebugDumpChildFrames(frame, "trade-quantity-root", 12);

        const uintptr_t child1 = UIMgr::GetChildFrameByOffset(frame, 1u);
        const uintptr_t child2 = UIMgr::GetChildFrameByOffset(frame, 2u);
        const uintptr_t child3 = UIMgr::GetChildFrameByOffset(frame, 3u);
        const uintptr_t child4 = UIMgr::GetChildFrameByOffset(frame, 4u);
        if (child1 > 0x10000) UIMgr::DebugDumpChildFrames(child1, "trade-quantity-root[1]", 12);
        if (child2 > 0x10000) UIMgr::DebugDumpChildFrames(child2, "trade-quantity-root[2]", 12);
        if (child3 > 0x10000) UIMgr::DebugDumpChildFrames(child3, "trade-quantity-root[3]", 12);
        if (child4 > 0x10000) UIMgr::DebugDumpChildFrames(child4, "trade-quantity-root[4]", 12);
    }

    const uintptr_t okBtn = UIMgr::GetChildFrameByOffset(frame, kTradeQuantityPromptOkChildOffsetId);
    const uintptr_t maxBtn = UIMgr::GetChildFrameByOffset(frame, kTradeQuantityPromptMaxChildOffsetId);
    LONG count = 0;
    if (mode == QuantityPromptAutomationMode::MaxOffer && maxBtn > 0x10000) {
        s_pendingQuantityPromptClicks[count++] = {UIMgr::GetFrameId(maxBtn), GetTickCount()};
    }
    if (mode == QuantityPromptAutomationMode::ValueOffer) {
        // The GmItemSplit popup's callback context (uictl_context) holds:
        //   +0x04: count (current selection, defaults to 1)
        //   +0x08: maxCount (full stack size)
        // Write the desired quantity directly to the count field so the
        // OK button commits it.  This avoids the fragile UI value-setting
        // paths that cannot update the popup's backing data.
        const uint32_t desiredQty = static_cast<uint32_t>(InterlockedCompareExchange(&s_pendingQuantityPromptValue, 0, 0));
        __try {
            const uintptr_t cbArrayData = *reinterpret_cast<uintptr_t*>(frame + 0xA8);
            const uint32_t cbArraySize = *reinterpret_cast<uint32_t*>(frame + 0xAC);
            for (uint32_t ci = 0; ci < cbArraySize && ci < 8; ++ci) {
                const uintptr_t entryBase = cbArrayData + ci * 12;
                const uintptr_t callbackFn = *reinterpret_cast<uintptr_t*>(entryBase);
                const uintptr_t uictlCtx = *reinterpret_cast<uintptr_t*>(entryBase + 4);
                if (callbackFn == s_chooseQuantityPopupHookAddr && uictlCtx > 0x10000) {
                    const uint32_t currentCount = *reinterpret_cast<uint32_t*>(uictlCtx + 0x04);
                    const uint32_t maxCount = *reinterpret_cast<uint32_t*>(uictlCtx + 0x08);
                    const uint32_t clampedQty = (desiredQty > maxCount) ? maxCount : desiredQty;
                    *reinterpret_cast<uint32_t*>(uictlCtx + 0x04) = clampedQty;
                    Log::Info("TradeMgr: ValueOffer wrote count=%u (was %u, max=%u) at ctx=0x%08X+0x04",
                              clampedQty, currentCount, maxCount, static_cast<unsigned>(uictlCtx));
                    break;
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Log::Warn("TradeMgr: OnChooseQuantityPopupUIMessage ValueOffer context write faulted");
        }
    }
    if (okBtn > 0x10000) {
        s_pendingQuantityPromptClicks[count++] = {UIMgr::GetFrameId(okBtn), GetTickCount()};
    }
    InterlockedExchange(&s_pendingQuantityPromptClickCount, count);
    InterlockedExchange(&s_pendingQuantityPromptMode, static_cast<LONG>(QuantityPromptAutomationMode::None));
    Log::Info("TradeMgr: OnChooseQuantityPopupUIMessage mode=%u msg=0x%X frameId=%u a1=0x%08X a2=0x%08X a3=0x%08X promptFrame=0x%08X queuedClicks=%u okFrameId=%u maxFrameId=%u",
              static_cast<uint32_t>(mode),
              messageId,
              frameId,
              static_cast<unsigned>(reinterpret_cast<uintptr_t>(a1)),
              static_cast<unsigned>(reinterpret_cast<uintptr_t>(a2)),
              static_cast<unsigned>(reinterpret_cast<uintptr_t>(a3)),
              static_cast<unsigned>(frame),
              static_cast<uint32_t>(count),
              okBtn > 0x10000 ? UIMgr::GetFrameId(okBtn) : 0u,
              maxBtn > 0x10000 ? UIMgr::GetFrameId(maxBtn) : 0u);
}

static bool EnsureChooseQuantityPopupHook() {
    if (s_chooseQuantityPopupHookInstalled) return true;
    static constexpr const char* kChooseQuantityAssertFile = "P:\\Code\\Gw\\Ui\\Game\\GmItemSplit.cpp";
    static constexpr const char* kChooseQuantityAssertMsgs[] = {
        "(*data)->maxCount > 1",
        "data->maxCount > 1",
        "inventorySlot",
    };
    static constexpr const char* kChooseQuantityAssertLabels[] = {
        "maxCount-deref",
        "maxCount-direct",
        "inventorySlot-fallback",
    };

    uintptr_t addr = 0;
    uintptr_t start = 0;
    const char* resolvedLabel = "none";
    const char* resolvedMsg = nullptr;
    ChooseQuantityPopupHookSeam resolvedSeam = ChooseQuantityPopupHookSeam::None;
    uintptr_t strongAddr = 0;
    uintptr_t strongStart = 0;
    const char* strongLabel = nullptr;
    const char* strongMsg = nullptr;
    ChooseQuantityPopupHookSeam strongSeam = ChooseQuantityPopupHookSeam::None;
    uintptr_t inventoryAddr = 0;
    uintptr_t inventoryStart = 0;
    const char* inventoryLabel = nullptr;
    const char* inventoryMsg = nullptr;
    auto recoverCanonicalFunctionStart = [](uintptr_t candidate, uintptr_t candidateStart) -> uintptr_t {
        const uintptr_t scanFloor = candidate > 0x180 ? candidate - 0x180 : 0;
        uintptr_t best = candidateStart;
        for (uintptr_t p = candidate; p > scanFloor + 8; --p) {
            __try {
                const uint8_t b0 = *reinterpret_cast<const uint8_t*>(p);
                const uint8_t b1 = *reinterpret_cast<const uint8_t*>(p + 1);
                const uint8_t b2 = *reinterpret_cast<const uint8_t*>(p + 2);
                const uint8_t b3 = *reinterpret_cast<const uint8_t*>(p + 3);
                const uint8_t b4 = *reinterpret_cast<const uint8_t*>(p + 4);
                const uint8_t b6 = *reinterpret_cast<const uint8_t*>(p + 6);
                const uint8_t b7 = *reinterpret_cast<const uint8_t*>(p + 7);
                const bool hotpatch = (b0 == 0x8B && b1 == 0xFF && b2 == 0x55 && b3 == 0x8B && b4 == 0xEC);
                const bool framedThiscall = (b0 == 0x55 && b1 == 0x8B && b2 == 0xEC && b3 == 0x83 && b4 == 0xEC && b6 == 0x56 && b7 == 0x8B);
                const bool framedGeneric = (b0 == 0x55 && b1 == 0x8B && b2 == 0xEC && (b3 == 0x83 || b3 == 0x56 || b3 == 0x57 || b3 == 0x6A || b3 == 0x51));
                if (hotpatch) {
                    return p;
                }
                if (framedThiscall) {
                    best = p;
                } else if (framedGeneric && best <= 0x10000) {
                    best = p;
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                return best;
            }
        }
        return best;
    };
    for (size_t i = 0; i < _countof(kChooseQuantityAssertMsgs); ++i) {
        uintptr_t candidate = Scanner::FindAssertion(
            kChooseQuantityAssertFile, kChooseQuantityAssertMsgs[i], 0);
        if (candidate <= 0x10000) {
            continue;
        }

        uintptr_t candidateStart = Scanner::ToFunctionStart(candidate, 0x1000);
        if (candidateStart > 0x10000) {
            candidateStart = recoverCanonicalFunctionStart(candidate, candidateStart);
        }
        const uintptr_t minAddr = candidate > 0x100 ? candidate - 0x100 : 0;
        for (uintptr_t p = candidate; candidateStart <= 0x10000 && p > minAddr + 1; --p) {
            __try {
                const uint8_t b = *reinterpret_cast<const uint8_t*>(p);
                if (b != 0xC3 && b != 0xC2) continue;
                uintptr_t startCandidate = p + (b == 0xC2 ? 3 : 1);
                while (startCandidate < candidate) {
                    const uint8_t pad = *reinterpret_cast<const uint8_t*>(startCandidate);
                    if (pad != 0xCC && pad != 0x90) break;
                    ++startCandidate;
                }
                if (startCandidate < candidate) {
                    candidateStart = startCandidate;
                    break;
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                candidateStart = 0;
                break;
            }
        }

        Log::Info("TradeMgr: ChooseQuantityPopup candidate[%s]='%s' addr=0x%08X start=0x%08X",
                  kChooseQuantityAssertLabels[i],
                  kChooseQuantityAssertMsgs[i],
                  static_cast<unsigned>(candidate),
                  static_cast<unsigned>(candidateStart));

        if (candidateStart > 0x10000) {
            const bool isInventorySlot = strcmp(kChooseQuantityAssertLabels[i], "inventorySlot-fallback") == 0;
            if (isInventorySlot) {
                if (inventoryStart <= 0x10000) {
                    inventoryAddr = candidate;
                    inventoryStart = candidateStart;
                    inventoryLabel = kChooseQuantityAssertLabels[i];
                    inventoryMsg = kChooseQuantityAssertMsgs[i];
                }
            } else if (strongStart <= 0x10000) {
                strongAddr = candidate;
                strongStart = candidateStart;
                strongLabel = kChooseQuantityAssertLabels[i];
                strongMsg = kChooseQuantityAssertMsgs[i];
                strongSeam = strcmp(kChooseQuantityAssertLabels[i], "maxCount-deref") == 0
                    ? ChooseQuantityPopupHookSeam::MaxCountDeref
                    : ChooseQuantityPopupHookSeam::MaxCountDirect;
            }

            if (!kPassiveOnlyChooseQuantityStrongSeams) {
                addr = candidate;
                start = candidateStart;
                resolvedLabel = kChooseQuantityAssertLabels[i];
                resolvedMsg = kChooseQuantityAssertMsgs[i];
                resolvedSeam = isInventorySlot
                    ? ChooseQuantityPopupHookSeam::InventorySlot
                    : strongSeam;
                break;
            }
        }
    }
    if (addr <= 0x10000 || start <= 0x10000) {
        if (kPassiveOnlyChooseQuantityStrongSeams && inventoryStart > 0x10000) {
            addr = inventoryAddr;
            start = inventoryStart;
            resolvedLabel = inventoryLabel ? inventoryLabel : "inventorySlot-fallback";
            resolvedMsg = inventoryMsg;
            resolvedSeam = ChooseQuantityPopupHookSeam::InventorySlot;
            if (strongStart > 0x10000) {
                Log::Info("TradeMgr: ChooseQuantityPopup preferring inventorySlot active hook at 0x%08X over passive stronger seam %s at 0x%08X",
                          static_cast<unsigned>(inventoryStart),
                          strongLabel ? strongLabel : "none",
                          static_cast<unsigned>(strongStart));
            }
        } else if (strongStart > 0x10000) {
            addr = strongAddr;
            start = strongStart;
            resolvedLabel = strongLabel ? strongLabel : "none";
            resolvedMsg = strongMsg;
            resolvedSeam = strongSeam;
        } else if (inventoryStart > 0x10000) {
            addr = inventoryAddr;
            start = inventoryStart;
            resolvedLabel = inventoryLabel ? inventoryLabel : "inventorySlot-fallback";
            resolvedMsg = inventoryMsg;
            resolvedSeam = ChooseQuantityPopupHookSeam::InventorySlot;
        }
    }
    if (addr <= 0x10000 || start <= 0x10000) {
        Log::Warn("TradeMgr: ChooseQuantityPopup could not resolve a hookable function start for any candidate");
        return false;
    }
    Log::Info("TradeMgr: ChooseQuantityPopup assertion[%s]='%s' addr=0x%08X start=0x%08X",
              resolvedLabel,
              resolvedMsg ? resolvedMsg : "",
              static_cast<unsigned>(addr),
              static_cast<unsigned>(start));
    addr = start;
    if (addr <= 0x10000) {
        Log::Warn("TradeMgr: ChooseQuantityPopup function-start scan failed from 0x%08X", static_cast<unsigned>(addr));
        return false;
    }
    const bool passiveOnly = kPassiveOnlyChooseQuantityStrongSeams
        && resolvedSeam != ChooseQuantityPopupHookSeam::None
        && resolvedSeam != ChooseQuantityPopupHookSeam::InventorySlot;
    void* detour = passiveOnly
        ? reinterpret_cast<void*>(&ChooseQuantityPopupPassiveDetourNaked)
        : reinterpret_cast<void*>(&OnChooseQuantityPopupUIMessage);
    if (MH_CreateHook(reinterpret_cast<void*>(addr), detour,
                      reinterpret_cast<void**>(&s_chooseQuantityPopupOriginalRaw)) != MH_OK) {
        Log::Warn("TradeMgr: ChooseQuantityPopup hook creation failed at 0x%08X", static_cast<unsigned>(addr));
        return false;
    }
    if (MH_EnableHook(reinterpret_cast<void*>(addr)) != MH_OK) {
        Log::Warn("TradeMgr: ChooseQuantityPopup hook enable failed at 0x%08X", static_cast<unsigned>(addr));
        MH_RemoveHook(reinterpret_cast<void*>(addr));
        s_chooseQuantityPopupOriginalRaw = nullptr;
        return false;
    }
    s_chooseQuantityPopupHookAddr = addr;
    s_chooseQuantityPopupHookInstalled = true;
    s_chooseQuantityPopupHookSeam = resolvedSeam;
    s_chooseQuantityPopupPassiveOnly = passiveOnly;
    Log::Info("TradeMgr: ChooseQuantityPopup hook installed at 0x%08X seam=%s mode=%s",
              static_cast<unsigned>(addr),
              ChooseQuantityPopupHookSeamToString(resolvedSeam),
              passiveOnly ? "passive" : "active");
    return true;
}

static bool DrainQueuedPromptClicks(const QueuedFrameClick* clicks, size_t count, uint32_t timeoutMs = 1000, uint32_t sleepMs = 60) {
    if (!clicks || !count) return false;
    bool pending[8] = {};
    if (count > _countof(pending)) return false;
    for (size_t i = 0; i < count; ++i) pending[i] = true;

    const DWORD start = GetTickCount();
    Sleep(120);
    while (GetTickCount() - start <= timeoutMs) {
        bool anyPending = false;
        bool clickedThisPass = false;
        for (size_t i = 0; i < count; ++i) {
            if (!pending[i]) continue;
            anyPending = true;
            const uintptr_t frame = UIMgr::GetFrameById(clicks[i].frame_id);
            if (frame < 0x10000) continue;
            bool clicked = UIMgr::ButtonClick(frame);
            if (!clicked) {
                clicked = UIMgr::ButtonClickImmediateFull(frame);
            }
            if (clicked) {
                pending[i] = false;
                clickedThisPass = true;
                Log::Info("TradeMgr: DrainQueuedPromptClicks clicked frameId=%u frame=0x%08X",
                          clicks[i].frame_id, static_cast<unsigned>(frame));
            }
        }
        if (!anyPending) return true;
        if (!clickedThisPass) Sleep(sleepMs);
        else Sleep(90);
    }

    for (size_t i = 0; i < count; ++i) {
        if (pending[i]) {
            Log::Warn("TradeMgr: DrainQueuedPromptClicks timed out frameId=%u", clicks[i].frame_id);
        }
    }
    return false;
}

static bool ObservePassiveTradeQuantityPrompt(const char* actionLabel, uint32_t itemId) {
    const uintptr_t frame = WaitForTradeQuantityPromptFrame();
    InterlockedExchange(&s_pendingQuantityPromptClickCount, 0);
    InterlockedExchange(&s_pendingQuantityPromptValue, 0);
    InterlockedExchange(&s_pendingQuantityPromptMode, static_cast<LONG>(QuantityPromptAutomationMode::None));
    if (frame < 0x10000) {
        Log::Warn("TradeMgr: %s passive seam observed no prompt frame item=%u seam=%s",
                  actionLabel ? actionLabel : "OfferItemPrompt",
                  itemId,
                  ChooseQuantityPopupHookSeamToString(s_chooseQuantityPopupHookSeam));
        return false;
    }
    Log::Info("TradeMgr: %s passive seam observed prompt item=%u seam=%s frame=0x%08X frameId=%u childCount=%u context=0x%08X",
              actionLabel ? actionLabel : "OfferItemPrompt",
              itemId,
              ChooseQuantityPopupHookSeamToString(s_chooseQuantityPopupHookSeam),
              static_cast<unsigned>(frame),
              UIMgr::GetFrameId(frame),
              UIMgr::GetChildFrameCount(frame),
              static_cast<unsigned>(UIMgr::GetFrameContext(frame)));
    return true;
}

uint32_t GetTradeQuantityPromptFrame() {
    return static_cast<uint32_t>(FindTradeQuantityPromptFrame());
}

uint32_t GetTradeQuantityPromptContext() {
    const uintptr_t frame = FindTradeQuantityPromptFrame();
    return frame ? static_cast<uint32_t>(UIMgr::GetFrameContext(frame)) : 0u;
}

uint32_t GetTradeQuantityPromptChildCount() {
    const uintptr_t frame = FindTradeQuantityPromptFrame();
    return frame ? UIMgr::GetChildFrameCount(frame) : 0u;
}

bool IsTradeQuantityPromptOpen() {
    return FindTradeQuantityPromptFrame() > 0x10000;
}

bool HasNativeRemoveItem() {
    return Offsets::TradeRemoveItem > 0x10000;
}

bool EnableTradeWindowCaptureForPlayerTrade() {
    InterlockedExchange(&s_tradeCartCaptureRequested, 1);
    InterlockedExchange(&s_tradeCartDisableQueued, 0);

    if (!s_updateTradeCartOriginal) {
        const bool installed = InstallTradeCartHook();
        if (installed) {
            Log::Info("TradeMgr: UpdateTradeCart hook armed on demand for player trade");
        } else {
            Log::Warn("TradeMgr: Failed to arm UpdateTradeCart hook for player trade");
        }
        return installed;
    }

    if (s_tradeCartHookEnabled) {
        Log::Info("TradeMgr: UpdateTradeCart hook already enabled; capture re-armed");
        return true;
    }

    if (Offsets::UpdateTradeCart <= 0x10000) {
        Log::Warn("TradeMgr: UpdateTradeCart offset not resolved for re-enable");
        return false;
    }

    const MH_STATUS mhStatus = MH_EnableHook(reinterpret_cast<LPVOID>(Offsets::UpdateTradeCart));
    if (mhStatus != MH_OK && mhStatus != MH_ERROR_ENABLED) {
        Log::Warn("TradeMgr: MH_EnableHook failed for UpdateTradeCart re-enable: %s",
                  MH_StatusToString(mhStatus));
        return false;
    }

    s_tradeCartHookEnabled = true;
    Log::Info("TradeMgr: UpdateTradeCart hook re-enabled for player trade capture");
    return true;
}

void InitiateTradeUiOnly(uint32_t agentId, uint32_t requestedPlayerNumber) {
    if (agentId == 0) return;
    uint32_t playerNumber = requestedPlayerNumber;
    if (auto* agent = AgentMgr::GetAgentByID(agentId)) {
        if (!playerNumber && agent->type == 0xDB) {
            playerNumber = reinterpret_cast<AgentLiving*>(agent)->player_number;
        }
    }

    const uint32_t tradeTarget = agentId;
    const uint32_t currentTargetBefore = AgentMgr::GetTargetId();
    Log::Info("TradeMgr: InitiateTradeUiOnly using UIMessage path agent=%u requestedPlayerNumber=%u resolvedPlayerNumber=%u targetAgent=%u currentTargetBefore=%u",
              agentId,
              requestedPlayerNumber,
              playerNumber,
              tradeTarget,
              currentTargetBefore);
    const uint32_t currentTargetAfter = AgentMgr::GetTargetId();
    const uintptr_t tradeButtonFrameRoot = UIMgr::GetFrameByHash(kTradeButtonFrameHash);
    const uintptr_t tradeButtonContext = tradeButtonFrameRoot ? UIMgr::GetFrameContext(tradeButtonFrameRoot) : 0u;
    Log::Info("TradeMgr: InitiateTradeUiOnly state before UIMessage currentTargetAfter=%u tradeButtonRoot=0x%08X rootState=0x%X rootChildOffset=%u rootContext=0x%08X",
              currentTargetAfter,
              static_cast<unsigned>(tradeButtonFrameRoot),
              tradeButtonFrameRoot ? UIMgr::GetFrameState(tradeButtonFrameRoot) : 0u,
              tradeButtonFrameRoot ? UIMgr::GetChildOffsetId(tradeButtonFrameRoot) : 0u,
              static_cast<unsigned>(tradeButtonContext));
    if (tradeButtonContext) {
        UIMgr::DebugDumpFramesForContext(tradeButtonContext, "trade_button_context", 64);
    }
    const uintptr_t actionPrimaryByContext = tradeButtonContext
        ? UIMgr::GetFrameByContextAndChildOffset(tradeButtonContext, kTradeButtonActionPrimaryChildOffsetId, tradeButtonFrameRoot)
        : 0u;
    const uintptr_t actionSecondaryByContext = tradeButtonContext
        ? UIMgr::GetFrameByContextAndChildOffset(tradeButtonContext, kTradeButtonActionSecondaryChildOffsetId, tradeButtonFrameRoot)
        : 0u;
    const uintptr_t rootChild0 = tradeButtonFrameRoot
        ? UIMgr::GetChildFrameByIndex(tradeButtonFrameRoot, 0)
        : 0u;
    const uintptr_t altByHash = UIMgr::GetFrameByHash(kTradeButtonAltHash);
    const uintptr_t altByContext = tradeButtonContext
        ? UIMgr::GetFrameByContextAndChildOffset(tradeButtonContext, kTradeButtonAltChildOffsetId, tradeButtonFrameRoot)
        : 0u;
    const uintptr_t followupFrame = actionPrimaryByContext
        ? actionPrimaryByContext
        : actionSecondaryByContext;

    if (tradeButtonFrameRoot) {
        UIMgr::DebugDumpChildFrames(tradeButtonFrameRoot, "trade_button_root", 12);
    }

    const uintptr_t clickFrame = rootChild0
        ? rootChild0
        : (tradeButtonFrameRoot
            ? tradeButtonFrameRoot
        : (altByContext ? altByContext
            : (actionPrimaryByContext ? actionPrimaryByContext
                : (actionSecondaryByContext ? actionSecondaryByContext : altByHash))));
    Log::Info("TradeMgr: InitiateTradeUiOnly clicking frame-plane trade action tradeTarget=%u rootFrame=0x%08X rootChild0=0x%08X followupFrame=0x%08X action123=0x%08X action122=0x%08X altByHash=0x%08X altByContext=0x%08X clickFrame=0x%08X",
              tradeTarget,
              static_cast<unsigned>(tradeButtonFrameRoot),
              static_cast<unsigned>(rootChild0),
              static_cast<unsigned>(followupFrame),
              static_cast<unsigned>(actionPrimaryByContext),
              static_cast<unsigned>(actionSecondaryByContext),
              static_cast<unsigned>(altByHash),
              static_cast<unsigned>(altByContext),
              static_cast<unsigned>(clickFrame));
    if (clickFrame) {
        const bool clicked = UIMgr::ButtonClick(clickFrame);
        Log::Info("TradeMgr: InitiateTradeUiOnly frame-plane click result=%u frame=0x%08X",
                  clicked ? 1u : 0u,
                  static_cast<unsigned>(clickFrame));
        if (s_tradeHackPatched) {
            Log::Info("TradeMgr: InitiateTradeUiOnly leaving trade patch enabled through trade-open transition");
        }
        return;
    }
    Log::Warn("TradeMgr: InitiateTradeUiOnly no clickable trade frame found; falling back to UIMessage");
    UIMgr::SendUIMessageAsm(kUiInitiateTrade, reinterpret_cast<void*>(static_cast<uintptr_t>(tradeTarget)), nullptr);
    if (s_tradeHackPatched) {
        Log::Info("TradeMgr: InitiateTradeUiOnly leaving trade patch enabled through UIMessage fallback transition");
    }
}

void InitiateTrade(uint32_t agentId, uint32_t requestedPlayerNumber) {
    if (agentId == 0) return;
    InterlockedExchange(&s_tradeWindowContext, 0);
    InterlockedExchange(&s_tradeWindowFrame, 0);
    InterlockedExchange(&s_tradeWindowCaptureCount, 0);
    EnableTradeWindowCaptureForPlayerTrade();
    // Trade hack patch disabled — it prevents trade from opening.
    // if (!s_tradeHackPatched) {
    //     ToggleTradePatch(true);
    // }
    const uint32_t currentTargetBefore = AgentMgr::GetTargetId();
    Log::Info("TradeMgr: InitiateTrade stage1 agent=%u requestedPlayerNumber=%u currentTargetBefore=%u",
              agentId, requestedPlayerNumber, currentTargetBefore);
    AgentMgr::CancelAction();
    Log::Info("TradeMgr: InitiateTrade stage1 after CancelAction");
    AgentMgr::ChangeTarget(agentId);
    const uint32_t currentTargetAfterChange = AgentMgr::GetTargetId();
    Log::Info("TradeMgr: InitiateTrade stage1 after ChangeTarget currentTarget=%u", currentTargetAfterChange);
    GameThread::EnqueuePost([agentId, requestedPlayerNumber]() {
        Log::Info("TradeMgr: InitiateTrade stage2 scheduling extra-frame UI dispatch agent=%u requestedPlayerNumber=%u",
                  agentId, requestedPlayerNumber);
        GameThread::EnqueuePost([agentId, requestedPlayerNumber]() {
            Log::Info("TradeMgr: InitiateTrade stage3 scheduling next-frame UI dispatch agent=%u requestedPlayerNumber=%u",
                      agentId, requestedPlayerNumber);
            GameThread::EnqueueSerialPre([agentId, requestedPlayerNumber]() {
                InitiateTradeUiOnly(agentId, requestedPlayerNumber);
            });
        });
    });
}

void CancelTrade(uint32_t actionRowIndex, int32_t preferredChildIndex, uint32_t transportMode) {
    const uintptr_t uiFrame = static_cast<uintptr_t>(GetTradeWindowUiFrame());
    const uintptr_t uiCtx = static_cast<uintptr_t>(GetTradeWindowUiContext());
    Log::Info("TradeMgr: CancelTrade uiFrame=0x%08X uiState=0x%X uiCtx=0x%08X childCount=%u row=%u preferredChild=%d transport=%u",
              static_cast<unsigned>(uiFrame),
              GetTradeWindowUiState(),
              static_cast<unsigned>(uiCtx),
              uiFrame ? UIMgr::GetChildFrameCount(uiFrame) : 0u,
              actionRowIndex,
              preferredChildIndex,
              transportMode);
    // Frame dumps removed — iterating stale frame pointers during trade
    // caused crashes after the trade window was open for >2 seconds.
    if (transportMode != 9u && uiFrame > 0x10000) {
        const uintptr_t actionRow = UIMgr::GetChildFrameByIndex(uiFrame, actionRowIndex);
        if (actionRow > 0x10000) {
            uint32_t cancelCandidates[6] = {};
            uint32_t candidateCount = 0;
            if (preferredChildIndex >= 0) {
                cancelCandidates[candidateCount++] = static_cast<uint32_t>(preferredChildIndex);
            } else {
                const uint32_t defaults[] = {4u, 5u, 0u, 1u, 2u, 3u};
                for (uint32_t idx : defaults) cancelCandidates[candidateCount++] = idx;
            }
            for (uint32_t ci = 0; ci < candidateCount; ++ci) {
                const uint32_t idx = cancelCandidates[ci];
                const uintptr_t candidate = UIMgr::GetChildFrameByIndex(actionRow, idx);
                if (candidate <= 0x10000) continue;
                if (transportMode == 2u) {
                    Log::Info("TradeMgr: CancelTrade trying UI candidate row[%u][%u] mouse-up frame=0x%08X frameId=%u childOffset=%u",
                              actionRowIndex,
                              idx,
                              static_cast<unsigned>(candidate),
                              UIMgr::GetFrameId(candidate),
                              UIMgr::GetChildOffsetId(candidate));
                    if (UIMgr::ButtonClick(candidate)) {
                        Log::Info("TradeMgr: CancelTrade UI mouse-up accepted on row[%u][%u]", actionRowIndex, idx);
                        return;
                    }
                    continue;
                }
                if (transportMode == 3u) {
                    Log::Info("TradeMgr: CancelTrade trying UI candidate row[%u][%u] full-mouse-click frame=0x%08X frameId=%u childOffset=%u",
                              actionRowIndex,
                              idx,
                              static_cast<unsigned>(candidate),
                              UIMgr::GetFrameId(candidate),
                              UIMgr::GetChildOffsetId(candidate));
                    if (UIMgr::ButtonClickFullMouseClick(candidate)) {
                        Log::Info("TradeMgr: CancelTrade UI full-mouse-click accepted on row[%u][%u]", actionRowIndex, idx);
                        return;
                    }
                    continue;
                }
                if (transportMode == 4u) {
                    Log::Info("TradeMgr: CancelTrade trying UI candidate row[%u][%u] full-mouse-up frame=0x%08X frameId=%u childOffset=%u",
                              actionRowIndex,
                              idx,
                              static_cast<unsigned>(candidate),
                              UIMgr::GetFrameId(candidate),
                              UIMgr::GetChildOffsetId(candidate));
                    if (UIMgr::ButtonClickFull(candidate)) {
                        Log::Info("TradeMgr: CancelTrade UI full-mouse-up accepted on row[%u][%u]", actionRowIndex, idx);
                        return;
                    }
                    continue;
                }
                Log::Info("TradeMgr: CancelTrade trying UI candidate row[%u][%u] mouse-click frame=0x%08X frameId=%u childOffset=%u",
                          actionRowIndex,
                          idx,
                          static_cast<unsigned>(candidate),
                          UIMgr::GetFrameId(candidate),
                          UIMgr::GetChildOffsetId(candidate));
                if (UIMgr::ButtonClickMouseClick(candidate)) {
                    Log::Info("TradeMgr: CancelTrade UI mouse-click accepted on row[%u][%u]", actionRowIndex, idx);
                    return;
                }
                if (transportMode == 1u) {
                    continue;
                }
                Log::Info("TradeMgr: CancelTrade trying UI candidate row[%u][%u] mouse-up frame=0x%08X frameId=%u childOffset=%u",
                          actionRowIndex,
                          idx,
                          static_cast<unsigned>(candidate),
                          UIMgr::GetFrameId(candidate),
                          UIMgr::GetChildOffsetId(candidate));
                if (UIMgr::ButtonClick(candidate)) {
                    Log::Info("TradeMgr: CancelTrade UI mouse-up accepted on row[%u][%u]", actionRowIndex, idx);
                    return;
                }
            }
            Log::Warn("TradeMgr: CancelTrade UI candidates present but no click succeeded");
            return;
        }
    }
    if (transportMode == 9u) {
        Log::Info("TradeMgr: CancelTrade transport=9 forcing native cancel after disabling trade patch");
    }
    if (s_tradeHackPatched) {
        ToggleTradePatch(false);
    }
    if (Offsets::TradeCancel > 0x10000) {
        TradeVoidNative fn = reinterpret_cast<TradeVoidNative>(Offsets::TradeCancel);
        Log::Info("TradeMgr: CancelTrade native call fn=0x%08X", static_cast<unsigned>(Offsets::TradeCancel));
        fn();
        Log::Info("TradeMgr: CancelTrade native call returned");
        return;
    }
    Log::Info("TradeMgr: CancelTrade raw packet fallback");
    CtoS::TradeCancel();
}

static void AcceptTradeImpl(uint32_t viewRetryCount) {
    if (viewRetryCount > 0u && ClickTradeWindowViewIfNeeded("accept_trade_view", viewRetryCount)) {
        Log::Info("TradeMgr: AcceptTrade clicked view button; scheduling retry retries=%u", viewRetryCount);
        QueueDelayedAcceptRetry(viewRetryCount);
        return;
    }
    if (Offsets::TradeAcceptOffer > 0x10000) {
        TradeVoidNative fn = reinterpret_cast<TradeVoidNative>(Offsets::TradeAcceptOffer);
        Log::Info("TradeMgr: AcceptTrade native call fn=0x%08X", static_cast<unsigned>(Offsets::TradeAcceptOffer));
        fn();
        Log::Info("TradeMgr: AcceptTrade native call returned");
        return;
    }
    Log::Info("TradeMgr: AcceptTrade raw packet fallback");
    CtoS::TradeAccept();
}

void AcceptTrade() {
    AcceptTradeImpl(3u);
}

static uintptr_t GetTradeWindowFromGameContext(bool logDiagnostic);

void OfferItem(uint32_t itemId, uint32_t quantity, uint32_t attemptsRemaining) {
    (void)attemptsRemaining;
    if (quantity == 0) quantity = 1;
    // Use GameContext+0x58 for trade window context and call from
    // GameThread post-dispatch (after original game callback runs).
    Log::Info("TradeMgr: OfferItem enqueuing post-dispatch offer item=%u qty=%u", itemId, quantity);
    GameThread::EnqueuePost([itemId, quantity]() {
        const uintptr_t ctx = GetTradeWindowFromGameContext();
        if (ctx <= 0x10000 || Offsets::OfferTradeItem <= 0x10000) {
            Log::Warn("TradeMgr: OfferItem post-dispatch — context unavailable ctx=0x%08X",
                      static_cast<unsigned>(ctx));
            return;
        }
        auto* window = reinterpret_cast<TradeWindowView*>(ctx);
        __try {
            if (window->state != 0) {
                Log::Info("TradeMgr: OfferItem post-dispatch — state=0x%X, skipping", window->state);
                return;
            }
            OfferTradeItemNative fn = reinterpret_cast<OfferTradeItemNative>(Offsets::OfferTradeItem);
            Log::Info("TradeMgr: OfferItem post-dispatch calling native item=%u qty=%u ctx=0x%08X",
                      itemId, quantity, static_cast<unsigned>(ctx));
            fn(window, nullptr, itemId, quantity, 1);
            Log::Info("TradeMgr: OfferItem post-dispatch native returned");
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Log::Warn("TradeMgr: OfferItem post-dispatch faulted");
        }
    });
    return;

    // Dead code: native path disabled
    Log::Warn("TradeMgr: OfferItem native context unavailable (fn=0x%08X)",
              static_cast<unsigned>(Offsets::OfferTradeItem));
}

void OfferItemPacket(uint32_t itemId, uint32_t quantity) {
    if (quantity == 0) {
        quantity = 1;
    }
    // Keep the trade hack patch ENABLED during packet offer.
    // Restoring it (0x55) before the packet caused GW to crash —
    // the validation function at TradeHackPatch needs to be bypassed
    // for packet-based offers to work.
    if (s_tradeHackPatched) {
        Log::Info("TradeMgr: OfferItemPacket keeping trade patch enabled for packet offer");
    }
    const bool laneAvailable = CtoS::IsBotshubCommandLaneAvailable();
    Log::Info("TradeMgr: OfferItemPacket item=%u qty=%u laneAvailable=%d", itemId, quantity, laneAvailable ? 1 : 0);
    if (laneAvailable && CtoS::TradeOfferItemBotshub(itemId, quantity)) {
        return;
    }
    Log::Warn("TradeMgr: OfferItemPacket falling back to direct raw packet item=%u qty=%u", itemId, quantity);
    CtoS::TradeOfferItem(itemId, quantity);
}

static bool TryReadTradeWindowCandidate(uintptr_t candidate, ResolvedTradeWindowView* out) {
    if (candidate <= 0x10000 || !out) {
        return false;
    }
    __try {
        auto* window = reinterpret_cast<TradeWindowView*>(candidate);
        const uint32_t itemsMax = window->items_max;
        const uint32_t itemsCount = window->items_count;
        const uint32_t frameId = window->frame_id;
        const uint32_t state = window->state;
        if (itemsMax == 0 || itemsMax > 64) {
            return false;
        }
        if (itemsCount > itemsMax) {
            return false;
        }
        if (frameId == 0 || frameId > 5000) {
            return false;
        }
        if (state != 0u && state != 0x1000u) {
            return false;
        }
        out->ctx = candidate;
        out->state = state;
        out->items_count = itemsCount;
        out->items_max = itemsMax;
        out->frame_id = frameId;
        out->framePtr = UIMgr::GetFrameById(frameId);
        out->score = 7u;
        if (state == 0u) {
            out->score += 1u;
        }
        if (out->framePtr > 0x10000) {
            out->score += 3u;
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static void PromoteTradeWindowCandidate(
    uintptr_t candidate,
    uintptr_t sourceValue,
    const char* sourceLabel,
    uint32_t sourceOffset,
    uint32_t derefCount,
    ResolvedTradeWindowView* best) {
    if (!best) {
        return;
    }
    ResolvedTradeWindowView resolved{};
    if (!TryReadTradeWindowCandidate(candidate, &resolved)) {
        return;
    }
    resolved.sourceValue = sourceValue;
    resolved.sourceLabel = sourceLabel ? sourceLabel : "";
    resolved.sourceOffset = sourceOffset;
    resolved.derefCount = derefCount;
    const bool isBetter =
        resolved.score > best->score
        || (resolved.score == best->score && resolved.derefCount < best->derefCount)
        || (resolved.score == best->score && resolved.derefCount == best->derefCount && best->ctx <= 0x10000);
    if (isBetter) {
        *best = resolved;
    }
}

static void ProbeTradeWindowSourceValue(
    uintptr_t sourceValue,
    const char* sourceLabel,
    uint32_t sourceOffset,
    ResolvedTradeWindowView* best) {
    if (sourceValue <= 0x10000 || !best) {
        return;
    }

    PromoteTradeWindowCandidate(sourceValue, sourceValue, sourceLabel, sourceOffset, 0u, best);

    uintptr_t deref1 = 0;
    __try {
        deref1 = *reinterpret_cast<uintptr_t*>(sourceValue);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        deref1 = 0;
    }
    PromoteTradeWindowCandidate(deref1, sourceValue, sourceLabel, sourceOffset, 1u, best);

    uintptr_t deref2 = 0;
    if (deref1 > 0x10000) {
        __try {
            deref2 = *reinterpret_cast<uintptr_t*>(deref1);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            deref2 = 0;
        }
    }
    PromoteTradeWindowCandidate(deref2, sourceValue, sourceLabel, sourceOffset, 2u, best);
}

static void ProbeTradeWindowPointerTable(
    uintptr_t base,
    uint32_t beginOffset,
    uint32_t endOffset,
    const char* sourceLabel,
    ResolvedTradeWindowView* best) {
    if (base <= 0x10000 || !best || beginOffset > endOffset) {
        return;
    }
    for (uint32_t offset = beginOffset; offset <= endOffset; offset += sizeof(uintptr_t)) {
        uintptr_t sourceValue = 0;
        __try {
            sourceValue = *reinterpret_cast<uintptr_t*>(base + offset);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            sourceValue = 0;
        }
        ProbeTradeWindowSourceValue(sourceValue, sourceLabel, offset, best);
    }
}

static uintptr_t GetTradeWindowFromGameContext(bool logDiagnostic) {
    const uintptr_t capturedCtx = static_cast<uintptr_t>(InterlockedCompareExchange(&s_tradeWindowContext, 0, 0));
    const uintptr_t uiCtx = static_cast<uintptr_t>(GetTradeWindowUiContext());
    const uintptr_t gc = Offsets::ResolveGameContext();
    uintptr_t tradeCtx = 0;
    if (gc > 0x10000) {
        __try {
            tradeCtx = *reinterpret_cast<uintptr_t*>(gc + 0x58);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            tradeCtx = 0;
        }
    }

    ResolvedTradeWindowView best{};
    PromoteTradeWindowCandidate(capturedCtx, capturedCtx, "captured", 0u, 0u, &best);
    PromoteTradeWindowCandidate(uiCtx, uiCtx, "uiCtx", 0u, 0u, &best);
    ProbeTradeWindowPointerTable(gc, 0x58u, 0x88u, "gc", &best);
    ProbeTradeWindowPointerTable(tradeCtx, 0x00u, 0x80u, "tradeCtx", &best);

    if (logDiagnostic) {
        if (best.ctx > 0x10000) {
            Log::Info("TradeMgr: ResolveTradeWindow best ctx=0x%08X source=%s+0x%X deref=%u slot=0x%08X state=0x%X items_count=%u items_max=%u frame=0x%08X framePtr=0x%08X score=%u captured=0x%08X uiCtx=0x%08X gc=0x%08X tradeCtx=0x%08X",
                      static_cast<unsigned>(best.ctx),
                      best.sourceLabel ? best.sourceLabel : "",
                      best.sourceOffset,
                      best.derefCount,
                      static_cast<unsigned>(best.sourceValue),
                      best.state,
                      best.items_count,
                      best.items_max,
                      static_cast<unsigned>(best.frame_id),
                      static_cast<unsigned>(best.framePtr),
                      best.score,
                      static_cast<unsigned>(capturedCtx),
                      static_cast<unsigned>(uiCtx),
                      static_cast<unsigned>(gc),
                      static_cast<unsigned>(tradeCtx));
        } else {
            Log::Warn("TradeMgr: ResolveTradeWindow found no plausible candidate captured=0x%08X uiCtx=0x%08X gc=0x%08X tradeCtx=0x%08X",
                      static_cast<unsigned>(capturedCtx),
                      static_cast<unsigned>(uiCtx),
                      static_cast<unsigned>(gc),
                      static_cast<unsigned>(tradeCtx));
            if (gc > 0x10000) {
                for (uint32_t offset = 0x58u; offset <= 0x78u; offset += sizeof(uintptr_t)) {
                    uintptr_t sourceValue = 0;
                    __try {
                        sourceValue = *reinterpret_cast<uintptr_t*>(gc + offset);
                    } __except (EXCEPTION_EXECUTE_HANDLER) {
                        sourceValue = 0;
                    }
                    Log::Info("TradeMgr: ResolveTradeWindow raw gc+0x%X=0x%08X",
                              offset,
                              static_cast<unsigned>(sourceValue));
                }
            }
            if (tradeCtx > 0x10000) {
                for (uint32_t offset = 0x00u; offset <= 0x30u; offset += sizeof(uintptr_t)) {
                    uintptr_t sourceValue = 0;
                    __try {
                        sourceValue = *reinterpret_cast<uintptr_t*>(tradeCtx + offset);
                    } __except (EXCEPTION_EXECUTE_HANDLER) {
                        sourceValue = 0;
                    }
                    Log::Info("TradeMgr: ResolveTradeWindow raw tradeCtx+0x%X=0x%08X",
                              offset,
                              static_cast<unsigned>(sourceValue));
                }
            }
        }
    }

    return best.ctx;
}

static void OfferItemPromptQuantityAttempt(uint32_t itemId, uint32_t attemptsRemaining) {
    const bool logDiagnostic = attemptsRemaining >= 5u || attemptsRemaining == 0u;
    const uintptr_t ctx = GetTradeWindowFromGameContext(logDiagnostic);
    const uintptr_t capturedCtx = static_cast<uintptr_t>(InterlockedCompareExchange(&s_tradeWindowContext, 0, 0));
    const uintptr_t uiFrame = static_cast<uintptr_t>(GetTradeWindowUiFrame());
    const uintptr_t uiCtx = static_cast<uintptr_t>(GetTradeWindowUiContext());

    Log::Info("TradeMgr: OfferItemPromptQuantity request item=%u ctx=0x%08X capturedCtx=0x%08X uiFrame=0x%08X uiCtx=0x%08X fn=0x%08X onGameThread=%u retries=%u",
              itemId,
              static_cast<unsigned>(ctx),
              static_cast<unsigned>(capturedCtx),
              static_cast<unsigned>(uiFrame),
              static_cast<unsigned>(uiCtx),
              static_cast<unsigned>(Offsets::OfferTradeItem),
              GameThread::IsOnGameThread() ? 1u : 0u,
              attemptsRemaining);
    if (ctx <= 0x10000) {
        if (attemptsRemaining > 0u) {
            Log::Warn("TradeMgr: OfferItemPromptQuantity waiting for trade window context item=%u retries=%u",
                      itemId,
                      attemptsRemaining);
            QueueDelayedPromptOpenRetry(itemId, attemptsRemaining);
        } else {
            Log::Warn("TradeMgr: OfferItemPromptQuantity native context unavailable after retries");
            QueueDisableTradeCartHookOnce();
        }
        return;
    }

    auto* window = reinterpret_cast<TradeWindowView*>(ctx);
    __try {
        Log::Info("TradeMgr: OfferItemPromptQuantity precheck state=0x%X items_count=%u items_max=%u frame=0x%08X",
                  window->state,
                  window->items_count,
                  window->items_max,
                  static_cast<unsigned>(window->frame_id));
        if (window->items_max == 0 || window->items_max > 64 || window->frame_id == 0) {
            if (attemptsRemaining > 0u) {
                Log::Warn("TradeMgr: OfferItemPromptQuantity implausible window ctx=0x%08X state=0x%X items_max=%u frame=0x%08X; retrying",
                          static_cast<unsigned>(ctx),
                          window->state,
                          window->items_max,
                          static_cast<unsigned>(window->frame_id));
                QueueDelayedPromptOpenRetry(itemId, attemptsRemaining);
            } else {
                Log::Warn("TradeMgr: OfferItemPromptQuantity implausible window ctx=0x%08X state=0x%X items_max=%u frame=0x%08X",
                          static_cast<unsigned>(ctx),
                          window->state,
                          window->items_max,
                          static_cast<unsigned>(window->frame_id));
                QueueDisableTradeCartHookOnce();
            }
            return;
        }
        if (window->state != 0) {
            if (attemptsRemaining > 0u) {
                Log::Warn("TradeMgr: OfferItemPromptQuantity blocked; trade window state=0x%X, retrying", window->state);
                QueueDelayedPromptOpenRetry(itemId, attemptsRemaining);
            } else {
                Log::Warn("TradeMgr: OfferItemPromptQuantity blocked; trade window state=0x%X", window->state);
                QueueDisableTradeCartHookOnce();
            }
            return;
        }
        OfferTradeItemNative fn = reinterpret_cast<OfferTradeItemNative>(Offsets::OfferTradeItem);
        fn(window, nullptr, itemId, 0, 1);
        Log::Info("TradeMgr: OfferItemPromptQuantity native call returned");
        QueueDisableTradeCartHookOnce();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Warn("TradeMgr: OfferItemPromptQuantity faulted");
        QueueDisableTradeCartHookOnce();
    }
}

void OfferItemPromptQuantity(uint32_t itemId) {
    if (Offsets::OfferTradeItem <= 0x10000) {
        Log::Warn("TradeMgr: OfferItemPromptQuantity native fn unavailable");
        return;
    }
    if (!GameThread::IsInitialized()) {
        Log::Warn("TradeMgr: OfferItemPromptQuantity GameThread not initialized");
        return;
    }

    InterlockedExchange(&s_pendingOffer.itemId, 0);
    InterlockedExchange(&s_pendingOffer.quantity, 0);
    InterlockedExchange(&s_pendingOffer.armed, 0);

    Log::Info("TradeMgr: OfferItemPromptQuantity queueing post-dispatch native prompt-open item=%u", itemId);
    GameThread::EnqueuePost([itemId]() {
        OfferItemPromptQuantityAttempt(itemId, 5u);
    });
}

// Forward declarations for functions defined later in this file
bool ConfirmTradeQuantityPromptValue(uint32_t quantity);
bool ConfirmTradeQuantityPromptMax();

bool OfferItemPromptValue(uint32_t itemId, uint32_t quantity) {
    if (quantity == 0) {
        Log::Warn("TradeMgr: OfferItemPromptValue invalid quantity=0");
        return false;
    }
    if (!EnsureChooseQuantityPopupHook()) return false;
    InterlockedExchange(&s_pendingQuantityPromptClickCount, 0);
    InterlockedExchange(&s_pendingQuantityPromptItemId, static_cast<LONG>(itemId));
    InterlockedExchange(&s_pendingQuantityPromptValue, static_cast<LONG>(quantity));
    InterlockedExchange(&s_pendingQuantityPromptMode, static_cast<LONG>(QuantityPromptAutomationMode::ValueOffer));
    OfferItemPromptQuantity(itemId);
    if (IsChooseQuantityPopupPassiveOnly()) {
        Log::Info("TradeMgr: OfferItemPromptValue passive seam active; observing prompt open only item=%u quantity=%u seam=%s",
                  itemId,
                  quantity,
                  ChooseQuantityPopupHookSeamToString(s_chooseQuantityPopupHookSeam));
        if (!ObservePassiveTradeQuantityPrompt("OfferItemPromptValue", itemId)) {
            return false;
        }
        return ConfirmTradeQuantityPromptValue(quantity);
    }
    const LONG count = InterlockedCompareExchange(&s_pendingQuantityPromptClickCount, 0, 0);
    if (count > 0) {
        const bool clicked = DrainQueuedPromptClicks(s_pendingQuantityPromptClicks, static_cast<size_t>(count));
        if (clicked && WaitForPromptCallbackOfferResult(itemId, quantity, "OfferItemPromptValue")) {
            return true;
        }
    }
    // Fall back to manual prompt interaction if the callback path did not close the prompt.
    uintptr_t frame = WaitForTradeQuantityPromptFrame();
    if (frame < 0x10000) {
        Log::Warn("TradeMgr: OfferItemPromptValue no prompt frame found after callback fallback");
        return false;
    }
    return ConfirmTradeQuantityPromptValue(quantity);
}

bool OfferItemPromptMax(uint32_t itemId) {
    if (!EnsureChooseQuantityPopupHook()) return false;
    InterlockedExchange(&s_pendingQuantityPromptClickCount, 0);
    InterlockedExchange(&s_pendingQuantityPromptItemId, static_cast<LONG>(itemId));
    InterlockedExchange(&s_pendingQuantityPromptMode, static_cast<LONG>(QuantityPromptAutomationMode::MaxOffer));
    OfferItemPromptQuantity(itemId);
    if (IsChooseQuantityPopupPassiveOnly()) {
        Log::Info("TradeMgr: OfferItemPromptMax passive seam active; observing prompt open only item=%u seam=%s",
                  itemId,
                  ChooseQuantityPopupHookSeamToString(s_chooseQuantityPopupHookSeam));
        if (!ObservePassiveTradeQuantityPrompt("OfferItemPromptMax", itemId)) {
            return false;
        }
        Item* item = ItemMgr::GetItemById(itemId);
        if (item && item->quantity > 1u) {
            Log::Info("TradeMgr: OfferItemPromptMax passive seam rerouting to exact quantity confirmation item=%u quantity=%u",
                      itemId,
                      item->quantity);
            return ConfirmTradeQuantityPromptValue(item->quantity);
        }
        return ConfirmTradeQuantityPromptMax();
    }
    const LONG count = InterlockedCompareExchange(&s_pendingQuantityPromptClickCount, 0, 0);
    if (count <= 0) {
        Log::Warn("TradeMgr: OfferItemPromptMax no prompt clicks were captured");
        return false;
    }
    const bool clicked = DrainQueuedPromptClicks(s_pendingQuantityPromptClicks, static_cast<size_t>(count));
    Item* item = ItemMgr::GetItemById(itemId);
    const uint32_t expectedQuantity = (item && item->quantity > 0u) ? item->quantity : 1u;
    return clicked && WaitForPromptCallbackOfferResult(itemId, expectedQuantity, "OfferItemPromptMax");
}

bool OfferItemPromptDefault(uint32_t itemId) {
    if (!EnsureChooseQuantityPopupHook()) return false;
    InterlockedExchange(&s_pendingQuantityPromptClickCount, 0);
    InterlockedExchange(&s_pendingQuantityPromptItemId, static_cast<LONG>(itemId));
    InterlockedExchange(&s_pendingQuantityPromptMode, static_cast<LONG>(QuantityPromptAutomationMode::DefaultOffer));
    OfferItemPromptQuantity(itemId);
    if (IsChooseQuantityPopupPassiveOnly()) {
        Log::Info("TradeMgr: OfferItemPromptDefault passive seam active; observing prompt open only item=%u seam=%s",
                  itemId,
                  ChooseQuantityPopupHookSeamToString(s_chooseQuantityPopupHookSeam));
        if (!ObservePassiveTradeQuantityPrompt("OfferItemPromptDefault", itemId)) {
            return false;
        }
        return ConfirmTradeQuantityPromptValue(1u);
    }
    const LONG count = InterlockedCompareExchange(&s_pendingQuantityPromptClickCount, 0, 0);
    if (count > 0) {
        const bool clicked = DrainQueuedPromptClicks(s_pendingQuantityPromptClicks, static_cast<size_t>(count));
        if (clicked && WaitForPromptCallbackOfferResult(itemId, 1u, "OfferItemPromptDefault")) {
            return true;
        }
    }

    uintptr_t frame = WaitForTradeQuantityPromptFrame();
    if (frame < 0x10000) {
        Log::Warn("TradeMgr: OfferItemPromptDefault no prompt frame found after open");
        return false;
    }
    DebugDumpTradeQuantityPromptTree(frame, "trade-quantity-default");
    return ConfirmTradeQuantityPromptValue(1u);
}

bool ConfirmTradeQuantityPromptMax() {
    uintptr_t frame = FindTradeQuantityPromptFrame();
    if (frame < 0x10000) {
        Log::Warn("TradeMgr: ConfirmTradeQuantityPromptMax no prompt frame found");
        return false;
    }
    DebugDumpTradeQuantityPromptTree(frame, "trade-quantity-max-visible");
    Sleep(250);
    frame = FindTradeQuantityPromptFrame();
    if (frame < 0x10000) {
        Log::Warn("TradeMgr: ConfirmTradeQuantityPromptMax prompt disappeared during post-visible wait");
        return false;
    }
    DebugDumpTradeQuantityPromptTree(frame, "trade-quantity-max-ready");
    const uintptr_t maxCandidates[] = {
        ResolvePromptNestedChildByOffset(frame, kTradeQuantityPromptMaxChildOffsetId, 3u),
        ResolvePromptNestedChildByOffset(frame, kTradeQuantityPromptMaxChildOffsetId, 2u),
        ResolvePromptNestedChildByOffset(frame, kTradeQuantityPromptMaxChildOffsetId, 0u),
        UIMgr::GetChildFrameByOffset(frame, kTradeQuantityPromptMaxChildOffsetId),
    };
    const uintptr_t okCandidates[] = {
        ResolvePromptNestedChildByOffset(frame, 6u, 2u),
        ResolvePromptNestedChildByOffset(frame, 6u, 1u),
        UIMgr::GetChildFrameByOffset(frame, kTradeQuantityPromptOkChildOffsetId),
    };

    for (size_t maxIndex = 0; maxIndex < _countof(maxCandidates); ++maxIndex) {
        frame = FindTradeQuantityPromptFrame();
        if (frame < 0x10000) {
            Log::Warn("TradeMgr: ConfirmTradeQuantityPromptMax prompt disappeared before max candidate %u", static_cast<unsigned>(maxIndex));
            return false;
        }

        const uintptr_t maxBtn = ChoosePromptClickFrame(&maxCandidates[maxIndex], 1);
        if (maxBtn < 0x10000) continue;

        const bool clickedMax = ClickTradeQuantityPromptButton(maxBtn, "max");
        Log::Info("TradeMgr: ConfirmTradeQuantityPromptMax max candidate=%u frame=0x%08X frameId=%u clicked=%u",
                  static_cast<unsigned>(maxIndex),
                  static_cast<unsigned>(maxBtn),
                  UIMgr::GetFrameId(maxBtn),
                  clickedMax ? 1u : 0u);
        if (!clickedMax) continue;

        frame = FindTradeQuantityPromptFrame();
        if (frame > 0x10000) {
            DebugDumpTradeQuantityPromptTree(frame, "trade-quantity-max-after-max");
        }

        for (size_t okIndex = 0; okIndex < _countof(okCandidates); ++okIndex) {
            frame = FindTradeQuantityPromptFrame();
            if (frame < 0x10000) {
                Log::Info("TradeMgr: ConfirmTradeQuantityPromptMax prompt closed after max candidate=%u before ok candidate=%u",
                          static_cast<unsigned>(maxIndex),
                          static_cast<unsigned>(okIndex));
                return true;
            }

            const uintptr_t okBtn = ChoosePromptClickFrame(&okCandidates[okIndex], 1);
            if (okBtn < 0x10000) continue;

            const bool clickedOk = ClickTradeQuantityPromptButton(okBtn, "ok");
            Log::Info("TradeMgr: ConfirmTradeQuantityPromptMax ok candidate=%u frame=0x%08X frameId=%u clicked=%u",
                      static_cast<unsigned>(okIndex),
                      static_cast<unsigned>(okBtn),
                      UIMgr::GetFrameId(okBtn),
                      clickedOk ? 1u : 0u);
            if (!clickedOk) continue;

            if (!IsTradeQuantityPromptOpen()) {
                Log::Info("TradeMgr: ConfirmTradeQuantityPromptMax succeeded with max candidate=%u ok candidate=%u",
                          static_cast<unsigned>(maxIndex),
                          static_cast<unsigned>(okIndex));
                return true;
            }

            frame = FindTradeQuantityPromptFrame();
            if (frame > 0x10000) {
                DebugDumpTradeQuantityPromptTree(frame, "trade-quantity-max-after-ok");
            }
        }
    }

    Log::Warn("TradeMgr: ConfirmTradeQuantityPromptMax exhausted prompt button combinations without closing prompt");
    return false;
}

bool ConfirmTradeQuantityPromptValue(uint32_t quantity) {
    if (quantity == 0) {
        Log::Warn("TradeMgr: ConfirmTradeQuantityPromptValue invalid quantity=0");
        return false;
    }

    uintptr_t frame = FindTradeQuantityPromptFrame();
    if (frame < 0x10000) {
        Log::Warn("TradeMgr: ConfirmTradeQuantityPromptValue no prompt frame found");
        return false;
    }
    DebugDumpTradeQuantityPromptTree(frame, "trade-quantity-value");
    DebugDumpTradeQuantityPromptCallbacks(frame, "trade-quantity-value");

    uint32_t backingQty = 0;
    const bool wroteBackingCount = TryWriteTradeQuantityPromptBackingCount(frame, quantity, "initial", &backingQty);

    if (quantity > 1u && TryPromptSpinnerAdjust(frame, quantity)) {
        uintptr_t okButtons[4] = {};
        const size_t okButtonCount = CollectPromptOkButtons(frame, okButtons, _countof(okButtons));
        for (size_t okIndex = 0; okIndex < okButtonCount; ++okIndex) {
            const uintptr_t okBtn = okButtons[okIndex];
            bool clickedOk = UIMgr::ButtonClick(okBtn);
            if (!clickedOk) {
                clickedOk = UIMgr::ButtonClickImmediateFull(okBtn);
            }
            Log::Info("TradeMgr: ConfirmTradeQuantityPromptValue spinner path okIndex=%u okFrame=0x%08X okFrameId=%u clicked=%u quantity=%u",
                      static_cast<unsigned>(okIndex),
                      static_cast<unsigned>(okBtn),
                      UIMgr::GetFrameId(okBtn),
                      clickedOk ? 1u : 0u,
                      quantity);
            if (!clickedOk) continue;
            Sleep(200);
            if (!IsTradeQuantityPromptOpen()) {
                Log::Info("TradeMgr: ConfirmTradeQuantityPromptValue spinner path closed prompt quantity=%u", quantity);
                return true;
            }
        }
    }

    const uintptr_t directValueFrame = UIMgr::GetChildFrameByOffset(frame, kTradeQuantityPromptValueChildOffsetId);
    if (directValueFrame > 0x10000 && EnterPromptQuantityByKeypress(directValueFrame, quantity)) {
        frame = FindTradeQuantityPromptFrame();
        if (frame < 0x10000) {
            Log::Info("TradeMgr: ConfirmTradeQuantityPromptValue prompt closed immediately after keypress quantity=%u", quantity);
            return true;
        }
        if (frame > 0x10000) {
            TryWriteTradeQuantityPromptBackingCount(frame, quantity, "keypress");
        }
        if (TryFinalizeTradeQuantityPromptByEnter(frame, directValueFrame, "keypress")) {
            Log::Info("TradeMgr: ConfirmTradeQuantityPromptValue Enter finalized keypress path quantity=%u", quantity);
            return true;
        }

        uintptr_t okButtons[4] = {};
        const size_t okButtonCount = CollectPromptOkButtons(frame, okButtons, _countof(okButtons));
        for (size_t okIndex = 0; okIndex < okButtonCount; ++okIndex) {
            const uintptr_t okBtn = okButtons[okIndex];
            const QueuedFrameClick click[] = {
                {UIMgr::GetFrameId(okBtn), GetTickCount()}
            };
            bool clickedOk = DrainQueuedPromptClicks(click, _countof(click));
            if (!clickedOk) {
                clickedOk = UIMgr::ButtonClick(okBtn);
            }
            Log::Info("TradeMgr: ConfirmTradeQuantityPromptValue keypress path okIndex=%u okFrame=0x%08X okFrameId=%u clicked=%u quantity=%u",
                      static_cast<unsigned>(okIndex),
                      static_cast<unsigned>(okBtn),
                      click[0].frame_id,
                      clickedOk ? 1u : 0u,
                      quantity);
            if (!clickedOk) continue;
            Sleep(200);
            if (!IsTradeQuantityPromptOpen()) {
                Log::Info("TradeMgr: ConfirmTradeQuantityPromptValue keypress path closed prompt quantity=%u", quantity);
                return true;
            }
        }
    }

    if (wroteBackingCount) {
        frame = FindTradeQuantityPromptFrame();
        if (frame < 0x10000) {
            Log::Info("TradeMgr: ConfirmTradeQuantityPromptValue prompt closed after backing-count write quantity=%u applied=%u",
                      quantity,
                      backingQty);
            return true;
        }
        if (frame > 0x10000 && TryFinalizeTradeQuantityPromptByEnter(frame, directValueFrame, "backing-count")) {
            Log::Info("TradeMgr: ConfirmTradeQuantityPromptValue Enter finalized backing-count path quantity=%u applied=%u",
                      quantity,
                      backingQty);
            return true;
        }
    }

    wchar_t quantityBuf[16] = {};
    _snwprintf_s(quantityBuf, _countof(quantityBuf), _TRUNCATE, L"%u", quantity);
    PromptValueAttempt attempts[8] = {};
    const size_t attemptCount = CollectPromptValueAttempts(frame, attempts, _countof(attempts));
    if (attemptCount == 0) {
        Log::Warn("TradeMgr: ConfirmTradeQuantityPromptValue found no value/ok candidate set for prompt=0x%08X",
                  static_cast<unsigned>(frame));
        return false;
    }
    for (size_t attemptIndex = 0; attemptIndex < attemptCount; ++attemptIndex) {
        const auto& attempt = attempts[attemptIndex];
        frame = FindTradeQuantityPromptFrame();
        if (frame < 0x10000) {
            Log::Warn("TradeMgr: ConfirmTradeQuantityPromptValue prompt disappeared before candidate %u label=%s mode=%s",
                      static_cast<unsigned>(attemptIndex), attempt.label, attempt.mode);
            return false;
        }

        uintptr_t valueFrame = attempt.valueFrame;
        uintptr_t commitParentFrame = attempt.commitParentFrame;
        uintptr_t okButtons[4] = {};
        const size_t okButtonCount = CollectPromptOkButtons(frame, okButtons, _countof(okButtons));
        Log::Info("TradeMgr: ConfirmTradeQuantityPromptValue frame=0x%08X childCount=%u candidate=%u label=%s mode=%s valueFrame=0x%08X commitParent=0x%08X okButtonCount=%u quantity=%u",
                  static_cast<unsigned>(frame),
                  UIMgr::GetChildFrameCount(frame),
                  static_cast<unsigned>(attemptIndex),
                  attempt.label,
                  attempt.mode,
                  static_cast<unsigned>(valueFrame),
                  static_cast<unsigned>(commitParentFrame),
                  static_cast<unsigned>(okButtonCount),
                  quantity);
        if (valueFrame < 0x10000 || commitParentFrame < 0x10000 || okButtonCount == 0) {
            continue;
        }

        if (attempt.focusFrame > 0x10000) {
            const bool focused = UIMgr::ButtonClick(attempt.focusFrame);
            Log::Info("TradeMgr: ConfirmTradeQuantityPromptValue candidate=%u label=%s focusFrame=0x%08X frameId=%u focused=%u",
                      static_cast<unsigned>(attemptIndex),
                      attempt.label,
                      static_cast<unsigned>(attempt.focusFrame),
                      UIMgr::GetFrameId(attempt.focusFrame),
                      focused ? 1u : 0u);
            Sleep(80);
        }

        TryPromptMouseQuantityVariants(valueFrame, quantity);

        bool setOk = false;
        if (strcmp(attempt.mode, "numeric_local") == 0) {
            setOk = UIMgr::SetNumericFrameLocalOnly(valueFrame, quantity);
        } else if (strcmp(attempt.mode, "editable_local") == 0) {
            setOk = UIMgr::SetEditableTextLocalOnly(valueFrame, quantityBuf);
        } else if (strcmp(attempt.mode, "numeric") == 0) {
            setOk = UIMgr::SetNumericFrameValue(valueFrame, quantity, commitParentFrame);
        } else {
            setOk = UIMgr::SetEditableTextValue(valueFrame, quantityBuf, commitParentFrame);
        }
        if (!setOk) {
            Log::Warn("TradeMgr: ConfirmTradeQuantityPromptValue failed setting value '%S' on candidate %u label=%s mode=%s",
                      quantityBuf, static_cast<unsigned>(attemptIndex), attempt.label, attempt.mode);
            continue;
        }

        Sleep(120);

        frame = FindTradeQuantityPromptFrame();
        if (frame < 0x10000) {
            Log::Info("TradeMgr: ConfirmTradeQuantityPromptValue prompt disappeared after text set on candidate %u label=%s mode=%s",
                      static_cast<unsigned>(attemptIndex), attempt.label, attempt.mode);
            return true;
        }
        const size_t refreshedOkButtonCount = CollectPromptOkButtons(frame, okButtons, _countof(okButtons));
        if (refreshedOkButtonCount == 0) {
            Log::Warn("TradeMgr: ConfirmTradeQuantityPromptValue Offer button missing after candidate %u label=%s mode=%s text set",
                      static_cast<unsigned>(attemptIndex), attempt.label, attempt.mode);
            continue;
        }
        uint32_t candidateAppliedQty = 0;
        const bool wroteCandidateBackingCount = TryWriteTradeQuantityPromptBackingCount(frame,
                                                                                         quantity,
                                                                                         attempt.label,
                                                                                         &candidateAppliedQty);
        if (TryFinalizeTradeQuantityPromptByEnter(frame, valueFrame, attempt.label)) {
            Log::Info("TradeMgr: ConfirmTradeQuantityPromptValue Enter finalized candidate %u label=%s mode=%s applied=%u wroteBacking=%u",
                      static_cast<unsigned>(attemptIndex),
                      attempt.label,
                      attempt.mode,
                      candidateAppliedQty,
                      wroteCandidateBackingCount ? 1u : 0u);
            return true;
        }

        for (size_t okIndex = 0; okIndex < refreshedOkButtonCount; ++okIndex) {
            const uintptr_t okBtn = okButtons[okIndex];
            const QueuedFrameClick clicks[] = {
                {UIMgr::GetFrameId(okBtn), GetTickCount()}
            };
            bool clickedOk = DrainQueuedPromptClicks(clicks, _countof(clicks));
            if (!clickedOk) {
                clickedOk = UIMgr::ButtonClick(okBtn);
            }
            Log::Info("TradeMgr: ConfirmTradeQuantityPromptValue candidate=%u label=%s mode=%s okIndex=%u okFrame=0x%08X okFrameId=%u clicked=%u quantity='%S'",
                      static_cast<unsigned>(attemptIndex),
                      attempt.label,
                      attempt.mode,
                      static_cast<unsigned>(okIndex),
                      static_cast<unsigned>(okBtn),
                      clicks[0].frame_id,
                      clickedOk ? 1u : 0u,
                      quantityBuf);
            if (!clickedOk) {
                continue;
            }

            Sleep(200);
            if (!IsTradeQuantityPromptOpen()) {
                Log::Info("TradeMgr: ConfirmTradeQuantityPromptValue prompt closed on candidate %u label=%s mode=%s okIndex=%u",
                          static_cast<unsigned>(attemptIndex), attempt.label, attempt.mode, static_cast<unsigned>(okIndex));
                return true;
            }
        }
    }

    const uint32_t pendingItemId = static_cast<uint32_t>(InterlockedCompareExchange(&s_pendingQuantityPromptItemId, 0, 0));
    uint32_t observedQuantity = 0;
    if (pendingItemId != 0
        && ReadTradePlayerOfferQuantity(pendingItemId, &observedQuantity)
        && observedQuantity >= quantity) {
        frame = FindTradeQuantityPromptFrame();
        Log::Info("TradeMgr: ConfirmTradeQuantityPromptValue treating stale prompt as satisfied item=%u requested=%u observed=%u promptFrame=0x%08X childCount=%u",
                  pendingItemId,
                  quantity,
                  observedQuantity,
                  static_cast<unsigned>(frame),
                  frame > 0x10000 ? UIMgr::GetChildFrameCount(frame) : 0u);
        return true;
    }

    Log::Warn("TradeMgr: ConfirmTradeQuantityPromptValue no candidate closed the prompt for quantity '%S'",
              quantityBuf);
    return false;
}

static void SubmitOfferImpl(uint32_t gold, uint32_t viewRetryCount) {
    if (viewRetryCount > 0u && ClickTradeWindowViewIfNeeded("submit_trade_offer_view", viewRetryCount)) {
        Log::Info("TradeMgr: SubmitOffer clicked view button; scheduling retry gold=%u retries=%u",
                  gold,
                  viewRetryCount);
        QueueDelayedSubmitRetry(gold, viewRetryCount);
        return;
    }
    if (gold == 0u && ClickTradeWindowRootButton(kTradeWindowSubmitButtonHash, "submit_trade_offer")) {
        Log::Info("TradeMgr: SubmitOffer queued via trade-window submit button gold=%u", gold);
        return;
    }
    if (Offsets::TradeSendOffer > 0x10000) {
        TradeDoActionNative fn = reinterpret_cast<TradeDoActionNative>(Offsets::TradeSendOffer);
        Log::Info("TradeMgr: SubmitOffer native call fn=0x%08X gold=%u",
                  static_cast<unsigned>(Offsets::TradeSendOffer), gold);
        fn(gold);
        Log::Info("TradeMgr: SubmitOffer native call returned");
        return;
    }
    Log::Info("TradeMgr: SubmitOffer raw packet fallback gold=%u", gold);
    CtoS::SendPacket(2, Packets::TRADE_SUBMIT_OFFER, gold);
}

void SubmitOffer(uint32_t gold) {
    SubmitOfferImpl(gold, 3u);
}

void ChangeOffer() {
    if (Offsets::TradeCancelOffer > 0x10000) {
        TradeVoidNative fn = reinterpret_cast<TradeVoidNative>(Offsets::TradeCancelOffer);
        Log::Info("TradeMgr: ChangeOffer native call fn=0x%08X",
                  static_cast<unsigned>(Offsets::TradeCancelOffer));
        fn();
        Log::Info("TradeMgr: ChangeOffer native call returned");
        return;
    }
    Log::Info("TradeMgr: ChangeOffer raw packet fallback");
    CtoS::SendPacket(1, Packets::TRADE_CHANGE_OFFER);
}

void RemoveItem(uint32_t slotOrItemId) {
    if (Offsets::TradeRemoveItem > 0x10000) {
        TradeDoActionNative fn = reinterpret_cast<TradeDoActionNative>(Offsets::TradeRemoveItem);
        fn(slotOrItemId);
        return;
    }
    Log::Warn("TradeMgr: TradeRemoveItem unresolved; using ChangeOffer fallback instead of raw remove packet (arg=%u)",
              slotOrItemId);
    ChangeOffer();
}

bool BuyMaterials(uint32_t modelId, uint32_t quantity) {
    if (modelId == 0 || quantity == 0) return false;
    if (!Offsets::RequestQuote || !Offsets::Transaction) return false;
    if (!TraderHook::IsInitialized() && !TraderHook::Initialize()) return false;
    if (!GameThread::IsInitialized() || GameThread::IsOnGameThread()) {
        Log::Warn("TradeMgr: BuyMaterials requires caller off the game thread model=%u qty=%u gt_init=%u on_gt=%u",
                  modelId, quantity, GameThread::IsInitialized() ? 1u : 0u, GameThread::IsOnGameThread() ? 1u : 0u);
        return false;
    }

    const uint32_t merchantCount = GetMerchantItemCount();
    if (merchantCount == 0) {
        Log::Warn("TradeMgr: BuyMaterials called with no merchant inventory open model=%u qty=%u", modelId, quantity);
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
            Log::Warn("TradeMgr: BuyMaterials could not resolve merchant item for model=%u pack=%u/%u",
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
            Log::Warn("TradeMgr: BuyMaterials quote failed model=%u pack=%u/%u quotedItem=%u quotedCost=%u",
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
            Log::Warn("TradeMgr: BuyMaterials transact did not change gold model=%u pack=%u/%u item=%u cost=%u goldBefore=%u goldAfter=%u",
                      modelId, pack + 1u, packs, quotedItemId, quotedCost,
                      goldBefore, ItemMgr::GetGoldCharacter());
            break;
        }

        ++boughtPacks;
        Sleep(ChatMgr::GetPing() + 250);
    }

    Log::Info("TradeMgr: BuyMaterials native flow model=%u requestedQty=%u requestedPacks=%u boughtPacks=%u",
              modelId, quantity, packs, boughtPacks);
    return boughtPacks > 0;
}

bool SellMaterialsToTrader(uint32_t itemId, uint32_t transactions) {
    if (itemId == 0 || transactions == 0) return false;
    if (!Offsets::RequestQuote || !Offsets::Transaction) return false;
    if (!TraderHook::IsInitialized() && !TraderHook::Initialize()) return false;
    if (!GameThread::IsInitialized() || GameThread::IsOnGameThread()) {
        Log::Warn("TradeMgr: SellMaterialsToTrader requires caller off the game thread item=%u tx=%u gt_init=%u on_gt=%u",
                  itemId, transactions, GameThread::IsInitialized() ? 1u : 0u, GameThread::IsOnGameThread() ? 1u : 0u);
        return false;
    }
    if (GetMerchantItemCount() == 0) {
        Log::Warn("TradeMgr: SellMaterialsToTrader called with no trader inventory open item=%u tx=%u",
                  itemId, transactions);
        return false;
    }

    Item* inventoryItem = ItemMgr::GetItemById(itemId);
    const uint32_t modelId = inventoryItem ? inventoryItem->model_id : 0u;
    if (!inventoryItem || modelId == 0) {
        Log::Warn("TradeMgr: SellMaterialsToTrader could not resolve inventory item=%u model=%u",
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
            Log::Warn("TradeMgr: SellMaterialsToTrader quote failed inventory=%u model=%u tx=%u/%u quoteObserved=%u quotedItem=%u quotedValue=%u",
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
            Log::Warn("TradeMgr: SellMaterialsToTrader transact did not change gold inventory=%u model=%u tx=%u/%u quotedItem=%u value=%u goldBefore=%u goldAfter=%u",
                      itemId, modelId, tx + 1u, transactions, quotedItemId, quotedValue,
                      goldBefore, ItemMgr::GetGoldCharacter());
            break;
        }

        ++soldCount;
        Sleep(ChatMgr::GetPing() + 250);
    }

    Log::Info("TradeMgr: SellMaterialsToTrader native flow item=%u requestedTx=%u soldTx=%u",
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
        Log::Warn("TradeMgr: CraftMerchantItem early reject Transaction=0x%08X GameThread=%u item=%u qty=%u gold=%u",
                  static_cast<unsigned>(Offsets::Transaction), GameThread::IsInitialized() ? 1u : 0u,
                  itemId, quantity, totalValue);
        return false;
    }
    if (!materialModelIds || !materialQuantities || materialCount == 0 || materialCount > kCrafterMaxMaterials) {
        Log::Warn("TradeMgr: CraftMerchantItem material args invalid mats=%p qtys=%p count=%u",
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
            Log::Warn("TradeMgr: CraftMerchantItem missing material model=%u qty=%u", modelId, needed);
            return false;
        }
        task.material_item_ids[i] = item->item_id;
        task.material_quantities[i] = needed;
    }

    Log::Info("TradeMgr: CraftMerchantItem dispatching via GameThread::EnqueueRaw (item=%u qty=%u gold=%u mats=%u matItemIds=[%u,%u])",
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

} // namespace GWA3::TradeMgr
