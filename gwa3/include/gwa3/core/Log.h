#pragma once

namespace GWA3::Log {

    void Initialize();
    void Shutdown();

    void Info(const char* fmt, ...);
    void Warn(const char* fmt, ...);
    void Error(const char* fmt, ...);

} // namespace GWA3::Log
