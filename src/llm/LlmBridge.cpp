#include <gwa3/llm/LlmBridge.h>
#include <gwa3/llm/IpcServer.h>
#include <gwa3/llm/GameSnapshot.h>
#include <gwa3/llm/ActionExecutor.h>
#include <gwa3/core/Log.h>

#include <Windows.h>
#include <nlohmann/json.hpp>
#include <atomic>
#include <string>

using json = nlohmann::json;

namespace GWA3::LLM {

    static HANDLE g_bridgeThread = nullptr;
    static std::atomic<bool> g_running{false};

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

        while (g_running.load()) {
            DWORD now = GetTickCount();

            // Only send snapshots when a client is connected
            if (IpcServer::IsClientConnected()) {
                // Tier 1: core state (every 200ms)
                if (now - lastTier1 >= TIER1_INTERVAL_MS) {
                    uint32_t len = 0;
                    char* snap = GameSnapshot::SerializeTier1(&len);
                    if (snap) {
                        IpcServer::Send(snap, len);
                        delete[] snap;
                    }
                    lastTier1 = now;
                }

                // Tier 2: nearby agents (every 500ms)
                if (now - lastTier2 >= TIER2_INTERVAL_MS) {
                    uint32_t len = 0;
                    char* snap = GameSnapshot::SerializeTier2(&len);
                    if (snap) {
                        IpcServer::Send(snap, len);
                        delete[] snap;
                    }
                    lastTier2 = now;
                }

                // Tier 3: full state (every 2s)
                if (now - lastTier3 >= TIER3_INTERVAL_MS) {
                    uint32_t len = 0;
                    char* snap = GameSnapshot::SerializeTier3(&len);
                    if (snap) {
                        IpcServer::Send(snap, len);
                        delete[] snap;
                    }
                    lastTier3 = now;
                }

                // Heartbeat (every 5s)
                if (now - lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
                    SendHeartbeat();
                    lastHeartbeat = now;
                }

                // Process inbound actions
                ProcessInboundMessages();
            } else {
                // Client disconnected — drain stale queued actions
                IpcServer::DrainInbound();

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

        g_running.store(true);
        g_bridgeThread = CreateThread(nullptr, 0, BridgeThread, nullptr, 0, nullptr);
        if (!g_bridgeThread) {
            GWA3::Log::Error("[LLM-Bridge] Failed to create bridge thread");
            g_running.store(false);
            ActionExecutor::Shutdown();
            IpcServer::Shutdown();
            return false;
        }

        GWA3::Log::Info("[LLM-Bridge] Initialized — listening on \\\\.\\pipe\\gwa3_llm");
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

        ActionExecutor::Shutdown();
        IpcServer::Shutdown();

        GWA3::Log::Info("[LLM-Bridge] Shutdown complete");
    }

    bool IsRunning() {
        return g_running.load();
    }

} // namespace GWA3::LLM
