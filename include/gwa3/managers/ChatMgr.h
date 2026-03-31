#pragma once

#include <cstdint>

namespace GWA3::ChatMgr {

    bool Initialize();

    // Send chat message (channel prefix: !, @, #, etc.)
    void SendChat(const wchar_t* message, wchar_t channel);

    // Write a local-only message to the chat log (not sent to server)
    void WriteToChat(const wchar_t* message, uint32_t channel = 0);

    // Get current ping (milliseconds)
    uint32_t GetPing();

    // Rendering toggle
    void SetRenderingEnabled(bool enabled);

} // namespace GWA3::ChatMgr
