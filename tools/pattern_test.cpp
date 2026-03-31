// GWA3-035: Pattern Health Check Tool
// Injects gwa3.dll, reads scan results via shared memory, reports pass/fail.
// Usage: pattern_test.exe [--pid <N>]
// Exit code: 0 if all P0/P1 pass, 1 if any fail.

#include <Windows.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <cstdio>
#include <cstring>
#include <ctime>
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

static HMODULE GetRemoteModuleHandle(HANDLE hProcess, const char* moduleName) {
    HMODULE hMods[1024];
    DWORD cbNeeded;
    if (!EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) return nullptr;
    for (DWORD i = 0; i < cbNeeded / sizeof(HMODULE); i++) {
        char modName[MAX_PATH];
        if (GetModuleFileNameExA(hProcess, hMods[i], modName, sizeof(modName))) {
            char* name = strrchr(modName, '\\');
            if (name && _stricmp(name + 1, moduleName) == 0) return hMods[i];
        }
    }
    return nullptr;
}

static std::string GetToolDir() {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    return std::string(path);
}

static bool InjectAndWait(DWORD pid, const char* dllPath) {
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) return false;

    if (GetRemoteModuleHandle(hProcess, "gwa3.dll")) {
        printf("[*] gwa3.dll already loaded in PID %lu\n", pid);
        CloseHandle(hProcess);
        return true;
    }

    size_t pathLen = strlen(dllPath) + 1;
    LPVOID remotePath = VirtualAllocEx(hProcess, nullptr, pathLen, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remotePath) { CloseHandle(hProcess); return false; }

    WriteProcessMemory(hProcess, remotePath, dllPath, pathLen, nullptr);

    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    FARPROC pLoadLibraryA = GetProcAddress(hKernel32, "LoadLibraryA");

    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(pLoadLibraryA), remotePath, 0, nullptr);
    if (!hThread) {
        VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    WaitForSingleObject(hThread, 15000);
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, remotePath, 0, MEM_RELEASE);

    // Wait for initialization (scanner + offsets resolve)
    printf("[*] Waiting for gwa3.dll initialization...\n");
    Sleep(5000);

    bool loaded = GetRemoteModuleHandle(hProcess, "gwa3.dll") != nullptr;
    CloseHandle(hProcess);
    return loaded;
}

static bool ReadPatternReport(DWORD pid, FILE* reportFile) {
    // Read the gwa3_log.txt that the DLL wrote during initialization
    // The Offsets module logs [OK] and [FAIL] for each pattern
    std::string logPath = GetToolDir() + "gwa3_log.txt";

    FILE* logFile = nullptr;
    fopen_s(&logFile, logPath.c_str(), "r");
    if (!logFile) {
        printf("[!] Cannot read log file: %s\n", logPath.c_str());
        return false;
    }

    int resolved = 0, failed = 0;
    int p0Fail = 0, p1Fail = 0, p2Fail = 0;
    char line[1024];

    printf("\n=== Pattern Scan Results (PID %lu) ===\n\n", pid);
    fprintf(reportFile, "=== Pattern Scan Results (PID %lu) ===\n\n", pid);

    while (fgets(line, sizeof(line), logFile)) {
        // Look for Offsets: [OK] and [FAIL] lines
        if (strstr(line, "Offsets: [OK]")) {
            resolved++;
            printf("  %s", line);
            fprintf(reportFile, "  %s", line);
        } else if (strstr(line, "Offsets: [FAIL]")) {
            failed++;
            if (strstr(line, "(P0)")) p0Fail++;
            else if (strstr(line, "(P1)")) p1Fail++;
            else p2Fail++;
            printf("  %s", line);
            fprintf(reportFile, "  %s", line);
        } else if (strstr(line, "Offsets: Resolved")) {
            printf("\n  %s", line);
            fprintf(reportFile, "\n  %s", line);
        } else if (strstr(line, "Scanner:") && strstr(line, "base=")) {
            printf("  %s", line);
            fprintf(reportFile, "  %s", line);
        }
    }
    fclose(logFile);

    printf("\n--- Summary ---\n");
    printf("  Resolved: %d\n", resolved);
    printf("  Failed:   %d (P0: %d, P1: %d, P2: %d)\n", failed, p0Fail, p1Fail, p2Fail);

    fprintf(reportFile, "\n--- Summary ---\n");
    fprintf(reportFile, "  Resolved: %d\n", resolved);
    fprintf(reportFile, "  Failed:   %d (P0: %d, P1: %d, P2: %d)\n", failed, p0Fail, p1Fail, p2Fail);

    bool criticalPass = (p0Fail == 0 && p1Fail == 0);
    if (criticalPass) {
        printf("\n  [PASS] All P0/P1 patterns resolved successfully.\n");
        fprintf(reportFile, "\n  [PASS] All P0/P1 patterns resolved successfully.\n");
    } else {
        printf("\n  [FAIL] %d critical pattern(s) failed.\n", p0Fail + p1Fail);
        fprintf(reportFile, "\n  [FAIL] %d critical pattern(s) failed.\n", p0Fail + p1Fail);
    }

    return criticalPass;
}

int main(int argc, char* argv[]) {
    printf("=== GWA3 Pattern Health Check ===\n\n");

    DWORD targetPid = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--pid") == 0 && i + 1 < argc) {
            targetPid = static_cast<DWORD>(atol(argv[++i]));
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: pattern_test.exe [--pid <N>]\n");
            printf("  Injects gwa3.dll, runs all pattern scans, reports results.\n");
            printf("  Exit code 0 = all P0/P1 pass, 1 = failure.\n");
            return 0;
        }
    }

    // Find GW process
    if (targetPid == 0) {
        std::vector<GWProcess> gwProcesses;
        EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&gwProcesses));
        if (gwProcesses.empty()) {
            printf("[!] No Guild Wars windows found.\n");
            return 1;
        }
        targetPid = gwProcesses[0].pid;
        printf("[*] Using first GW instance: PID %lu — \"%s\"\n", targetPid, gwProcesses[0].windowTitle);
    }

    // Inject
    std::string dllPath = GetToolDir() + "gwa3.dll";
    DWORD attrib = GetFileAttributesA(dllPath.c_str());
    if (attrib == INVALID_FILE_ATTRIBUTES) {
        printf("[!] gwa3.dll not found at: %s\n", dllPath.c_str());
        return 1;
    }

    printf("[*] Injecting gwa3.dll into PID %lu...\n", targetPid);
    if (!InjectAndWait(targetPid, dllPath.c_str())) {
        printf("[!] Injection failed.\n");
        return 1;
    }
    printf("[+] Injection successful, reading scan results...\n");

    // Open report file
    char timestamp[64];
    time_t now = time(nullptr);
    struct tm tm_buf;
    localtime_s(&tm_buf, &now);
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &tm_buf);

    std::string reportPath = GetToolDir() + "pattern_report.txt";
    FILE* reportFile = nullptr;
    fopen_s(&reportFile, reportPath.c_str(), "a");
    if (reportFile) {
        fprintf(reportFile, "\n========================================\n");
        fprintf(reportFile, "Pattern Health Check — %s (PID %lu)\n", timestamp, targetPid);
        fprintf(reportFile, "========================================\n\n");
    } else {
        printf("[!] Cannot create report file, printing to stdout only.\n");
        reportFile = stdout;
    }

    bool pass = ReadPatternReport(targetPid, reportFile);

    if (reportFile != stdout) {
        fclose(reportFile);
        printf("\n[*] Report saved to: %s\n", reportPath.c_str());
    }

    return pass ? 0 : 1;
}
