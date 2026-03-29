#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

ConsoleWrite("=== Direct SendFrameUIMsg Click ===" & @CRLF)

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
ConsoleWrite("Connected. At char select: " & IsAtCharSelect() & @CRLF)

; Get Play button frame directly (not through GWCA)
Local $result = GetFrameByHash($FRAME_HASH_PLAY_BUTTON)
If $result[0] = 0 Then
    ConsoleWrite("Play button not found" & @CRLF)
    Exit
EndIf
Local $framePtr = Int($result[0])
Local $frameId = $result[1]
ConsoleWrite("Play frame: ptr=0x" & Hex($framePtr) & " id=" & $frameId & @CRLF)

; The CONFIRMED correct game function for SendFrameUIMsg
Local $sendFrameFunc = 0x007986D0  ; confirmed by both our scan AND gwca.dll

; Build shellcode that:
; 1. Sets ECX = framePtr + 0xA8 (callbacks array = this ptr)
; 2. Pushes lParam=0, wParam=&action, msgid=0x31
; 3. Calls SendFrameUIMsg
; 4. Then does same with msgid that the actual game's internal caller uses (0x2B)
; 5. Jumps to CommandReturn

Local $scMem = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
    'handle', $processHandle, 'ptr', 0, 'ulong_ptr', 128, _
    'dword', 0x1000, 'dword', 0x40)
Local $scAddr = Int($scMem[0])
Local $actionAddr = $scAddr + 64  ; action data at offset 64

; Write action data: {frame_id, child_offset_id, action_state=MouseDown, 0, 0}
Local $childOff = MemoryRead($processHandle, $framePtr + 0xB8, 'dword')
Local $ad = DllStructCreate('dword;dword;dword;dword;dword')
DllStructSetData($ad, 1, $frameId)
DllStructSetData($ad, 2, $childOff)
DllStructSetData($ad, 3, 0x6)  ; MouseDown
DllStructSetData($ad, 4, 0)
DllStructSetData($ad, 5, 0)
DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
    'handle', $processHandle, 'ptr', Ptr($actionAddr), _
    'ptr', DllStructGetPtr($ad), 'ulong_ptr', 20, 'ulong_ptr*', 0)

; Calibrate CommandReturn
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

; Store CommandReturn and a test flag
Local $crPtrAddr = $scAddr + 80
Local $testFlagAddr = $scAddr + 84
MemoryWrite($processHandle, $crPtrAddr, $crActual, 'dword')
MemoryWrite($processHandle, $testFlagAddr, 0, 'dword')

; Build shellcode:
; mov ecx, <framePtr + 0xA8>   ; B9 <le32>
; push 0                       ; 6A 00
; push <actionAddr>             ; 68 <le32>
; push 0x31                    ; 6A 31
; call <sendFrameFunc>          ; E8 <rel32>
; mov dword [testFlagAddr], 1   ; C7 05 <le32> 01 00 00 00
; jmp [crPtrAddr]               ; FF 25 <le32>

Local $sc = DllStructCreate('byte[48]')
Local $p = 1
Local $thisPtr = $framePtr + 0xA8

; mov ecx, thisPtr
DllStructSetData($sc, 1, 0xB9, $p)
$p += 1
_WriteLE32($sc, $p, $thisPtr)
$p += 4

; push 0
DllStructSetData($sc, 1, 0x6A, $p)
$p += 1
DllStructSetData($sc, 1, 0x00, $p)
$p += 1

; push actionAddr
DllStructSetData($sc, 1, 0x68, $p)
$p += 1
_WriteLE32($sc, $p, $actionAddr)
$p += 4

; push 0x31
DllStructSetData($sc, 1, 0x6A, $p)
$p += 1
DllStructSetData($sc, 1, 0x31, $p)
$p += 1

; call sendFrameFunc
DllStructSetData($sc, 1, 0xE8, $p)
$p += 1
_WriteLE32($sc, $p, $sendFrameFunc - ($scAddr + $p - 1 + 4))
$p += 4

; mov dword [testFlagAddr], 1
DllStructSetData($sc, 1, 0xC7, $p)
$p += 1
DllStructSetData($sc, 1, 0x05, $p)
$p += 1
_WriteLE32($sc, $p, $testFlagAddr)
$p += 4
_WriteLE32($sc, $p, 1)
$p += 4

; jmp [crPtrAddr]
DllStructSetData($sc, 1, 0xFF, $p)
$p += 1
DllStructSetData($sc, 1, 0x25, $p)
$p += 1
_WriteLE32($sc, $p, $crPtrAddr)
$p += 4

; Write shellcode
DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
    'handle', $processHandle, 'ptr', Ptr($scAddr), _
    'ptr', DllStructGetPtr($sc), 'ulong_ptr', $p - 1, 'ulong_ptr*', 0)

ConsoleWrite("Shellcode at 0x" & Hex($scAddr) & " (" & ($p-1) & " bytes)" & @CRLF)
ConsoleWrite("thisPtr=0x" & Hex($thisPtr) & " actionAddr=0x" & Hex($actionAddr) & @CRLF)
ConsoleWrite("CommandReturn=0x" & Hex($crActual) & @CRLF)

; Screenshot before
Local $hWnd = $game_clients[$game_clients[0][0]][2]
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\direct_click_before.png', $hWnd)

; Sync queue counter from game before enqueueing
$queue_counter = MemoryRead($processHandle, GetLabel('QueueCounter'), 'dword')
ConsoleWrite("Synced queue counter = " & $queue_counter & @CRLF)

; Queue command
Local $cmd = DllStructCreate('dword;dword')
DllStructSetData($cmd, 1, $scAddr)
DllStructSetData($cmd, 2, 0)
Enqueue(DllStructGetPtr($cmd), DllStructGetSize($cmd))
ConsoleWrite("Command queued at slot " & ($queue_counter) & @CRLF)

Sleep(2000)

; Check if queue slot was consumed
Local $slotAddr = Int(GetLabel('QueueBase')) + (($queue_counter - 1) * 256)
If $queue_counter > 0 Then
    Local $slotVal = MemoryRead($processHandle, $slotAddr, 'dword')
    ConsoleWrite("Queue slot[" & ($queue_counter - 1) & "] = 0x" & Hex($slotVal) & " (0=consumed)" & @CRLF)
EndIf
ConsoleWrite("Game QueueCounter now: " & MemoryRead($processHandle, GetLabel('QueueCounter'), 'dword') & @CRLF)

; Check test flag
Local $flag = MemoryRead($processHandle, $testFlagAddr, 'dword')
ConsoleWrite("Test flag = " & $flag & " (1 = shellcode executed)" & @CRLF)

Sleep(3000)

; Screenshot after
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\direct_click_after.png', $hWnd)
Local $status = MemoryRead($processHandle, GetLabel('StatusCode'), 'dword')
ConsoleWrite("StatusCode: " & $status & @CRLF)

If $status <> 0 Then
    ConsoleWrite("*** PLAY CLICKED! ***" & @CRLF)
Else
    ConsoleWrite("Still at char select" & @CRLF)
    ; Try MouseUp too
    ConsoleWrite("Trying MouseUp..." & @CRLF)
    DllStructSetData($ad, 3, 0x7)  ; MouseUp
    DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
        'handle', $processHandle, 'ptr', Ptr($actionAddr), _
        'ptr', DllStructGetPtr($ad), 'ulong_ptr', 20, 'ulong_ptr*', 0)
    MemoryWrite($processHandle, $testFlagAddr, 0, 'dword')
    $queue_counter = MemoryRead($processHandle, GetLabel('QueueCounter'), 'dword')
    Enqueue(DllStructGetPtr($cmd), DllStructGetSize($cmd))
    Sleep(3000)
    $flag = MemoryRead($processHandle, $testFlagAddr, 'dword')
    $status = MemoryRead($processHandle, GetLabel('StatusCode'), 'dword')
    ConsoleWrite("MouseUp flag=" & $flag & " status=" & $status & @CRLF)
EndIf

ConsoleWrite("=== DONE ===" & @CRLF)
