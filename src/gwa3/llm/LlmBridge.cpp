#include <gwa3/llm/LlmBridge.h>
#include <gwa3/llm/IpcServer.h>
#include <gwa3/llm/GameSnapshot.h>
#include <gwa3/llm/ActionExecutor.h>
#include <gwa3/llm/EventPush.h>
#include <gwa3/core/Log.h>

#include <Windows.h>
#include <nlohmann/json.hpp>
#include <atomic>
#include <string>

using json = nlohmann::json;

namespace GWA3::LLM {

    static HANDLE g_bridgeThread = nullptr;
    static std::atomic<bool> g_running{false};
    static std::atomic<DWORD> g_snapshotPauseUntil{0};

    // Snapshot intervals (milliseconds)
    static constexpr DWORD TIER1_INTERVAL_MS = 200;
    static constexpr DWORD TIER2_INTERVAL_MS = 500;
    static constexpr DWORD TIER3_INTERVAL_MS = 2000;
    static constexpr DWORD HEARTBEAT_INTERVAL_MS = 5000;
    static constexpr DWORD ACTION_POLL_MS = 50;

    static void SendHeartbeat() {
        json j;
        j["type"] = "heartbeat";
        std::string s = j.dump();
        IpcServer::Send(s.c_str(), static_cast<uint32_t>(s.size()));
    }

    static void ProcessInboundMessages() {
        while (IpcServer::HasPending()) {
            uint32_t len = 0;
            char* msg = IpcServer::Dequeue(&len);
            if (!msg) break;

            try {
                json j = json::parse(msg, msg + len);
                std::string type = j.value("type", "");

                if (type == "action") {
                    std::string name = j.value("name", "");
                    std::string requestId = j.value("request_id", "");
                    std::string paramsStr;
                    if (j.contains("params")) {
                        paramsStr = j["params"].dump();
                    }
                    ActionExecutor::Execute(
                        name.c_str(),
                        paramsStr.c_str(),
                        requestId.c_str()
                    );
                } else if (type == "subscribe") {
                    // Future: adjust snapshot tiers/frequency
                    GWA3::Log::Info("[LLM-Bridge] Subscribe message received (not yet implemented)");
                } else if (type == "config") {
                    // Future: runtime config changes
                    GWA3::Log::Info("[LLM-Bridge] Config message received (not yet implemented)");
                } else {
                    GWA3::Log::Warn("[LLM-Bridge] Unknown message type: %s", type.c_str());
                }
            } catch (const std::exception& e) {
                GWA3::Log::Warn("[LLM-Bridge] Failed to parse inbound message: %s", e.what());
            }

            IpcServer::FreeMsgBuf(msg);
        }
    }

    // Main bridge thread: sends snapshots at tiered intervals, processes inbound actions
    static DWORD WINAPI BridgeThread(LPVOID) {
        GWA3::Log::Info("[LLM-Bridge] Bridge thread started");

        DWORD lastTier1 = 0;
        DWORD lastTier2 = 0;
        DWORD lastTier3 = 0;
        DWORD lastHeartbeat = 0;
        bool firstTier1Trace = false;
        uint32_t tier2TraceCount = 0;
        uint32_t tier3TraceCount = 0;

        while (g_running.load()) {
            DWORD now = GetTickCount();
            const DWORD pauseUntil = g_snapshotPauseUntil.load();
            const bool snapshotsPaused = pauseUntil != 0 && static_cast<int32_t>(pauseUntil - now) > 0;

            // Only send snapshots when a client is connected
            if (IpcServer::IsClientConnected()) {
                // Tier 1: core state (every 200ms)
                if (!snapshotsPaused && now - lastTier1 >= TIER1_INTERVAL_MS) {
                    if (!firstTier1Trace) {
                        GWA3::Log::Info("[LLM-Bridge] Tier1 begin");
                    }
                    uint32_t len = 0;
                    char* snap = GameSnapshot::SerializeTier1(&len);
                    if (!firstTier1Trace) {
                        GWA3::Log::Info("[LLM-Bridge] Tier1 serialized len=%u ptr=0x%08X", len, static_cast<unsigned>(reinterpret_cast<uintptr_t>(snap)));
                    }
                    if (snap) {
                        if (!firstTier1Trace) {
                            GWA3::Log::Info("[LLM-Bridge] Tier1 send begin");
                        }
                        IpcServer::Send(snap, len);
                        if (!firstTier1Trace) {
                            GWA3::Log::Info("[LLM-Bridge] Tier1 send end");
                        }
                        delete[] snap;
                    }
                    if (!firstTier1Trace) {
                        GWA3::Log::Info("[LLM-Bridge] Tier1 end");
                        firstTier1Trace = true;
                    }
                    lastTier1 = now;
                }

                // Tier 2: nearby agents (every 500ms)
                if (!snapshotsPaused && now - lastTier2 >= TIER2_INTERVAL_MS) {
                    uint32_t len = 0;
                    if (tier2TraceCount < 5) {
                        GWA3::Log::Info("[LLM-Bridge] Tier2 begin");
                    }
                    char* snap = GameSnapshot::SerializeTier2(&len);
                    if (snap) {
                        if (tier2TraceCount < 5) {
                            GWA3::Log::Info("[LLM-Bridge] Tier2 serialized len=%u ptr=0x%08X", len, static_cast<unsigned>(reinterpret_cast<uintptr_t>(snap)));
                            GWA3::Log::Info("[LLM-Bridge] Tier2 send begin");
                        }
                        bool sent = IpcServer::Send(snap, len);
                        if (tier2TraceCount < 5) {
                            GWA3::Log::Info("[LLM-Bridge] Tier2 send end sent=%u", sent ? 1u : 0u);
                        }
                        delete[] snap;
                    }
                    if (tier2TraceCount < 5) {
                        GWA3::Log::Info("[LLM-Bridge] Tier2 end");
                    }
                    tier2TraceCount++;
                    lastTier2 = now;
                }

                // Tier 3: full state (every 2s)
                if (!snapshotsPaused && now - lastTier3 >= TIER3_INTERVAL_MS) {
                    uint32_t len = 0;
                    if (tier3TraceCount < 3) {
                        GWA3::Log::Info("[LLM-Bridge] Tier3 begin");
                    }
                    char* snap = GameSnapshot::SerializeTier3(&len);
                    if (snap) {
                        if (tier3TraceCount < 3) {
                            GWA3::Log::Info("[LLM-Bridge] Tier3 serialized len=%u ptr=0x%08X", len, static_cast<unsigned>(reinterpret_cast<uintptr_t>(snap)));
                            GWA3::Log::Info("[LLM-Bridge] Tier3 send begin");
                        }
                        bool sent = IpcServer::Send(snap, len);
                        if (tier3TraceCount < 3) {
                            GWA3::Log::Info("[LLM-Bridge] Tier3 send end sent=%u", sent ? 1u : 0u);
                        }
                        delete[] snap;
                    }
                    if (tier3TraceCount < 3) {
                        GWA3::Log::Info("[LLM-Bridge] Tier3 end");
                    }
                    tier3TraceCount++;
                    lastTier3 = now;
                }

                // Heartbeat (every 5s)
                if (now - lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
                    SendHeartbeat();
                    lastHeartbeat = now;
                }

                // Inbound actions are drained by the init thread via
                // DrainInboundActions() for safe game thread dispatch.
                // The bridge thread no longer processes actions directly.
            }

            Sleep(ACTION_POLL_MS);
        }

        GWA3::Log::Info("[LLM-Bridge] Bridge thread exiting");
        return 0;
    }

    bool Initialize() {
        if (g_running.load()) return true;

        GWA3::Log::Info("[LLM-Bridge] Initializing...");

        if (!IpcServer::Initialize()) {
            GWA3::Log::Error("[LLM-Bridge] IpcServer initialization failed");
            return false;
        }

        if (!ActionExecutor::Initialize()) {
            GWA3::Log::Error("[LLM-Bridge] ActionExecutor initialization failed");
            IpcServer::Shutdown();
            return false;
        }

        // EventPush is optional — don't fail bridge init if it can't hook
        if (!EventPush::Initialize()) {
            GWA3::Log::Warn("[LLM-Bridge] EventPush initialization failed — events won't stream");
        }

        g_running.store(true);
        g_bridgeThread = CreateThread(nullptr, 0, BridgeThread, nullptr, 0, nullptr);
        if (!g_bridgeThread) {
            GWA3::Log::Error("[LLM-Bridge] Failed to create bridge thread");
            g_running.store(false);
            EventPush::Shutdown();
            ActionExecutor::Shutdown();
            IpcServer::Shutdown();
            return false;
        }

        GWA3::Log::Info("[LLM-Bridge] Initialized — listening on %s", IpcServer::GetPipeName());
        return true;
    }

    void Shutdown() {
        if (!g_running.load()) return;
        g_running.store(false);

        if (g_bridgeThread) {
            WaitForSingleObject(g_bridgeThread, 5000);
            CloseHandle(g_bridgeThread);
            g_bridgeThread = nullptr;
        }

        EventPush::Shutdown();
        ActionExecutor::Shutdown();
        IpcServer::Shutdown();

        GWA3::Log::Info("[LLM-Bridge] Shutdown complete");
    }

    bool IsRunning() {
        return g_running.load();
    }

    void PauseSnapshotsFor(unsigned long milliseconds) {
        const DWORD now = GetTickCount();
        const DWORD until = now + milliseconds;
        g_snapshotPauseUntil.store(until);
        GWA3::Log::Info("[LLM-Bridge] Snapshots paused for %lu ms (until=%lu)", milliseconds, until);
    }

    void DrainInboundActions() {
        ProcessInboundMessages();
    }

} // namespace GWA3::LLM
