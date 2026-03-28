#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

ConsoleWrite("=== GWCA Click via Game Thread ===" & @CRLF)

; Use the already-running GW with GWCA injected (PID 48784)
ScanAndUpdateGameClients()
SelectClient(1)
InitializeGameClientForGWA2(False)
Local $processHandle = GetProcessHandle()
ConsoleWrite("Connected. At char select: " & IsAtCharSelect() & @CRLF)

; gwca.dll is already loaded from the previous test at 0x70590000
Local $gwcaBase = 0x70590000

; ButtonFrame::Click at gwcaBase + 0x16660
; GetFrameById at gwcaBase + 0x25CC0
Local $clickFunc = $gwcaBase + 0x16660
Local $getFrameById = $gwcaBase + 0x25CC0

; Build shellcode that calls GWCA's GetFrameById(40) then ButtonFrame::Click()
; This time, queue it through our command queue (game thread)
Local $scMem = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
    'handle', $processHandle, 'ptr', 0, 'ulong_ptr', 64, _
    'dword', 0x1000, 'dword', 0x40)
Local $scAddr = Int($scMem[0])

Local $sc = DllStructCreate('byte[40]')
Local $p = 1

; push 40 (frame_id of Play button)
DllStructSetData($sc, 1, 0x6A, $p)
$p += 1
DllStructSetData($sc, 1, 40, $p)  ; frame_id = 40
$p += 1

; call GetFrameById (E8 rel32) — __cdecl, returns Frame* in eax
DllStructSetData($sc, 1, 0xE8, $p)
$p += 1
_WriteLE32($sc, $p, $getFrameById - ($scAddr + $p - 1 + 4))
$p += 4

; add esp, 4 (cdecl cleanup)
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

; jz skip (2 + 5 = 7 bytes to skip: mov ecx,eax + call Click)
DllStructSetData($sc, 1, 0x74, $p)
$p += 1
DllStructSetData($sc, 1, 0x07, $p)
$p += 1

; mov ecx, eax (__thiscall: this = Frame*)
DllStructSetData($sc, 1, 0x8B, $p)
$p += 1
DllStructSetData($sc, 1, 0xC8, $p)
$p += 1

; call ButtonFrame::Click (E8 rel32) — __thiscall, ret 4 (cleans 1 stack arg?? no, Click has no args besides this)
DllStructSetData($sc, 1, 0xE8, $p)
$p += 1
_WriteLE32($sc, $p, $clickFunc - ($scAddr + $p - 1 + 4))
$p += 4

; skip: jmp CommandReturn (via stored address)
; We need to return to the main loop. Store CommandReturn addr.
Local $crLabel = Int(GetLabel('CommandReturn'))
Local $crBuf = DllStructCreate('byte[16]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $processHandle, 'ptr', Ptr($crLabel - 8), _
    'ptr', DllStructGetPtr($crBuf), 'ulong_ptr', 16, 'ulong_ptr*', 0)
Local $crActual = $crLabel
For $ci = 1 To 13
    If DllStructGetData($crBuf, 1, $ci) = 0x8B And DllStructGetData($crBuf, 1, $ci+1) = 0x0D Then
        $crActual = $crLabel - 8 + ($ci - 1)
        ExitLoop
    EndIf
Next

; Store CommandReturn address at scAddr+32 (after shellcode)
Local $crPtr = $scAddr + 32
MemoryWrite($processHandle, $crPtr, $crActual, 'dword')

; jmp [crPtr] = FF 25 <le32>
DllStructSetData($sc, 1, 0xFF, $p)
$p += 1
DllStructSetData($sc, 1, 0x25, $p)
$p += 1
_WriteLE32($sc, $p, $crPtr)
$p += 4

; Write shellcode
DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
    'handle', $processHandle, 'ptr', Ptr($scAddr), _
    'ptr', DllStructGetPtr($sc), 'ulong_ptr', $p - 1, 'ulong_ptr*', 0)

ConsoleWrite("Shellcode at 0x" & Hex($scAddr) & " CommandReturn at 0x" & Hex($crActual) & @CRLF)

; Screenshot before
Local $hWnd = $game_clients[$game_clients[0][0]][2]
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\gwca_click_before.png', $hWnd)

; Queue via command queue (game thread!)
Local $cmd = DllStructCreate('dword;dword')
DllStructSetData($cmd, 1, $scAddr)
DllStructSetData($cmd, 2, 0)
Enqueue(DllStructGetPtr($cmd), DllStructGetSize($cmd))
ConsoleWrite("Command queued. Waiting 5s..." & @CRLF)
Sleep(5000)

; Screenshot after
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\gwca_click_after.png', $hWnd)
Local $status = MemoryRead($processHandle, GetLabel('StatusCode'), 'dword')
ConsoleWrite("StatusCode: " & $status & @CRLF)

If $status <> 0 Then
    ConsoleWrite("*** PLAY BUTTON CLICKED! Map loading! ***" & @CRLF)
Else
    ConsoleWrite("Still at char select" & @CRLF)
EndIf

ConsoleWrite("=== DONE ===" & @CRLF)
