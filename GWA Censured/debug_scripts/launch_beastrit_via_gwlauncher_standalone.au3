#include "..\lib\botshub\JSON.au3"

Global Const $ACCOUNTS_PATH = "C:\Users\Robert\Documents\GWA Censured X BotsHub\GWA Censured\Accounts.json"
Global Const $TARGET_CHARACTER = "B E A S T R I T"
Global Const $OVERRIDE_GW_PATH = "C:\Program Files (x86)\Guild Wars - Copy\Gw.exe"
Global Const $LOG_PATH = "C:\Users\Robert\Documents\GWA Censured X BotsHub\GWA Censured\debug_scripts\launch_beastrit_via_gwlauncher_standalone.log"
Global Const $TRACE_PATH = "C:\Users\Robert\Documents\GWA Censured X BotsHub\GWA Censured\debug_scripts\launch_beastrit_via_gwlauncher_standalone.trace.log"


; --- Constants ---
Global Const $GWLAUNCHER_CREATE_SUSPENDED   = 0x00000004
Global Const $GWLAUNCHER_MEM_COMMIT_RESERVE = 0x00003000 ; MEM_COMMIT | MEM_RESERVE
Global Const $GWLAUNCHER_PAGE_READWRITE     = 0x00000004
Global Const $GWLAUNCHER_MEM_RELEASE        = 0x00008000
Global Const $GWLAUNCHER_PROCESS_ALL_ACCESS = 0x001F0FFF

; Multiclient patch signature â€” the byte pattern located near the mutex check
Global Const $GWLAUNCHER_MC_SIGNATURE = "0x56,0x57,0x68,0x00,0x01,0x00,0x00,0x89,0x85,0xF4,0xFE,0xFF,0xFF,0xC7,0x00,0x00,0x00,0x00,0x00"

; Patch bytes: xor eax,eax; nop; ret â€” makes the mutex function return 0 (success)
; Written at (signature_match - 0x1A) to replace the function prologue
Global Const $GWLAUNCHER_MC_PATCH = "0x31,0xC0,0x90,0xC3"

; Size of GW .text section to read for signature scanning
Global Const $GWLAUNCHER_SCAN_SIZE = 0x48D000

; ============================================================================
; GWLauncher_Launch
; ============================================================================
; Launches a Guild Wars client with optional multiclient patching.
;
; Parameters:
;   $gwPath     â€” Full path to Gw.exe
;   $email      â€” (Optional) Login email
;   $password   â€” (Optional) Login password
;   $character  â€” (Optional) Character name to auto-select
;   $extraArgs  â€” (Optional) Additional command-line arguments
;
; Returns:
;   Success â€” Array: [0] = PID, [1] = process handle, [2] = thread handle
;   Failure â€” 0
; ============================================================================
Func GWLauncher_Launch($gwPath, $email = '', $password = '', $character = '', $extraArgs = '')
    If Not FileExists($gwPath) Then
        ConsoleWrite("[GWLauncher] Error: Gw.exe not found at: " & $gwPath & @CRLF)
        Return 0
    EndIf

    ; Set registry keys so GW finds its install location
    Local $gwDir = StringRegExpReplace($gwPath, "\\[^\\]+$", "")
    RegWrite("HKCU\Software\ArenaNet\Guild Wars", "Path", "REG_SZ", $gwPath)
    RegWrite("HKCU\Software\ArenaNet\Guild Wars", "Src", "REG_SZ", $gwDir)
    If @error Then
        ConsoleWrite("[GWLauncher] Warning: Could not write registry keys." & @CRLF)
    EndIf

    ; Build command line
    Local $cmdLine = '"' & $gwPath & '"'

    If $email <> '' And $password <> '' Then
        $cmdLine &= ' -email "' & $email & '" -password "' & $password & '"'
        If $character <> '' Then
            $cmdLine &= ' -character "' & $character & '"'
        EndIf
    EndIf

    If $extraArgs <> '' Then
        $cmdLine &= ' ' & $extraArgs
    EndIf

    ConsoleWrite("[GWLauncher] Command prepared for target account" & @CRLF)

    FileWrite($TRACE_PATH, "before_startupinfo" & @CRLF)

    ; Prepare STARTUPINFO (68 bytes, 32-bit)
    Local $startupInfo = DllStructCreate( _
        "dword cb;" & _
        "ptr Reserved;" & _
        "ptr Desktop;" & _
        "ptr Title;" & _
        "dword X;" & _
        "dword Y;" & _
        "dword XSize;" & _
        "dword YSize;" & _
        "dword XCountChars;" & _
        "dword YCountChars;" & _
        "dword FillAttribute;" & _
        "dword Flags;" & _
        "word ShowWindow;" & _
        "word Reserved2;" & _
        "ptr Reserved3;" & _
        "ptr StdInput;" & _
        "ptr StdOutput;" & _
        "ptr StdError")
    FileWrite($TRACE_PATH, "after_startupinfo" & @CRLF)
    DllStructSetData($startupInfo, "cb", DllStructGetSize($startupInfo))
    FileWrite($TRACE_PATH, "after_startupinfo_cb" & @CRLF)

    ; Prepare PROCESS_INFORMATION
    FileWrite($TRACE_PATH, "before_processinfo" & @CRLF)
    Local $processInfo = DllStructCreate( _
        "handle hProcess;" & _
        "handle hThread;" & _
        "dword dwProcessId;" & _
        "dword dwThreadId")
    FileWrite($TRACE_PATH, "after_processinfo" & @CRLF)

    FileWrite($TRACE_PATH, "before_create_process" & @CRLF)

    ; CreateProcessW with CREATE_SUSPENDED
    Local $ret = DllCall("kernel32.dll", "bool", "CreateProcessW", _
        "ptr", 0, _
        "wstr", $cmdLine, _
        "ptr", 0, _
        "ptr", 0, _
        "bool", False, _
        "dword", $GWLAUNCHER_CREATE_SUSPENDED, _
        "ptr", 0, _
        "ptr", 0, _
        "ptr", DllStructGetPtr($startupInfo), _
        "ptr", DllStructGetPtr($processInfo))

    FileWrite($TRACE_PATH, "after_create_process error=" & @error & @CRLF)
    If @error Or $ret[0] = 0 Then
        ConsoleWrite("[GWLauncher] Error: CreateProcessW failed. @error=" & @error & @CRLF)
        FileWrite($TRACE_PATH, "create_process_failed" & @CRLF)
        Return 0
    EndIf

    Local $hProcess = DllStructGetData($processInfo, "hProcess")
    Local $hThread  = DllStructGetData($processInfo, "hThread")
    Local $pid      = DllStructGetData($processInfo, "dwProcessId")

    ConsoleWrite("[GWLauncher] Process created (suspended). PID=" & $pid & @CRLF)
    FileWrite($TRACE_PATH, "created_pid=" & $pid & @CRLF)

    ; Apply multiclient patch
    Local $patchResult = _GWLauncher_McPatch($hProcess)
    If Not $patchResult Then
        ConsoleWrite("[GWLauncher] Warning: Multiclient patch failed. Resuming anyway." & @CRLF)
    Else
        ConsoleWrite("[GWLauncher] Multiclient patch applied successfully." & @CRLF)
    EndIf

    ; Resume the main thread
    Local $resumeRet = DllCall("kernel32.dll", "dword", "ResumeThread", "handle", $hThread)
    If @error Then
        ConsoleWrite("[GWLauncher] Error: ResumeThread failed. @error=" & @error & @CRLF)
        ; Close handles on failure
        DllCall("kernel32.dll", "bool", "CloseHandle", "handle", $hThread)
        DllCall("kernel32.dll", "bool", "CloseHandle", "handle", $hProcess)
        Return 0
    EndIf

    ConsoleWrite("[GWLauncher] Thread resumed. GW client running. PID=" & $pid & @CRLF)

    ; Return process info
    Local $result[3]
    $result[0] = $pid
    $result[1] = $hProcess
    $result[2] = $hThread
    Return $result
EndFunc

; ============================================================================
; _GWLauncher_McPatch
; ============================================================================
; Applies the multiclient patch to a suspended GW process.
;
; The patch neutralizes the mutex check that prevents multiple GW instances.
; It finds a known byte signature in the .text section and overwrites the
; function prologue 0x1A bytes before the match with: xor eax,eax; nop; ret
;
; Parameters:
;   $hProcess â€” Handle to the suspended process (must have write access)
;
; Returns:
;   True on success, False on failure
; ============================================================================
Func _GWLauncher_McPatch($hProcess)
    ; Get module base address
    Local $moduleBase = _GWLauncher_GetModuleBase($hProcess)
    If $moduleBase = 0 Then
        ConsoleWrite("[GWLauncher] McPatch: Failed to get module base address." & @CRLF)
        Return False
    EndIf

    ConsoleWrite("[GWLauncher] McPatch: Module base = 0x" & Hex($moduleBase) & @CRLF)

    ; Read GW .text section into local buffer
    Local $buffer = DllStructCreate("byte[" & $GWLAUNCHER_SCAN_SIZE & "]")
    Local $bytesRead = 0

    Local $ret = DllCall("kernel32.dll", "bool", "ReadProcessMemory", _
        "handle", $hProcess, _
        "ptr", $moduleBase, _
        "ptr", DllStructGetPtr($buffer), _
        "ulong_ptr", $GWLAUNCHER_SCAN_SIZE, _
        "ulong_ptr*", 0)

    If @error Or $ret[0] = 0 Then
        ConsoleWrite("[GWLauncher] McPatch: ReadProcessMemory failed. @error=" & @error & @CRLF)
        Return False
    EndIf

    ConsoleWrite("[GWLauncher] McPatch: Read " & $GWLAUNCHER_SCAN_SIZE & " bytes from process memory." & @CRLF)

    ; Build needle byte array from signature
    Local $sigParts = StringSplit($GWLAUNCHER_MC_SIGNATURE, ",", 2) ; No count in [0]
    Local $needle = DllStructCreate("byte[" & UBound($sigParts) & "]")
    For $i = 0 To UBound($sigParts) - 1
        DllStructSetData($needle, 1, Int($sigParts[$i]), $i + 1)
    Next

    ; Search for signature
    Local $offset = _GWLauncher_SearchBytes($buffer, $needle)
    If $offset = -1 Then
        ConsoleWrite("[GWLauncher] McPatch: Signature not found in process memory." & @CRLF)
        Return False
    EndIf

    ConsoleWrite("[GWLauncher] McPatch: Signature found at offset 0x" & Hex($offset) & @CRLF)

    ; Calculate patch address: signature_offset - 0x1A relative to module base
    If $offset < 0x1A Then
        ConsoleWrite("[GWLauncher] McPatch: Signature offset too small for patch (0x" & Hex($offset) & ")." & @CRLF)
        Return False
    EndIf

    Local $patchOffset = $offset - 0x1A
    Local $patchAddress = $moduleBase + $patchOffset

    ConsoleWrite("[GWLauncher] McPatch: Patching at address 0x" & Hex($patchAddress) & @CRLF)

    ; Build patch bytes: xor eax,eax (31 C0); nop (90); ret (C3)
    Local $patchParts = StringSplit($GWLAUNCHER_MC_PATCH, ",", 2)
    Local $patch = DllStructCreate("byte[" & UBound($patchParts) & "]")
    For $i = 0 To UBound($patchParts) - 1
        DllStructSetData($patch, 1, Int($patchParts[$i]), $i + 1)
    Next

    ; Write patch to process memory
    Local $writeRet = DllCall("kernel32.dll", "bool", "WriteProcessMemory", _
        "handle", $hProcess, _
        "ptr", $patchAddress, _
        "ptr", DllStructGetPtr($patch), _
        "ulong_ptr", UBound($patchParts), _
        "ulong_ptr*", 0)

    If @error Or $writeRet[0] = 0 Then
        ConsoleWrite("[GWLauncher] McPatch: WriteProcessMemory failed. @error=" & @error & @CRLF)
        Return False
    EndIf

    ConsoleWrite("[GWLauncher] McPatch: Patch written successfully (" & UBound($patchParts) & " bytes)." & @CRLF)
    Return True
EndFunc

; ============================================================================
; _GWLauncher_GetModuleBase
; ============================================================================
; Retrieves the image base address of a process via NtQueryInformationProcess.
;
; Reads the PEB (Process Environment Block) to find ImageBaseAddress, then
; adds 0x1000 to skip the PE headers and point to the .text section.
;
; Parameters:
;   $hProcess â€” Handle to the target process
;
; Returns:
;   ImageBaseAddress + 0x1000, or 0 on failure
; ============================================================================
Func _GWLauncher_GetModuleBase($hProcess)
    ; PROCESS_BASIC_INFORMATION structure
    Local $pbi = DllStructCreate( _
        "ulong ExitStatus;" & _
        "ptr PebBaseAddress;" & _
        "ulong_ptr AffinityMask;" & _
        "long BasePriority;" & _
        "ulong_ptr UniqueProcessId;" & _
        "ulong_ptr ParentProcessId")

    ; Query basic process information (class 0 = ProcessBasicInformation)
    Local $ret = DllCall("ntdll.dll", "long", "NtQueryInformationProcess", _
        "handle", $hProcess, _
        "dword", 0, _
        "ptr", DllStructGetPtr($pbi), _
        "ulong", DllStructGetSize($pbi), _
        "ulong*", 0)

    If @error Then
        ConsoleWrite("[GWLauncher] GetModuleBase: NtQueryInformationProcess DllCall failed. @error=" & @error & @CRLF)
        Return 0
    EndIf

    ; NTSTATUS: 0 = STATUS_SUCCESS
    If $ret[0] <> 0 Then
        ConsoleWrite("[GWLauncher] GetModuleBase: NtQueryInformationProcess returned NTSTATUS 0x" & Hex($ret[0]) & @CRLF)
        Return 0
    EndIf

    Local $pebAddress = DllStructGetData($pbi, "PebBaseAddress")
    If $pebAddress = 0 Then
        ConsoleWrite("[GWLauncher] GetModuleBase: PEB address is null." & @CRLF)
        Return 0
    EndIf

    ConsoleWrite("[GWLauncher] GetModuleBase: PEB at 0x" & Hex($pebAddress) & @CRLF)

    ; Read ImageBaseAddress from PEB (offset 8 in the PEB struct)
    ; PEB layout: BYTE InheritedAddressSpace (1), BYTE ReadImageFileExecOptions (1),
    ;             BYTE BeingDebugged (1), BYTE padding (1), ptr Mutant (4),
    ;             ptr ImageBaseAddress (4) â€” at offset 8
    Local $pebBuffer = DllStructCreate("byte[12]") ; Read enough to cover offset 8 + ptr size
    Local $readRet = DllCall("kernel32.dll", "bool", "ReadProcessMemory", _
        "handle", $hProcess, _
        "ptr", $pebAddress, _
        "ptr", DllStructGetPtr($pebBuffer), _
        "ulong_ptr", 12, _
        "ulong_ptr*", 0)

    If @error Or $readRet[0] = 0 Then
        ConsoleWrite("[GWLauncher] GetModuleBase: Failed to read PEB. @error=" & @error & @CRLF)
        Return 0
    EndIf

    ; Extract ImageBaseAddress at offset 8 (as a 4-byte pointer, 32-bit)
    Local $imageBaseBuf = DllStructCreate("ptr ImageBase", DllStructGetPtr($pebBuffer) + 8)
    Local $imageBase = DllStructGetData($imageBaseBuf, "ImageBase")

    If $imageBase = 0 Then
        ConsoleWrite("[GWLauncher] GetModuleBase: ImageBaseAddress is null." & @CRLF)
        Return 0
    EndIf

    ; Add 0x1000 to skip PE headers -> points to .text section start
    Local $textBase = $imageBase + 0x1000
    ConsoleWrite("[GWLauncher] GetModuleBase: ImageBase=0x" & Hex($imageBase) & " TextBase=0x" & Hex($textBase) & @CRLF)
    Return $textBase
EndFunc

; ============================================================================
; _GWLauncher_SearchBytes
; ============================================================================
; Searches for a byte pattern (needle) within a larger byte buffer (haystack).
;
; Parameters:
;   $haystack â€” DllStruct containing the byte buffer to search
;   $needle   â€” DllStruct containing the byte pattern to find
;
; Returns:
;   Zero-based offset of the match, or -1 if not found
; ============================================================================
Func _GWLauncher_SearchBytes($haystack, $needle)
    Local $haystackSize = DllStructGetSize($haystack)
    Local $needleSize = DllStructGetSize($needle)

    If $needleSize = 0 Or $haystackSize = 0 Or $needleSize > $haystackSize Then
        Return -1
    EndIf

    Local $searchLimit = $haystackSize - $needleSize

    For $i = 0 To $searchLimit
        Local $found = True
        For $j = 0 To $needleSize - 1
            ; DllStructGetData byte index is 1-based
            If DllStructGetData($haystack, 1, $i + $j + 1) <> DllStructGetData($needle, 1, $j + 1) Then
                $found = False
                ExitLoop
            EndIf
        Next
        If $found Then Return $i
    Next

    Return -1
EndFunc

; ============================================================================
; GWLauncher_GetGWPath
; ============================================================================
; Reads the Guild Wars install path from the Windows registry.
;
; Returns:
;   The Gw.exe path string, or empty string if not found
; ============================================================================
Func GWLauncher_GetGWPath()
    Local $path = RegRead("HKCU\Software\ArenaNet\Guild Wars", "Path")
    If @error Then
        ConsoleWrite("[GWLauncher] GetGWPath: Registry key not found." & @CRLF)
        Return ''
    EndIf
    ConsoleWrite("[GWLauncher] GetGWPath: " & $path & @CRLF)
    Return $path
EndFunc

; ============================================================================
; GWLauncher_InjectDLL
; ============================================================================
; Injects a DLL into a target process using the classic LoadLibraryW method.
;
; Steps:
;   1. Resolve LoadLibraryW address in kernel32.dll
;   2. Allocate memory in target process for the DLL path
;   3. Write the DLL path (Unicode) into the allocated memory
;   4. Create a remote thread calling LoadLibraryW with the path as argument
;   5. Wait for the thread to complete
;   6. Free the allocated memory
;
; Parameters:
;   $hProcess â€” Handle to the target process (must have appropriate access)
;   $dllPath  â€” Full path to the DLL to inject
;
; Returns:
;   True on success, False on failure
; ============================================================================
Func GWLauncher_InjectDLL($hProcess, $dllPath)
    If Not FileExists($dllPath) Then
        ConsoleWrite("[GWLauncher] InjectDLL: DLL not found: " & $dllPath & @CRLF)
        Return False
    EndIf

    ; Get handle to kernel32.dll
    Local $hKernel32 = DllCall("kernel32.dll", "handle", "GetModuleHandleW", "wstr", "kernel32.dll")
    If @error Or $hKernel32[0] = 0 Then
        ConsoleWrite("[GWLauncher] InjectDLL: GetModuleHandle(kernel32) failed." & @CRLF)
        Return False
    EndIf
    $hKernel32 = $hKernel32[0]

    ; Get address of LoadLibraryW
    Local $pLoadLibrary = DllCall("kernel32.dll", "ptr", "GetProcAddress", _
        "handle", $hKernel32, _
        "str", "LoadLibraryW")
    If @error Or $pLoadLibrary[0] = 0 Then
        ConsoleWrite("[GWLauncher] InjectDLL: GetProcAddress(LoadLibraryW) failed." & @CRLF)
        Return False
    EndIf
    $pLoadLibrary = $pLoadLibrary[0]

    ConsoleWrite("[GWLauncher] InjectDLL: LoadLibraryW at 0x" & Hex($pLoadLibrary) & @CRLF)

    ; Calculate buffer size for Unicode DLL path (chars + null terminator) * 2 bytes
    Local $pathLen = (StringLen($dllPath) + 1) * 2

    ; Allocate memory in target process
    Local $pRemoteBuf = DllCall("kernel32.dll", "ptr", "VirtualAllocEx", _
        "handle", $hProcess, _
        "ptr", 0, _
        "ulong_ptr", $pathLen, _
        "dword", $GWLAUNCHER_MEM_COMMIT_RESERVE, _
        "dword", $GWLAUNCHER_PAGE_READWRITE)

    If @error Or $pRemoteBuf[0] = 0 Then
        ConsoleWrite("[GWLauncher] InjectDLL: VirtualAllocEx failed." & @CRLF)
        Return False
    EndIf
    $pRemoteBuf = $pRemoteBuf[0]

    ConsoleWrite("[GWLauncher] InjectDLL: Allocated " & $pathLen & " bytes at 0x" & Hex($pRemoteBuf) & @CRLF)

    ; Write DLL path as Unicode string into allocated memory
    Local $pathStruct = DllStructCreate("wchar[" & (StringLen($dllPath) + 1) & "]")
    DllStructSetData($pathStruct, 1, $dllPath)

    Local $writeRet = DllCall("kernel32.dll", "bool", "WriteProcessMemory", _
        "handle", $hProcess, _
        "ptr", $pRemoteBuf, _
        "ptr", DllStructGetPtr($pathStruct), _
        "ulong_ptr", $pathLen, _
        "ulong_ptr*", 0)

    If @error Or $writeRet[0] = 0 Then
        ConsoleWrite("[GWLauncher] InjectDLL: WriteProcessMemory failed." & @CRLF)
        ; Cleanup allocated memory
        DllCall("kernel32.dll", "bool", "VirtualFreeEx", _
            "handle", $hProcess, _
            "ptr", $pRemoteBuf, _
            "ulong_ptr", 0, _
            "dword", $GWLAUNCHER_MEM_RELEASE)
        Return False
    EndIf

    ; Create remote thread calling LoadLibraryW(pRemoteBuf)
    Local $hRemoteThread = DllCall("kernel32.dll", "handle", "CreateRemoteThread", _
        "handle", $hProcess, _
        "ptr", 0, _
        "ulong_ptr", 0, _
        "ptr", $pLoadLibrary, _
        "ptr", $pRemoteBuf, _
        "dword", 0, _
        "dword*", 0)

    If @error Or $hRemoteThread[0] = 0 Then
        ConsoleWrite("[GWLauncher] InjectDLL: CreateRemoteThread failed." & @CRLF)
        DllCall("kernel32.dll", "bool", "VirtualFreeEx", _
            "handle", $hProcess, _
            "ptr", $pRemoteBuf, _
            "ulong_ptr", 0, _
            "dword", $GWLAUNCHER_MEM_RELEASE)
        Return False
    EndIf
    $hRemoteThread = $hRemoteThread[0]

    ConsoleWrite("[GWLauncher] InjectDLL: Remote thread created. Waiting for completion..." & @CRLF)

    ; Wait for the remote thread to finish (10 second timeout)
    Local $waitRet = DllCall("kernel32.dll", "dword", "WaitForSingleObject", _
        "handle", $hRemoteThread, _
        "dword", 10000)

    If @error Then
        ConsoleWrite("[GWLauncher] InjectDLL: WaitForSingleObject failed. @error=" & @error & @CRLF)
    ElseIf $waitRet[0] <> 0 Then
        ; 0 = WAIT_OBJECT_0 (success), 0x102 = WAIT_TIMEOUT
        ConsoleWrite("[GWLauncher] InjectDLL: WaitForSingleObject returned 0x" & Hex($waitRet[0]) & @CRLF)
    EndIf

    ; Cleanup: close remote thread handle
    DllCall("kernel32.dll", "bool", "CloseHandle", "handle", $hRemoteThread)

    ; Cleanup: free allocated memory in target process
    DllCall("kernel32.dll", "bool", "VirtualFreeEx", _
        "handle", $hProcess, _
        "ptr", $pRemoteBuf, _
        "ulong_ptr", 0, _
        "dword", $GWLAUNCHER_MEM_RELEASE)

    ConsoleWrite("[GWLauncher] InjectDLL: DLL injection complete: " & $dllPath & @CRLF)
    Return True
EndFunc


; ============================================================================
; Account Management
; ============================================================================

;~ Load accounts from Accounts.json
;~ @param $filePath - path to Accounts.json (default: script dir)
;~ @return Array of account maps, or empty array on failure
;~   Each account map has keys: character, email, gwpath, password, extraargs, elevated, title, Name
Func GWLauncher_LoadAccounts($filePath = '')
    If $filePath = '' Then $filePath = @ScriptDir & '\Accounts.json'
    If Not FileExists($filePath) Then
        ConsoleWrite('[GWLauncher] Accounts file not found: ' & $filePath & @CRLF)
        Local $empty[0]
        Return $empty
    EndIf

    Local $json = FileRead($filePath)
    Local $accounts = _JSON_Parse($json)
    If @error Or Not IsArray($accounts) Then
        ConsoleWrite('[GWLauncher] Failed to parse Accounts.json' & @CRLF)
        Local $empty[0]
        Return $empty
    EndIf

    ConsoleWrite('[GWLauncher] Loaded ' & UBound($accounts) & ' accounts' & @CRLF)
    Return $accounts
EndFunc

;~ Get account names for display (redacted â€” no emails/passwords)
;~ @param $accounts - array from GWLauncher_LoadAccounts()
;~ @return Pipe-delimited string of character names for combo box
Func GWLauncher_GetAccountNames($accounts)
    Local $names = ''
    For $i = 0 To UBound($accounts) - 1
        Local $name = ''
        Local $a = $accounts[$i]
        If IsMap($a) Then
            If MapExists($a, 'Name') Then $name = $a['Name']
            If $name = '' And MapExists($a, 'character') Then $name = $a['character']
            If $name = '' And MapExists($a, 'title') Then $name = $a['title']
        EndIf
        If $name = '' Then $name = 'Account ' & ($i + 1)
        $names &= $name & '|'
    Next
    Return StringTrimRight($names, 1)
EndFunc

;~ Launch a specific account by index
;~ @param $accounts - array from GWLauncher_LoadAccounts()
;~ @param $index - 0-based index into accounts array
;~ @return Result from GWLauncher_Launch(), or 0 on failure
Func GWLauncher_LaunchAccount($accounts, $index)
    If $index < 0 Or $index >= UBound($accounts) Then
        ConsoleWrite('[GWLauncher] Invalid account index: ' & $index & @CRLF)
        Return 0
    EndIf

    Local $acct = $accounts[$index]
    If Not IsMap($acct) Then Return 0

    Local $gwPath = ''
    Local $email = ''
    Local $password = ''
    Local $character = ''
    Local $extraArgs = ''

    If MapExists($acct, 'gwpath') Then $gwPath = $acct['gwpath']
    If MapExists($acct, 'email') Then $email = $acct['email']
    If MapExists($acct, 'password') Then $password = $acct['password']
    If MapExists($acct, 'character') Then $character = $acct['character']
    If MapExists($acct, 'extraargs') Then $extraArgs = $acct['extraargs']

    If $gwPath = '' Or Not FileExists($gwPath) Then
        ConsoleWrite('[GWLauncher] GW path not found for account ' & $index & @CRLF)
        Return 0
    EndIf

    Local $displayName = $character
    If $displayName = '' Then $displayName = 'Account ' & ($index + 1)
    ConsoleWrite('[GWLauncher] Launching account: ' & $displayName & @CRLF)

    Return GWLauncher_Launch($gwPath, $email, $password, $character, $extraArgs)
EndFunc

;~ Find account index by character name (case-insensitive)
;~ @return 0-based index, or -1 if not found
Func GWLauncher_FindAccountByCharacter($accounts, $characterName)
    For $i = 0 To UBound($accounts) - 1
        Local $a = $accounts[$i]
        If IsMap($a) Then
            Local $name = ''
            If MapExists($a, 'character') Then $name = $a['character']
            If $name = '' And MapExists($a, 'Name') Then $name = $a['Name']
            If StringLower($name) = StringLower($characterName) Then Return $i
        EndIf
    Next
    Return -1
EndFunc

;~ Fully automated: launch account, wait for login, scan for client, connect
;~ No UI interaction needed â€” designed for headless/scripted operation
;~ @param $characterName - character name to launch and connect to
;~ @param $accountsFile - path to Accounts.json (default: script dir)

Local $accounts = GWLauncher_LoadAccounts($ACCOUNTS_PATH)
Local $idx = GWLauncher_FindAccountByCharacter($accounts, $TARGET_CHARACTER)
If $idx < 0 Then
    FileDelete($LOG_PATH)
    FileWrite($LOG_PATH, "GWLAUNCHER_ERROR=account_not_found" & @CRLF)
    Exit 2
EndIf

If $OVERRIDE_GW_PATH <> "" Then
    Local $acct = $accounts[$idx]
    If IsMap($acct) Then
        $acct['gwpath'] = $OVERRIDE_GW_PATH
        $accounts[$idx] = $acct
    EndIf
EndIf

Local $result = GWLauncher_LaunchAccount($accounts, $idx)
If $result = 0 Then
    FileDelete($LOG_PATH)
    FileWrite($LOG_PATH, "GWLAUNCHER_ERROR=launch_failed" & @CRLF)
    Exit 3
EndIf

FileDelete($LOG_PATH)
FileWrite($LOG_PATH, "GWLAUNCHER_PID=" & $result[0] & @CRLF)
ConsoleWrite("GWLAUNCHER_PID=" & $result[0] & @CRLF)
Exit 0
