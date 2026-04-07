#include <gwa3/core/Log.h>

#include <Windows.h>
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <mutex>

namespace GWA3::Log {

static FILE* g_logFile = nullptr;
static std::mutex g_mutex;
static bool g_initialized = false;
static bool g_consoleCreated = false;

static void GetTimestamp(char* buf, size_t len) {
    time_t now = time(nullptr);
    struct tm tm_buf;
    localtime_s(&tm_buf, &now);
    strftime(buf, len, "%Y-%m-%d %H:%M:%S", &tm_buf);
}

static bool g_consoleAttached = false;

static void LogMessage(const char* level, const char* fmt, va_list args) {
    std::lock_guard<std::mutex> lock(g_mutex);

    char timestamp[64];
    GetTimestamp(timestamp, sizeof(timestamp));

    char message[2048];
    vsnprintf(message, sizeof(message), fmt, args);

    char line[2200];
    snprintf(line, sizeof(line), "[%s] [%s] %s\n", timestamp, level, message);

    if (g_logFile) {
        fputs(line, g_logFile);
        fflush(g_logFile);
    }

    // Write to console (GW.exe console window)
    if (g_consoleAttached) {
        fputs(line, stdout);
        fflush(stdout);
    }

    OutputDebugStringA(line);
}

void Initialize() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_initialized) return;

    // Get DLL directory for log file placement
    char dllPath[MAX_PATH];
    HMODULE hSelf = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&Initialize),
        &hSelf
    );
    GetModuleFileNameA(hSelf, dllPath, MAX_PATH);

    // Strip filename, append log name
    char* lastSlash = strrchr(dllPath, '\\');
    if (lastSlash) *(lastSlash + 1) = '\0';
    strcat_s(dllPath, "gwa3_log.txt");

    fopen_s(&g_logFile, dllPath, "a");

    // Always create a dedicated console so injected runs have a visible live log window.
    if (AllocConsole()) {
        FILE* dummy = nullptr;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        freopen_s(&dummy, "CONOUT$", "w", stderr);
        SetConsoleTitleA("gwa3 live log");
        g_consoleAttached = true;
        g_consoleCreated = true;
    }

    g_initialized = true;
}

void Shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_logFile) {
        fclose(g_logFile);
        g_logFile = nullptr;
    }
    if (g_consoleCreated) {
        FreeConsole();
        g_consoleCreated = false;
    }
    g_consoleAttached = false;
    g_initialized = false;
}

void Info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    LogMessage("INFO", fmt, args);
    va_end(args);
}

void Warn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    LogMessage("WARN", fmt, args);
    va_end(args);
}

void Error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    LogMessage("ERROR", fmt, args);
    va_end(args);
}

} // namespace GWA3::Log
