// Standalone DLL Injector for gwa3.dll -> GW.exe
// Supports multi-client: --all, --list, --eject, --pid
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
    bool injected; // true if gwa3.dll is already loaded
};

// ===== Window/Process Discovery =====

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
    gw.injected = false;
    processes->push_back(gw);
    return TRUE;
}

static std::vector<GWProcess> FindGWProcesses() {
    std::vector<GWProcess> processes;
    EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&processes));

    // Check injection status for each
    for (auto& gw : processes) {
        HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, gw.pid);
        if (hProc) {
            HMODULE hMods[1024];
            DWORD cbNeeded;
            if (EnumProcessModules(hProc, hMods, sizeof(hMods), &cbNeeded)) {
                for (DWORD i = 0; i < cbNeeded / sizeof(HMODULE); i++) {
                    char modName[MAX_PATH];
                    if (GetModuleFileNameExA(hProc, hMods[i], modName, sizeof(modName))) {
                        char* name = strrchr(modName, '\\');
                        if (name && _stricmp(name + 1, "gwa3.dll") == 0) {
                            gw.injected = true;
                            break;
                        }
                    }
                }
            }
            CloseHandle(hProc);
        }
    }
    return processes;
}

// ===== DLL Path =====

static std::string s_dllNameOverride; // set by --dll flag

static std::string GetDllPath() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    char* lastSlash = strrchr(exePath, '\\');
    if (lastSlash) *(lastSlash + 1) = '\0';
    std::string name = s_dllNameOverride.empty() ? "gwa3.dll" : s_dllNameOverride;
    return std::string(exePath) + name;
}

// ===== Remote Module Lookup =====

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

// ===== Mode Flags =====

static bool SetModeFlag(const char* flagName) {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat_s(path, flagName);

    FILE* f = nullptr;
    fopen_s(&f, path, "w");
    if (f) {
        fprintf(f, "1\n");
        fclose(f);
        printf("[*] Mode flag set: %s\n", path);
        return true;
    }
    return false;
}

static void ClearModeFlags() {
    char dir[MAX_PATH];
    GetModuleFileNameA(nullptr, dir, MAX_PATH);
    char* slash = strrchr(dir, '\\');
    if (slash) *(slash + 1) = '\0';

    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%sgwa3_llm_mode.flag", dir);
    DeleteFileA(path);
    snprintf(path, sizeof(path), "%sgwa3_llm_advisory.flag", dir);
    DeleteFileA(path);
}

// ===== Injection =====

static bool InjectDll(DWORD pid, const char* dllPath) {
    printf("[*] Injecting into PID %lu...\n", pid);
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        printf("[!] OpenProcess failed (error %lu). Run as Administrator?\n", GetLastError());
        return false;
    }

    // Check if already injected
    std::string dllFileName = s_dllNameOverride.empty() ? "gwa3.dll" : s_dllNameOverride;
    HMODULE existing = GetRemoteModuleHandle(hProcess, dllFileName.c_str());
    if (existing) {
        printf("[!] %s already loaded at 0x%08X in PID %lu\n", dllFileName.c_str(),
               (unsigned)(uintptr_t)existing, pid);
        CloseHandle(hProcess);
        return false;
    }

    size_t pathLen = strlen(dllPath) + 1;

    LPVOID remotePath = VirtualAllocEx(hProcess, nullptr, pathLen, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remotePath) {
        printf("[!] VirtualAllocEx failed (error %lu)\n", GetLastError());
        CloseHandle(hProcess);
        return false;
    }

    if (!WriteProcessMemory(hProcess, remotePath, dllPath, pathLen, nullptr)) {
        printf("[!] WriteProcessMemory failed (error %lu)\n", GetLastError());
        VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    FARPROC pLoadLibraryA = GetProcAddress(hKernel32, "LoadLibraryA");

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

    WaitForSingleObject(hThread, 10000);

    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);

    HMODULE hRemoteDll = GetRemoteModuleHandle(hProcess, dllFileName.c_str());
    CloseHandle(hProcess);

    if (hRemoteDll) {
        printf("[+] SUCCESS: %s loaded at 0x%08X in PID %lu\n", dllFileName.c_str(),
               (unsigned)(uintptr_t)hRemoteDll, pid);
        return true;
    } else {
        printf("[!] FAILED: LoadLibraryA returned 0x%08X â€” DLL not found\n", exitCode);
        return false;
    }
}

// ===== Ejection =====

static bool EjectDll(DWORD pid) {
    printf("[*] Ejecting gwa3.dll from PID %lu...\n", pid);
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        printf("[!] OpenProcess failed (error %lu)\n", GetLastError());
        return false;
    }

    HMODULE hRemoteDll = GetRemoteModuleHandle(hProcess, "gwa3.dll");
    if (!hRemoteDll) {
        printf("[!] gwa3.dll is not loaded in PID %lu\n", pid);
        CloseHandle(hProcess);
        return false;
    }

    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    FARPROC pFreeLibrary = GetProcAddress(hKernel32, "FreeLibrary");

    HANDLE hThread = CreateRemoteThread(
        hProcess, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(pFreeLibrary),
        hRemoteDll, 0, nullptr
    );
    if (!hThread) {
        printf("[!] CreateRemoteThread(FreeLibrary) failed (error %lu)\n", GetLastError());
        CloseHandle(hProcess);
        return false;
    }

    WaitForSingleObject(hThread, 10000);

    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);

    // Verify ejection
    HMODULE stillThere = GetRemoteModuleHandle(hProcess, "gwa3.dll");
    CloseHandle(hProcess);

    if (!stillThere) {
        printf("[+] SUCCESS: gwa3.dll ejected from PID %lu\n", pid);
        return true;
    } else {
        printf("[!] FAILED: gwa3.dll still loaded after FreeLibrary (rc=%lu)\n", exitCode);
        return false;
    }
}

// ===== CLI =====

static void PrintUsage(const char* argv0) {
    printf("Usage: %s [command] [options]\n\n", argv0);
    printf("Commands:\n");
    printf("  (default)       Auto-detect and inject into GW.exe\n");
    printf("  --list          Show all GW windows with PID and injection status\n");
    printf("  --all           Inject into ALL running GW instances\n");
    printf("  --eject         Eject gwa3.dll from all injected instances\n");
    printf("  --eject --pid N Eject from specific PID\n");
    printf("  --llm           Inject in LLM agent mode (named pipe bridge for Gemma 4)\n");
    printf("  --llm-advisory  Inject in advisory mode (Froggy bot + LLM bridge together)\n");
    printf("\nOptions:\n");
    printf("  --pid <N>       Target specific process ID\n");
    printf("  --dll <name>    DLL filename next to injector.exe (default: gwa3.dll)\n");
    printf("  -h, --help      Show this help\n");
}

int main(int argc, char* argv[]) {
    printf("=== GWA3 Multi-Client Injector ===\n\n");

    DWORD targetPid = 0;
    bool doList = false;
    bool doAll = false;
    bool doEject = false;
    bool doLlm = false;
    bool doLlmAdvisory = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--pid") == 0 && i + 1 < argc) {
            targetPid = static_cast<DWORD>(atol(argv[++i]));
        } else if (strcmp(argv[i], "--dll") == 0 && i + 1 < argc) {
            s_dllNameOverride = argv[++i];
        } else if (strcmp(argv[i], "--list") == 0) {
            doList = true;
        } else if (strcmp(argv[i], "--all") == 0) {
            doAll = true;
        } else if (strcmp(argv[i], "--eject") == 0) {
            doEject = true;
        } else if (strcmp(argv[i], "--llm") == 0) {
            doLlm = true;
        } else if (strcmp(argv[i], "--llm-advisory") == 0) {
            doLlmAdvisory = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            PrintUsage(argv[0]);
            return 0;
        } else {
            printf("[!] Unknown argument: %s\n", argv[i]);
            PrintUsage(argv[0]);
            return 1;
        }
    }

    // === --list ===
    if (doList) {
        auto processes = FindGWProcesses();
        if (processes.empty()) {
            printf("No Guild Wars windows found.\n");
            return 0;
        }
        printf("  %-8s %-10s %-10s %s\n", "PID", "INJECTED", "HWND", "TITLE");
        printf("  %-8s %-10s %-10s %s\n", "---", "--------", "----", "-----");
        for (const auto& gw : processes) {
            printf("  %-8lu %-10s 0x%08X %s\n",
                   gw.pid,
                   gw.injected ? "YES" : "no",
                   (unsigned)(uintptr_t)gw.hwnd,
                   gw.windowTitle);
        }
        printf("\n%zu client(s) found.\n", processes.size());
        return 0;
    }

    // === --eject ===
    if (doEject) {
        if (targetPid != 0) {
            return EjectDll(targetPid) ? 0 : 1;
        }
        // Eject from all injected instances
        auto processes = FindGWProcesses();
        int ejected = 0, failed = 0;
        for (const auto& gw : processes) {
            if (gw.injected) {
                if (EjectDll(gw.pid)) ejected++;
                else failed++;
            }
        }
        if (ejected == 0 && failed == 0) {
            printf("No injected GW instances found.\n");
        } else {
            printf("\nEjected: %d, Failed: %d\n", ejected, failed);
        }
        return failed > 0 ? 1 : 0;
    }

    // Resolve DLL path
    std::string dllPath = GetDllPath();
    DWORD attrib = GetFileAttributesA(dllPath.c_str());
    if (attrib == INVALID_FILE_ATTRIBUTES) {
        printf("[!] gwa3.dll not found at: %s\n", dllPath.c_str());
        printf("    Build the DLL first and place it next to injector.exe\n");
        return 1;
    }
    printf("[*] DLL: %s\n", dllPath.c_str());

    // === Mode flags ===
    ClearModeFlags();
    if (doLlm) SetModeFlag("gwa3_llm_mode.flag");
    if (doLlmAdvisory) SetModeFlag("gwa3_llm_advisory.flag");

    // === --pid (single target) ===
    if (targetPid != 0) {
        return InjectDll(targetPid, dllPath.c_str()) ? 0 : 1;
    }

    auto gwProcesses = FindGWProcesses();
    if (gwProcesses.empty()) {
        printf("[!] No Guild Wars windows found.\n");
        return 1;
    }

    // === --all ===
    if (doAll) {
        printf("[*] Found %zu GW instance(s)\n\n", gwProcesses.size());
        int injected = 0, skipped = 0, failed = 0;
        for (const auto& gw : gwProcesses) {
            if (gw.injected) {
                printf("[*] PID %lu â€” already injected, skipping\n", gw.pid);
                skipped++;
            } else {
                if (InjectDll(gw.pid, dllPath.c_str())) injected++;
                else failed++;
            }
        }
        printf("\nInjected: %d, Skipped: %d, Failed: %d\n", injected, skipped, failed);
        return failed > 0 ? 1 : 0;
    }

    // === Default: single/interactive ===
    if (gwProcesses.size() == 1) {
        printf("[*] Found: PID %lu â€” \"%s\"%s\n\n",
               gwProcesses[0].pid, gwProcesses[0].windowTitle,
               gwProcesses[0].injected ? " [INJECTED]" : "");
        if (gwProcesses[0].injected) {
            printf("[!] Already injected. Use --eject to remove.\n");
            return 1;
        }
        return InjectDll(gwProcesses[0].pid, dllPath.c_str()) ? 0 : 1;
    }

    // Multiple windows â€” prompt
    printf("[*] Found %zu Guild Wars windows:\n\n", gwProcesses.size());
    for (size_t i = 0; i < gwProcesses.size(); i++) {
        printf("  [%zu] PID %lu â€” \"%s\"%s\n",
               i + 1, gwProcesses[i].pid, gwProcesses[i].windowTitle,
               gwProcesses[i].injected ? " [INJECTED]" : "");
    }

    printf("\nSelect window (1-%zu), or 0 for all: ", gwProcesses.size());
    fflush(stdout);

    int choice = -1;
    if (scanf_s("%d", &choice) != 1 || choice < 0 || choice > static_cast<int>(gwProcesses.size())) {
        printf("[!] Invalid selection.\n");
        return 1;
    }

    printf("\n");

    if (choice == 0) {
        // Inject all
        int injected = 0, failed = 0;
        for (const auto& gw : gwProcesses) {
            if (!gw.injected) {
                if (InjectDll(gw.pid, dllPath.c_str())) injected++;
                else failed++;
            }
        }
        printf("\nInjected: %d, Failed: %d\n", injected, failed);
        return failed > 0 ? 1 : 0;
    }

    return InjectDll(gwProcesses[choice - 1].pid, dllPath.c_str()) ? 0 : 1;
}
