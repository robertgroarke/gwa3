#pragma once

#include <cstdint>

namespace GWA3::LLM::ActionExecutor {

    struct ActionResult {
        bool success;
        char error[256]; // empty string if success
    };

    // Initialize the action dispatch table.
    bool Initialize();

    // Shutdown and clear dispatch table.
    void Shutdown();

    // Parse and execute a named action with JSON params.
    // Validates inputs, then dispatches via GameThread::Enqueue().
    // Returns immediately with validation result; actual execution is async.
    // requestId is echoed back in the action_result message.
    ActionResult Execute(const char* actionName, const char* paramsJson, const char* requestId);

} // namespace GWA3::LLM::ActionExecutor
