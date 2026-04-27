#pragma once

namespace GWA3::LLM {

    // Initialize the LLM bridge subsystem:
    //  1. Starts IpcServer (named pipe listener)
    //  2. Starts snapshot timer thread (sends periodic game state)
    //  3. Starts action poll loop (dequeues and executes bridge commands)
    //
    // Call after GameThread::Initialize() succeeds.
    bool Initialize();

    // Shutdown the LLM bridge subsystem.
    // Call before GameThread::Shutdown().
    void Shutdown();

    // Returns true if the LLM bridge is running.
    bool IsRunning();

    // Temporarily suppress snapshot emission while keeping the bridge thread alive.
    void PauseSnapshotsFor(unsigned long milliseconds);

    // Drain inbound action messages from the IPC queue.
    // Call from the init thread to dispatch actions from a safe thread context.
    void DrainInboundActions();

} // namespace GWA3::LLM
