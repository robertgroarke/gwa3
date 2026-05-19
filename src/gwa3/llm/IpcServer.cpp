#include <gwa3/llm/IpcServer.h>
#include <gwa3/llm/Protocol.h>
#include <gwa3/core/Log.h>

#include <atomic>
#include <cctype>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <nlohmann/json.hpp>
#include <new>
#include <queue>
#include <string>

using json = nlohmann::json;

namespace GWA3::LLM::IpcServer {

#ifdef GWA3_PIPE_NAME
    static constexpr const char* PIPE_NAME = GWA3_PIPE_NAME;
#else
    static constexpr const char* PIPE_NAME = "\\\\.\\pipe\\gwa3_llm";
#endif
    static constexpr DWORD PIPE_BUFFER_SIZE = 64 * 1024;  // 64KB
    static constexpr DWORD CONNECT_TIMEOUT_MS = 500;
    static constexpr DWORD WRITE_TIMEOUT_MS = 2000;
    static constexpr size_t MAX_OUTBOUND_QUEUE = 256;

    static HANDLE g_pipe = INVALID_HANDLE_VALUE;
    static HANDLE g_thread = nullptr;
    static HANDLE g_senderThread = nullptr;
    static std::atomic<bool> g_running{false};
    static std::atomic<bool> g_clientConnected{false};
    static std::mutex g_pipeMutex;

    // Inbound message queue (bridge -> gwa3)
    struct InboundMsg {
        char* data;
        uint32_t length;
    };
    static std::mutex g_inboundMutex;
    static std::queue<InboundMsg> g_inboundQueue;

    // Outbound message queue (gwa3 -> bridge). Multiple producer threads enqueue
    // messages here; one sender thread drains it so game-facing threads never
    // block in WriteFile when the Python bridge stalls.
    struct OutboundMsg {
        std::string payload;
        OutboundPriority priority;
    };
    static std::mutex g_outboundMutex;
    static std::condition_variable g_outboundCv;
    static std::deque<OutboundMsg> g_outboundQueue;

    // Returns: 1 = bytes available, 0 = no bytes yet, -1 = pipe broken (client disconnected)
    static int PipeCheckState() {
        if (g_pipe == INVALID_HANDLE_VALUE) return -1;
        if (!g_clientConnected.load()) return -1;
        DWORD bytesAvail = 0;
        if (!PeekNamedPipe(g_pipe, nullptr, 0, nullptr, &bytesAvail, nullptr)) {
            DWORD err = GetLastError();
            if (err == ERROR_BROKEN_PIPE ||
                err == ERROR_NO_DATA ||
                err == ERROR_PIPE_NOT_CONNECTED ||
                err == ERROR_OPERATION_ABORTED ||
                err == ERROR_INVALID_HANDLE) {
                return -1; // client disconnected
            }
            return 0; // transient error, retry
        }
        return bytesAvail > 0 ? 1 : 0;
    }

    static bool WaitForOverlapped(HANDLE pipe, OVERLAPPED& ov, DWORD timeoutMs, DWORD* outBytes) {
        const DWORD wait = WaitForSingleObject(ov.hEvent, timeoutMs);
        if (wait == WAIT_TIMEOUT) {
            CancelIoEx(pipe, &ov);
            WaitForSingleObject(ov.hEvent, 100);
            SetLastError(WAIT_TIMEOUT);
            return false;
        }
        if (wait != WAIT_OBJECT_0) {
            CancelIoEx(pipe, &ov);
            SetLastError(wait);
            return false;
        }
        return GetOverlappedResult(pipe, &ov, outBytes, FALSE) != FALSE;
    }

    // Write exactly `count` bytes to the pipe using overlapped I/O. Returns
    // false on failure or timeout; caller owns disconnect/reconnect policy.
    static bool PipeWriteAllWithTimeout(const void* buf, DWORD count, DWORD timeoutMs) {
        const uint8_t* p = static_cast<const uint8_t*>(buf);
        DWORD remaining = count;
        while (remaining > 0) {
            DWORD written = 0;
            OVERLAPPED ov = {};
            ov.hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
            if (!ov.hEvent) {
                return false;
            }

            BOOL ok = WriteFile(g_pipe, p, remaining, &written, &ov);
            if (!ok) {
                const DWORD err = GetLastError();
                if (err != ERROR_IO_PENDING ||
                    !WaitForOverlapped(g_pipe, ov, timeoutMs, &written)) {
                    CloseHandle(ov.hEvent);
                    return false;
                }
            }
            CloseHandle(ov.hEvent);
            if (written == 0) return false;
            p += written;
            remaining -= written;
        }
        return true;
    }

    static bool WriteMessagePayloadWithTimeout(const char* payload, uint32_t length, DWORD timeoutMs) {
        if (!payload || length == 0) return false;
        if (!PipeWriteAllWithTimeout(&length, 4, timeoutMs)) return false;
        if (!PipeWriteAllWithTimeout(payload, length, timeoutMs)) return false;
        return true;
    }

    static std::string InferLaneName() {
        const char* marker = "gwa3_llm";
        const char* found = strstr(PIPE_NAME, marker);
        if (!found) return "default";

        found += strlen(marker);
        if (*found == '_' || *found == '-') {
            ++found;
        }
        if (!*found) return "default";

        std::string lane(found);
        for (char& ch : lane) {
            const unsigned char c = static_cast<unsigned char>(ch);
            if (!std::isalnum(c) && ch != '_' && ch != '-') {
                ch = '_';
            }
        }
        return lane.empty() ? "default" : lane;
    }

    // Read exactly `count` bytes from the pipe. Returns false on failure/disconnect.
    static bool PipeReadAll(void* buf, DWORD count) {
        uint8_t* p = static_cast<uint8_t*>(buf);
        DWORD remaining = count;
        while (remaining > 0) {
            DWORD bytesRead = 0;
            OVERLAPPED ov = {};
            ov.hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
            if (!ov.hEvent) return false;

            BOOL ok = ReadFile(g_pipe, p, remaining, &bytesRead, &ov);
            if (!ok) {
                const DWORD err = GetLastError();
                if (err != ERROR_IO_PENDING) {
                    CloseHandle(ov.hEvent);
                    return false;
                }
                while (g_running.load()) {
                    const DWORD wait = WaitForSingleObject(ov.hEvent, 500);
                    if (wait == WAIT_OBJECT_0) break;
                    if (wait != WAIT_TIMEOUT) {
                        CancelIoEx(g_pipe, &ov);
                        CloseHandle(ov.hEvent);
                        return false;
                    }
                }
                if (!g_running.load()) {
                    CancelIoEx(g_pipe, &ov);
                    CloseHandle(ov.hEvent);
                    return false;
                }
                if (!GetOverlappedResult(g_pipe, &ov, &bytesRead, FALSE)) {
                    CloseHandle(ov.hEvent);
                    return false;
                }
            }
            CloseHandle(ov.hEvent);
            if (bytesRead == 0) return false; // pipe closed
            p += bytesRead;
            remaining -= bytesRead;
        }
        return true;
    }

    // Read one length-prefixed message from the pipe.
    // Returns heap-allocated buffer (caller frees) or nullptr on error.
    static char* ReadMessage(uint32_t* outLength) {
        uint32_t msgLen = 0;
        if (!PipeReadAll(&msgLen, 4)) return nullptr;
        if (msgLen == 0 || msgLen > 1024 * 1024) return nullptr; // sanity: max 1MB

        char* buf = new (std::nothrow) char[msgLen + 1];
        if (!buf) return nullptr;

        if (!PipeReadAll(buf, msgLen)) {
            delete[] buf;
            return nullptr;
        }
        buf[msgLen] = '\0';
        *outLength = msgLen;
        return buf;
    }

    static bool SendServerHello() {
        json hello;
        hello["type"] = "hello";
        hello["role"] = "gwa3";
        GWA3::LLM::StampProtocol(hello);
        hello["lane"] = InferLaneName();
        hello["pipe_name"] = PIPE_NAME;

        const std::string payload = hello.dump();
        std::lock_guard<std::mutex> lock(g_pipeMutex);
        return WriteMessagePayloadWithTimeout(payload.c_str(),
                                              static_cast<uint32_t>(payload.size()),
                                              WRITE_TIMEOUT_MS);
    }

    static bool ValidateClientHello(const char* payload, uint32_t length) {
        if (!payload || length == 0) return false;

        try {
            json hello = json::parse(payload, payload + length);
            const std::string type = hello.value("type", "");
            const int version = GWA3::LLM::ReadProtocolVersion(hello);
            if (type != "hello") {
                GWA3::Log::Warn("[LLM-IPC] Expected client hello, got type=%s", type.c_str());
                return false;
            }
            if (version != static_cast<int>(GWA3::LLM::IPC_PROTOCOL_VERSION)) {
                GWA3::Log::Warn("[LLM-IPC] Protocol mismatch: bridge=%d gwa3=%u",
                                version,
                                GWA3::LLM::IPC_PROTOCOL_VERSION);
                return false;
            }
            const std::string clientLane = hello.value("lane", "");
            const std::string expectedLane = InferLaneName();
            if (!clientLane.empty() && clientLane != expectedLane) {
                GWA3::Log::Warn("[LLM-IPC] Client lane=%s connected to lane=%s",
                                clientLane.c_str(),
                                expectedLane.c_str());
            }
            return true;
        } catch (const std::exception& e) {
            GWA3::Log::Warn("[LLM-IPC] Invalid client hello: %s", e.what());
            return false;
        }
    }

    static bool ReceiveClientHello() {
        uint32_t msgLen = 0;
        char* msg = ReadMessage(&msgLen);
        if (!msg) {
            GWA3::Log::Warn("[LLM-IPC] Client disconnected before protocol hello");
            return false;
        }

        const bool ok = ValidateClientHello(msg, msgLen);
        delete[] msg;
        return ok;
    }

    static uint8_t PriorityValue(OutboundPriority priority) {
        return static_cast<uint8_t>(priority);
    }

    static void ClearOutboundQueue() {
        std::lock_guard<std::mutex> lock(g_outboundMutex);
        g_outboundQueue.clear();
    }

    static bool QueueOutbound(const char* payload, uint32_t length, OutboundPriority priority) {
        if (!payload || length == 0) return false;
        if (!g_clientConnected.load()) return false;

        std::lock_guard<std::mutex> lock(g_outboundMutex);
        if (g_outboundQueue.size() >= MAX_OUTBOUND_QUEUE) {
            auto lowest = g_outboundQueue.begin();
            for (auto it = g_outboundQueue.begin(); it != g_outboundQueue.end(); ++it) {
                if (PriorityValue(it->priority) < PriorityValue(lowest->priority)) {
                    lowest = it;
                }
            }
            if (PriorityValue(priority) <= PriorityValue(lowest->priority)) {
                return false;
            }
            GWA3::Log::Warn("[LLM-IPC] Dropping queued outbound frame priority=%u for priority=%u",
                            PriorityValue(lowest->priority),
                            PriorityValue(priority));
            g_outboundQueue.erase(lowest);
        }

        g_outboundQueue.push_back({
            std::string(payload, payload + length),
            priority,
        });
        g_outboundCv.notify_one();
        return true;
    }

    static bool PopNextOutbound(OutboundMsg& out) {
        std::unique_lock<std::mutex> lock(g_outboundMutex);
        g_outboundCv.wait(lock, []() {
            return !g_running.load() || !g_outboundQueue.empty();
        });
        if (!g_running.load()) return false;
        if (g_outboundQueue.empty()) return false;

        auto best = g_outboundQueue.begin();
        for (auto it = g_outboundQueue.begin(); it != g_outboundQueue.end(); ++it) {
            if (PriorityValue(it->priority) > PriorityValue(best->priority)) {
                best = it;
            }
        }

        out = std::move(*best);
        g_outboundQueue.erase(best);
        return true;
    }

    static void DisconnectClientAfterSendFailure(DWORD error) {
        {
            std::lock_guard<std::mutex> lock(g_pipeMutex);
            if (g_pipe != INVALID_HANDLE_VALUE) {
                CancelIoEx(g_pipe, nullptr);
                DisconnectNamedPipe(g_pipe);
            }
        }
        g_clientConnected.store(false);
        ClearOutboundQueue();
        GWA3::Log::Warn("[LLM-IPC] Outbound write failed/timeout err=%u; disconnected client for reconnect",
                        error);
    }

    static DWORD WINAPI SenderThread(LPVOID) {
        GWA3::Log::Info("[LLM-IPC] Sender thread started");

        while (g_running.load()) {
            OutboundMsg msg;
            if (!PopNextOutbound(msg)) {
                continue;
            }
            if (!g_clientConnected.load()) {
                continue;
            }

            bool ok = false;
            DWORD err = 0;
            {
                std::lock_guard<std::mutex> lock(g_pipeMutex);
                if (g_clientConnected.load() && g_pipe != INVALID_HANDLE_VALUE) {
                    ok = WriteMessagePayloadWithTimeout(msg.payload.c_str(),
                                                        static_cast<uint32_t>(msg.payload.size()),
                                                        WRITE_TIMEOUT_MS);
                    if (!ok) {
                        err = GetLastError();
                    }
                }
            }

            if (!ok) {
                DisconnectClientAfterSendFailure(err);
            }
        }

        GWA3::Log::Info("[LLM-IPC] Sender thread exiting");
        return 0;
    }

    // IPC thread: creates pipe, waits for client, reads messages.
    static DWORD WINAPI IpcThread(LPVOID) {
        GWA3::Log::Info("[LLM-IPC] IPC thread started");

        while (g_running.load()) {
            // Create a new pipe instance for each client connection
            g_pipe = CreateNamedPipeA(
                PIPE_NAME,
                PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                1,                    // max instances
                PIPE_BUFFER_SIZE,
                PIPE_BUFFER_SIZE,
                CONNECT_TIMEOUT_MS,
                nullptr
            );

            if (g_pipe == INVALID_HANDLE_VALUE) {
                GWA3::Log::Error("[LLM-IPC] CreateNamedPipe failed: %u", GetLastError());
                Sleep(1000);
                continue;
            }

            GWA3::Log::Info("[LLM-IPC] Waiting for client on %s", PIPE_NAME);

            // Wait for a client to connect (blocking, but we check g_running periodically)
            OVERLAPPED ov = {};
            ov.hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
            BOOL connectedNow = ConnectNamedPipe(g_pipe, &ov);
            DWORD err = connectedNow ? ERROR_PIPE_CONNECTED : GetLastError();
            if (err == ERROR_IO_PENDING) {
                // Wait with timeout so we can check g_running
                while (g_running.load()) {
                    DWORD wait = WaitForSingleObject(ov.hEvent, 500);
                    if (wait == WAIT_OBJECT_0) break;
                }
                if (!g_running.load()) {
                    CancelIo(g_pipe);
                    CloseHandle(ov.hEvent);
                    {
                        std::lock_guard<std::mutex> lock(g_pipeMutex);
                        CloseHandle(g_pipe);
                        g_pipe = INVALID_HANDLE_VALUE;
                    }
                    break;
                }
            } else if (err != ERROR_PIPE_CONNECTED && err != 0) {
                GWA3::Log::Error("[LLM-IPC] ConnectNamedPipe failed: %u", err);
                CloseHandle(ov.hEvent);
                {
                    std::lock_guard<std::mutex> lock(g_pipeMutex);
                    CloseHandle(g_pipe);
                    g_pipe = INVALID_HANDLE_VALUE;
                }
                Sleep(500);
                continue;
            }

            CloseHandle(ov.hEvent);
            if (!SendServerHello() || !ReceiveClientHello()) {
                GWA3::Log::Warn("[LLM-IPC] Protocol handshake failed; closing client");
                {
                    std::lock_guard<std::mutex> lock(g_pipeMutex);
                    DisconnectNamedPipe(g_pipe);
                    CloseHandle(g_pipe);
                    g_pipe = INVALID_HANDLE_VALUE;
                }
                continue;
            }

            g_clientConnected.store(true);
            GWA3::Log::Info("[LLM-IPC] Client connected (protocol v%u)", GWA3::LLM::IPC_PROTOCOL_VERSION);

            // Read messages from the client until disconnect or shutdown
            while (g_running.load()) {
                int state = PipeCheckState();
                if (state < 0) {
                    // Pipe broken — client disconnected
                    GWA3::Log::Info("[LLM-IPC] PipeCheckState: pipe broken, ending session");
                    break;
                }
                if (state == 0) {
                    Sleep(10);
                    continue;
                }
                uint32_t msgLen = 0;
                char* msg = ReadMessage(&msgLen);
                if (!msg) {
                    // Client disconnected or read error
                    break;
                }

                // Enqueue for the bot thread to process
                {
                    std::lock_guard<std::mutex> lock(g_inboundMutex);
                    g_inboundQueue.push({msg, msgLen});
                }
            }

            g_clientConnected.store(false);
            ClearOutboundQueue();
            GWA3::Log::Info("[LLM-IPC] Client disconnected");

            {
                std::lock_guard<std::mutex> lock(g_pipeMutex);
                if (g_pipe != INVALID_HANDLE_VALUE) {
                    DisconnectNamedPipe(g_pipe);
                    CloseHandle(g_pipe);
                    g_pipe = INVALID_HANDLE_VALUE;
                }
            }
        }

        GWA3::Log::Info("[LLM-IPC] IPC thread exiting");
        return 0;
    }

    bool Initialize() {
        if (g_running.load()) return true;
        g_running.store(true);

        g_thread = CreateThread(nullptr, 0, IpcThread, nullptr, 0, nullptr);
        if (!g_thread) {
            GWA3::Log::Error("[LLM-IPC] Failed to create IPC thread");
            g_running.store(false);
            return false;
        }

        g_senderThread = CreateThread(nullptr, 0, SenderThread, nullptr, 0, nullptr);
        if (!g_senderThread) {
            GWA3::Log::Error("[LLM-IPC] Failed to create sender thread");
            g_running.store(false);
            g_outboundCv.notify_all();
            WaitForSingleObject(g_thread, 5000);
            CloseHandle(g_thread);
            g_thread = nullptr;
            return false;
        }

        GWA3::Log::Info("[LLM-IPC] Initialized");
        return true;
    }

    void Shutdown() {
        if (!g_running.load()) return;
        g_running.store(false);
        g_outboundCv.notify_all();

        // If pipe is waiting for connection, break it
        if (g_pipe != INVALID_HANDLE_VALUE) {
            CancelIoEx(g_pipe, nullptr);
        }

        if (g_thread) {
            WaitForSingleObject(g_thread, 5000);
            CloseHandle(g_thread);
            g_thread = nullptr;
        }

        if (g_senderThread) {
            WaitForSingleObject(g_senderThread, 5000);
            CloseHandle(g_senderThread);
            g_senderThread = nullptr;
        }

        // Drain inbound queue
        {
            std::lock_guard<std::mutex> lock(g_inboundMutex);
            while (!g_inboundQueue.empty()) {
                delete[] g_inboundQueue.front().data;
                g_inboundQueue.pop();
            }
        }
        ClearOutboundQueue();

        GWA3::Log::Info("[LLM-IPC] Shutdown complete");
    }

    bool Send(const char* json, uint32_t length) {
        return Send(json, length, OutboundPriority::Snapshot);
    }

    bool Send(const char* json, uint32_t length, OutboundPriority priority) {
        return QueueOutbound(json, length, priority);
    }

    bool HasPending() {
        std::lock_guard<std::mutex> lock(g_inboundMutex);
        return !g_inboundQueue.empty();
    }

    char* Dequeue(uint32_t* outLength) {
        std::lock_guard<std::mutex> lock(g_inboundMutex);
        if (g_inboundQueue.empty()) {
            *outLength = 0;
            return nullptr;
        }
        InboundMsg msg = g_inboundQueue.front();
        g_inboundQueue.pop();
        *outLength = msg.length;
        return msg.data;
    }

    void FreeMsgBuf(char* buf) {
        delete[] buf;
    }

    bool IsClientConnected() {
        return g_clientConnected.load();
    }

    const char* GetPipeName() {
        return PIPE_NAME;
    }

} // namespace GWA3::LLM::IpcServer
