#include <gwa3/utils/EncStringCache.h>
#include <gwa3/core/Log.h>
#include <gwa3/core/Offsets.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include <Windows.h>

namespace GWA3::EncStringCache {

// GWCA pattern: fire ValidateAsyncDecodeStr directly (NOT via GameThread),
// pass a heap-allocated context that the callback writes into and then
// frees. No caller-side wait. Lots of prior GWCA use (AgentMgr, ItemMgr,
// tooltip rendering) validates this as a safe, thread-flexible pattern;
// our earlier attempt to go via GameThread::Enqueue + blocking wait is
// what was destabilising GW. See QUEST_LOG_RESEARCH.md.

typedef void(__cdecl* DecodeCallback)(void*, wchar_t*);
typedef void(__cdecl* ValidateAsyncDecodeStrFn)(const wchar_t*, DecodeCallback, void*);

static ValidateAsyncDecodeStrFn s_decodeFn = nullptr;

// Pace submissions so we don't spam the decoder faster than GWCA clients
// normally would. Tooltip-scale usage is ~1-2 decodes per second.
static constexpr uint32_t kInterDecodeSleepMs = 400;

// Safety cap on the pending queue. A full quest log has ~40 enc strings;
// 512 allows plenty of headroom for other callers.
static constexpr size_t kMaxQueueSize = 512;

static std::mutex s_mu;
static std::condition_variable s_cv;
static std::unordered_map<std::wstring, std::string> s_cache;
static std::deque<std::wstring> s_queue;
static std::unordered_set<std::wstring> s_pending;
static std::atomic<bool> s_running{false};
static std::atomic<bool> s_stopping{false};
static std::thread s_worker;

// Context passed to the game's decoder. The callback owns the memory —
// GW never surfaces an error path, so if the callback never fires the
// ctx leaks. That's bounded: worst case is one leaked ~64-byte ctx per
// Prime that times out forever, and Prime itself is LLM-driven.
struct DecodeCtx {
    std::wstring key;            // cache key (= the encoded wide-string)
    std::atomic<bool> claimed{false};
};

// SEH probe of GameContext.text_parser. Kept in its own function so MSVC
// doesn't complain about __try in a function that also manages C++
// objects with destructors.
static void ProbeTextParser(uintptr_t gc, uintptr_t* outTextParser, uint32_t* outLanguageId) {
    *outTextParser = 0;
    *outLanguageId = 0xFFFFFFFF;
    if (gc <= 0x10000) return;
    __try {
        uintptr_t tp = *reinterpret_cast<uintptr_t*>(gc + 0x18);
        *outTextParser = tp;
        if (tp > 0x10000) {
            *outLanguageId = *reinterpret_cast<uint32_t*>(tp + 0x1D0);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

static std::string Utf8FromWide(const wchar_t* w) {
    if (!w || !w[0]) return {};
    char buf[1024] = {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, buf, sizeof(buf) - 1,
                                nullptr, nullptr);
    return (n > 0) ? std::string(buf) : std::string{};
}

static void __cdecl CacheCallback(void* param, wchar_t* decoded) {
    auto* ctx = static_cast<DecodeCtx*>(param);
    if (!ctx) return;

    // Guard against a game that somehow fires the callback twice.
    bool expected = false;
    if (!ctx->claimed.compare_exchange_strong(expected, true)) {
        delete ctx;
        return;
    }

    if (decoded) {
        std::string utf8 = Utf8FromWide(decoded);
        if (!utf8.empty()) {
            std::lock_guard<std::mutex> lock(s_mu);
            s_cache.emplace(ctx->key, std::move(utf8));
            s_pending.erase(ctx->key);
        } else {
            std::lock_guard<std::mutex> lock(s_mu);
            s_pending.erase(ctx->key);
        }
    } else {
        std::lock_guard<std::mutex> lock(s_mu);
        s_pending.erase(ctx->key);
    }
    delete ctx;
}

static void WorkerLoop() {
    while (!s_stopping.load()) {
        std::wstring key;
        {
            std::unique_lock<std::mutex> lock(s_mu);
            s_cv.wait(lock, [] {
                return s_stopping.load() || !s_queue.empty();
            });
            if (s_stopping.load()) return;
            key = std::move(s_queue.front());
            s_queue.pop_front();
        }

        if (!s_decodeFn) {
            std::lock_guard<std::mutex> lock(s_mu);
            s_pending.erase(key);
            continue;
        }

        // Fire-and-forget. The ctx is owned by the callback (which runs
        // on whatever thread GW decides, whenever GW decides) — it
        // populates the cache and frees the ctx.
        auto* ctx = new DecodeCtx{};
        ctx->key = key;
        s_decodeFn(ctx->key.c_str(), CacheCallback, ctx);

        Sleep(kInterDecodeSleepMs);
    }
}

bool Initialize() {
    if (s_running.load()) return true;

    // Two independent scans for the same function. GWCA's byte-pattern
    // scan is considered authoritative upstream; our assertion-based
    // scan is what QuestMgr + prior testing has been using. Log both
    // so we can spot the case where they disagree — a difference there
    // is the smoking gun for the delayed-crash we've been chasing. See
    // QUEST_LOG_RESEARCH.md.
    const uintptr_t assertAddr = Offsets::ValidateAsyncDecodeStr;
    const uintptr_t gwcaAddr   = Offsets::ValidateAsyncDecodeStrGwca;
    Log::Info("EncStringCache: scan assertion=0x%08X gwca=0x%08X match=%s",
              static_cast<unsigned>(assertAddr),
              static_cast<unsigned>(gwcaAddr),
              (assertAddr == gwcaAddr) ? "yes" : "NO");

    // Read-only probe of the GWCA-documented text_parser context chain.
    // GWCA's AsyncDecodeStr wrappers read/write text_parser->language_id
    // around every decode call; if that chain is null or garbage in our
    // injection state, the decoder is being driven blind and that could
    // explain the delayed crashes. See QUEST_LOG_RESEARCH.md.
    uintptr_t gc = Offsets::ResolveGameContext();
    uintptr_t textParser = 0;
    uint32_t languageId = 0;
    ProbeTextParser(gc, &textParser, &languageId);
    Log::Info("EncStringCache: gameContext=0x%08X textParser=0x%08X languageId=%u",
              static_cast<unsigned>(gc),
              static_cast<unsigned>(textParser),
              languageId);

    // Prefer the GWCA-scanned address when present — that's what GWCA
    // and GWToolbox actually call.
    if (gwcaAddr > 0x10000) {
        s_decodeFn = reinterpret_cast<ValidateAsyncDecodeStrFn>(gwcaAddr);
    } else if (assertAddr > 0x10000) {
        s_decodeFn = reinterpret_cast<ValidateAsyncDecodeStrFn>(assertAddr);
    }

    s_stopping.store(false);
    s_running.store(true);
    try {
        s_worker = std::thread(WorkerLoop);
    } catch (...) {
        s_running.store(false);
        Log::Warn("EncStringCache: failed to start worker thread");
        return false;
    }
    Log::Info("EncStringCache: worker started (decode=0x%08X gap=%ums)",
              static_cast<unsigned>(reinterpret_cast<uintptr_t>(s_decodeFn)),
              kInterDecodeSleepMs);
    return true;
}

void Shutdown() {
    if (!s_running.load()) return;
    s_stopping.store(true);
    s_cv.notify_all();
    if (s_worker.joinable()) {
        s_worker.join();
    }
    std::lock_guard<std::mutex> lock(s_mu);
    s_cache.clear();
    s_queue.clear();
    s_pending.clear();
    s_running.store(false);
    Log::Info("EncStringCache: worker stopped");
}

std::string Lookup(const wchar_t* enc) {
    if (!enc || !enc[0]) return {};
    std::wstring key(enc);

    std::lock_guard<std::mutex> lock(s_mu);
    auto it = s_cache.find(key);
    if (it != s_cache.end()) {
        return it->second;
    }
    return {};
}

void Prime(const wchar_t* enc) {
    if (!enc || !enc[0]) return;
    std::wstring key(enc);

    std::lock_guard<std::mutex> lock(s_mu);
    if (s_cache.count(key) || s_pending.count(key)) return;
    if (s_queue.size() >= kMaxQueueSize) return;
    s_pending.insert(key);
    s_queue.push_back(std::move(key));
    s_cv.notify_one();
}

void Clear() {
    std::lock_guard<std::mutex> lock(s_mu);
    s_cache.clear();
    s_queue.clear();
    s_pending.clear();
}

} // namespace GWA3::EncStringCache
