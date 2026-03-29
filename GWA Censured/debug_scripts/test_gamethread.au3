#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== GameThread Test ===" & @CRLF)

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
Local $ph = GetProcessHandle()

; Step 1: Inject gwca.dll
Local $dllPath = "c:\Users\Robert\Documents\GWA Censured X BotsHub\toolbox\GWToolboxpp-master\Dependencies\GWCA\bin\gwca.dll"
Local $dllPathW = StringToBinary($dllPath, 2) & Binary("0x0000")
Local $pathLen = BinaryLen($dllPathW)

Local $remotePath = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
    'handle', $ph, 'ptr', 0, 'ulong_ptr', $pathLen, _
    'dword', 0x1000, 'dword', 0x04)
Local $rpa = $remotePath[0]

Local $pathBuf = DllStructCreate('byte[' & $pathLen & ']')
DllStructSetData($pathBuf, 1, $dllPathW)
DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
    'handle', $ph, 'ptr', $rpa, _
    'ptr', DllStructGetPtr($pathBuf), 'ulong_ptr', $pathLen, 'ulong_ptr*', 0)

Local $k32 = DllCall('kernel32.dll', 'ptr', 'GetModuleHandleW', 'wstr', 'kernel32.dll')
Local $loadLib = DllCall('kernel32.dll', 'ptr', 'GetProcAddress', 'ptr', $k32[0], 'str', 'LoadLibraryW')

Local $t1 = DllCall($kernel_handle, 'handle', 'CreateRemoteThread', _
    'handle', $ph, 'ptr', 0, 'ulong_ptr', 0, _
    'ptr', $loadLib[0], 'ptr', $rpa, 'dword', 0, 'dword*', 0)
DllCall($kernel_handle, 'dword', 'WaitForSingleObject', 'handle', $t1[0], 'dword', 10000)
Local $ec = DllCall($kernel_handle, 'bool', 'GetExitCodeThread', 'handle', $t1[0], 'dword*', 0)
Local $gwcaBase = $ec[2]
DllCall($kernel_handle, 'bool', 'CloseHandle', 'handle', $t1[0])
ConsoleWrite("gwca.dll at 0x" & Hex($gwcaBase) & @CRLF)

; Step 2: Initialize Scanner with GW module
Local $gwBase = $pe_sections_ranges[0][0] - 0x1000
Local $scanInit = $gwcaBase + 0x21850
Local $t2 = DllCall($kernel_handle, 'handle', 'CreateRemoteThread', _
    'handle', $ph, 'ptr', 0, 'ulong_ptr', 0, _
    'ptr', Ptr($scanInit), 'ptr', Ptr($gwBase), 'dword', 0, 'dword*', 0)
DllCall($kernel_handle, 'dword', 'WaitForSingleObject', 'handle', $t2[0], 'dword', 10000)
DllCall($kernel_handle, 'bool', 'CloseHandle', 'handle', $t2[0])
ConsoleWrite("Scanner initialized" & @CRLF)

; Step 3: Initialize GW (sets up game thread hook)
Local $gwInit = $gwcaBase + 0x18E90
Local $t3 = DllCall($kernel_handle, 'handle', 'CreateRemoteThread', _
    'handle', $ph, 'ptr', 0, 'ulong_ptr', 0, _
    'ptr', Ptr($gwInit), 'ptr', 0, 'dword', 0, 'dword*', 0)
DllCall($kernel_handle, 'dword', 'WaitForSingleObject', 'handle', $t3[0], 'dword', 15000)
DllCall($kernel_handle, 'bool', 'CloseHandle', 'handle', $t3[0])
ConsoleWrite("GW::Initialize done" & @CRLF)

; Step 4: Enable game thread hooks
Local $enableHooks = $gwcaBase + 0x196C0
Local $t4 = DllCall($kernel_handle, 'handle', 'CreateRemoteThread', _
    'handle', $ph, 'ptr', 0, 'ulong_ptr', 0, _
    'ptr', Ptr($enableHooks), 'ptr', 0, 'dword', 0, 'dword*', 0)
DllCall($kernel_handle, 'dword', 'WaitForSingleObject', 'handle', $t4[0], 'dword', 5000)
DllCall($kernel_handle, 'bool', 'CloseHandle', 'handle', $t4[0])
ConsoleWrite("GameThread::EnableHooks done" & @CRLF)

Sleep(2000)

; Step 5: Build shellcode that calls ButtonClick via GameThread::Enqueue
; But GameThread::Enqueue takes std::function<void()> which is complex.
; Instead, build shellcode that:
;   1. Gets Frame* via GetFrameById(40)
;   2. Calls ButtonFrame::Click() (which calls MouseAction + SendFrameUIMsg)
; And call this shellcode via GameThread::Enqueue by writing to GWCA's queue.
;
; Actually, the simpler approach: build shellcode that calls
; GWCA's ButtonClick(frame_ptr) — which is just a thin wrapper.
; Then use CreateRemoteThread BUT through GWCA's GameThread mechanism.
;
; Or even simpler: write a shellcode that checks IsInGameThread() first,
; and if true, calls Click directly. If false, it sleeps and retries.
; But CreateRemoteThread is never the game thread.
;
; THE REAL APPROACH: build a shellcode that:
; 1. Calls GameThread::Enqueue with a function pointer
; But std::function is a C++ object...
;
; Let me try a DIFFERENT approach: write the shellcode address
; directly into GWCA's game thread callback list.

; Check if game thread callbacks are being processed
; GameThread stores callbacks in its data section
; Let me read GWCA's internal game thread state
Local $isInGameThread = $gwcaBase + 0x19E10
ConsoleWrite("IsInGameThread at 0x" & Hex($isInGameThread) & @CRLF)

; Call IsInGameThread from the game thread by... we can't easily.
; But we can check GWCA's internal game thread hook state.
; The GameThread module hooks a game function. If the hook fires at char select,
; GWCA can process enqueued callbacks.

; Let me try the simplest thing: just call GWCA's ButtonClick(frame_ptr)
; from a remote thread. The earlier test showed it doesn't work from a remote thread.
; But maybe with EnableHooks called, GWCA's internal hooks redirect the call?

; Actually wait — GWCA::UI::ButtonClick calls ButtonFrame::Click which calls
; MouseAction which calls GWCA's SendFrameUIMessage wrapper (not the game function directly).
; GWCA's wrapper checks hooks and then calls the game function.
; Maybe the wrapper needs to be on the game thread too.

; Let me try: call GetFrameById + ButtonFrame::Click via CreateRemoteThread
; with GWCA fully initialized. The earlier attempt crashed because GWCA wasn't initialized.
; Now it IS initialized.

Local $getFrameById = $gwcaBase + 0x25CC0
Local $btnClick = $gwcaBase + 0x16660  ; ButtonFrame::Click (__thiscall)

; Build shellcode:
; push 40             ; frame_id
; call GetFrameById   ; __cdecl, returns Frame*
; add esp, 4
; test eax, eax
; jz skip
; mov ecx, eax        ; this = Frame*
; push 6              ; ActionState::MouseDown
; call ButtonFrame::Click  ; __thiscall
; push 40
; call GetFrameById
; add esp, 4
; test eax, eax
; jz skip
; mov ecx, eax
; push 7              ; ActionState::MouseUp  (actually Click calls MouseAction twice)
; ... wait, Click() takes no args, it calls MouseAction(6) then MouseAction(7) internally

; Actually ButtonFrame::Click has no args (just this in ECX).
; Let me just call it:

Local $mem = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
    'handle', $ph, 'ptr', 0, 'ulong_ptr', 64, _
    'dword', 0x1000, 'dword', 0x40)
Local $scAddr = Int($mem[0])

Local $sc = DllStructCreate('byte[32]')
Local $p = 1

; push 40
DllStructSetData($sc, 1, 0x6A, $p)
$p += 1
DllStructSetData($sc, 1, 40, $p)
$p += 1

; call GetFrameById (cdecl)
DllStructSetData($sc, 1, 0xE8, $p)
$p += 1
_WriteLE32($sc, $p, $getFrameById - ($scAddr + $p - 1 + 4))
$p += 4

; add esp, 4
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

; jz skip (skip Click call = 2 + 5 = 7 bytes)
DllStructSetData($sc, 1, 0x74, $p)
$p += 1
DllStructSetData($sc, 1, 0x07, $p)
$p += 1

; mov ecx, eax
DllStructSetData($sc, 1, 0x8B, $p)
$p += 1
DllStructSetData($sc, 1, 0xC8, $p)
$p += 1

; call ButtonFrame::Click (thiscall, no stack args, returns bool)
DllStructSetData($sc, 1, 0xE8, $p)
$p += 1
_WriteLE32($sc, $p, $btnClick - ($scAddr + $p - 1 + 4))
$p += 4

; skip: ret
DllStructSetData($sc, 1, 0xC3, $p)

DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
    'handle', $ph, 'ptr', Ptr($scAddr), _
    'ptr', DllStructGetPtr($sc), 'ulong_ptr', $p, 'ulong_ptr*', 0)

ConsoleWrite("Shellcode at 0x" & Hex($scAddr) & @CRLF)

; Execute
ConsoleWrite(@CRLF & "=== Calling ButtonFrame::Click via GWCA ===" & @CRLF)
Local $t5 = DllCall($kernel_handle, 'handle', 'CreateRemoteThread', _
    'handle', $ph, 'ptr', 0, 'ulong_ptr', 0, _
    'ptr', Ptr($scAddr), 'ptr', 0, 'dword', 0, 'dword*', 0)
If IsArray($t5) And $t5[0] <> 0 Then
    DllCall($kernel_handle, 'dword', 'WaitForSingleObject', 'handle', $t5[0], 'dword', 5000)
    DllCall($kernel_handle, 'bool', 'CloseHandle', 'handle', $t5[0])
    ConsoleWrite("Click executed" & @CRLF)
EndIf

Sleep(5000)
Local $status = MemoryRead($ph, GetLabel('StatusCode'), 'dword')
ConsoleWrite("StatusCode: " & $status & @CRLF)

If $status <> 0 Then
    ConsoleWrite("*** PLAY CLICKED! ***" & @CRLF)
Else
    ConsoleWrite("Still at char select" & @CRLF)
EndIf

ConsoleWrite("=== DONE ===" & @CRLF)
