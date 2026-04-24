#include <gwa3/core/TraderHook.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Scanner.h>
#include <gwa3/core/Log.h>
#include <gwa3/core/HookMarker.h>
#include <gwa3/core/CallbackRegistry.h>

#include <MinHook.h>
#include <Windows.h>
#include <cstring>

namespace GWA3::TraderHook {

static bool s_initialized = false;
static volatile LONG s_quoteId = 0;
static volatile LONG s_costItemId = 0;
static volatile LONG s_costValue = 0;
static volatile uintptr_t s_debugEbx = 0;

static constexpr uint32_t kVendorQuote = 0x100000BDu;

struct VendorQuotePacket {
    uint32_t item_id;
    uint32_t price;
};

struct QuoteInfo {
    uint32_t unknown;
    uint32_t item_count;
    uint32_t* item_ids;
};

struct TraderResponseCostRead {
    uintptr_t cost_ptr;
    uint32_t item_id;
    uint32_t cost;
};

static HookEntry s_vendorQuoteHookEntry{};

typedef void (__cdecl *RequestQuoteFn)(uint32_t type, uint32_t unknown,
                                       QuoteInfo give, QuoteInfo recv);
typedef void (__cdecl *TraderResponseFn)(uint32_t param1, uintptr_t responseStructPtr);

static RequestQuoteFn s_requestQuoteOriginal = nullptr;
static TraderResponseFn s_traderResponseOriginal = nullptr;
static uintptr_t s_traderFuncStart = 0;

static bool TryReadVendorQuotePacket(void* wParam, uint32_t* itemId, uint32_t* price) {
    __try {
        auto* pkt = reinterpret_cast<VendorQuotePacket*>(wParam);
        if (!pkt || !itemId || !price) return false;
        *itemId = pkt->item_id;
        *price = pkt->price;
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool TryReadQuotedItemId(const QuoteInfo& info, uint32_t* quotedItemId) {
    __try {
        if (info.item_count == 0 || !info.item_ids || !quotedItemId) return false;
        *quotedItemId = info.item_ids[0];
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool TryReadTraderResponseCost(uintptr_t responseStructPtr, TraderResponseCostRead* out) {
    __try {
        if (responseStructPtr <= 0x10000 || !out) return false;
        uintptr_t costPtr = *reinterpret_cast<uintptr_t*>(responseStructPtr + 0x28);
        if (costPtr <= 0x10000) return false;
        out->cost_ptr = costPtr;
        out->item_id = *reinterpret_cast<uint32_t*>(costPtr);
        out->cost = *reinterpret_cast<uint32_t*>(costPtr + 4);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static void LogTraderResponseCandidates(uintptr_t responseStructPtr) {
    __try {
        for (uint32_t off = 0; off < 0x40; off += 4) {
            uint32_t val = *reinterpret_cast<uint32_t*>(responseStructPtr + off);
            if (val > 0 && val < 10000) {
                Log::Info("TraderHook:   [+0x%02X] = %u (possible cost?)", off, val);
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        Log::Warn("TraderHook: Exception scanning response struct offsets");
    }
}

static void OnVendorQuote(HookStatus*, uint32_t, void* wParam, void*) {
    if (!wParam) return;

    uint32_t itemId = 0;
    uint32_t price = 0;
    if (!TryReadVendorQuotePacket(wParam, &itemId, &price)) return;

    InterlockedExchange(&s_costItemId, static_cast<LONG>(itemId));
    InterlockedExchange(&s_costValue, static_cast<LONG>(price));
    LONG newId = InterlockedIncrement(&s_quoteId);
    if (newId >= 200) InterlockedExchange(&s_quoteId, 1);
    Log::Info("TraderHook: kVendorQuote item=%u price=%u quoteId=%u",
              itemId, price, static_cast<uint32_t>(s_quoteId));
}

static void __cdecl RequestQuoteDetour(uint32_t type, uint32_t unknown,
                                       QuoteInfo give, QuoteInfo recv) {
    int _hookMarkerPrev = HookMarker::t_activeHookId;
    HookMarker::Enter(HookMarker::HookId::RequestQuoteDetour);
    uint32_t giveItemId = 0;
    uint32_t recvItemId = 0;
    TryReadQuotedItemId(give, &giveItemId);
    TryReadQuotedItemId(recv, &recvItemId);
    const uint32_t loggedItemId = recvItemId ? recvItemId : giveItemId;
    Log::Info("TraderHook: RequestQuote type=0x%X give=%u recv=%u",
              type, giveItemId, recvItemId);
    if (loggedItemId) {
        InterlockedExchange(&s_costItemId, static_cast<LONG>(loggedItemId));
    }
    if (s_requestQuoteOriginal) {
        s_requestQuoteOriginal(type, unknown, give, recv);
    }
    HookMarker::Leave(_hookMarkerPrev);
}

static void __cdecl TraderResponseDetour(uint32_t param1, uintptr_t responseStructPtr) {
    int _hookMarkerPrev = HookMarker::t_activeHookId;
    HookMarker::Enter(HookMarker::HookId::TraderResponseDetour);
    if (s_traderResponseOriginal) {
        s_traderResponseOriginal(param1, responseStructPtr);
    }

    if (responseStructPtr <= 0x10000) return;

    s_debugEbx = responseStructPtr;
    TraderResponseCostRead costRead{};
    if (TryReadTraderResponseCost(responseStructPtr, &costRead)) {
        Log::Info("TraderHook: Response param1=%u struct=0x%08X [+28]=0x%08X -> item=%u cost=%u",
                  param1, static_cast<unsigned>(responseStructPtr),
                  static_cast<unsigned>(costRead.cost_ptr), costRead.item_id, costRead.cost);
        if (costRead.cost > 0 && costRead.cost < 1000000) {
            InterlockedExchange(&s_costItemId, static_cast<LONG>(costRead.item_id));
            InterlockedExchange(&s_costValue, static_cast<LONG>(costRead.cost));
            LONG newId = InterlockedIncrement(&s_quoteId);
            if (newId >= 200) InterlockedExchange(&s_quoteId, 1);
        }
        return;
    }

    Log::Info("TraderHook: Response [+28] unreadable or invalid. Scanning offsets...");
    LogTraderResponseCandidates(responseStructPtr);
    HookMarker::Leave(_hookMarkerPrev);
}

bool Initialize() {
    if (s_initialized) return true;

    MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
        Log::Warn("TraderHook: MH_Initialize failed: %s", MH_StatusToString(status));
    }

    if (Offsets::RequestQuote > 0x10000) {
        status = MH_CreateHook(
            reinterpret_cast<void*>(Offsets::RequestQuote),
            reinterpret_cast<void*>(&RequestQuoteDetour),
            reinterpret_cast<void**>(&s_requestQuoteOriginal));
        if (status == MH_OK || status == MH_ERROR_ALREADY_CREATED) {
            MH_EnableHook(reinterpret_cast<void*>(Offsets::RequestQuote));
            Log::Info("TraderHook: RequestQuote MinHook at 0x%08X",
                      static_cast<unsigned>(Offsets::RequestQuote));
        }
    }

    CallbackRegistry::RegisterUIMessageCallback(&s_vendorQuoteHookEntry, kVendorQuote, OnVendorQuote, 0x1);

    if (Offsets::Trader > 0x10000) {
        s_traderFuncStart = Scanner::ToFunctionStart(Offsets::Trader, 0x80);
        if (s_traderFuncStart > 0x10000) {
            Log::Info("TraderHook: Response function start at 0x%08X (hook site was 0x%08X, delta=0x%X)",
                      static_cast<unsigned>(s_traderFuncStart),
                      static_cast<unsigned>(Offsets::Trader),
                      static_cast<unsigned>(Offsets::Trader - s_traderFuncStart));

            status = MH_CreateHook(
                reinterpret_cast<void*>(s_traderFuncStart),
                reinterpret_cast<void*>(&TraderResponseDetour),
                reinterpret_cast<void**>(&s_traderResponseOriginal));
            if (status == MH_OK || status == MH_ERROR_ALREADY_CREATED) {
                status = MH_EnableHook(reinterpret_cast<void*>(s_traderFuncStart));
                if (status == MH_OK || status == MH_ERROR_ENABLED) {
                    Log::Info("TraderHook: Response MinHook installed at 0x%08X trampoline=0x%08X",
                              static_cast<unsigned>(s_traderFuncStart),
                              reinterpret_cast<uintptr_t>(s_traderResponseOriginal));
                } else {
                    Log::Warn("TraderHook: MH_EnableHook(Response) failed: %s", MH_StatusToString(status));
                }
            } else {
                Log::Warn("TraderHook: MH_CreateHook(Response) failed: %s", MH_StatusToString(status));
            }
        } else {
            Log::Warn("TraderHook: Could not find function start from 0x%08X",
                      static_cast<unsigned>(Offsets::Trader));
        }
    }

    Reset();
    s_initialized = true;
    Log::Info("TraderHook: Initialized");
    return true;
}

void Shutdown() {
    if (!s_initialized) return;
    if (Offsets::RequestQuote > 0x10000) {
        MH_DisableHook(reinterpret_cast<void*>(Offsets::RequestQuote));
        MH_RemoveHook(reinterpret_cast<void*>(Offsets::RequestQuote));
    }
    if (s_traderFuncStart > 0x10000) {
        MH_DisableHook(reinterpret_cast<void*>(s_traderFuncStart));
        MH_RemoveHook(reinterpret_cast<void*>(s_traderFuncStart));
    }
    CallbackRegistry::RemoveUIMessageCallback(&s_vendorQuoteHookEntry, kVendorQuote);
    s_requestQuoteOriginal = nullptr;
    s_traderResponseOriginal = nullptr;
    Reset();
    s_initialized = false;
}

bool IsInitialized() { return s_initialized; }

void Reset() {
    s_quoteId = 0;
    s_costItemId = 0;
    s_costValue = 0;
}

uint32_t GetQuoteId() { return static_cast<uint32_t>(s_quoteId); }
uint32_t GetCostItemId() { return static_cast<uint32_t>(s_costItemId); }
uint32_t GetCostValue() { return static_cast<uint32_t>(s_costValue); }
uintptr_t GetDebugEbx() { return s_debugEbx; }

} // namespace GWA3::TraderHook
