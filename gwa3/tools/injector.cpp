// GWA3-002: Standalone DLL Injector for gwa3.dll -> GW.exe
// Finds GW.exe by window class, injects gwa3.dll via CreateRemoteThread(LoadLibraryA)

#include <Windows.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#pragma comment(lib, "Psapi.lib")

static const char* GW_WINDOW_CLASS = "ArenaNet_Dx_Window_Class";

struct GWProcess {
    DWORD pid;
    HWND hwnd;
    char windowTitle[256];
};

static BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam) {
    char className[256];
    if (GetClassNameA(hwnd, className, sizeof(className)) == 0) return TRUE;
    if (strcmp(className, GW_WINDOW_CLASS) != 0) return TRUE;
    if (!IsWindowVisible(hwnd)) return TRUE;

    auto* processes = reinterpret_cast<std::vector<GWProcess>*>(lParam);
    GWProcess gw{};
    gw.hwnd = hwnd;
    GetWindowThreadProcessId(hwnd, &gw.pid);
    GetWindowTextA(hwnd, gw.windowTitle, sizeof(gw.windowTitle));
    processes->push_back(gw);
    return TRUE;
}

static std::string GetDllPath() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    char* lastSlash = strrchr(exePath, '\\');
    if (lastSlash) *(lastSlash + 1) = '\0';
    return std::string(exePath) + "gwa3.dll";
}

static HMODULE GetRemoteModuleHandle(HANDLE hProcess, const char* moduleName) {
    HMODULE hMods[1024];
    DWORD cbNeeded;
    if (!EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) return nullptr;

    for (DWORD i = 0; i < cbNeeded / sizeof(HMODULE); i++) {
        char modName[MAX_PATH];
        if (GetModuleFileNameExA(hProcess, hMods[i], modName, sizeof(modName))) {
            char* name = strrchr(modName, '\\');
            if (name && _stricmp(name + 1, moduleName) == 0) {
                return hMods[i];
            }
        }
    }
    return nullptr;
}

static bool InjectDll(DWORD pid, const char* dllPath) {
    printf("[*] Opening process %lu...\n", pid);
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        printf("[!] OpenProcess failed (error %lu). Run as Administrator?\n", GetLastError());
        return false;
    }

    // Check if already injected
    HMODULE existing = GetRemoteModuleHandle(hProcess, "gwa3.dll");
    if (existing) {
        printf("[!] gwa3.dll is already loaded at 0x%08X in PID %lu\n",
               (unsigned)(uintptr_t)existing, pid);
        CloseHandle(hProcess);
        return false;
    }

    size_t pathLen = strlen(dllPath) + 1;

    printf("[*] Allocating %zu bytes in remote process...\n", pathLen);
    LPVOID remotePath = VirtualAllocEx(hProcess, nullptr, pathLen, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remotePath) {
        printf("[!] VirtualAllocEx failed (error %lu)\n", GetLastError());
        CloseHandle(hProcess);
        return false;
    }

    printf("[*] Writing DLL path to remote memory...\n");
    if (!WriteProcessMemory(hProcess, remotePath, dllPath, pathLen, nullptr)) {
        printf("[!] WriteProcessMemory failed (error %lu)\n", GetLastError());
        VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    FARPROC pLoadLibraryA = GetProcAddress(hKernel32, "LoadLibraryA");

    printf("[*] Creating remote thread (LoadLibraryA)...\n");
    HANDLE hThread = CreateRemoteThread(
        hProcess, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(pLoadLibraryA),
        remotePath, 0, nullptr
    );
    if (!hThread) {
        printf("[!] CreateRemoteThread failed (error %lu)\n", GetLastError());
        VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    printf("[*] Waiting for remote thread...\n");
    WaitForSingleObject(hThread, 10000);

    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);

    VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);

    // Verify injection by finding the module
    HMODULE hRemoteDll = GetRemoteModuleHandle(hProcess, "gwa3.dll");
    CloseHandle(hProcess);

    if (hRemoteDll) {
        printf("[+] SUCCESS: gwa3.dll loaded at 0x%08X in PID %lu\n",
               (unsigned)(uintptr_t)hRemoteDll, pid);
        return true;
    } else {
        printf("[!] FAILED: LoadLibraryA returned 0x%08X — DLL not found in module list\n", exitCode);
        return false;
    }
}

static void PrintUsage(const char* argv0) {
    printf("Usage: %s [--pid <N>]\n", argv0);
    printf("  --pid <N>   Inject into specific process ID\n");
    printf("  (no args)   Auto-detect GW.exe windows\n");
}

int main(int argc, char* argv[]) {
    printf("=== GWA3 DLL Injector ===\n\n");

    DWORD targetPid = 0;

    // Parse args
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--pid") == 0 && i + 1 < argc) {
            targetPid = static_cast<DWORD>(atol(argv[++i]));
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            PrintUsage(argv[0]);
            return 0;
        } else {
            printf("[!] Unknown argument: %s\n", argv[i]);
            PrintUsage(argv[0]);
            return 1;
        }
    }

    // Resolve DLL path (same directory as injector.exe)
    std::string dllPath = GetDllPath();
    DWORD attrib = GetFileAttributesA(dllPath.c_str());
    if (attrib == INVALID_FILE_ATTRIBUTES) {
        printf("[!] gwa3.dll not found at: %s\n", dllPath.c_str());
        printf("    Build the DLL first and place it next to injector.exe\n");
        return 1;
    }
    printf("[*] DLL path: %s\n", dllPath.c_str());

    if (targetPid != 0) {
        // Direct PID injection
        printf("[*] Target PID: %lu\n\n", targetPid);
        return InjectDll(targetPid, dllPath.c_str()) ? 0 : 1;
    }

    // Auto-detect GW windows
    std::vector<GWProcess> gwProcesses;
    EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&gwProcesses));

    if (gwProcesses.empty()) {
        printf("[!] No Guild Wars windows found (class: %s)\n", GW_WINDOW_CLASS);
        printf("    Make sure GW.exe is running.\n");
        return 1;
    }

    if (gwProcesses.size() == 1) {
        printf("[*] Found GW.exe: PID %lu — \"%s\"\n\n",
               gwProcesses[0].pid, gwProcesses[0].windowTitle);
        return InjectDll(gwProcesses[0].pid, dllPath.c_str()) ? 0 : 1;
    }

    // Multiple GW windows — prompt user
    printf("[*] Found %zu Guild Wars windows:\n\n", gwProcesses.size());
    for (size_t i = 0; i < gwProcesses.size(); i++) {
        printf("  [%zu] PID %lu — \"%s\"\n",
               i + 1, gwProcesses[i].pid, gwProcesses[i].windowTitle);
    }

    printf("\nSelect window (1-%zu): ", gwProcesses.size());
    fflush(stdout);

    int choice = 0;
    if (scanf_s("%d", &choice) != 1 || choice < 1 || choice > static_cast<int>(gwProcesses.size())) {
        printf("[!] Invalid selection.\n");
        return 1;
    }

    printf("\n");
    return InjectDll(gwProcesses[choice - 1].pid, dllPath.c_str()) ? 0 : 1;
}
