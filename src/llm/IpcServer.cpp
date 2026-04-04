#include <gwa3/llm/IpcServer.h>
#include <gwa3/core/Log.h>

#include <atomic>
#include <mutex>
#include <queue>

namespace GWA3::LLM::IpcServer {

    static constexpr const char* PIPE_NAME = "\\\\.\\pipe\\gwa3_llm";
    static constexpr DWORD PIPE_BUFFER_SIZE = 64 * 1024;  // 64KB
    static constexpr DWORD CONNECT_TIMEOUT_MS = 500;

    static HANDLE g_pipe = INVALID_HANDLE_VALUE;
    static HANDLE g_thread = nullptr;
    static std::atomic<bool> g_running{false};
    static std::atomic<bool> g_clientConnected{false};

    // Inbound message queue (bridge -> gwa3)
    struct InboundMsg {
        char* data;
        uint32_t length;
    };
    static std::mutex g_inboundMutex;
    static std::queue<InboundMsg> g_inboundQueue;

    // Write exactly `count` bytes to the pipe. Returns false on failure.
    static bool PipeWriteAll(const void* buf, DWORD count) {
        const uint8_t* p = static_cast<const uint8_t*>(buf);
        DWORD remaining = count;
        while (remaining > 0) {
            DWORD written = 0;
            if (!WriteFile(g_pipe, p, remaining, &written, nullptr)) {
                return false;
            }
            p += written;
            remaining -= written;
        }
        return true;
    }

    // Read exactly `count` bytes from the pipe. Returns false on failure/disconnect.
    static bool PipeReadAll(void* buf, DWORD count) {
        uint8_t* p = static_cast<uint8_t*>(buf);
        DWORD remaining = count;
        while (remaining > 0) {
            DWORD bytesRead = 0;
            if (!ReadFile(g_pipe, p, remaining, &bytesRead, nullptr)) {
                return false;
            }
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
            ConnectNamedPipe(g_pipe, &ov);

            DWORD err = GetLastError();
            if (err == ERROR_IO_PENDING) {
                // Wait with timeout so we can check g_running
                while (g_running.load()) {
                    DWORD wait = WaitForSingleObject(ov.hEvent, 500);
                    if (wait == WAIT_OBJECT_0) break;
                }
                if (!g_running.load()) {
                    CancelIo(g_pipe);
                    CloseHandle(ov.hEvent);
                    CloseHandle(g_pipe);
                    g_pipe = INVALID_HANDLE_VALUE;
                    break;
                }
            } else if (err != ERROR_PIPE_CONNECTED && err != 0) {
                GWA3::Log::Error("[LLM-IPC] ConnectNamedPipe failed: %u", err);
                CloseHandle(ov.hEvent);
                CloseHandle(g_pipe);
                g_pipe = INVALID_HANDLE_VALUE;
                Sleep(500);
                continue;
            }

            CloseHandle(ov.hEvent);
            g_clientConnected.store(true);
            GWA3::Log::Info("[LLM-IPC] Client connected");

            // Read messages from the client until disconnect or shutdown
            while (g_running.load()) {
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
            GWA3::Log::Info("[LLM-IPC] Client disconnected");

            DisconnectNamedPipe(g_pipe);
            CloseHandle(g_pipe);
            g_pipe = INVALID_HANDLE_VALUE;
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

        GWA3::Log::Info("[LLM-IPC] Initialized");
        return true;
    }

    void Shutdown() {
        if (!g_running.load()) return;
        g_running.store(false);

        // If pipe is waiting for connection, break it
        if (g_pipe != INVALID_HANDLE_VALUE) {
            CancelIoEx(g_pipe, nullptr);
        }

        if (g_thread) {
            WaitForSingleObject(g_thread, 5000);
            CloseHandle(g_thread);
            g_thread = nullptr;
        }

        // Drain inbound queue
        {
            std::lock_guard<std::mutex> lock(g_inboundMutex);
            while (!g_inboundQueue.empty()) {
                delete[] g_inboundQueue.front().data;
                g_inboundQueue.pop();
            }
        }

        GWA3::Log::Info("[LLM-IPC] Shutdown complete");
    }

    bool Send(const char* json, uint32_t length) {
        if (!g_clientConnected.load() || g_pipe == INVALID_HANDLE_VALUE) {
            return false;
        }

        // Length-prefix framing: [4-byte uint32 length][payload]
        if (!PipeWriteAll(&length, 4)) return false;
        if (!PipeWriteAll(json, length)) return false;
        return true;
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
    void DrainInbound() {
        while (HasPending()) {
            uint32_t len = 0;
            char* msg = Dequeue(&len);
            if (msg) FreeMsgBuf(msg);
        }
    }

    bool IsClientConnected() {
        return g_clientConnected.load();
    }

} // namespace GWA3::LLM::IpcServer
