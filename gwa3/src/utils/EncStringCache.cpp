#include <gwa3/utils/EncStringCache.h>
#include <gwa3/utils/StringEncoding.h>
#include <gwa3/core/Log.h>

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

// How long we wait on each ValidateAsyncDecodeStr callback. The game's
// async decoder usually responds well inside 500 ms on a loaded map;
// anything that times out is retried on the next Lookup.
static constexpr uint32_t kDecodeTimeoutMs = 500;

// Minimum gap between successive decode submissions. Keeps the
// GameThread pre-dispatch queue from ever filling up with enc-decode
// calls (which caused a client crash in earlier sync-decode attempts).
static constexpr uint32_t kInterDecodeSleepMs = 50;

// Safety cap on the pending queue. A full quest log has ~40 enc
// strings; we allow 10x headroom.
static constexpr size_t kMaxQueueSize = 512;

static std::mutex s_mu;
static std::condition_variable s_cv;
static std::unordered_map<std::wstring, std::string> s_cache;
static std::deque<std::wstring> s_queue;
static std::unordered_set<std::wstring> s_pending;
static std::atomic<bool> s_running{false};
static std::atomic<bool> s_stopping{false};
static std::thread s_worker;

static std::string Utf8FromWide(const wchar_t* w) {
    if (!w || !w[0]) return {};
    char buf[1024] = {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, buf, sizeof(buf) - 1,
                                nullptr, nullptr);
    return (n > 0) ? std::string(buf) : std::string{};
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

        wchar_t decoded[512] = {};
        uint32_t n = StringEncoding::DecodeStr(key.c_str(), decoded, 512,
                                               kDecodeTimeoutMs);
        std::string utf8 = (n > 0) ? Utf8FromWide(decoded) : std::string{};

        {
            std::lock_guard<std::mutex> lock(s_mu);
            if (!utf8.empty()) {
                s_cache.emplace(key, std::move(utf8));
            }
            // Always clear pending — failures can be retried on the
            // next Lookup. (Cache only holds successes.)
            s_pending.erase(key);
        }

        // Pace submissions so we don't saturate the GameThread queue.
        Sleep(kInterDecodeSleepMs);
    }
}

bool Initialize() {
    if (s_running.load()) return true;
    s_stopping.store(false);
    s_running.store(true);
    try {
        s_worker = std::thread(WorkerLoop);
    } catch (...) {
        s_running.store(false);
        Log::Warn("EncStringCache: failed to start worker thread");
        return false;
    }
    Log::Info("EncStringCache: worker started (timeout=%ums, gap=%ums)",
              kDecodeTimeoutMs, kInterDecodeSleepMs);
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
    if (s_pending.count(key) == 0 && s_queue.size() < kMaxQueueSize) {
        s_pending.insert(key);
        s_queue.push_back(key);
        s_cv.notify_one();
    }
    return {};
}

void Clear() {
    std::lock_guard<std::mutex> lock(s_mu);
    s_cache.clear();
    s_queue.clear();
    s_pending.clear();
}

} // namespace GWA3::EncStringCache
