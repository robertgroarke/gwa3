#pragma once

namespace GWA3::CrashDiag {

    bool Initialize();
    void CaptureProcessState(const char* tag);
    void Shutdown();

} // namespace GWA3::CrashDiag
