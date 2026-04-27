#include <gwa3/utils/StringEncoding.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>

#include <Windows.h>
#include <atomic>
#include <cstring>
#include <cwchar>

namespace GWA3::StringEncoding {

// GW's ValidateAsyncDecodeStr signature:
//   void __cdecl ValidateAsyncDecodeStr(wchar_t* encodedStr, DecodeCallback callback, void* param)
typedef void(__cdecl* DecodeCallback)(void*, wchar_t*);
typedef void(__cdecl* ValidateAsyncDecodeStrFn)(wchar_t*, DecodeCallback, void*);

static ValidateAsyncDecodeStrFn s_decodeStrFn = nullptr;
static bool s_initialized = false;

// Encoded string format constants (from GWA2_Assembly.au3)
static constexpr uint16_t ENC_WORD_BASE      = 0x0100;
static constexpr uint16_t ENC_WORD_BIT_MORE  = 0x8000;
static constexpr uint16_t ENC_WORD_VALUE_MASK = 0x7F00;

bool Initialize() {
    if (s_initialized) return true;

    if (Offsets::ValidateAsyncDecodeStr) {
        s_decodeStrFn = reinterpret_cast<ValidateAsyncDecodeStrFn>(Offsets::ValidateAsyncDecodeStr);
    }

    s_initialized = true;
    Log::Info("StringEncoding: Initialized (decode=%s)",
              s_decodeStrFn ? "OK" : "MISSING");
    return s_decodeStrFn != nullptr;
}

// --- Blocking decode ---
//
// ValidateAsyncDecodeStr's callback is fire-and-forget and its timing is
// unbounded (we've observed >500 ms for uncached entries on a fresh map
// load). Older versions of this file put the context on the stack,
// closed the event handle on timeout, and returned — but if the callback
// later fired, it accessed freed stack memory and wrote to a closed
// event handle. GW watchdog-detected that as a crash the first time a
// fresh quest-name decode ran from the EncStringCache worker thread.
//
// The fix below: ctx lives on the heap, ownership is shared between the
// caller and the callback via a `state` CAS:
//   0 pending  → caller and callback both hold a reference
//   1 fulfilled → callback won, stored result, signalled, left cleanup
//                 of ctx to the caller
//   2 abandoned → caller timed out first; callback when it eventually
//                 fires sees abandoned, does nothing, and frees ctx.
//
// The encoded-string copy is held inside ctx so the decoder's pointer
// stays valid across our own timeout.

struct BlockingDecodeContext {
    std::atomic<int> state{0};        // 0 pending, 1 fulfilled, 2 abandoned
    uint32_t         outBufSize{0};
    uint32_t         charsWritten{0};
    wchar_t          result[513]{};
    wchar_t          encCopy[513]{};  // stays alive while decoder may use it
    HANDLE           event{nullptr};
};

static void DestroyCtx(BlockingDecodeContext* ctx) {
    if (!ctx) return;
    if (ctx->event) CloseHandle(ctx->event);
    delete ctx;
}

static void __cdecl BlockingDecodeCallback(void* param, wchar_t* decodedString) {
    auto* ctx = static_cast<BlockingDecodeContext*>(param);
    if (!ctx) return;

    int expected = 0;
    if (!ctx->state.compare_exchange_strong(expected, 1)) {
        // Caller already abandoned — we own cleanup.
        DestroyCtx(ctx);
        return;
    }

    ctx->charsWritten = 0;
    if (decodedString) {
        size_t len = wcslen(decodedString);
        if (len >= sizeof(ctx->result) / sizeof(wchar_t)) {
            len = sizeof(ctx->result) / sizeof(wchar_t) - 1;
        }
        wmemcpy(ctx->result, decodedString, len);
        ctx->result[len] = L'\0';
        ctx->charsWritten = static_cast<uint32_t>(len);
    }

    SetEvent(ctx->event);
    // Caller will destroy ctx once it has read result.
}

uint32_t DecodeStr(const wchar_t* encStr, wchar_t* outBuf, uint32_t outBufSize,
                   uint32_t timeoutMs) {
    if (!s_decodeStrFn || !encStr || !outBuf || outBufSize == 0) return 0;
    if (!IsValidEncStr(encStr)) return 0;

    outBuf[0] = L'\0';

    auto* ctx = new BlockingDecodeContext();
    ctx->outBufSize = outBufSize;
    ctx->event = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (!ctx->event) { delete ctx; return 0; }

    size_t encLen = wcslen(encStr);
    constexpr size_t kMaxEnc = sizeof(ctx->encCopy) / sizeof(wchar_t) - 1;
    if (encLen > kMaxEnc) encLen = kMaxEnc;
    wmemcpy(ctx->encCopy, encStr, encLen);
    ctx->encCopy[encLen] = L'\0';

    HANDLE waitEvent = ctx->event;
    GameThread::Enqueue([ctx]() {
        s_decodeStrFn(ctx->encCopy, BlockingDecodeCallback, ctx);
    });

    DWORD result = WaitForSingleObject(waitEvent, timeoutMs);

    if (result == WAIT_OBJECT_0) {
        // Callback fulfilled (state == 1) — copy result and destroy.
        uint32_t n = ctx->charsWritten;
        if (n >= outBufSize) n = outBufSize - 1;
        wmemcpy(outBuf, ctx->result, n);
        outBuf[n] = L'\0';
        DestroyCtx(ctx);
        return n;
    }

    // Timeout: try to abandon. If callback won between our wait-return
    // and this CAS, we take ownership and destroy.
    int expected = 0;
    if (ctx->state.compare_exchange_strong(expected, 2)) {
        Log::Warn("StringEncoding: DecodeStr timed out after %ums (abandoned)", timeoutMs);
        // Callback will destroy ctx when it eventually fires.
    } else {
        DestroyCtx(ctx);
    }
    return 0;
}

// IsValidEncStr, DecodeEncValue, UInt32ToEncStr are in EncStrCodec.cpp
// (separate compilation unit without Windows.h dependency)

} // namespace GWA3::StringEncoding
