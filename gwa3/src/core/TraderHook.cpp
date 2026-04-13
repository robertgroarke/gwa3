#include <gwa3/core/TraderHook.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Log.h>
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

// kVendorQuote UIMessage (0x100000BD) — native game message with quote price
static constexpr uint32_t kVendorQuote = 0x100000BDu;
struct VendorQuotePacket {
    uint32_t item_id;
    uint32_t price;
};
static HookEntry s_vendorQuoteHookEntry{};

static void OnVendorQuote(HookStatus*, uint32_t, void* wParam, void*) {
    if (!wParam) return;
    __try {
        auto* pkt = reinterpret_cast<VendorQuotePacket*>(wParam);
        InterlockedExchange(&s_costItemId, static_cast<LONG>(pkt->item_id));
        InterlockedExchange(&s_costValue, static_cast<LONG>(pkt->price));
        LONG newId = InterlockedIncrement(&s_quoteId);
        if (newId >= 200) InterlockedExchange(&s_quoteId, 1);
        Log::Info("TraderHook: kVendorQuote item=%u price=%u quoteId=%u",
                  pkt->item_id, pkt->price, static_cast<uint32_t>(s_quoteId));
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        Log::Warn("TraderHook: kVendorQuote exception reading wParam");
    }
}

// ===== MinHook-based RequestQuote hook (GWCA pattern) =====
// Hook the native RequestQuote function to:
// 1. Capture which item is being quoted (from recv.item_ids)
// 2. Call the original function via trampoline
// 3. The game processes the quote internally
//
// GWCA declares RequestQuote as:
//   void __cdecl RequestQuote(TransactionType type, uint32_t unknown,
//                             QuoteInfo give, QuoteInfo recv)
// where QuoteInfo = { uint32_t unknown, uint32_t item_count, uint32_t* item_ids }

struct QuoteInfo {
    uint32_t unknown;
    uint32_t item_count;
    uint32_t* item_ids;
};

typedef void (__cdecl *RequestQuoteFn)(uint32_t type, uint32_t unknown,
                                       QuoteInfo give, QuoteInfo recv);

static RequestQuoteFn s_requestQuoteOriginal = nullptr;

static void __cdecl RequestQuoteDetour(uint32_t type, uint32_t unknown,
                                        QuoteInfo give, QuoteInfo recv) {
    // Capture the quoted item from recv
    uint32_t quotedItemId = 0;
    if (recv.item_count > 0 && recv.item_ids) {
        __try {
            quotedItemId = recv.item_ids[0];
        } __except(EXCEPTION_EXECUTE_HANDLER) {}
    }

    Log::Info("TraderHook: RequestQuote type=0x%X item=%u recv_count=%u",
              type, quotedItemId, recv.item_count);

    // Store the quoted item ID — this is what we're asking the price for
    if (quotedItemId) {
        InterlockedExchange(&s_costItemId, static_cast<LONG>(quotedItemId));
    }

    // Call the original function — this sends the quote request to the server
    if (s_requestQuoteOriginal) {
        s_requestQuoteOriginal(type, unknown, give, recv);
    }

    // Increment quoteId to signal that a quote request was sent
    LONG newId = InterlockedIncrement(&s_quoteId);
    if (newId >= 200) InterlockedExchange(&s_quoteId, 1);

    Log::Info("TraderHook: RequestQuote returned — quoteId=%u costItem=%u",
              static_cast<uint32_t>(s_quoteId), quotedItemId);
}

// ===== Trader response hook (naked ASM — kept for quote price capture) =====
// This fires when the server sends back the quote price.
// The hook site is at Offsets::Trader (mid-function).
// Original bytes: mov ebx, [ebp+0Ch]; mov esi, eax

static constexpr uint32_t kPatchSize = 5;
static uintptr_t s_returnAddr = 0;
static uint8_t s_savedBytes[kPatchSize] = {};

static __declspec(naked) void TraderResponseDetourNaked() {
    __asm {
        mov dword ptr [s_debugEbx], ebx

        // The 2nd parameter is at [ebp+0Ch] in the caller's frame.
        // Read it and try to extract cost from [param+28].
        push eax
        push ecx

        mov eax, dword ptr [ebp+0Ch]
        cmp eax, 0x10000
        jb response_skip

        // eax = 2nd parameter (response struct pointer)
        mov ecx, dword ptr [eax+28]
        cmp ecx, 0x10000
        jb response_skip

        // ecx = pointer to {itemId, costValue}
        mov eax, [ecx]
        mov dword ptr [s_costValue], eax  // Store the cost/price

    response_skip:
        pop ecx
        pop eax

        // Replay original instructions
        mov ebx, dword ptr [ebp+0Ch]
        mov esi, eax

        jmp [s_returnAddr]
    }
}

bool Initialize() {
    if (s_initialized) return true;

    // === Hook 1: MinHook on RequestQuote function ===
    if (Offsets::RequestQuote > 0x10000) {
        MH_STATUS status = MH_Initialize();
        if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
            Log::Warn("TraderHook: MH_Initialize failed: %s", MH_StatusToString(status));
        }

        status = MH_CreateHook(
            reinterpret_cast<void*>(Offsets::RequestQuote),
            reinterpret_cast<void*>(&RequestQuoteDetour),
            reinterpret_cast<void**>(&s_requestQuoteOriginal));
        if (status == MH_OK || status == MH_ERROR_ALREADY_CREATED) {
            status = MH_EnableHook(reinterpret_cast<void*>(Offsets::RequestQuote));
            if (status == MH_OK || status == MH_ERROR_ENABLED) {
                Log::Info("TraderHook: RequestQuote MinHook installed at 0x%08X trampoline=0x%08X",
                          static_cast<unsigned>(Offsets::RequestQuote),
                          reinterpret_cast<uintptr_t>(s_requestQuoteOriginal));
            } else {
                Log::Warn("TraderHook: MH_EnableHook(RequestQuote) failed: %s", MH_StatusToString(status));
            }
        } else {
            Log::Warn("TraderHook: MH_CreateHook(RequestQuote) failed: %s", MH_StatusToString(status));
        }
    }

    // === Hook 2: kVendorQuote UIMessage callback (price capture) ===
    if (CallbackRegistry::RegisterUIMessageCallback(&s_vendorQuoteHookEntry, kVendorQuote, OnVendorQuote, 0x1)) {
        Log::Info("TraderHook: kVendorQuote (0x%X) callback registered", kVendorQuote);
    } else {
        Log::Warn("TraderHook: Failed to register kVendorQuote callback");
    }

    // === Hook 3: Naked ASM on Trader response handler (legacy price capture) ===
    if (Offsets::Trader > 0x10000) {
        const uintptr_t hookAddr = Offsets::Trader;
        s_returnAddr = hookAddr + kPatchSize;
        memcpy(s_savedBytes, reinterpret_cast<void*>(hookAddr), kPatchSize);

        DWORD oldProtect = 0;
        if (VirtualProtect(reinterpret_cast<void*>(hookAddr), kPatchSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            uint8_t patch[kPatchSize];
            patch[0] = 0xE9;
            const int32_t rel = static_cast<int32_t>(
                reinterpret_cast<uintptr_t>(&TraderResponseDetourNaked) - (hookAddr + 5));
            memcpy(patch + 1, &rel, sizeof(rel));
            memcpy(reinterpret_cast<void*>(hookAddr), patch, kPatchSize);
            FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(hookAddr), kPatchSize);
            VirtualProtect(reinterpret_cast<void*>(hookAddr), kPatchSize, oldProtect, &oldProtect);
            Log::Info("TraderHook: Response handler installed at 0x%08X -> return at 0x%08X",
                      hookAddr, s_returnAddr);
        }
    }

    Reset();
    s_initialized = true;
    Log::Info("TraderHook: Initialized");
    return true;
}

void Shutdown() {
    if (!s_initialized) return;

    // Remove MinHook
    if (Offsets::RequestQuote > 0x10000) {
        MH_DisableHook(reinterpret_cast<void*>(Offsets::RequestQuote));
        MH_RemoveHook(reinterpret_cast<void*>(Offsets::RequestQuote));
        s_requestQuoteOriginal = nullptr;
    }

    // Remove kVendorQuote callback
    CallbackRegistry::RemoveUIMessageCallback(&s_vendorQuoteHookEntry, kVendorQuote);

    // Remove naked ASM hook
    if (Offsets::Trader > 0x10000) {
        const uintptr_t hookAddr = Offsets::Trader;
        DWORD oldProtect = 0;
        if (VirtualProtect(reinterpret_cast<void*>(hookAddr), kPatchSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            memcpy(reinterpret_cast<void*>(hookAddr), s_savedBytes, kPatchSize);
            FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(hookAddr), kPatchSize);
            VirtualProtect(reinterpret_cast<void*>(hookAddr), kPatchSize, oldProtect, &oldProtect);
        }
    }

    Reset();
    s_initialized = false;
    Log::Info("TraderHook: Shutdown");
}

bool IsInitialized() {
    return s_initialized;
}

void Reset() {
    s_quoteId = 0;
    s_costItemId = 0;
    s_costValue = 0;
}

uint32_t GetQuoteId() {
    return static_cast<uint32_t>(s_quoteId);
}

uint32_t GetCostItemId() {
    return static_cast<uint32_t>(s_costItemId);
}

uint32_t GetCostValue() {
    return static_cast<uint32_t>(s_costValue);
}

uintptr_t GetDebugEbx() { return s_debugEbx; }

} // namespace GWA3::TraderHook
