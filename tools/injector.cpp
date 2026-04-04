// GWA3-002/034: Standalone DLL Injector for gwa3.dll -> GW.exe
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

static std::string GetDllPath() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    char* lastSlash = strrchr(exePath, '\\');
    if (lastSlash) *(lastSlash + 1) = '\0';
    return std::string(exePath) + "gwa3.dll";
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

// ===== Remote Environment Variable =====

static bool SetRemoteEnvVar(DWORD pid, const char* name, const char* value) {
    // Write "NAME=VALUE\0" to remote process, then call SetEnvironmentVariableA
    // We use a shellcode approach: write name and value separately, call the API
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) return false;

    size_t nameLen = strlen(name) + 1;
    size_t valLen = strlen(value) + 1;

    // Allocate remote buffers
    LPVOID remoteName = VirtualAllocEx(hProcess, nullptr, nameLen, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    LPVOID remoteVal = VirtualAllocEx(hProcess, nullptr, valLen, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteName || !remoteVal) {
        if (remoteName) VirtualFreeEx(hProcess, remoteName, 0, MEM_RELEASE);
        if (remoteVal) VirtualFreeEx(hProcess, remoteVal, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    WriteProcessMemory(hProcess, remoteName, name, nameLen, nullptr);
    WriteProcessMemory(hProcess, remoteVal, value, valLen, nullptr);

    // We can't directly call SetEnvironmentVariableA with 2 args via CreateRemoteThread
    // (it only passes one LPVOID). Instead, write a tiny shellcode stub.
    // Simpler alternative: just set the env var in OUR process before injecting,
    // since child DLL inherits the env... but GW is already running.
    //
    // Actually, simplest: write the flag to a well-known file that the DLL checks.
    // OR: use a named shared memory section.
    //
    // For now, use a flag file approach — write a marker file next to the DLL.

    VirtualFreeEx(hProcess, remoteName, 0, MEM_RELEASE);
    VirtualFreeEx(hProcess, remoteVal, 0, MEM_RELEASE);
    CloseHandle(hProcess);
    return false; // fallback to file-based approach
}

static bool SetTestModeFlag(const char* flagName) {
    // Write a flag file next to the injector that the DLL will check
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
        printf("[*] Test mode flag set: %s\n", path);
        return true;
    }
    return false;
}

static void ClearTestModeFlags() {
    char dir[MAX_PATH];
    GetModuleFileNameA(nullptr, dir, MAX_PATH);
    char* slash = strrchr(dir, '\\');
    if (slash) *(slash + 1) = '\0';

    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%sgwa3_smoke_test.flag", dir);
    DeleteFileA(path);
    snprintf(path, sizeof(path), "%sgwa3_test_bot.flag", dir);
    DeleteFileA(path);
    snprintf(path, sizeof(path), "%sgwa3_test_commands.flag", dir);
    DeleteFileA(path);
    snprintf(path, sizeof(path), "%sgwa3_test_integration.flag", dir);
    DeleteFileA(path);
    snprintf(path, sizeof(path), "%sgwa3_test_npc_dialog.flag", dir);
    DeleteFileA(path);
    snprintf(path, sizeof(path), "%sgwa3_test_merchant_quote.flag", dir);
    DeleteFileA(path);
    snprintf(path, sizeof(path), "%sgwa3_test_merchant_variant_standard_ptr.flag", dir);
    DeleteFileA(path);
    snprintf(path, sizeof(path), "%sgwa3_test_merchant_variant_legacy_id.flag", dir);
    DeleteFileA(path);
    snprintf(path, sizeof(path), "%sgwa3_test_merchant_variant_legacy_ptr.flag", dir);
    DeleteFileA(path);
    snprintf(path, sizeof(path), "%sgwa3_test_merchant_stage_travel_only.flag", dir);
    DeleteFileA(path);
    snprintf(path, sizeof(path), "%sgwa3_test_merchant_stage_approach_only.flag", dir);
    DeleteFileA(path);
    snprintf(path, sizeof(path), "%sgwa3_test_merchant_stage_target_only.flag", dir);
    DeleteFileA(path);
    snprintf(path, sizeof(path), "%sgwa3_test_merchant_stage_interact_packet_only.flag", dir);
    DeleteFileA(path);
    snprintf(path, sizeof(path), "%sgwa3_test_merchant_stage_interact_only.flag", dir);
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
    HMODULE existing = GetRemoteModuleHandle(hProcess, "gwa3.dll");
    if (existing) {
        printf("[!] gwa3.dll already loaded at 0x%08X in PID %lu\n",
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

    HMODULE hRemoteDll = GetRemoteModuleHandle(hProcess, "gwa3.dll");
    CloseHandle(hProcess);

    if (hRemoteDll) {
        printf("[+] SUCCESS: gwa3.dll loaded at 0x%08X in PID %lu\n",
               (unsigned)(uintptr_t)hRemoteDll, pid);
        return true;
    } else {
        printf("[!] FAILED: LoadLibraryA returned 0x%08X — DLL not found\n", exitCode);
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
    printf("  --smoke         Inject in smoke test mode (GWA3_SMOKE_TEST=1)\n");
    printf("  --test-bot      Inject in bot framework test mode (GWA3_TEST_BOT=1)\n");
    printf("  --test-commands Inject in behavioral command test mode (GWA3_TEST_COMMANDS=1)\n");
    printf("  --test-integ    Inject in integration test mode (char select -> game)\n");
    printf("  --test-npc      Inject in isolated NPC/dialog test mode\n");
    printf("  --test-merchant Inject in isolated merchant/trader quote test mode\n");
    printf("  --llm           Inject in LLM agent mode (named pipe bridge for Gemma 4)\n");
    printf("  --merchant-variant <standard-id|standard-ptr|legacy-id|legacy-ptr>\n");
    printf("  --merchant-stage <full|travel-only|approach-only|target-only|interact-packet-only|interact-only>\n");
    printf("\nOptions:\n");
    printf("  --pid <N>       Target specific process ID\n");
    printf("  -h, --help      Show this help\n");
}

int main(int argc, char* argv[]) {
    printf("=== GWA3 Multi-Client Injector ===\n\n");

    DWORD targetPid = 0;
    bool doList = false;
    bool doAll = false;
    bool doEject = false;
    bool doSmoke = false;
    bool doTestBot = false;
    bool doTestCommands = false;
    bool doTestInteg = false;
    bool doTestNpc = false;
    bool doTestMerchant = false;
    bool doLlm = false;
    const char* merchantVariantFlag = nullptr;
    const char* merchantStageFlag = nullptr;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--pid") == 0 && i + 1 < argc) {
            targetPid = static_cast<DWORD>(atol(argv[++i]));
        } else if (strcmp(argv[i], "--list") == 0) {
            doList = true;
        } else if (strcmp(argv[i], "--all") == 0) {
            doAll = true;
        } else if (strcmp(argv[i], "--eject") == 0) {
            doEject = true;
        } else if (strcmp(argv[i], "--smoke") == 0) {
            doSmoke = true;
        } else if (strcmp(argv[i], "--test-bot") == 0) {
            doTestBot = true;
        } else if (strcmp(argv[i], "--test-commands") == 0) {
            doTestCommands = true;
        } else if (strcmp(argv[i], "--test-integ") == 0) {
            doTestInteg = true;
        } else if (strcmp(argv[i], "--test-npc") == 0) {
            doTestNpc = true;
        } else if (strcmp(argv[i], "--test-merchant") == 0) {
            doTestMerchant = true;
        } else if (strcmp(argv[i], "--llm") == 0) {
            doLlm = true;
        } else if (strcmp(argv[i], "--merchant-variant") == 0 && i + 1 < argc) {
            const char* variant = argv[++i];
            if (strcmp(variant, "standard-id") == 0) {
                merchantVariantFlag = nullptr;
            } else if (strcmp(variant, "standard-ptr") == 0) {
                merchantVariantFlag = "gwa3_test_merchant_variant_standard_ptr.flag";
            } else if (strcmp(variant, "legacy-id") == 0) {
                merchantVariantFlag = "gwa3_test_merchant_variant_legacy_id.flag";
            } else if (strcmp(variant, "legacy-ptr") == 0) {
                merchantVariantFlag = "gwa3_test_merchant_variant_legacy_ptr.flag";
            } else {
                printf("[!] Unknown merchant variant: %s\n", variant);
                PrintUsage(argv[0]);
                return 1;
            }
        } else if (strcmp(argv[i], "--merchant-stage") == 0 && i + 1 < argc) {
            const char* stage = argv[++i];
            if (strcmp(stage, "full") == 0) {
                merchantStageFlag = nullptr;
            } else if (strcmp(stage, "travel-only") == 0) {
                merchantStageFlag = "gwa3_test_merchant_stage_travel_only.flag";
            } else if (strcmp(stage, "approach-only") == 0) {
                merchantStageFlag = "gwa3_test_merchant_stage_approach_only.flag";
            } else if (strcmp(stage, "target-only") == 0) {
                merchantStageFlag = "gwa3_test_merchant_stage_target_only.flag";
            } else if (strcmp(stage, "interact-packet-only") == 0) {
                merchantStageFlag = "gwa3_test_merchant_stage_interact_packet_only.flag";
            } else if (strcmp(stage, "interact-only") == 0) {
                merchantStageFlag = "gwa3_test_merchant_stage_interact_only.flag";
            } else {
                printf("[!] Unknown merchant stage: %s\n", stage);
                PrintUsage(argv[0]);
                return 1;
            }
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

    // === Test mode flags ===
    ClearTestModeFlags();
    if (doSmoke) SetTestModeFlag("gwa3_smoke_test.flag");
    if (doTestBot) SetTestModeFlag("gwa3_test_bot.flag");
    if (doTestCommands) SetTestModeFlag("gwa3_test_commands.flag");
    if (doTestInteg) SetTestModeFlag("gwa3_test_integration.flag");
    if (doTestNpc) SetTestModeFlag("gwa3_test_npc_dialog.flag");
    if (doTestMerchant) SetTestModeFlag("gwa3_test_merchant_quote.flag");
    if (doTestMerchant && merchantVariantFlag) SetTestModeFlag(merchantVariantFlag);
    if (doTestMerchant && merchantStageFlag) SetTestModeFlag(merchantStageFlag);
    if (doLlm) SetTestModeFlag("gwa3_llm_mode.flag");

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
                printf("[*] PID %lu — already injected, skipping\n", gw.pid);
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
        printf("[*] Found: PID %lu — \"%s\"%s\n\n",
               gwProcesses[0].pid, gwProcesses[0].windowTitle,
               gwProcesses[0].injected ? " [INJECTED]" : "");
        if (gwProcesses[0].injected) {
            printf("[!] Already injected. Use --eject to remove.\n");
            return 1;
        }
        return InjectDll(gwProcesses[0].pid, dllPath.c_str()) ? 0 : 1;
    }

    // Multiple windows — prompt
    printf("[*] Found %zu Guild Wars windows:\n\n", gwProcesses.size());
    for (size_t i = 0; i < gwProcesses.size(); i++) {
        printf("  [%zu] PID %lu — \"%s\"%s\n",
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
