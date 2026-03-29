#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== ECX Value Test ===" & @CRLF)

ScanAndUpdateGameClients()
SelectClient(1)
InitializeGameClientForGWA2(False)
Local $ph = GetProcessHandle()

If Not IsAtCharSelect() Then
    ConsoleWrite("Not at char select" & @CRLF)
    Exit
EndIf

Local $result = GetFrameByHash($FRAME_HASH_PLAY_BUTTON)
If $result[0] = 0 Then
    ConsoleWrite("Play button not found" & @CRLF)
    Exit
EndIf

Local $framePtr = Int($result[0])
Local $frameId = $result[1]
Local $childOffsetId = MemoryRead($ph, $framePtr + 0xB8, 'dword')
Local $context = _GetFrameContext($framePtr)
Local $sendFunc = Int(GetLabel('SendFrameUIMsg'))

ConsoleWrite("Frame: ptr=0x" & Hex($framePtr) & " id=" & $frameId & @CRLF)
ConsoleWrite("Context: 0x" & Hex($context) & @CRLF)
ConsoleWrite("SendFrameUIMsg: 0x" & Hex($sendFunc) & @CRLF)
ConsoleWrite("ECX with +0x84: 0x" & Hex($context + 0x84) & @CRLF)
ConsoleWrite("ECX with +0xA8: 0x" & Hex($context + 0xA8) & @CRLF)

; Try with context + 0xA8 (what GWCA uses)
Local $thisPtr = $context + 0xA8
ConsoleWrite(@CRLF & "Testing with ECX = context + 0xA8 = 0x" & Hex($thisPtr) & @CRLF)

; Alloc memory
Local $mem = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
    'handle', $ph, 'ptr', 0, 'ulong_ptr', 64, _
    'dword', 0x1000, 'dword', 0x40)
Local $scAddr = Int($mem[0])
Local $dataAddr = $scAddr + 32

; Write action data: MouseUp (0x7) only
Local $ad = DllStructCreate('dword;dword;dword;dword;dword')
DllStructSetData($ad, 1, $frameId)
DllStructSetData($ad, 2, $childOffsetId)
DllStructSetData($ad, 3, 0x7)  ; MouseUp
DllStructSetData($ad, 4, 0)
DllStructSetData($ad, 5, 0)
DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
    'handle', $ph, 'ptr', Ptr($dataAddr), _
    'ptr', DllStructGetPtr($ad), 'ulong_ptr', 20, 'ulong_ptr*', 0)

; Build shellcode
Local $sc = DllStructCreate('byte[24]')
Local $p = 1
DllStructSetData($sc, 1, 0xB9, $p)     ; mov ecx
$p += 1
_WriteLE32($sc, $p, $thisPtr)
$p += 4
DllStructSetData($sc, 1, 0x6A, $p)     ; push 0
$p += 1
DllStructSetData($sc, 1, 0x00, $p)
$p += 1
DllStructSetData($sc, 1, 0x68, $p)     ; push dataAddr
$p += 1
_WriteLE32($sc, $p, $dataAddr)
$p += 4
DllStructSetData($sc, 1, 0x6A, $p)     ; push 0x31
$p += 1
DllStructSetData($sc, 1, 0x31, $p)
$p += 1
DllStructSetData($sc, 1, 0xE8, $p)     ; call sendFunc
$p += 1
_WriteLE32($sc, $p, $sendFunc - ($scAddr + $p - 1 + 4))
$p += 4
DllStructSetData($sc, 1, 0xC3, $p)     ; ret

; Dump shellcode
Local $hex = ""
For $b = 1 To $p
    $hex &= Hex(DllStructGetData($sc, 1, $b), 2) & " "
Next
ConsoleWrite("Shellcode: " & $hex & @CRLF)

; Write shellcode
DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
    'handle', $ph, 'ptr', Ptr($scAddr), _
    'ptr', DllStructGetPtr($sc), 'ulong_ptr', $p, 'ulong_ptr*', 0)

; Execute via CreateRemoteThread
ConsoleWrite("Executing via CreateRemoteThread..." & @CRLF)
Local $th = DllCall($kernel_handle, 'handle', 'CreateRemoteThread', _
    'handle', $ph, 'ptr', 0, 'ulong_ptr', 0, _
    'ptr', Ptr($scAddr), 'ptr', 0, 'dword', 0, 'dword*', 0)
If IsArray($th) And $th[0] <> 0 Then
    Local $waitResult = DllCall($kernel_handle, 'dword', 'WaitForSingleObject', 'handle', $th[0], 'dword', 5000)
    ConsoleWrite("WaitForSingleObject result: " & $waitResult[0] & " (0=WAIT_OBJECT_0, 258=TIMEOUT)" & @CRLF)
    DllCall($kernel_handle, 'bool', 'CloseHandle', 'handle', $th[0])
EndIf

Sleep(3000)
ConsoleWrite("Still at char select: " & IsAtCharSelect() & @CRLF)
ConsoleWrite("=== DONE ===" & @CRLF)
