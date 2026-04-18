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

    if (Offsets::ValidateAsyncDecodeStr > 0x10000) {
        s_decodeFn = reinterpret_cast<ValidateAsyncDecodeStrFn>(
            Offsets::ValidateAsyncDecodeStr);
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
    Log::Info("EncStringCache: worker started (decode=%s gap=%ums)",
              s_decodeFn ? "resolved" : "MISSING",
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
