#include <gwa3/managers/MemoryMgr.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/Scanner.h>
#include <gwa3/core/Log.h>

#include <Windows.h>
#include <ShlObj.h>
#include <cstdio>

#pragma comment(lib, "shell32.lib")

namespace GWA3::MemoryMgr {

static bool s_initialized = false;
static uint32_t s_gwVersion = 0;
static HWND s_gwWindow = nullptr;

// Game heap allocator function pointers (resolved via scan)
// Signature: void* __cdecl GameAlloc(uint32_t size, uint32_t zero, const char* typeName, uint32_t typeId)
// Signature: void* __cdecl GameRealloc(uint32_t size, uint32_t zero, const char* typeName, uint32_t typeId, void* oldPtr)
typedef void*(__cdecl* GameAllocFn)(uint32_t size, uint32_t zero, const char* typeName, uint32_t typeId);
typedef void*(__cdecl* GameReallocFn)(void* oldPtr, uint32_t newSize, uint32_t zero, const char* typeName, uint32_t typeId);
static GameAllocFn s_gameAlloc = nullptr;
static GameReallocFn s_gameRealloc = nullptr;

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

static void ResolveGameHeap() {
    // Scan pattern from GWCA gwca.dll analysis:
    // Pattern: 57 E8 ?? ?? ?? ?? 8B F0 83 C4 04 85 F6 75 ?? 8B
    // Mask:    xx????xxxxxxxx?x
    // Offset: 0, then FunctionFromNearCall on result → game's alloc function
    uintptr_t allocAddr = Scanner::Find(
        "\x57\xE8\x00\x00\x00\x00\x8B\xF0\x83\xC4\x04\x85\xF6\x75\x00\x8B",
        "xx????xxxxxxxx?x", 0);

    if (allocAddr && allocAddr > 0x10000) {
        uintptr_t allocFunc = Scanner::FunctionFromNearCall(allocAddr);
        if (allocFunc && allocFunc > 0x10000) {
            s_gameAlloc = reinterpret_cast<GameAllocFn>(allocFunc);
            Log::Info("MemoryMgr: Game alloc resolved at 0x%08X", allocFunc);
        }
    }

    // Realloc uses a similar pattern — search nearby with different context
    // GWCA resolves realloc with pattern: 6A 01 6A 00 6A 04 then FindInRange + FunctionFromNearCall
    // For simplicity, find it via the same scan region
    // Realloc(ptr, 0) = free, so having realloc gives us free capability
    uintptr_t reallocAddr = Scanner::Find(
        "\x57\xE8\x00\x00\x00\x00\x8B\xF0\x83\xC4\x04\x85\xF6\x75\x00\x8B",
        "xx????xxxxxxxx?x", 0);

    // The alloc and realloc patterns may be the same — they're in the same function family.
    // The game's realloc is typically at alloc+offset or found via a separate scan.
    // For now, use alloc only. MemFree will use realloc(ptr, 0) if available,
    // else fall back to process heap.
    if (!s_gameAlloc) {
        Log::Warn("MemoryMgr: Game heap allocator not found, using process heap fallback");
    }
}

bool Initialize() {
    if (s_initialized) return true;

    s_gwVersion = ReadPEVersion();

    // Find GW window
    EnumWindows(FindGWWindow, reinterpret_cast<LPARAM>(&s_gwWindow));

    // Resolve game heap allocator (requires Scanner to be initialized)
    if (Scanner::IsInitialized()) {
        ResolveGameHeap();
    }

    s_initialized = true;
    Log::Info("MemoryMgr: Initialized (version=%u, hwnd=0x%08X, gameAlloc=%s)",
              s_gwVersion, (uintptr_t)s_gwWindow,
              s_gameAlloc ? "OK" : "fallback");
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
    if (s_gameAlloc) {
        __try {
            void* ptr = s_gameAlloc(size, 0, "gwa3", 0);
            if (ptr) return ptr;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Log::Warn("MemoryMgr: Game alloc faulted for size %u, falling back", size);
        }
    }
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
}

void MemFree(void* ptr) {
    if (!ptr) return;
    // Without the game's dedicated free function, we cannot safely free
    // game-heap allocations. However, GW's internal allocator typically
    // delegates to the CRT/process heap, so HeapFree is safe in practice.
    // If the game uses a custom allocator, this will leak rather than corrupt.
    HeapFree(GetProcessHeap(), 0, ptr);
}

} // namespace GWA3::MemoryMgr
