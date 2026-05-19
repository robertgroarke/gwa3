#pragma once

#include <Windows.h>
#include <cstdint>

namespace GWA3::LLM::IpcServer {

    enum class OutboundPriority : uint8_t {
        Snapshot = 0,
        Event = 1,
        Heartbeat = 2,
        ActionResult = 3,
    };

    // Initialize the named pipe server on a dedicated thread.
    // Pipe name: \\.\pipe\gwa3_llm
    // Returns true if the IPC thread started successfully.
    bool Initialize();

    // Shutdown the pipe server, close handles, join the IPC thread.
    void Shutdown();

    // --- Outbound (gwa3 -> bridge) ---

    // Send a JSON message to the connected bridge client.
    // Thread-safe. Messages are length-prefixed (4-byte uint32 + payload).
    // Returns false if no client is connected or the outbound queue is full.
    bool Send(const char* json, uint32_t length);
    bool Send(const char* json, uint32_t length, OutboundPriority priority);

    // --- Inbound (bridge -> gwa3) ---

    // Check if there are pending inbound messages.
    bool HasPending();

    // Dequeue the next inbound message. Caller must free the returned buffer
    // with FreeMsgBuf(). Returns nullptr if queue is empty.
    // outLength receives the JSON payload length (excluding null terminator).
    char* Dequeue(uint32_t* outLength);

    // Free a buffer returned by Dequeue().
    void FreeMsgBuf(char* buf);

    // --- Status ---

    // Returns true if a bridge client is currently connected.
    bool IsClientConnected();

    // Returns the pipe name this server listens on.
    const char* GetPipeName();

} // namespace GWA3::LLM::IpcServer
