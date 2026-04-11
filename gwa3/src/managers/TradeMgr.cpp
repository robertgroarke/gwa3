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
#include <gwa3/managers/ItemMgr.h>
#include <gwa3/managers/UIMgr.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>

#include <MinHook.h>
#include <Windows.h>
#include <cstring>

namespace GWA3::TradeMgr {

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
static volatile LONG s_tradeWindowContext = 0;
static volatile LONG s_tradeWindowFrame = 0;
static volatile LONG s_tradeWindowCaptureCount = 0;
static uint8_t s_tradeHackOriginalByte = 0;
static bool s_tradeHackOriginalByteKnown = false;
static bool s_tradeHackPatched = false;
static uintptr_t s_partyButtonHookAddr = 0;
static uintptr_t s_partyButtonReturnAddr = 0;
static uintptr_t s_partyButtonTrampoline = 0;
static uint8_t s_partyButtonSavedBytes[kTradeHookPatchSize] = {};
static GWA3::HookEntry s_tradeUiEntry{nullptr};
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
enum class QuantityPromptAutomationMode : uint32_t {
    None = 0,
    DefaultOffer = 1,
    MaxOffer = 2,
    ValueOffer = 3
};
static volatile LONG s_pendingQuantityPromptMode = static_cast<LONG>(QuantityPromptAutomationMode::None);
static volatile LONG s_pendingQuantityPromptValue = 0;
static QueuedFrameClick s_pendingQuantityPromptClicks[2] = {};
static volatile LONG s_pendingQuantityPromptClickCount = 0;
// Temporary isolation switch for player-trade crash debugging.
// Merchant/native transaction coverage does not need the trade-cart hook.
static constexpr bool kDisableTradeCartHookForDebug = true;
static constexpr uint32_t kUiTradePlayerUpdated = 0x10000105u;
static constexpr uint32_t kUiTradeSessionStart = 0x10000165u;
static constexpr uint32_t kUiTradeSessionUpdated = 0x1000016Bu;
static constexpr uint32_t kUiInitiateTrade = UIMgr::MSG_INITIATE_TRADE;
static constexpr uint32_t kUiSendMerchantRequestQuote = 0x30000006u;
static constexpr uint32_t kUiSendMerchantTransactItem = 0x30000007u;
static constexpr uint32_t kTradeButtonFrameHash = 3422277079u;
static constexpr uint32_t kTradeButtonActionPrimaryChildOffsetId = 123u;
static constexpr uint32_t kTradeButtonActionSecondaryChildOffsetId = 122u;
static constexpr uint32_t kTradeButtonAltHash = 1687064728u;
static constexpr uint32_t kTradeButtonAltChildOffsetId = 126u;
static constexpr uint32_t kTradeWindowFrameHash = 3198579276u;
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
static ChooseQuantityPopupNative s_chooseQuantityPopupOriginal = nullptr;

static __declspec(naked) void PartyWindowButtonDetourNaked() {
    __asm {
        pushfd
        pushad

        inc dword ptr [s_partyButtonCallbackHitCount]
        mov eax, dword ptr [esp + 24]
        mov dword ptr [s_partyButtonCallbackLastThis], eax
        mov eax, dword ptr [esp + 40]
        mov dword ptr [s_partyButtonCallbackLastArg], eax

        popad
        popfd
        jmp [s_partyButtonTrampoline]
    }
}

static bool InstallPartyWindowButtonHook() {
    if (s_partyButtonTrampoline) return true;
    if (Offsets::PartyWindowButtonCallback <= 0x10000) return false;

    s_partyButtonHookAddr = Offsets::PartyWindowButtonCallback;
    s_partyButtonReturnAddr = s_partyButtonHookAddr + kTradeHookPatchSize;
    memcpy(s_partyButtonSavedBytes, reinterpret_cast<void*>(s_partyButtonHookAddr), kTradeHookPatchSize);

    s_partyButtonTrampoline = reinterpret_cast<uintptr_t>(
        VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!s_partyButtonTrampoline) {
        Log::Warn("TradeMgr: PartyWindowButtonCallback trampoline alloc failed");
        return false;
    }

    memcpy(reinterpret_cast<void*>(s_partyButtonTrampoline), s_partyButtonSavedBytes, kTradeHookPatchSize);
    const uintptr_t trampJump = s_partyButtonTrampoline + kTradeHookPatchSize;
    *reinterpret_cast<uint8_t*>(trampJump) = 0xE9;
    const int32_t trampRel = static_cast<int32_t>(s_partyButtonReturnAddr - (trampJump + 5));
    memcpy(reinterpret_cast<void*>(trampJump + 1), &trampRel, sizeof(trampRel));

    DWORD oldProtect = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(s_partyButtonHookAddr), kTradeHookPatchSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        VirtualFree(reinterpret_cast<void*>(s_partyButtonTrampoline), 0, MEM_RELEASE);
        s_partyButtonTrampoline = 0;
        Log::Warn("TradeMgr: PartyWindowButtonCallback VirtualProtect failed");
        return false;
    }

    uint8_t patch[kTradeHookPatchSize];
    patch[0] = 0xE9;
    const int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&PartyWindowButtonDetourNaked) - (s_partyButtonHookAddr + 5));
    memcpy(patch + 1, &rel, sizeof(rel));
    memcpy(reinterpret_cast<void*>(s_partyButtonHookAddr), patch, kTradeHookPatchSize);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(s_partyButtonHookAddr), kTradeHookPatchSize);
    VirtualProtect(reinterpret_cast<void*>(s_partyButtonHookAddr), kTradeHookPatchSize, oldProtect, &oldProtect);

    Log::Info("TradeMgr: PartyWindowButtonCallback hook installed at 0x%08X -> trampoline 0x%08X",
              static_cast<unsigned>(s_partyButtonHookAddr),
              static_cast<unsigned>(s_partyButtonTrampoline));
    return true;
}

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
        __try {
            fieldBC = *reinterpret_cast<uint32_t*>(child + 0xBC);
            fieldC0 = *reinterpret_cast<uint32_t*>(child + 0xC0);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            fieldBC = 0;
            fieldC0 = 0;
        }
        Log::Info("TradeMgr: Quantity prompt child offset=%u frame=0x%08X frameId=%u fieldBC=%u fieldC0=%u",
                  offset,
                  static_cast<unsigned>(child),
                  UIMgr::GetFrameId(child),
                  fieldBC,
                  fieldC0);
        char childLabel[64] = {};
        sprintf_s(childLabel, "%s[%u]", label ? label : "trade-quantity-root", offset);
        UIMgr::DebugDumpChildFrames(child, childLabel, 12);
    }
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

static void ResetTradeUiTap() {
    s_tradeUiPlayerUpdatedCount = 0;
    s_tradeUiInitiateCount = 0;
    s_tradeUiSessionStartCount = 0;
    s_tradeUiSessionUpdatedCount = 0;
    s_tradeUiLastInitiateWParam = 0;
    s_tradeUiLastSessionStartState = 0;
    s_tradeUiLastSessionStartPlayerNumber = 0;
}

static void EnsureTradeUiTap() {
    static bool s_registered = false;
    if (s_registered) return;
    if (!CallbackRegistry::Initialize()) {
        Log::Warn("TradeMgr: CallbackRegistry unavailable for trade UI tap");
        return;
    }
    ResetTradeUiTap();
    CallbackRegistry::RegisterUIMessageCallback(
        &s_tradeUiEntry, kUiTradePlayerUpdated,
        [](GWA3::HookStatus*, uint32_t, void* wparam, void*) {
            const uint32_t count = static_cast<uint32_t>(InterlockedIncrement(&s_tradeUiPlayerUpdatedCount));
            Log::Info("TradeMgr: UI kTradePlayerUpdated hit=%u wparam=0x%08X",
                      count, static_cast<unsigned>(reinterpret_cast<uintptr_t>(wparam)));
        }, -1);
    CallbackRegistry::RegisterUIMessageCallback(
        &s_tradeUiEntry, kUiInitiateTrade,
        [](GWA3::HookStatus*, uint32_t, void* wparam, void*) {
            const uint32_t arg = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(wparam));
            InterlockedExchange(&s_tradeUiLastInitiateWParam, static_cast<LONG>(arg));
            const uint32_t count = static_cast<uint32_t>(InterlockedIncrement(&s_tradeUiInitiateCount));
            Log::Info("TradeMgr: UI kInitiateTrade hit=%u wparam=0x%08X",
                      count, arg);
        }, -1);
    CallbackRegistry::RegisterUIMessageCallback(
        &s_tradeUiEntry, kUiTradeSessionStart,
        [](GWA3::HookStatus*, uint32_t, void* wparam, void*) {
            uint32_t state = 0;
            uint32_t playerNumber = 0;
            __try {
                if (wparam) {
                    const auto* payload = reinterpret_cast<const TradeSessionStartPayload*>(wparam);
                    state = payload->trade_state;
                    playerNumber = payload->player_number;
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                state = 0;
                playerNumber = 0;
            }
            InterlockedExchange(&s_tradeUiLastSessionStartState, static_cast<LONG>(state));
            InterlockedExchange(&s_tradeUiLastSessionStartPlayerNumber, static_cast<LONG>(playerNumber));
            const uint32_t count = static_cast<uint32_t>(InterlockedIncrement(&s_tradeUiSessionStartCount));
            Log::Info("TradeMgr: UI kTradeSessionStart hit=%u state=%u playerNumber=%u wparam=0x%08X",
                      count, state, playerNumber, static_cast<unsigned>(reinterpret_cast<uintptr_t>(wparam)));
        }, -1);
    CallbackRegistry::RegisterUIMessageCallback(
        &s_tradeUiEntry, kUiTradeSessionUpdated,
        [](GWA3::HookStatus*, uint32_t, void* wparam, void*) {
            const uint32_t count = static_cast<uint32_t>(InterlockedIncrement(&s_tradeUiSessionUpdatedCount));
            Log::Info("TradeMgr: UI kTradeSessionUpdated hit=%u wparam=0x%08X",
                      count, static_cast<unsigned>(reinterpret_cast<uintptr_t>(wparam)));
        }, -1);
    s_registered = true;
    Log::Info("TradeMgr: Trade UI tap registered (initiate=0x%08X playerUpdated=0x%08X sessionStart=0x%08X sessionUpdated=0x%08X)",
              kUiInitiateTrade, kUiTradePlayerUpdated, kUiTradeSessionStart, kUiTradeSessionUpdated);
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
    __try {
        auto** address = reinterpret_cast<uintptr_t**>(eaxValue);
        address += 2;
        if (!address || !*address) return;
        address = reinterpret_cast<uintptr_t**>(*address);
        if (!address || !*address) return;
        auto* window = reinterpret_cast<TradeWindowView*>(*address);
        if (!window) return;
        s_tradeWindowContext = static_cast<LONG>(reinterpret_cast<uintptr_t>(window));
        s_tradeWindowFrame = static_cast<LONG>(window->frame_id);
        const LONG captureCount = InterlockedIncrement(&s_tradeWindowCaptureCount);
        if (captureCount <= 10) {
            Log::Info("TradeMgr: CaptureTradeWindowContext[%ld] eax=0x%08X ctx=0x%08X frame=0x%08X state=0x%X items_count=%u items_max=%u",
                      captureCount,
                      static_cast<unsigned>(eaxValue),
                      static_cast<unsigned>(reinterpret_cast<uintptr_t>(window)),
                      static_cast<unsigned>(window->frame_id),
                      window->state,
                      window->items_count,
                      window->items_max);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        s_tradeWindowContext = 0;
        s_tradeWindowFrame = 0;
    }
}

static void __cdecl OnUpdateTradeCart(void* eaxValue, void* a1, void* a2) {
    CaptureTradeWindowContext(reinterpret_cast<uintptr_t>(eaxValue));
    if (s_updateTradeCartOriginal) {
        s_updateTradeCartOriginal(eaxValue, a1, a2);
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
    return true;
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
    EnsureTradeUiTap();
    if (!InstallPartyWindowButtonHook()) {
        Log::Warn("TradeMgr: PartyWindowButtonCallback hook unavailable");
    }
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

static uintptr_t FindTradeQuantityPromptFrame() {
    const uintptr_t tradeWindowFrame = GetTradeWindowUiFrame();
    uintptr_t frame = UIMgr::GetVisibleFrameByChildOffsetAndChildCount(
        kTradeQuantityPromptChildOffsetId, 5, 5, tradeWindowFrame, 0);
    if (frame > 0x10000) return frame;

    frame = UIMgr::GetVisibleFrameByChildOffsetAndChildCount(
        kTradeQuantityPromptChildOffsetId, 5, 8, tradeWindowFrame, 0);
    if (frame > 0x10000) return frame;

    return UIMgr::GetVisibleFrameByChildOffsetAndChildCount(
        kTradeQuantityPromptChildOffsetId, 1, 4, tradeWindowFrame, 0);
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

static void __cdecl OnChooseQuantityPopupUIMessage(void* a1, void* a2, void* a3) {
    if (s_chooseQuantityPopupOriginal) {
        s_chooseQuantityPopupOriginal(a1, a2, a3);
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
                const uint8_t b5 = *reinterpret_cast<const uint8_t*>(p + 5);
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
            if (i < 2) {
                const uintptr_t delta = candidate > candidateStart ? (candidate - candidateStart) : 0;
                if (delta < 0x200) {
                    Log::Warn("TradeMgr: ChooseQuantityPopup rejecting %s candidate addr=0x%08X start=0x%08X delta=0x%X as likely interior block",
                              kChooseQuantityAssertLabels[i],
                              static_cast<unsigned>(candidate),
                              static_cast<unsigned>(candidateStart),
                              static_cast<unsigned>(delta));
                    candidateStart = 0;
                }
            }
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
            addr = candidate;
            start = candidateStart;
            resolvedLabel = kChooseQuantityAssertLabels[i];
            resolvedMsg = kChooseQuantityAssertMsgs[i];
            break;
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
    if (MH_CreateHook(reinterpret_cast<void*>(addr), &OnChooseQuantityPopupUIMessage,
                      reinterpret_cast<void**>(&s_chooseQuantityPopupOriginal)) != MH_OK) {
        Log::Warn("TradeMgr: ChooseQuantityPopup hook creation failed at 0x%08X", static_cast<unsigned>(addr));
        return false;
    }
    if (MH_EnableHook(reinterpret_cast<void*>(addr)) != MH_OK) {
        Log::Warn("TradeMgr: ChooseQuantityPopup hook enable failed at 0x%08X", static_cast<unsigned>(addr));
        MH_RemoveHook(reinterpret_cast<void*>(addr));
        s_chooseQuantityPopupOriginal = nullptr;
        return false;
    }
    s_chooseQuantityPopupHookAddr = addr;
    s_chooseQuantityPopupHookInstalled = true;
    Log::Info("TradeMgr: ChooseQuantityPopup hook installed at 0x%08X", static_cast<unsigned>(addr));
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
            bool clicked = UIMgr::ButtonClickImmediateFull(frame);
            if (!clicked) {
                clicked = UIMgr::ButtonClick(frame);
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
    if (s_tradeCartTrampoline) return true;
    const bool installed = InstallTradeCartHook();
    if (installed) {
        Log::Info("TradeMgr: Enabled UpdateTradeCart hook on demand for player trade");
    } else {
        Log::Warn("TradeMgr: Failed to enable UpdateTradeCart hook on demand for player trade");
    }
    return installed;
}

void InitiateTrade(uint32_t agentId, uint32_t requestedPlayerNumber) {
    if (agentId == 0) return;
    InterlockedExchange(&s_tradeWindowContext, 0);
    InterlockedExchange(&s_tradeWindowFrame, 0);
    InterlockedExchange(&s_tradeWindowCaptureCount, 0);
    uint32_t playerNumber = requestedPlayerNumber;
    if (auto* agent = AgentMgr::GetAgentByID(agentId)) {
        if (!playerNumber && agent->type == 0xDB) {
            playerNumber = reinterpret_cast<AgentLiving*>(agent)->player_number;
        }
    }

    const uint32_t tradeTarget = agentId;
    const uint32_t currentTargetBefore = AgentMgr::GetTargetId();
    Log::Info("TradeMgr: InitiateTrade using native-player-interact + UIMessage path agent=%u requestedPlayerNumber=%u resolvedPlayerNumber=%u targetAgent=%u currentTargetBefore=%u",
              agentId,
              requestedPlayerNumber,
              playerNumber,
              tradeTarget,
              currentTargetBefore);
    if (tradeTarget != currentTargetBefore) {
        AgentMgr::ChangeTarget(tradeTarget);
    }
    AgentMgr::InteractPlayer(tradeTarget);
    AgentMgr::CallTarget(tradeTarget);
    const uint32_t currentTargetAfter = AgentMgr::GetTargetId();
    const uintptr_t tradeButtonFrameRoot = UIMgr::GetFrameByHash(kTradeButtonFrameHash);
    const uintptr_t tradeButtonContext = tradeButtonFrameRoot ? UIMgr::GetFrameContext(tradeButtonFrameRoot) : 0u;
    Log::Info("TradeMgr: InitiateTrade state after ChangeTarget+native InteractPlayer+CallTarget currentTargetAfter=%u tradeButtonRoot=0x%08X rootState=0x%X rootChildOffset=%u rootContext=0x%08X",
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
    const uintptr_t altByHash = UIMgr::GetFrameByHash(kTradeButtonAltHash);
    const uintptr_t altByContext = tradeButtonContext
        ? UIMgr::GetFrameByContextAndChildOffset(tradeButtonContext, kTradeButtonAltChildOffsetId, tradeButtonFrameRoot)
        : 0u;
    const uintptr_t followupFrame = actionPrimaryByContext
        ? actionPrimaryByContext
        : actionSecondaryByContext;

    if (tradeButtonFrameRoot) {
        const bool clickedRoot = UIMgr::ButtonClick(tradeButtonFrameRoot);
        bool clickedFollowup = false;
        if (followupFrame && followupFrame != tradeButtonFrameRoot) {
            clickedFollowup = UIMgr::ButtonClick(followupFrame);
        }
        Log::Info("TradeMgr: InitiateTrade trade-button click root=%u followup=%u rootFrame=0x%08X followupFrame=0x%08X action123=0x%08X action122=0x%08X altByHash=0x%08X altByContext=0x%08X",
                  clickedRoot ? 1u : 0u,
                  clickedFollowup ? 1u : 0u,
                  static_cast<unsigned>(tradeButtonFrameRoot),
                  static_cast<unsigned>(followupFrame),
                  static_cast<unsigned>(actionPrimaryByContext),
                  static_cast<unsigned>(actionSecondaryByContext),
                  static_cast<unsigned>(altByHash),
                  static_cast<unsigned>(altByContext));
        EnableTradeWindowCaptureForPlayerTrade();
        return;
    }

    Log::Warn("TradeMgr: InitiateTrade trade button missing; falling back to UIMessage(0x%08X, agent=%u)",
              kUiInitiateTrade,
              tradeTarget);
    UIMgr::SendUIMessageAsm(kUiInitiateTrade, reinterpret_cast<void*>(static_cast<uintptr_t>(tradeTarget)), nullptr);
}

void CancelTrade() {
    if (s_tradeHackPatched) {
        ToggleTradePatch(false);
    }
    if (Offsets::TradeCancel > 0x10000) {
        TradeVoidNative fn = reinterpret_cast<TradeVoidNative>(Offsets::TradeCancel);
        fn();
        return;
    }
    CtoS::TradeCancel();
}

void AcceptTrade() {
    if (Offsets::TradeAcceptOffer > 0x10000) {
        TradeVoidNative fn = reinterpret_cast<TradeVoidNative>(Offsets::TradeAcceptOffer);
        fn();
        return;
    }
    CtoS::TradeAccept();
}

void OfferItem(uint32_t itemId, uint32_t quantity) {
    uintptr_t ctx = static_cast<uintptr_t>(s_tradeWindowContext);
    const uintptr_t capturedFrame = static_cast<uintptr_t>(s_tradeWindowFrame);
    const uintptr_t uiFrame = static_cast<uintptr_t>(GetTradeWindowUiFrame());
    const uintptr_t uiCtx = static_cast<uintptr_t>(GetTradeWindowUiContext());
    const bool usingUiFallback = ctx <= 0x10000 && uiCtx > 0x10000;
    if (usingUiFallback) {
        ctx = uiCtx;
    }
    Log::Info("TradeMgr: OfferItem request item=%u qty=%u ctx=0x%08X frame=0x%08X uiCtx=0x%08X uiFrame=0x%08X fn=0x%08X captures=%ld usingUiFallback=%u",
              itemId,
              quantity,
              static_cast<unsigned>(ctx),
              static_cast<unsigned>(capturedFrame),
              static_cast<unsigned>(uiCtx),
              static_cast<unsigned>(uiFrame),
              static_cast<unsigned>(Offsets::OfferTradeItem),
              s_tradeWindowCaptureCount,
              usingUiFallback ? 1u : 0u);
    if (Offsets::OfferTradeItem > 0x10000 && ctx > 0x10000) {
        auto* window = reinterpret_cast<TradeWindowView*>(ctx);
        __try {
            Log::Info("TradeMgr: OfferItem native precheck state=0x%X items_count=%u items_max=%u frame=0x%08X",
                      window->state,
                      window->items_count,
                      window->items_max,
                      static_cast<unsigned>(window->frame_id));
            const bool plausibleUiFallback =
                !usingUiFallback
                || ((window->frame_id == uiFrame || uiFrame == 0)
                    && window->items_count <= window->items_max
                    && window->items_max <= 64);
            if (window->state == 0 && plausibleUiFallback) {
                OfferTradeItemNative fn = reinterpret_cast<OfferTradeItemNative>(Offsets::OfferTradeItem);
                fn(window, nullptr, itemId, quantity, 1);
                Log::Info("TradeMgr: OfferItem native call returned");
                return;
            }
            Log::Warn("TradeMgr: OfferItem native path blocked; trade window state=0x%X frame=0x%X plausibleUiFallback=%u",
                      window->state,
                      static_cast<unsigned>(window->frame_id),
                      plausibleUiFallback ? 1u : 0u);
            return;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Log::Warn("TradeMgr: OfferItem native path faulted; falling back to raw packet");
        }
    }

    Log::Warn("TradeMgr: OfferItem native context unavailable (ctx=0x%08X fn=0x%08X); falling back to raw packet",
              static_cast<unsigned>(ctx),
              static_cast<unsigned>(Offsets::OfferTradeItem));
    if (!s_tradeHackPatched) {
        ToggleTradePatch(true);
    }
    CtoS::SendPacket(3, Packets::TRADE_OFFER_ITEM, itemId, quantity);
}

void OfferItemPromptQuantity(uint32_t itemId) {
    uintptr_t ctx = static_cast<uintptr_t>(s_tradeWindowContext);
    const uintptr_t uiCtx = static_cast<uintptr_t>(GetTradeWindowUiContext());
    if (ctx <= 0x10000 && uiCtx > 0x10000) {
        ctx = uiCtx;
    }
    Log::Info("TradeMgr: OfferItemPromptQuantity request item=%u ctx=0x%08X uiCtx=0x%08X fn=0x%08X",
              itemId,
              static_cast<unsigned>(ctx),
              static_cast<unsigned>(uiCtx),
              static_cast<unsigned>(Offsets::OfferTradeItem));
    if (Offsets::OfferTradeItem <= 0x10000 || ctx <= 0x10000) {
        Log::Warn("TradeMgr: OfferItemPromptQuantity native context unavailable");
        return;
    }
    auto* window = reinterpret_cast<TradeWindowView*>(ctx);
    __try {
        Log::Info("TradeMgr: OfferItemPromptQuantity precheck state=0x%X items_count=%u items_max=%u frame=0x%08X",
                  window->state,
                  window->items_count,
                  window->items_max,
                  static_cast<unsigned>(window->frame_id));
        if (window->state != 0) {
            Log::Warn("TradeMgr: OfferItemPromptQuantity blocked; trade window state=0x%X", window->state);
            return;
        }
        OfferTradeItemNative fn = reinterpret_cast<OfferTradeItemNative>(Offsets::OfferTradeItem);
        fn(window, nullptr, itemId, 0, 1);
        Log::Info("TradeMgr: OfferItemPromptQuantity native call returned");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log::Warn("TradeMgr: OfferItemPromptQuantity faulted");
    }
}

bool OfferItemPromptValue(uint32_t itemId, uint32_t quantity) {
    if (quantity == 0) {
        Log::Warn("TradeMgr: OfferItemPromptValue invalid quantity=0");
        return false;
    }
    if (!EnsureChooseQuantityPopupHook()) return false;
    InterlockedExchange(&s_pendingQuantityPromptClickCount, 0);
    InterlockedExchange(&s_pendingQuantityPromptValue, static_cast<LONG>(quantity));
    InterlockedExchange(&s_pendingQuantityPromptMode, static_cast<LONG>(QuantityPromptAutomationMode::ValueOffer));
    OfferItemPromptQuantity(itemId);
    const LONG count = InterlockedCompareExchange(&s_pendingQuantityPromptClickCount, 0, 0);
    if (count > 0) {
        const bool clicked = DrainQueuedPromptClicks(s_pendingQuantityPromptClicks, static_cast<size_t>(count));
        Sleep(250);
        if (clicked && !IsTradeQuantityPromptOpen()) {
            Log::Info("TradeMgr: OfferItemPromptValue callback path succeeded item=%u quantity=%u", itemId, quantity);
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
    InterlockedExchange(&s_pendingQuantityPromptMode, static_cast<LONG>(QuantityPromptAutomationMode::MaxOffer));
    OfferItemPromptQuantity(itemId);
    const LONG count = InterlockedCompareExchange(&s_pendingQuantityPromptClickCount, 0, 0);
    if (count <= 0) {
        Log::Warn("TradeMgr: OfferItemPromptMax no prompt clicks were captured");
        return false;
    }
    const bool clicked = DrainQueuedPromptClicks(s_pendingQuantityPromptClicks, static_cast<size_t>(count));
    Sleep(250);
    return clicked && !IsTradeQuantityPromptOpen();
}

bool OfferItemPromptDefault(uint32_t itemId) {
    if (!EnsureChooseQuantityPopupHook()) return false;
    InterlockedExchange(&s_pendingQuantityPromptClickCount, 0);
    InterlockedExchange(&s_pendingQuantityPromptMode, static_cast<LONG>(QuantityPromptAutomationMode::DefaultOffer));
    OfferItemPromptQuantity(itemId);
    const LONG count = InterlockedCompareExchange(&s_pendingQuantityPromptClickCount, 0, 0);
    if (count > 0) {
        const bool clicked = DrainQueuedPromptClicks(s_pendingQuantityPromptClicks, static_cast<size_t>(count));
        Sleep(250);
        if (clicked && !IsTradeQuantityPromptOpen()) {
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

        const QueuedFrameClick maxClick[] = {
            {UIMgr::GetFrameId(maxBtn), GetTickCount()}
        };
        bool clickedMax = DrainQueuedPromptClicks(maxClick, _countof(maxClick));
        if (!clickedMax) {
            clickedMax = UIMgr::ButtonClick(maxBtn);
        }
        Log::Info("TradeMgr: ConfirmTradeQuantityPromptMax max candidate=%u frame=0x%08X frameId=%u clicked=%u",
                  static_cast<unsigned>(maxIndex),
                  static_cast<unsigned>(maxBtn),
                  maxClick[0].frame_id,
                  clickedMax ? 1u : 0u);
        if (!clickedMax) continue;

        Sleep(120);

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

            const QueuedFrameClick okClick[] = {
                {UIMgr::GetFrameId(okBtn), GetTickCount()}
            };
            bool clickedOk = DrainQueuedPromptClicks(okClick, _countof(okClick));
            if (!clickedOk) {
                clickedOk = UIMgr::ButtonClick(okBtn);
            }
            Log::Info("TradeMgr: ConfirmTradeQuantityPromptMax ok candidate=%u frame=0x%08X frameId=%u clicked=%u",
                      static_cast<unsigned>(okIndex),
                      static_cast<unsigned>(okBtn),
                      okClick[0].frame_id,
                      clickedOk ? 1u : 0u);
            if (!clickedOk) continue;

            Sleep(200);
            if (!IsTradeQuantityPromptOpen()) {
                Log::Info("TradeMgr: ConfirmTradeQuantityPromptMax succeeded with max candidate=%u ok candidate=%u",
                          static_cast<unsigned>(maxIndex),
                          static_cast<unsigned>(okIndex));
                return true;
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

    if (quantity > 1u && TryPromptSpinnerAdjust(frame, quantity)) {
        uintptr_t okButtons[4] = {};
        const size_t okButtonCount = CollectPromptOkButtons(frame, okButtons, _countof(okButtons));
        for (size_t okIndex = 0; okIndex < okButtonCount; ++okIndex) {
            const uintptr_t okBtn = okButtons[okIndex];
            bool clickedOk = UIMgr::ButtonClickImmediateFull(okBtn);
            if (!clickedOk) {
                clickedOk = UIMgr::ButtonClick(okBtn);
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

    Log::Warn("TradeMgr: ConfirmTradeQuantityPromptValue no candidate closed the prompt for quantity '%S'",
              quantityBuf);
    return false;
}

void SubmitOffer(uint32_t gold) {
    if (Offsets::TradeSendOffer > 0x10000) {
        TradeDoActionNative fn = reinterpret_cast<TradeDoActionNative>(Offsets::TradeSendOffer);
        Log::Info("TradeMgr: SubmitOffer native call fn=0x%08X gold=%u",
                  static_cast<unsigned>(Offsets::TradeSendOffer),
                  gold);
        fn(gold);
        Log::Info("TradeMgr: SubmitOffer native call returned");
        return;
    }
    Log::Info("TradeMgr: SubmitOffer raw packet fallback gold=%u", gold);
    CtoS::SendPacket(2, Packets::TRADE_SUBMIT_OFFER, gold);
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

Item* GetMerchantItemByPosition(uint32_t itemPosition) {
    uintptr_t merchantBase = 0;
    uint32_t merchantSize = 0;
    if (!GetMerchantItemsBaseAndSize(merchantBase, merchantSize)) return nullptr;
    if (itemPosition == 0 || itemPosition > merchantSize) return nullptr;

    uint32_t itemId = 0;
    if (!ReadU32(merchantBase + 4 * (itemPosition - 1), itemId)) return nullptr;
    return itemId ? GetMerchantItemPtrByItemId(itemId) : nullptr;
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
    // merchant stock we exercise in Froggy/bridge tests.
    const uint32_t unitValue = item->value * 2;
    if (unitValue == 0 || !Offsets::Transaction) return false;

    const uint32_t totalValue = unitValue * quantity;
    GameThread::Enqueue([itemId, quantity, totalValue]() {
        TransactionBuyNative(quantity, itemId, totalValue);
    });
    return true;
}

bool BuyMerchantItemByModelId(uint32_t modelId, uint32_t quantity) {
    Item* item = GetMerchantItemByModelId(modelId);
    if (!item || quantity == 0) return false;

    const uint32_t itemId = item->item_id;
    const uint32_t unitValue = item->value * 2;
    if (!itemId || unitValue == 0 || !Offsets::Transaction) return false;

    const uint32_t totalValue = unitValue * quantity;
    GameThread::Enqueue([itemId, quantity, totalValue]() {
        TransactionBuyNative(quantity, itemId, totalValue);
    });
    return true;
}

bool SellMerchantItem(uint32_t itemId, uint32_t quantity, uint32_t totalValue) {
    if (!itemId || !Offsets::Transaction) return false;
    GameThread::Enqueue([itemId, quantity, totalValue]() {
        TransactionSellNative(quantity, itemId, totalValue);
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

bool RequestTraderQuoteByModelId(uint32_t modelId) {
    const uint32_t itemId = GetMerchantItemIdByModelId(modelId);
    return itemId != 0 && RequestTraderQuoteByItemId(itemId);
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
        return false;
    }
    if (!materialModelIds || !materialQuantities || materialCount == 0 || materialCount > kCrafterMaxMaterials) {
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
