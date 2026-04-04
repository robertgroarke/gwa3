#pragma once

#include <Windows.h>
#include <cstdint>

namespace GWA3::LLM::IpcServer {

    // Initialize the named pipe server on a dedicated thread.
    // Pipe name: \\.\pipe\gwa3_llm
    // Returns true if the IPC thread started successfully.
    bool Initialize();

    // Shutdown the pipe server, close handles, join the IPC thread.
    void Shutdown();

    // --- Outbound (gwa3 -> bridge) ---

    // Send a JSON message to the connected bridge client.
    // Thread-safe. Messages are length-prefixed (4-byte uint32 + payload).
    // Returns false if no client is connected or write fails.
    bool Send(const char* json, uint32_t length);

    // --- Inbound (bridge -> gwa3) ---

    // Check if there are pending inbound messages.
    bool HasPending();

    // Dequeue the next inbound message. Caller must free the returned buffer
    // with FreeMsgBuf(). Returns nullptr if queue is empty.
    // outLength receives the JSON payload length (excluding null terminator).
    char* Dequeue(uint32_t* outLength);
n    // Drain all pending inbound messages (used on client disconnect).
    void DrainInbound();

    // Free a buffer returned by Dequeue().
    void FreeMsgBuf(char* buf);

    // --- Status ---

    // Returns true if a bridge client is currently connected.
    bool IsClientConnected();

} // namespace GWA3::LLM::IpcServer
