#include <gwa3/core/RuntimeWatchdog.h>

#include <gwa3/core/CrashDiag.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/MemoryMgr.h>

#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace GWA3::RuntimeWatchdog {
namespace {

volatile bool s_running = false;
volatile bool s_failureDetected = false;
HANDLE s_thread = nullptr;
Options s_options = {};
char s_failureReason[128] = {};
bool s_screenshotTaken = false;
uint32_t s_lastMapId = 0;

void SetFailureReason(const char* reason) {
    if (!reason) reason = "unknown";
    strncpy_s(s_failureReason, reason, _TRUNCATE);
    s_failureDetected = true;
}

bool BuildScreenshotPath(const char* tag, char* outPath, size_t outPathSize) {
    if (!tag || !*tag || !outPath || outPathSize == 0) return false;

    char modulePath[MAX_PATH] = {};
    HMODULE hSelf = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCSTR>(&BuildScreenshotPath),
                            &hSelf)) {
        return false;
    }
    if (!GetModuleFileNameA(hSelf, modulePath, MAX_PATH)) return false;

    char* slash = strrchr(modulePath, '\\');
    if (!slash) return false;
    *slash = '\0';

    char screenshotDir[MAX_PATH] = {};
    snprintf(screenshotDir, sizeof(screenshotDir), "%s\\screenshots", modulePath);
    CreateDirectoryA(screenshotDir, nullptr);

    SYSTEMTIME st = {};
    GetLocalTime(&st);
    snprintf(outPath,
             outPathSize,
             "%s\\watchdog_%s_%04u%02u%02u_%02u%02u%02u.bmp",
             screenshotDir,
             tag,
             st.wYear,
             st.wMonth,
             st.wDay,
             st.wHour,
             st.wMinute,
             st.wSecond);
    return true;
}

bool SaveBitmapToBmp(HBITMAP hBitmap, HDC hdc, const char* path) {
    if (!hBitmap || !hdc || !path || !*path) return false;

    BITMAP bmp = {};
    if (!GetObject(hBitmap, sizeof(bmp), &bmp)) return false;

    BITMAPINFOHEADER bi = {};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bmp.bmWidth;
    bi.biHeight = bmp.bmHeight;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    const DWORD imageBytes = static_cast<DWORD>(bmp.bmWidth * bmp.bmHeight * 4);
    void* pixels = std::malloc(imageBytes);
    if (!pixels) return false;

    BITMAPINFO info = {};
    info.bmiHeader = bi;
    const int scanLines = GetDIBits(hdc,
                                    hBitmap,
                                    0,
                                    static_cast<UINT>(bmp.bmHeight),
                                    pixels,
                                    &info,
                                    DIB_RGB_COLORS);
    if (scanLines == 0) {
        std::free(pixels);
        return false;
    }

    FILE* f = nullptr;
    fopen_s(&f, path, "wb");
    if (!f) {
        std::free(pixels);
        return false;
    }

    BITMAPFILEHEADER bfh = {};
    bfh.bfType = 0x4D42;
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfh.bfSize = bfh.bfOffBits + imageBytes;

    fwrite(&bfh, sizeof(bfh), 1, f);
    fwrite(&bi, sizeof(bi), 1, f);
    fwrite(pixels, imageBytes, 1, f);
    fclose(f);
    std::free(pixels);
    return true;
}

void CaptureScreenshot(const char* tag, HWND hwnd) {
    if (s_screenshotTaken) return;
    if (!hwnd || !IsWindow(hwnd)) return;

    RECT rect = {};
    if (!GetWindowRect(hwnd, &rect)) return;
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) return;

    HDC windowDc = GetWindowDC(hwnd);
    if (!windowDc) return;

    HDC memDc = CreateCompatibleDC(windowDc);
    HBITMAP bmp = CreateCompatibleBitmap(windowDc, width, height);
    HGDIOBJ oldObj = nullptr;
    if (memDc && bmp) {
        oldObj = SelectObject(memDc, bmp);
        BitBlt(memDc, 0, 0, width, height, windowDc, 0, 0, SRCCOPY);
    }

    char screenshotPath[MAX_PATH] = {};
    bool saved = false;
    if (memDc && bmp && BuildScreenshotPath(tag ? tag : "failure", screenshotPath, sizeof(screenshotPath))) {
        saved = SaveBitmapToBmp(bmp, memDc, screenshotPath);
    }

    if (oldObj) SelectObject(memDc, oldObj);
    if (bmp) DeleteObject(bmp);
    if (memDc) DeleteDC(memDc);
    ReleaseDC(hwnd, windowDc);

    if (!saved) return;

    s_screenshotTaken = true;
    Log::Error("[WATCHDOG] Screenshot saved: %s", screenshotPath);
    Log::Error("WATCHDOG_SCREENSHOT: %s", screenshotPath);
}

void TerminateIfConfigured(uint32_t exitCode) {
    if (!s_options.terminate_on_failure) return;
    Log::Error("[WATCHDOG] Terminating GW process");
    Log::Shutdown();
    Sleep(100);
    TerminateProcess(GetCurrentProcess(), exitCode);
}

bool DetectCrashDialog(HWND& crashHwnd) {
    const DWORD myPid = GetCurrentProcessId();
    HWND hwnd = nullptr;
    while ((hwnd = FindWindowExA(nullptr, hwnd, "#32770", "Gw.exe")) != nullptr) {
        DWORD windowPid = 0;
        GetWindowThreadProcessId(hwnd, &windowPid);
        if (windowPid == myPid) {
            crashHwnd = hwnd;
            return true;
        }
    }
    return false;
}

DWORD WINAPI ThreadProc(LPVOID) {
    s_lastMapId = MapMgr::GetMapId();
    DWORD lastProgressLog = 0;

    while (s_running) {
        Sleep(s_options.poll_ms ? s_options.poll_ms : 1000u);

        HWND gwHwnd = static_cast<HWND>(MemoryMgr::GetGWWindowHandle());

        if (gwHwnd && IsWindow(gwHwnd)) {
            DWORD_PTR result = 0;
            LRESULT lr = SendMessageTimeoutA(gwHwnd,
                                             WM_NULL,
                                             0,
                                             0,
                                             SMTO_ABORTIFHUNG,
                                             s_options.window_hung_timeout_ms,
                                             &result);
            if (lr == 0 && GetLastError() != 0) {
                SetFailureReason("window_hung");
                Log::Error("[WATCHDOG] GW window is not responding");
                CrashDiag::CaptureProcessState("watchdog_window_hung");
                CaptureScreenshot("window_hung", gwHwnd);
                TerminateIfConfigured(0xDEAD);
            }
        }

        HWND crashHwnd = nullptr;
        if (DetectCrashDialog(crashHwnd)) {
            SetFailureReason("crash_dialog");
            Log::Error("[WATCHDOG] GW crash dialog detected");
            CrashDiag::CaptureProcessState("watchdog_crash_dialog");
            CaptureScreenshot("crash_dialog", crashHwnd);
            TerminateIfConfigured(0xCDA1);
        }

        const uint32_t currentMapId = MapMgr::GetMapId();
        if (s_lastMapId > 0 && currentMapId == 0) {
            SetFailureReason("disconnect");
            Log::Error("[WATCHDOG] MapID dropped from %u to 0", s_lastMapId);
            CrashDiag::CaptureProcessState("watchdog_disconnect");
            CaptureScreenshot("disconnect", gwHwnd);
            TerminateIfConfigured(0xDC);
        }
        if (currentMapId > 0) {
            s_lastMapId = currentMapId;
        }

        if (GameThread::IsInitialized() && currentMapId > 0 &&
            !GameThread::IsResponsive(s_options.game_thread_idle_limit_ms)) {
            SetFailureReason("game_thread_stalled");
            Log::Error("[WATCHDOG] GameThread hook stalled for more than %u ms",
                       s_options.game_thread_idle_limit_ms);
            CrashDiag::CaptureProcessState("watchdog_game_thread_stalled");
            CaptureScreenshot("game_thread_stalled", gwHwnd);
            TerminateIfConfigured(0xDEAD);
        }

        const DWORD now = GetTickCount();
        if (now - lastProgressLog >= 30000u) {
            Log::Info("[WATCHDOG] runtime alive map=%u gameThreadResponsive=%d",
                      currentMapId,
                      GameThread::IsResponsive(s_options.game_thread_idle_limit_ms) ? 1 : 0);
            lastProgressLog = now;
        }
    }

    return 0;
}

bool IsDisabledByEnv() {
    char value[16] = {};
    return GetEnvironmentVariableA("GWA3_DISABLE_RUNTIME_WATCHDOG", value, sizeof(value)) > 0;
}

} // namespace

bool Start(const Options& options) {
    if (s_running) return true;
    if (IsDisabledByEnv()) {
        Log::Warn("[WATCHDOG] Runtime watchdog disabled by GWA3_DISABLE_RUNTIME_WATCHDOG");
        return false;
    }

    s_options = options;
    s_failureDetected = false;
    s_failureReason[0] = '\0';
    s_screenshotTaken = false;
    s_lastMapId = 0;
    s_running = true;
    s_thread = CreateThread(nullptr, 0, ThreadProc, nullptr, 0, nullptr);
    if (!s_thread) {
        s_running = false;
        Log::Error("[WATCHDOG] Failed to create runtime watchdog thread");
        return false;
    }
    Log::Info("[WATCHDOG] Runtime watchdog started");
    return true;
}

void Stop(bool waitForThread) {
    s_running = false;
    if (!s_thread) return;
    if (waitForThread) {
        const DWORD waitResult = WaitForSingleObject(s_thread, 5000);
        if (waitResult == WAIT_TIMEOUT) {
            Log::Warn("[WATCHDOG] Stop timed out; detaching watchdog thread");
        }
    }
    CloseHandle(s_thread);
    s_thread = nullptr;
    Log::Info("[WATCHDOG] Runtime watchdog stopped");
}

bool IsRunning() {
    return s_running;
}

bool FailureDetected() {
    return s_failureDetected;
}

const char* FailureReason() {
    return s_failureReason[0] ? s_failureReason : "none";
}

} // namespace GWA3::RuntimeWatchdog
