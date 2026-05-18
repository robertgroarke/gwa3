#include "IntegrationTestInternal.h"

#include <gwa3/core/CrashDiag.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>
#include <gwa3/core/RenderHook.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/MemoryMgr.h>

#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace GWA3::SmokeTest {

namespace {

volatile bool s_watchdogRunning = false;
volatile bool s_crashDetected = false;
HANDLE s_watchdogThread = nullptr;
volatile bool s_watchdogAllowHungWindowKill = true;

volatile bool s_disconnectDetected = false;
uint32_t s_watchdogLastMapId = 0;
volatile bool s_watchdogCrashScreenshotTaken = false;

bool BuildWatchdogScreenshotPath(const char* tag, char* outPath, size_t outPathSize) {
    if (!tag || !*tag || !outPath || outPathSize == 0) return false;

    char modulePath[MAX_PATH] = {};
    HMODULE hSelf = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCSTR>(&BuildWatchdogScreenshotPath), &hSelf)) {
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
    snprintf(outPath, outPathSize,
             "%s\\watchdog_%s_%04u%02u%02u_%02u%02u%02u.bmp",
             screenshotDir,
             tag,
             st.wYear, st.wMonth, st.wDay,
             st.wHour, st.wMinute, st.wSecond);
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
    const int scanLines = GetDIBits(hdc, hBitmap, 0, static_cast<UINT>(bmp.bmHeight), pixels, &info, DIB_RGB_COLORS);
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

void CaptureWatchdogScreenshot(const char* tag, HWND hwnd) {
    if (s_watchdogCrashScreenshotTaken) return;
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
    if (memDc && bmp && BuildWatchdogScreenshotPath(tag ? tag : "crash", screenshotPath, sizeof(screenshotPath))) {
        saved = SaveBitmapToBmp(bmp, memDc, screenshotPath);
    }

    if (oldObj) SelectObject(memDc, oldObj);
    if (bmp) DeleteObject(bmp);
    if (memDc) DeleteDC(memDc);
    ReleaseDC(hwnd, windowDc);

    if (!saved) return;

    s_watchdogCrashScreenshotTaken = true;
    Log::Error("[WATCHDOG] Screenshot saved: %s", screenshotPath);
    Log::Error("WATCHDOG_SCREENSHOT: %s", screenshotPath);
}

void LogWatchdogTestState() {
    Log::Error("[WATCHDOG] Last known test state: %d passed, %d failed, %d skipped",
               GetIntegrationPassedCount(),
               GetIntegrationFailedCount(),
               GetIntegrationSkippedCount());
}

DWORD WINAPI WatchdogThread(LPVOID) {
    uint32_t lastHeartbeat = RenderHook::GetHeartbeat();
    int stallCount = 0;
    s_watchdogLastMapId = MapMgr::GetMapId();

    while (s_watchdogRunning) {
        Sleep(1000);

        // RenderHook shuts down after bootstrap, so only treat a stalled heartbeat
        // as a crash while the GameThread is not active.
        uint32_t hb = RenderHook::GetHeartbeat();
        if (!GameThread::IsInitialized() && hb == lastHeartbeat && hb > 0) {
            stallCount++;
            if (stallCount >= 3) {
                bool hookIntact = RenderHook::IsHookIntact();
                Log::Error("[WATCHDOG] !!! RENDER FROZEN - heartbeat stuck at %u for >3s. GW likely crashed !!!", hb);
                Log::Error("[WATCHDOG] Hook JMP intact: %s", hookIntact ? "YES" : "NO - OVERWRITTEN!");
                LogWatchdogTestState();
                s_crashDetected = true;
                CrashDiag::CaptureProcessState("watchdog_render_frozen");
                CaptureWatchdogScreenshot("render_frozen", static_cast<HWND>(MemoryMgr::GetGWWindowHandle()));
#if CRASH_TEST == 0
                Log::Error("[WATCHDOG] Terminating GW process...");
                TerminateProcess(GetCurrentProcess(), 0xDEAD);
#else
                Log::Error("[WATCHDOG] (CRASH_TEST mode - NOT killing, continuing observation)");
                stallCount = 0;
#endif
            }
        } else {
            stallCount = 0;
        }
        lastHeartbeat = hb;

        {
            HWND gwHwnd = static_cast<HWND>(MemoryMgr::GetGWWindowHandle());
            if (gwHwnd && s_watchdogAllowHungWindowKill) {
                DWORD_PTR result = 0;
                LRESULT lr = SendMessageTimeoutA(gwHwnd, WM_NULL, 0, 0,
                                                  SMTO_ABORTIFHUNG, 2000, &result);
                if (lr == 0 && GetLastError() != 0) {
                    Log::Error("[WATCHDOG] !!! GW WINDOW NOT RESPONDING - crash dialog likely visible !!!");
                    LogWatchdogTestState();
                    s_crashDetected = true;
                    CrashDiag::CaptureProcessState("watchdog_window_hung");
                    CaptureWatchdogScreenshot("window_hung", gwHwnd);
                    Log::Error("[WATCHDOG] Terminating hung GW process...");
                    Log::Shutdown();
                    Sleep(100);
                    TerminateProcess(GetCurrentProcess(), 0xDEAD);
                }
            }
        }

        {
            DWORD myPid = GetCurrentProcessId();
            HWND crashHwnd = nullptr;
            HWND hwnd = nullptr;
            while ((hwnd = FindWindowExA(nullptr, hwnd, "#32770", "Gw.exe")) != nullptr) {
                DWORD windowPid = 0;
                GetWindowThreadProcessId(hwnd, &windowPid);
                if (windowPid == myPid) { crashHwnd = hwnd; break; }
            }
            if (crashHwnd) {
                Log::Error("[WATCHDOG] !!! GW CRASH DIALOG DETECTED (#32770 'Gw.exe') hwnd=0x%08X pid=%u !!!",
                           reinterpret_cast<uintptr_t>(crashHwnd), myPid);
                LogWatchdogTestState();
                s_crashDetected = true;
                CrashDiag::CaptureProcessState("watchdog_crash_dialog");
                CaptureWatchdogScreenshot("crash_dialog", crashHwnd);
                Log::Error("[WATCHDOG] Terminating after crash dialog...");
                Log::Shutdown();
                Sleep(100);
                TerminateProcess(GetCurrentProcess(), 0xCDA1);
            }
        }

        uint32_t currentMapId = MapMgr::GetMapId();
        if (s_watchdogLastMapId > 0 && currentMapId == 0) {
            Log::Error("[WATCHDOG] !!! DISCONNECT DETECTED - MapID dropped from %u to 0 !!!",
                       s_watchdogLastMapId);
            LogWatchdogTestState();
            s_disconnectDetected = true;
            CrashDiag::CaptureProcessState("watchdog_disconnect");

            Log::Error("[WATCHDOG] Terminating GW process after disconnect...");
            Log::Shutdown();
            Sleep(100);
            TerminateProcess(GetCurrentProcess(), 0xDC);
        }
        if (currentMapId > 0) {
            s_watchdogLastMapId = currentMapId;
        }
    }
    return 0;
}

} // namespace

void StartWatchdog() {
    s_watchdogRunning = true;
    s_crashDetected = false;
    s_disconnectDetected = false;
    s_watchdogLastMapId = 0;
    s_watchdogCrashScreenshotTaken = false;
    s_watchdogAllowHungWindowKill = true;
    s_watchdogThread = CreateThread(nullptr, 0, WatchdogThread, nullptr, 0, nullptr);
}

void StopWatchdog(bool waitForThread) {
    s_watchdogRunning = false;
    if (s_watchdogThread) {
        if (waitForThread) {
            const DWORD waitResult = WaitForSingleObject(s_watchdogThread, 5000);
            if (waitResult == WAIT_TIMEOUT) {
                Log::Warn("[WATCHDOG] StopWatchdog timed out waiting for watchdog thread; detaching handle");
            }
        }
        CloseHandle(s_watchdogThread);
        s_watchdogThread = nullptr;
    }
}

void SetWatchdogHungWindowKillEnabled(bool enabled) {
    s_watchdogAllowHungWindowKill = enabled;
}

bool ShouldAbortForRuntimeFailure() {
    return s_crashDetected || s_disconnectDetected;
}

const char* RuntimeFailureReason() {
    return s_disconnectDetected ? "disconnect detected" : "crash detected";
}

bool RuntimeCrashDetected() {
    return s_crashDetected;
}

bool RuntimeDisconnectDetected() {
    return s_disconnectDetected;
}

bool AbortWorkflowIfRuntimeFailed(const char* afterStep) {
    if (!ShouldAbortForRuntimeFailure()) {
        return false;
    }

    IntReport("[FAIL] Workflow aborted after %s - %s", afterStep, RuntimeFailureReason());
    AddIntegrationFailure();
    return true;
}

} // namespace GWA3::SmokeTest
