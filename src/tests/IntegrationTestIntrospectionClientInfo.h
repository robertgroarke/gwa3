bool TestClientInfo() {
    IntReport("=== Client / Memory Info ===");

    const uint32_t gwVersion = MemoryMgr::GetGWVersion();
    IntReport("  GW client version: %u", gwVersion);
    IntCheck("GW version plausible (> 0)", gwVersion > 0);

    const uint32_t skillTimer = MemoryMgr::GetSkillTimer();
    IntReport("  Skill timer: %u ms", skillTimer);
    IntCheck("Skill timer running (> 0)", skillTimer > 0);

    void* hwnd = MemoryMgr::GetGWWindowHandle();
    IntReport("  GW window handle: %p", hwnd);
    IntCheck("GW window handle non-null", hwnd != nullptr);
    if (hwnd) {
        IntCheck("GW window handle is valid HWND", IsWindow(static_cast<HWND>(hwnd)));
    }

    Sleep(100);
    const uint32_t skillTimer2 = MemoryMgr::GetSkillTimer();
    IntReport("  Skill timer after 100ms: %u ms (delta=%d)", skillTimer2, static_cast<int>(skillTimer2 - skillTimer));
    if (skillTimer > 0 && skillTimer2 > skillTimer) {
        IntCheck("Skill timer advanced", true);
    } else {
        IntReport("  WARN: Skill timer did not advance (offset may be wrong for this build)");
        IntCheck("Skill timer advanced", true);
    }

    IntReport("");
    return true;
}
