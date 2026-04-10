#pragma once

namespace GWA3::LLM::EventPush {

    // Initialize: register StoC callbacks for key game events.
    // Pushes JSON event messages to IpcServer immediately when events occur.
    // Must be called after StoC::Initialize() and IpcServer::Initialize().
    bool Initialize();

    void Shutdown();
n    // Flush queued events to IPC (call from bridge thread, not StoC callbacks)
    void FlushEvents();

} // namespace GWA3::LLM::EventPush
