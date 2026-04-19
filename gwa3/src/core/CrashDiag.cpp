#include <gwa3/core/CrashDiag.h>
#include <gwa3/core/Log.h>
#include <gwa3/core/HookMarker.h>

#include <Windows.h>
#include <DbgHelp.h>

#include <cstdio>
#include <cstring>

namespace GWA3::CrashDiag {

namespace {

static PVOID s_vectoredHandler = nullptr;
static LPTOP_LEVEL_EXCEPTION_FILTER s_prevUnhandledFilter = nullptr;
static volatile LONG s_loggedUnhandled = 0;
static volatile LONG s_loggedStackCookie = 0;
static volatile LONG s_loggedHardFault = 0;
static volatile LONG s_dumpSequence = 0;
static bool s_symbolsInitialized = false;

bool ShouldLogStackCookie(DWORD code) {
    return code == static_cast<DWORD>(STATUS_STACK_BUFFER_OVERRUN) ||
           code == EXCEPTION_STACK_OVERFLOW ||
           code == EXCEPTION_ILLEGAL_INSTRUCTION ||
           code == EXCEPTION_PRIV_INSTRUCTION;
}

// "Hard fault" exceptions Ã¢â‚¬â€ likely to be the actual process-killing
// crash rather than something GW's internal __try/__except is
// expected to catch. We gate these to once-per-session because first-
// chance AVs can be frequent in normal operation (stack probes,
// structured handling, etc.); logging every one would drown the log.
bool IsHardFault(DWORD code) {
    return code == EXCEPTION_ACCESS_VIOLATION ||
           code == EXCEPTION_INT_DIVIDE_BY_ZERO ||
           code == EXCEPTION_ARRAY_BOUNDS_EXCEEDED ||
           code == EXCEPTION_DATATYPE_MISALIGNMENT ||
           code == EXCEPTION_IN_PAGE_ERROR ||
           code == EXCEPTION_NONCONTINUABLE_EXCEPTION;
}

void BuildOutputPath(const char* extension, char* outPath, size_t outPathSize) {
    if (!outPath || outPathSize == 0) return;
    outPath[0] = '\0';

    char modulePath[MAX_PATH] = {};
    HMODULE hSelf = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCSTR>(&BuildOutputPath), &hSelf)) {
        return;
    }
    if (!GetModuleFileNameA(hSelf, modulePath, MAX_PATH)) {
        return;
    }

    char* slash = strrchr(modulePath, '\\');
    if (!slash) return;
    *slash = '\0';

    char dumpDir[MAX_PATH] = {};
    snprintf(dumpDir, sizeof(dumpDir), "%s\\crashdumps", modulePath);
    CreateDirectoryA(dumpDir, nullptr);

    SYSTEMTIME st = {};
    GetLocalTime(&st);
    const LONG seq = InterlockedIncrement(&s_dumpSequence);
    snprintf(outPath, outPathSize,
             "%s\\gwa3_crash_%04u%02u%02u_%02u%02u%02u_pid%u_tid%u_%ld.%s",
             dumpDir,
             st.wYear, st.wMonth, st.wDay,
             st.wHour, st.wMinute, st.wSecond,
             GetCurrentProcessId(),
             GetCurrentThreadId(),
             seq,
             extension ? extension : "log");
}

void ResolveModuleForAddress(const void* addr, char* modulePath, size_t modulePathSize,
                             uintptr_t* outBase, uintptr_t* outOffset) {
    if (modulePath && modulePathSize > 0) {
        modulePath[0] = '\0';
    }
    if (outBase) *outBase = 0;
    if (outOffset) *outOffset = 0;
    if (!addr) return;

    HMODULE hModule = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCSTR>(addr), &hModule)) {
        return;
    }

    if (modulePath && modulePathSize > 0) {
        GetModuleFileNameA(hModule, modulePath, static_cast<DWORD>(modulePathSize));
    }

    if (outBase) {
        *outBase = reinterpret_cast<uintptr_t>(hModule);
    }
    if (outOffset) {
        const uintptr_t base = reinterpret_cast<uintptr_t>(hModule);
        *outOffset = reinterpret_cast<uintptr_t>(addr) - base;
    }
}

void WriteMiniDump(const char* tag, EXCEPTION_POINTERS* exceptionPointers) {
    char dumpPath[MAX_PATH] = {};
    BuildOutputPath("dmp", dumpPath, sizeof(dumpPath));
    if (!dumpPath[0]) return;

    HANDLE file = CreateFileA(dumpPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        GWA3::Log::Error("CrashDiag: CreateFile failed for dump %s err=%u", dumpPath, GetLastError());
        return;
    }

    const auto dumpType = static_cast<MINIDUMP_TYPE>(
        MiniDumpNormal |
        MiniDumpWithThreadInfo |
        MiniDumpWithIndirectlyReferencedMemory |
        MiniDumpScanMemory
    );
    MINIDUMP_EXCEPTION_INFORMATION mei = {};
    MINIDUMP_EXCEPTION_INFORMATION* meiPtr = nullptr;
    if (exceptionPointers) {
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = exceptionPointers;
        mei.ClientPointers = FALSE;
        meiPtr = &mei;
    }
    const BOOL ok = MiniDumpWriteDump(GetCurrentProcess(),
                                      GetCurrentProcessId(),
                                      file,
                                      dumpType,
                                      meiPtr,
                                      nullptr,
                                      nullptr);
    const DWORD err = ok ? ERROR_SUCCESS : GetLastError();
    CloseHandle(file);

    if (ok) {
        GWA3::Log::Error("CrashDiag: %s minidump written to %s", tag ? tag : "exception", dumpPath);
    } else {
        GWA3::Log::Error("CrashDiag: MiniDumpWriteDump failed for %s err=%u", dumpPath, err);
    }
}

bool EnsureSymbols() {
    if (s_symbolsInitialized) {
        return true;
    }

    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
    if (!SymInitialize(GetCurrentProcess(), nullptr, TRUE)) {
        GWA3::Log::Error("CrashDiag: SymInitialize failed err=%u", GetLastError());
        return false;
    }

    s_symbolsInitialized = true;
    return true;
}

void LogStackTrace(const char* tag, const CONTEXT* sourceCtx) {
#if defined(_M_IX86)
    if (!sourceCtx || !EnsureSymbols()) {
        return;
    }

    CONTEXT ctx = *sourceCtx;
    STACKFRAME64 frame = {};
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrPC.Offset = ctx.Eip;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = ctx.Ebp;
    frame.AddrStack.Mode = AddrModeFlat;
    frame.AddrStack.Offset = ctx.Esp;

    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    constexpr unsigned kMaxFrames = 24;

    for (unsigned i = 0; i < kMaxFrames; ++i) {
        const DWORD64 addr = frame.AddrPC.Offset;
        if (addr == 0) {
            break;
        }

        char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        DWORD64 displacement = 0;

        char modulePath[MAX_PATH] = {};
        uintptr_t moduleBase = 0;
        uintptr_t moduleOffset = 0;
        ResolveModuleForAddress(reinterpret_cast<const void*>(static_cast<uintptr_t>(addr)),
                                modulePath, sizeof(modulePath),
                                &moduleBase, &moduleOffset);

        IMAGEHLP_LINE64 line = {};
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisplacement = 0;
        const BOOL haveSymbol = SymFromAddr(process, addr, &displacement, symbol);
        const BOOL haveLine = SymGetLineFromAddr64(process, addr, &lineDisplacement, &line);

        if (haveSymbol && haveLine) {
            GWA3::Log::Error("CrashDiag: %s stack[%u] 0x%08X %s +0x%X [%s:%lu]",
                             tag ? tag : "exception",
                             i,
                             static_cast<unsigned>(addr),
                             symbol->Name,
                             static_cast<unsigned>(displacement),
                             line.FileName,
                             static_cast<unsigned long>(line.LineNumber));
        } else if (haveSymbol) {
            GWA3::Log::Error("CrashDiag: %s stack[%u] 0x%08X %s +0x%X [%s+0x%08X]",
                             tag ? tag : "exception",
                             i,
                             static_cast<unsigned>(addr),
                             symbol->Name,
                             static_cast<unsigned>(displacement),
                             modulePath[0] ? modulePath : "<unknown>",
                             static_cast<unsigned>(moduleOffset));
        } else {
            GWA3::Log::Error("CrashDiag: %s stack[%u] 0x%08X [%s+0x%08X]",
                             tag ? tag : "exception",
                             i,
                             static_cast<unsigned>(addr),
                             modulePath[0] ? modulePath : "<unknown>",
                             static_cast<unsigned>(moduleOffset));
        }

        if (!StackWalk64(IMAGE_FILE_MACHINE_I386,
                         process,
                         thread,
                         &frame,
                         &ctx,
                         nullptr,
                         SymFunctionTableAccess64,
                         SymGetModuleBase64,
                         nullptr)) {
            break;
        }
    }
#endif
}

void LogExceptionContext(const char* tag, EXCEPTION_POINTERS* ep) {
    if (!ep || !ep->ExceptionRecord) return;

    const EXCEPTION_RECORD* rec = ep->ExceptionRecord;
    const CONTEXT* ctx = ep->ContextRecord;

    char faultModule[MAX_PATH] = {};
    uintptr_t faultBase = 0;
    uintptr_t faultOffset = 0;
    ResolveModuleForAddress(rec->ExceptionAddress, faultModule, sizeof(faultModule), &faultBase, &faultOffset);

    GWA3::Log::Error("CrashDiag: %s code=0x%08X flags=0x%08X addr=0x%08X module=%s+0x%08X tid=%u",
                     tag ? tag : "exception",
                     rec->ExceptionCode,
                     rec->ExceptionFlags,
                     static_cast<unsigned>(reinterpret_cast<uintptr_t>(rec->ExceptionAddress)),
                     faultModule[0] ? faultModule : "<unknown>",
                     static_cast<unsigned>(faultOffset),
                     GetCurrentThreadId());

    if (rec->NumberParameters > 0) {
        char paramsLine[512] = {};
        size_t written = snprintf(paramsLine, sizeof(paramsLine),
                                  "CrashDiag: %s params[%lu]",
                                  tag ? tag : "exception",
                                  static_cast<unsigned long>(rec->NumberParameters));
        for (ULONG i = 0; i < rec->NumberParameters && i < EXCEPTION_MAXIMUM_PARAMETERS; ++i) {
            if (written >= sizeof(paramsLine)) break;
            written += snprintf(paramsLine + written,
                                sizeof(paramsLine) - written,
                                " p%lu=0x%08X",
                                static_cast<unsigned long>(i),
                                static_cast<unsigned>(rec->ExceptionInformation[i]));
        }
        GWA3::Log::Error("%s", paramsLine);
    }

#if defined(_M_IX86)
    if (ctx) {
        char ipModule[MAX_PATH] = {};
        uintptr_t ipBase = 0;
        uintptr_t ipOffset = 0;
        ResolveModuleForAddress(reinterpret_cast<const void*>(ctx->Eip), ipModule, sizeof(ipModule), &ipBase, &ipOffset);
        GWA3::Log::Error("CrashDiag: %s regs EIP=0x%08X ESP=0x%08X EBP=0x%08X EAX=0x%08X EBX=0x%08X ECX=0x%08X EDX=0x%08X ESI=0x%08X EDI=0x%08X",
                         tag ? tag : "exception",
                         static_cast<unsigned>(ctx->Eip),
                         static_cast<unsigned>(ctx->Esp),
                         static_cast<unsigned>(ctx->Ebp),
                         static_cast<unsigned>(ctx->Eax),
                         static_cast<unsigned>(ctx->Ebx),
                         static_cast<unsigned>(ctx->Ecx),
                         static_cast<unsigned>(ctx->Edx),
                         static_cast<unsigned>(ctx->Esi),
                         static_cast<unsigned>(ctx->Edi));
        GWA3::Log::Error("CrashDiag: %s control EFlags=0x%08X module=%s+0x%08X",
                         tag ? tag : "exception",
                         static_cast<unsigned>(ctx->EFlags),
                         ipModule[0] ? ipModule : "<unknown>",
                         static_cast<unsigned>(ipOffset));
        LogStackTrace(tag, ctx);
    }
#endif
}

LONG CALLBACK VectoredExceptionHandler(EXCEPTION_POINTERS* ep) {
    if (!ep || !ep->ExceptionRecord) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    const DWORD code = ep->ExceptionRecord->ExceptionCode;

    if (ShouldLogStackCookie(code)) {
        if (InterlockedCompareExchange(&s_loggedStackCookie, 1, 0) == 0) {
            __try { HookMarker::DumpOnCrash(); } __except(EXCEPTION_EXECUTE_HANDLER) { GWA3::Log::Error("CrashDiag: DumpOnCrash itself crashed in VEH-cookie"); }
            LogExceptionContext("VEH-cookie", ep);
            WriteMiniDump("veh-cookie", ep);
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // Hard faults: capture the FIRST one per session. First-chance AVs
    // can happen in normal operation when __try/__except is catching a
    // recoverable condition, but the first hard fault is overwhelmingly
    // the signal we want for a real crash Ã¢â‚¬â€ UEF doesn't fire if GW's
    // own SEH catches it (and GW's crash dialog overrides UEF anyway).
    if (IsHardFault(code)) {
        if (InterlockedCompareExchange(&s_loggedHardFault, 1, 0) == 0) {
            __try { HookMarker::DumpOnCrash(); } __except(EXCEPTION_EXECUTE_HANDLER) { GWA3::Log::Error("CrashDiag: DumpOnCrash itself crashed in VEH-hard"); }
            LogExceptionContext("VEH-hard", ep);
            WriteMiniDump("veh-hard", ep);
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

LONG WINAPI UnhandledExceptionFilterThunk(EXCEPTION_POINTERS* ep) {
    if (InterlockedCompareExchange(&s_loggedUnhandled, 1, 0) == 0) {
        __try { HookMarker::DumpOnCrash(); } __except(EXCEPTION_EXECUTE_HANDLER) { GWA3::Log::Error("CrashDiag: DumpOnCrash itself crashed in UEF"); }
        LogExceptionContext("UEF", ep);
        WriteMiniDump("uef", ep);
    }

    if (s_prevUnhandledFilter) {
        return s_prevUnhandledFilter(ep);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

} // namespace

bool Initialize() {
    if (s_vectoredHandler) {
        return true;
    }

    s_loggedUnhandled = 0;
    s_loggedStackCookie = 0;
    s_loggedHardFault = 0;
    s_vectoredHandler = AddVectoredExceptionHandler(1, &VectoredExceptionHandler);
    s_prevUnhandledFilter = SetUnhandledExceptionFilter(&UnhandledExceptionFilterThunk);

    if (!s_vectoredHandler) {
        GWA3::Log::Error("CrashDiag: AddVectoredExceptionHandler failed err=%u", GetLastError());
        return false;
    }

    GWA3::Log::Info("CrashDiag: Installed VEH and unhandled exception filter");
    return true;
}

void CaptureProcessState(const char* tag) {
    GWA3::Log::Error("CrashDiag: capture requested tag=%s tid=%u",
                     tag ? tag : "manual",
                     GetCurrentThreadId());
    __try { HookMarker::DumpOnCrash(); } __except(EXCEPTION_EXECUTE_HANDLER) { GWA3::Log::Error("CrashDiag: DumpOnCrash itself crashed in capture"); }
    WriteMiniDump(tag ? tag : "manual", nullptr);
}

void Shutdown() {
    if (s_vectoredHandler) {
        RemoveVectoredExceptionHandler(s_vectoredHandler);
        s_vectoredHandler = nullptr;
    }
    if (s_symbolsInitialized) {
        SymCleanup(GetCurrentProcess());
        s_symbolsInitialized = false;
    }
    SetUnhandledExceptionFilter(s_prevUnhandledFilter);
    s_prevUnhandledFilter = nullptr;
}

} // namespace GWA3::CrashDiag



