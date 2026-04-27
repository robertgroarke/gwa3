#pragma once

#include <cstdint>

namespace GWA3::RuntimeWatchdog {

struct Options {
    uint32_t poll_ms = 1000u;
    uint32_t window_hung_timeout_ms = 2000u;
    uint32_t game_thread_idle_limit_ms = 15000u;
    bool terminate_on_failure = true;
};

bool Start(const Options& options = {});
void Stop(bool waitForThread = true);
bool IsRunning();
bool FailureDetected();
const char* FailureReason();

} // namespace GWA3::RuntimeWatchdog
