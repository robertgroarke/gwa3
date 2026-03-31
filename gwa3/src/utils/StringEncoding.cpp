#include <gwa3/utils/StringEncoding.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>

#include <Windows.h>
#include <cstring>

namespace GWA3::StringEncoding {

// GW's ValidateAsyncDecodeStr signature:
//   void __cdecl ValidateAsyncDecodeStr(wchar_t* encodedStr, DecodeCallback callback, void* param)
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

struct BlockingDecodeContext {
    wchar_t* outBuf;
    uint32_t outBufSize;
    uint32_t charsWritten;
    HANDLE   event;
};

static void __cdecl BlockingDecodeCallback(void* param, wchar_t* decodedString) {
    auto* ctx = static_cast<BlockingDecodeContext*>(param);
    if (!ctx) return;

    ctx->charsWritten = 0;
    if (decodedString && ctx->outBuf && ctx->outBufSize > 0) {
        // Copy decoded string to output buffer
        size_t len = wcslen(decodedString);
        if (len >= ctx->outBufSize) len = ctx->outBufSize - 1;
        wmemcpy(ctx->outBuf, decodedString, len);
        ctx->outBuf[len] = L'\0';
        ctx->charsWritten = static_cast<uint32_t>(len);
    }

    SetEvent(ctx->event);
}

uint32_t DecodeStr(const wchar_t* encStr, wchar_t* outBuf, uint32_t outBufSize,
                   uint32_t timeoutMs) {
    if (!s_decodeStrFn || !encStr || !outBuf || outBufSize == 0) return 0;
    if (!IsValidEncStr(encStr)) return 0;

    // Clear output
    outBuf[0] = L'\0';

    BlockingDecodeContext ctx{};
    ctx.outBuf = outBuf;
    ctx.outBufSize = outBufSize;
    ctx.charsWritten = 0;
    ctx.event = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (!ctx.event) return 0;

    // Copy encoded string to a local buffer (game may need it to persist)
    size_t encLen = wcslen(encStr);
    if (encLen > 512) encLen = 512;
    wchar_t encCopy[513];
    wmemcpy(encCopy, encStr, encLen);
    encCopy[encLen] = L'\0';

    // Enqueue the decode call on the game thread
    auto* ctxPtr = &ctx;
    wchar_t* encPtr = encCopy;
    GameThread::Enqueue([ctxPtr, encPtr]() {
        s_decodeStrFn(encPtr, BlockingDecodeCallback, ctxPtr);
    });

    // Wait for callback
    DWORD result = WaitForSingleObject(ctx.event, timeoutMs);
    CloseHandle(ctx.event);

    if (result != WAIT_OBJECT_0) {
        Log::Warn("StringEncoding: DecodeStr timed out after %ums", timeoutMs);
        return 0;
    }

    return ctx.charsWritten;
}

// --- Async decode ---

bool DecodeStrAsync(const wchar_t* encStr, DecodeCallback callback, void* param) {
    if (!s_decodeStrFn || !encStr || !callback) return false;
    if (!IsValidEncStr(encStr)) return false;

    // Copy encoded string — caller's buffer may go out of scope
    size_t encLen = wcslen(encStr);
    if (encLen > 512) encLen = 512;

    // Allocate a copy that persists until the callback fires.
    // The game's decode function should call the callback before returning
    // from the game thread, so the stack copy in the lambda is fine.
    wchar_t encCopy[513];
    wmemcpy(encCopy, encStr, encLen);
    encCopy[encLen] = L'\0';

    GameThread::Enqueue([encCopy, callback, param]() {
        // encCopy is captured by value (array in lambda struct)
        wchar_t localCopy[513];
        wmemcpy(localCopy, encCopy, 513);
        s_decodeStrFn(localCopy, callback, param);
    });

    return true;
}

// IsValidEncStr, DecodeEncValue, UInt32ToEncStr are in EncStrCodec.cpp
// (separate compilation unit without Windows.h dependency)

} // namespace GWA3::StringEncoding
