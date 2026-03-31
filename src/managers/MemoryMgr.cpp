#include <gwa3/managers/MemoryMgr.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Log.h>

#include <Windows.h>
#include <ShlObj.h>
#include <cstdio>

#pragma comment(lib, "shell32.lib")

namespace GWA3::MemoryMgr {

static bool s_initialized = false;
static uint32_t s_gwVersion = 0;
static HWND s_gwWindow = nullptr;

// Callback for EnumThreadWindows to find the GW window
static BOOL CALLBACK FindGWWindow(HWND hwnd, LPARAM lParam) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != GetCurrentProcessId()) return TRUE;

    char className[64];
    GetClassNameA(hwnd, className, sizeof(className));
    // GW uses "ArenaNet_Dx_Window_Class"
    if (strstr(className, "ArenaNet") != nullptr) {
        *reinterpret_cast<HWND*>(lParam) = hwnd;
        return FALSE; // stop enumeration
    }
    return TRUE;
}

static uint32_t ReadPEVersion() {
    HMODULE gwModule = GetModuleHandleA(nullptr);
    if (!gwModule) return 0;

    // Read version from the VS_FIXEDFILEINFO in the PE resource
    HRSRC hRes = FindResourceA(gwModule, MAKEINTRESOURCEA(VS_VERSION_INFO), RT_VERSION);
    if (!hRes) return 0;

    HGLOBAL hData = LoadResource(gwModule, hRes);
    if (!hData) return 0;

    void* pData = LockResource(hData);
    if (!pData) return 0;

    VS_FIXEDFILEINFO* pFileInfo = nullptr;
    UINT len = 0;
    if (!VerQueryValueA(pData, "\\", reinterpret_cast<void**>(&pFileInfo), &len)) return 0;
    if (!pFileInfo) return 0;

    // GW build number is in the low word of FileVersionLS
    return pFileInfo->dwFileVersionLS & 0xFFFF;
}

bool Initialize() {
    if (s_initialized) return true;

    s_gwVersion = ReadPEVersion();

    // Find GW window
    EnumWindows(FindGWWindow, reinterpret_cast<LPARAM>(&s_gwWindow));

    s_initialized = true;
    Log::Info("MemoryMgr: Initialized (version=%u, hwnd=0x%08X)", s_gwVersion, (uintptr_t)s_gwWindow);
    return true;
}

uint32_t GetGWVersion() {
    return s_gwVersion;
}

uint32_t GetSkillTimer() {
    if (!Offsets::SkillTimer) return 0;
    return *reinterpret_cast<uint32_t*>(Offsets::SkillTimer);
}

void* GetGWWindowHandle() {
    return s_gwWindow;
}

bool GetPersonalDir(wchar_t* buf, uint32_t bufLen) {
    if (!buf || bufLen == 0) return false;

    // GW stores user data in Documents\Guild Wars
    wchar_t docsPath[MAX_PATH];
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, docsPath))) {
        return false;
    }

    int written = _snwprintf_s(buf, bufLen, _TRUNCATE, L"%s\\Guild Wars", docsPath);
    return written > 0;
}

void* MemAlloc(uint32_t size) {
    // TODO: hook into GW's game heap allocator when offset is available
    // For now, use process heap as fallback
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
}

void MemFree(void* ptr) {
    if (!ptr) return;
    // TODO: use GW's game heap free when offset is available
    HeapFree(GetProcessHeap(), 0, ptr);
}

} // namespace GWA3::MemoryMgr
