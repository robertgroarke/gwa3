#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

ConsoleWrite("=== GWCA DLL Injection Test ===" & @CRLF)

; Use running GW or launch fresh
ScanAndUpdateGameClients()
If $game_clients[0][0] = 0 Then
    Local $accounts = GWLauncher_LoadAccounts()
    Local $idx = GWLauncher_FindAccountByCharacter($accounts, "D I S C O P A N I C")
    GWLauncher_LaunchAccount($accounts, $idx)
    Sleep(20000)
    ScanAndUpdateGameClients()
EndIf

SelectClient($game_clients[0][0])
InitializeGameClientForGWA2(False)
Local $processHandle = GetProcessHandle()
Local $gwPID = $game_clients[$game_clients[0][0]][0]
ConsoleWrite("Connected to PID " & $gwPID & @CRLF)

; Verify at char select
ConsoleWrite("At char select: " & IsAtCharSelect() & @CRLF)

; === Step 1: Inject gwca.dll ===
Local $dllPath = "c:\Users\Robert\Documents\GWA Censured X BotsHub\toolbox\GWToolboxpp-master\Dependencies\GWCA\bin\gwca.dll"
ConsoleWrite(@CRLF & "=== Injecting gwca.dll ===" & @CRLF)

; Write DLL path as wide string into GW process memory
Local $dllPathW = StringToBinary($dllPath, 2) & Binary("0x0000")  ; null-terminated UTF-16
Local $pathLen = BinaryLen($dllPathW)
Local $remotePath = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
    'handle', $processHandle, 'ptr', 0, 'ulong_ptr', $pathLen, _
    'dword', 0x1000, 'dword', 0x04)  ; MEM_COMMIT, PAGE_READWRITE
If Not IsArray($remotePath) Or $remotePath[0] = 0 Then
    ConsoleWrite("VirtualAllocEx failed" & @CRLF)
    Exit 1
EndIf
Local $remotePathAddr = $remotePath[0]

Local $pathBuf = DllStructCreate('byte[' & $pathLen & ']')
DllStructSetData($pathBuf, 1, $dllPathW)
DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
    'handle', $processHandle, 'ptr', $remotePathAddr, _
    'ptr', DllStructGetPtr($pathBuf), 'ulong_ptr', $pathLen, 'ulong_ptr*', 0)

; Get LoadLibraryW address from kernel32.dll
Local $k32 = DllCall('kernel32.dll', 'ptr', 'GetModuleHandleW', 'wstr', 'kernel32.dll')
Local $loadLib = DllCall('kernel32.dll', 'ptr', 'GetProcAddress', _
    'ptr', $k32[0], 'str', 'LoadLibraryW')
ConsoleWrite("LoadLibraryW at 0x" & Hex($loadLib[0]) & @CRLF)

; Create remote thread to call LoadLibraryW(dllPath)
Local $thread = DllCall($kernel_handle, 'handle', 'CreateRemoteThread', _
    'handle', $processHandle, 'ptr', 0, 'ulong_ptr', 0, _
    'ptr', $loadLib[0], 'ptr', $remotePathAddr, 'dword', 0, 'dword*', 0)

If Not IsArray($thread) Or $thread[0] = 0 Then
    ConsoleWrite("CreateRemoteThread failed" & @CRLF)
    Exit 1
EndIf

ConsoleWrite("Injection thread created. Waiting..." & @CRLF)
DllCall($kernel_handle, 'dword', 'WaitForSingleObject', 'handle', $thread[0], 'dword', 10000)

; Get the return value (HMODULE of loaded DLL)
Local $exitCode = DllCall($kernel_handle, 'bool', 'GetExitCodeThread', _
    'handle', $thread[0], 'dword*', 0)
Local $gwcaModule = $exitCode[2]
DllCall($kernel_handle, 'bool', 'CloseHandle', 'handle', $thread[0])

ConsoleWrite("gwca.dll loaded at 0x" & Hex($gwcaModule) & @CRLF)
If $gwcaModule = 0 Then
    ConsoleWrite("ERROR: gwca.dll failed to load" & @CRLF)
    Exit 1
EndIf

; === Step 2: Initialize GWCA Scanner ===
; GW::Scanner::Initialize(HINSTANCE hModule)
; We need to pass the GW.exe module handle (usually 0x00400000 or wherever it loaded)
; The GW module base can be read from the PEB

; Actually, Scanner::Initialize(HINSTANCE) takes the GW module handle
; Get it from our scan: the .text section start - 0x1000 gives the image base
Local $gwImageBase = $pe_sections_ranges[0][0] - 0x1000
ConsoleWrite("GW image base: 0x" & Hex($gwImageBase) & @CRLF)

; Scanner::Initialize is at gwca.dll offset 0x21850 from base
; But with ASLR, the actual address is gwcaModule + (0x10021850 - 0x10000000)
Local $scannerInit = $gwcaModule + 0x21850
ConsoleWrite("Scanner::Initialize at 0x" & Hex($scannerInit) & @CRLF)

; Call Scanner::Initialize(gwImageBase) via CreateRemoteThread
Local $thread2 = DllCall($kernel_handle, 'handle', 'CreateRemoteThread', _
    'handle', $processHandle, 'ptr', 0, 'ulong_ptr', 0, _
    'ptr', Ptr($scannerInit), 'ptr', Ptr($gwImageBase), 'dword', 0, 'dword*', 0)
If IsArray($thread2) And $thread2[0] <> 0 Then
    DllCall($kernel_handle, 'dword', 'WaitForSingleObject', 'handle', $thread2[0], 'dword', 10000)
    DllCall($kernel_handle, 'bool', 'CloseHandle', 'handle', $thread2[0])
    ConsoleWrite("Scanner initialized" & @CRLF)
Else
    ConsoleWrite("Scanner init failed" & @CRLF)
EndIf

; === Step 3: Initialize GW (finds all game functions) ===
Local $gwInit = $gwcaModule + 0x18E90
ConsoleWrite("GW::Initialize at 0x" & Hex($gwInit) & @CRLF)

Local $thread3 = DllCall($kernel_handle, 'handle', 'CreateRemoteThread', _
    'handle', $processHandle, 'ptr', 0, 'ulong_ptr', 0, _
    'ptr', Ptr($gwInit), 'ptr', 0, 'dword', 0, 'dword*', 0)
If IsArray($thread3) And $thread3[0] <> 0 Then
    DllCall($kernel_handle, 'dword', 'WaitForSingleObject', 'handle', $thread3[0], 'dword', 15000)
    DllCall($kernel_handle, 'bool', 'CloseHandle', 'handle', $thread3[0])
    ConsoleWrite("GW::Initialize completed" & @CRLF)
Else
    ConsoleWrite("GW::Initialize failed" & @CRLF)
EndIf

; === Step 4: Click the Play button ===
; ButtonClick(Frame* frame) is at gwca.dll offset 0x255E0
; But ButtonClick takes a Frame* pointer. We need to:
; 1. Call GetFrameById(40) to get the Frame pointer
; 2. Cast to ButtonFrame*
; 3. Call Click()
;
; For simplicity, let's build a small shellcode that:
;   push 40              ; frame_id
;   call GetFrameById    ; returns Frame* in eax
;   test eax,eax
;   jz skip
;   mov ecx, eax         ; this = Frame* (for ButtonFrame::Click which is __thiscall)
;   call Click
; skip:
;   ret

Local $getFrameById = $gwcaModule + 0x25CC0
Local $clickFunc = $gwcaModule + 0x16660  ; ButtonFrame::Click
ConsoleWrite(@CRLF & "GetFrameById at 0x" & Hex($getFrameById) & @CRLF)
ConsoleWrite("ButtonFrame::Click at 0x" & Hex($clickFunc) & @CRLF)

; Allocate shellcode memory
Local $scMem = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
    'handle', $processHandle, 'ptr', 0, 'ulong_ptr', 64, _
    'dword', 0x1000, 'dword', 0x40)
Local $scAddr = Int($scMem[0])

; Build shellcode
Local $sc = DllStructCreate('byte[40]')
Local $p = 1

; push 40 (frame_id of Play button)
DllStructSetData($sc, 1, 0x6A, $p)
$p += 1
DllStructSetData($sc, 1, 40, $p)
$p += 1

; call GetFrameById  (E8 rel32)
DllStructSetData($sc, 1, 0xE8, $p)
$p += 1
Local $rel1 = $getFrameById - ($scAddr + $p - 1 + 4)
_WriteLE32($sc, $p, $rel1)
$p += 4

; add esp, 4 (cdecl cleanup for GetFrameById)
DllStructSetData($sc, 1, 0x83, $p)
$p += 1
DllStructSetData($sc, 1, 0xC4, $p)
$p += 1
DllStructSetData($sc, 1, 0x04, $p)
$p += 1

; test eax, eax
DllStructSetData($sc, 1, 0x85, $p)
$p += 1
DllStructSetData($sc, 1, 0xC0, $p)
$p += 1

; jz skip (skip 5 bytes: mov ecx + call)
DllStructSetData($sc, 1, 0x74, $p)
$p += 1
DllStructSetData($sc, 1, 0x07, $p)  ; skip 7 bytes
$p += 1

; mov ecx, eax (__thiscall: ecx = this = Frame*)
DllStructSetData($sc, 1, 0x8B, $p)
$p += 1
DllStructSetData($sc, 1, 0xC8, $p)
$p += 1

; call ButtonFrame::Click (E8 rel32)
DllStructSetData($sc, 1, 0xE8, $p)
$p += 1
Local $rel2 = $clickFunc - ($scAddr + $p - 1 + 4)
_WriteLE32($sc, $p, $rel2)
$p += 4

; ret
DllStructSetData($sc, 1, 0xC3, $p)

; Write shellcode
DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
    'handle', $processHandle, 'ptr', Ptr($scAddr), _
    'ptr', DllStructGetPtr($sc), 'ulong_ptr', $p, 'ulong_ptr*', 0)

ConsoleWrite("Shellcode at 0x" & Hex($scAddr) & " (" & $p & " bytes)" & @CRLF)

; Screenshot before
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\gwca_before.png', $game_clients[$game_clients[0][0]][2])

; Execute the click shellcode
ConsoleWrite(@CRLF & "=== Step 4: Click Play via GWCA (SKIPPED - testing init only) ===" & @CRLF)
; SKIP the click for now — testing if init alone causes crash
ConsoleWrite("Click SKIPPED — testing init stability" & @CRLF)

; Instead, read GWCA's data section to see what function addresses it found
; SendFrameUIMessage func ptr is at gwcaModule + (0x1008A3A0 - 0x10000000) = +0x8A3A0
Local $gwcaSendFrame = MemoryRead($processHandle, $gwcaModule + 0x8A3A0, 'dword')
ConsoleWrite("GWCA SendFrameUIMsg ptr = 0x" & Hex($gwcaSendFrame) & @CRLF)

; Also read other key function pointers from gwca.dll data
Local $gwcaGetChild = MemoryRead($processHandle, $gwcaModule + 0x8A37C, 'dword')
ConsoleWrite("GWCA GetChildFrame ptr = 0x" & Hex($gwcaGetChild) & @CRLF)

Local $gwcaRootFrame = MemoryRead($processHandle, $gwcaModule + 0x8A410, 'dword')
ConsoleWrite("GWCA RootFrame ptr = 0x" & Hex($gwcaRootFrame) & @CRLF)

Sleep(5000)

; Screenshot after
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\gwca_after.png', $game_clients[$game_clients[0][0]][2])

Local $status = MemoryRead($processHandle, GetLabel('StatusCode'), 'dword')
ConsoleWrite("StatusCode: " & $status & @CRLF)

If $status <> 0 Then
    ConsoleWrite("MAP LOADING! Play button was clicked!" & @CRLF)
Else
    ConsoleWrite("Still at char select" & @CRLF)
EndIf

ConsoleWrite("=== DONE ===" & @CRLF)
