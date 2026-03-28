#include-once
; JSON.au3 resolves through Froggy_Includes.au3 master include chain

; ============================================================================
; GWLauncher.au3 — Guild Wars Multiclient Launcher Module
; ============================================================================
;
; Launches Guild Wars clients with multiclient support by patching the
; mutex check in a suspended process before resuming execution.
;
; Public API:
;   GWLauncher_Launch()             — Launch a GW client (optionally with credentials)
;   GWLauncher_GetGWPath()          — Read GW install path from registry
;   GWLauncher_InjectDLL()          — Inject a DLL into a running process
;   GWLauncher_LoadAccounts()       — Load accounts from Accounts.json
;   GWLauncher_LaunchAccount()      — Launch a specific account by index
;   GWLauncher_AutoLaunchAndConnect() — Full headless: launch, wait, scan, connect
;
; Internal:
;   _GWLauncher_McPatch()         — Apply multiclient mutex patch
;   _GWLauncher_GetModuleBase()   — Read image base from PEB
;   _GWLauncher_SearchBytes()     — Binary pattern search
;
; All Win32 interaction uses DllCall. No external includes required.
; ============================================================================

; --- Constants ---
Global Const $GWLAUNCHER_CREATE_SUSPENDED   = 0x00000004
Global Const $GWLAUNCHER_MEM_COMMIT_RESERVE = 0x00003000 ; MEM_COMMIT | MEM_RESERVE
Global Const $GWLAUNCHER_PAGE_READWRITE     = 0x00000004
Global Const $GWLAUNCHER_MEM_RELEASE        = 0x00008000
Global Const $GWLAUNCHER_PROCESS_ALL_ACCESS = 0x001F0FFF

; Multiclient patch signature — the byte pattern located near the mutex check
Global Const $GWLAUNCHER_MC_SIGNATURE = "0x56,0x57,0x68,0x00,0x01,0x00,0x00,0x89,0x85,0xF4,0xFE,0xFF,0xFF,0xC7,0x00,0x00,0x00,0x00,0x00"

; Patch bytes: xor eax,eax; nop; ret — makes the mutex function return 0 (success)
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
;   $gwPath     — Full path to Gw.exe
;   $email      — (Optional) Login email
;   $password   — (Optional) Login password
;   $character  — (Optional) Character name to auto-select
;   $extraArgs  — (Optional) Additional command-line arguments
;
; Returns:
;   Success — Array: [0] = PID, [1] = process handle, [2] = thread handle
;   Failure — 0
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

    ConsoleWrite("[GWLauncher] Command: " & $cmdLine & @CRLF)

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
    DllStructSetData($startupInfo, "cb", DllStructGetSize($startupInfo))

    ; Prepare PROCESS_INFORMATION
    Local $processInfo = DllStructCreate( _
        "handle hProcess;" & _
        "handle hThread;" & _
        "dword dwProcessId;" & _
        "dword dwThreadId")

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

    If @error Or $ret[0] = 0 Then
        ConsoleWrite("[GWLauncher] Error: CreateProcessW failed. @error=" & @error & @CRLF)
        Return 0
    EndIf

    Local $hProcess = DllStructGetData($processInfo, "hProcess")
    Local $hThread  = DllStructGetData($processInfo, "hThread")
    Local $pid      = DllStructGetData($processInfo, "dwProcessId")

    ConsoleWrite("[GWLauncher] Process created (suspended). PID=" & $pid & @CRLF)

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
;   $hProcess — Handle to the suspended process (must have write access)
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
;   $hProcess — Handle to the target process
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
    ;             ptr ImageBaseAddress (4) — at offset 8
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
;   $haystack — DllStruct containing the byte buffer to search
;   $needle   — DllStruct containing the byte pattern to find
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
;   $hProcess — Handle to the target process (must have appropriate access)
;   $dllPath  — Full path to the DLL to inject
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

;~ Get account names for display (redacted — no emails/passwords)
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
;~ No UI interaction needed — designed for headless/scripted operation
;~ @param $characterName - character name to launch and connect to
;~ @param $accountsFile - path to Accounts.json (default: script dir)
;~ @param $timeout - max seconds to wait for client to appear (default: 120)
;~ @return True on success (client connected), False on failure
;~ Select a character and press Play on the GW character select screen
;~ Uses PreGameContext memory structure to select the character by index,
;~ then sends Enter key to press Play. No mouse interaction needed.
;~
;~ PreGameContext layout (from GWCA):
;~   +0x000  frame_id
;~   +0x124  chosen_character_index (uint32)
;~   +0x148  chars array (Array<LoginCharacter>)
;~   LoginCharacter: uint32 unk0, wchar_t name[20]
;~
;~ @param $characterName - character name to select (or '' for current selection)
;~ @param $gwWindowTitle - GW window title for sending Enter key
;~ @return True on success
Func GWLauncher_ClickPlay($gwWindowTitle = '', $characterName = '')
    ; Find GW window
    Local $hWnd = 0
    If $gwWindowTitle <> '' Then
        $hWnd = WinWait($gwWindowTitle, '', 10)
    Else
        $hWnd = WinWait('Guild Wars', '', 10)
    EndIf
    If $hWnd = 0 Then
        ConsoleWrite('[GWLauncher] ClickPlay: GW window not found' & @CRLF)
        Return False
    EndIf

    ; If character name provided, select it via PreGameContext
    If $characterName <> '' Then
        Local $processHandle = GetProcessHandle()
        ; Read PreGameContext base from $pre_game_address
        Local $preGamePtr = MemRead($pre_game_address)
        If $preGamePtr <> 0 Then
            ; Read chars array: offset 0x148 = buffer ptr, 0x14C = size
            Local $charsPtr = MemRead($preGamePtr + 0x148)
            Local $charsCount = MemRead($preGamePtr + 0x14C)
            ConsoleWrite('[GWLauncher] PreGame ptr=0x' & Hex($preGamePtr) & ' chars=' & $charsCount & ' at 0x' & Hex($charsPtr) & @CRLF)

            ; Sanity check: GW accounts have at most 28 character slots
            If $charsCount > 28 Or $charsCount <= 0 Or $charsPtr < 0x10000 Then
                ConsoleWrite('[GWLauncher] Invalid chars array (count=' & $charsCount & ' ptr=0x' & Hex($charsPtr) & '). Skipping selection.' & @CRLF)
                $charsCount = 0
            EndIf

            ; Each LoginCharacter is 4 + 20*2 = 44 bytes (uint32 + wchar[20])
            Local $charSize = 44
            For $c = 0 To $charsCount - 1
                Local $nameAddr = $charsPtr + ($c * $charSize) + 4  ; skip uint32 unk0
                Local $charName = MemRead($nameAddr, 'wchar[20]')
                $charName = StringStripWS($charName, 3)
                ConsoleWrite('[GWLauncher] Character ' & $c & ': "' & $charName & '"' & @CRLF)
                If $charName = $characterName Then
                    ; Write chosen_character_index
                    MemWrite($preGamePtr + 0x124, $c, 'dword')
                    ConsoleWrite('[GWLauncher] Selected character index ' & $c & @CRLF)
                    Sleep(500)
                    ExitLoop
                EndIf
            Next
        Else
            ConsoleWrite('[GWLauncher] PreGameContext not available (already in game?)' & @CRLF)
        EndIf
    EndIf

    ; Press Enter to click Play (works on character select screen)
    WinActivate($hWnd)
    Sleep(300)
    ControlSend($hWnd, '', '', '{ENTER}')
    ConsoleWrite('[GWLauncher] Sent Enter key to press Play' & @CRLF)
    Sleep(1000)
    Return True
EndFunc

;~ Dismiss the "Reconnect to previous session?" dialog
;~ Sends Right arrow (to select No) then Enter to confirm
;~ @param $choice - "yes" or "no" (default: "no")
;~ @param $gwWindowTitle - GW window title
;~ @return True if dialog was found and dismissed
Func GWLauncher_HandleReconnectDialog($choice = 'no', $gwWindowTitle = '')
    Local $hWnd = 0
    If $gwWindowTitle <> '' Then
        $hWnd = WinWait($gwWindowTitle, '', 5)
    Else
        $hWnd = WinWait('Guild Wars', '', 5)
    EndIf
    If $hWnd = 0 Then Return False

    WinActivate($hWnd)
    Sleep(500)

    ; The reconnect dialog has Yes (left) and No (right) buttons
    ; Default focus might be on Yes. Arrow keys navigate between them.
    If StringLower($choice) = 'no' Then
        ; Press Right to move to No, then Enter to confirm
        ControlSend($hWnd, '', '', '{RIGHT}')
        Sleep(200)
        ControlSend($hWnd, '', '', '{ENTER}')
        ConsoleWrite('[GWLauncher] Reconnect dialog: selected No' & @CRLF)
    Else
        ; Press Enter to accept Yes (default/left button)
        ControlSend($hWnd, '', '', '{ENTER}')
        ConsoleWrite('[GWLauncher] Reconnect dialog: selected Yes' & @CRLF)
    EndIf

    Sleep(1000)
    Return True
EndFunc

;~ Set the Froggy GUI controls for a character (hero config, add heroes checkbox)
;~ Call this AFTER the Froggy bot GUI is created but BEFORE clicking Start
;~ @param $characterName - character name to look up config for
Func GWLauncher_ConfigureFroggyGUI($characterName)
    Local $heroConfig = GWLauncher_GetHeroConfig($characterName)
    ConsoleWrite('[GWLauncher] Configuring GUI for ' & $characterName & ': heroes=' & $heroConfig & @CRLF)

    ; Set the Add Heroes checkbox to checked
    If Not GUI_IsAddHeroesChecked() Then
        GUICtrlSetState($GUI_GroupSettings_CheckAddHeroes, $GUI_CHECKED)
        ConsoleWrite('[GWLauncher] Checked Add Heroes' & @CRLF)
    EndIf

    ; Set the hero config dropdown
    Local $hWnd = WinGetHandle('Froggy HM v1.6')
    If $hWnd Then
        ControlCommand($hWnd, '', '[CLASS:ComboBox; INSTANCE:1]', 'SelectString', $heroConfig)
        ConsoleWrite('[GWLauncher] Set hero dropdown to: ' & $heroConfig & @CRLF)
    EndIf
EndFunc

;~ Get the hero config name for a character from AccountConfigs.json
;~ @return Config name (e.g. "Mercs", "Standard") or "Standard" as default
Func GWLauncher_GetHeroConfig($characterName, $configFile = '')
    If $configFile = '' Then $configFile = @ScriptDir & '\AccountConfigs.json'
    If Not FileExists($configFile) Then Return 'Standard'

    Local $parsed = _JSON_Parse(FileRead($configFile))
    If @error Or Not IsMap($parsed) Then Return 'Standard'

    If MapExists($parsed, $characterName) Then
        Local $charConfig = $parsed[$characterName]
        If IsMap($charConfig) And MapExists($charConfig, 'hero_config') Then
            Return $charConfig['hero_config']
        EndIf
    EndIf

    Return 'Standard'
EndFunc

;~ Fully automated: launch account, wait for login, scan for client, connect
Func GWLauncher_AutoLaunchAndConnect($characterName, $accountsFile = '', $timeout = 120)
    ConsoleWrite('[GWLauncher] Auto-launch: ' & $characterName & @CRLF)

    ; Load accounts
    Local $accounts = GWLauncher_LoadAccounts($accountsFile)
    If UBound($accounts) = 0 Then
        ConsoleWrite('[GWLauncher] No accounts loaded' & @CRLF)
        Return False
    EndIf

    ; Find account by character name
    Local $idx = GWLauncher_FindAccountByCharacter($accounts, $characterName)
    If $idx = -1 Then
        ConsoleWrite('[GWLauncher] Character not found in accounts: ' & $characterName & @CRLF)
        Return False
    EndIf

    ; Check if already running
    ScanAndUpdateGameClients()
    If IsArray($game_clients) And $game_clients[0][0] > 0 Then
        Local $existing = FindClientIndexByCharacterName($characterName)
        If $existing > 0 Then
            ConsoleWrite('[GWLauncher] Client already running, connecting...' & @CRLF)
            SelectClient($existing)
            Return True
        EndIf
    EndIf

    ; Launch the account
    Local $result = GWLauncher_LaunchAccount($accounts, $idx)
    If $result = 0 Then
        ConsoleWrite('[GWLauncher] Failed to launch client' & @CRLF)
        Return False
    EndIf

    ; Wait for GW window, handle reconnect, press Play, then connect
    ConsoleWrite('[GWLauncher] Waiting for client to log in (timeout: ' & $timeout & 's)...' & @CRLF)

    ; Phase 1: Wait for GW window to appear (15s)
    ConsoleWrite('[GWLauncher] Phase 1: Waiting 15s for GW window...' & @CRLF)
    Sleep(15000)

    ; Phase 2: Handle reconnect dialog (dismiss with No)
    ConsoleWrite('[GWLauncher] Phase 2: Handling reconnect dialog...' & @CRLF)
    Local $gwTitle = 'Guild Wars'
    ; Try character-specific title first, then generic
    If WinExists('Guild Wars - ' & $characterName) Then $gwTitle = 'Guild Wars - ' & $characterName
    GWLauncher_HandleReconnectDialog('no', $gwTitle)
    Sleep(3000)

    ; Phase 3: Press Play
    ConsoleWrite('[GWLauncher] Phase 3: Pressing Play...' & @CRLF)
    GWLauncher_ClickPlay($gwTitle, $characterName)
    Sleep(5000)

    ; Phase 4: Wait for client to appear in-game (map loaded)
    Local $waitTimer = TimerInit()
    While TimerDiff($waitTimer) < (($timeout - 25) * 1000)
        Sleep(5000)

        ScanAndUpdateGameClients()
        If IsArray($game_clients) And $game_clients[0][0] > 0 Then
            Local $clientIdx = FindClientIndexByCharacterName($characterName)
            If $clientIdx > 0 Then
                ConsoleWrite('[GWLauncher] Client found! Connecting to: ' & $characterName & @CRLF)
                SelectClient($clientIdx)
                Return True
            EndIf
        EndIf

        ConsoleWrite('[GWLauncher] Still waiting... (' & Int((TimerDiff($waitTimer) / 1000) + 25) & 's)' & @CRLF)
    WEnd

    ConsoleWrite('[GWLauncher] Timeout waiting for client: ' & $characterName & @CRLF)
    Return False
EndFunc
