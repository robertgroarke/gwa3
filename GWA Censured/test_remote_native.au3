#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

ConsoleWrite("=== Native Click via CreateRemoteThread ===" & @CRLF)

ScanAndUpdateGameClients()
SelectClient(1)
InitializeGameClientForGWA2(False)
Local $ph = GetProcessHandle()
Local $hWnd = $game_clients[$game_clients[0][0]][2]

ConsoleWrite("At char select: " & IsAtCharSelect() & @CRLF)

Local $pf = GetFrameByHash($FRAME_HASH_PLAY_BUTTON)
Local $fp = Int($pf[0])
Local $fid = $pf[1]
Local $co = MemoryRead($ph, $fp + 0xB8, 'dword')
ConsoleWrite("Play: ptr=0x" & Hex($fp) & " id=" & $fid & @CRLF)

; Get parent frame context
Local $ctx = _GetFrameContext($fp)
ConsoleWrite("Context (parent frame): 0x" & Hex($ctx) & @CRLF)

Local $sendFunc = Int(GetLabel('SendFrameUIMsg'))
ConsoleWrite("SendFrameUIMsg: 0x" & Hex($sendFunc) & @CRLF)

; Alloc shellcode + data
Local $mem = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
    'handle', $ph, 'ptr', 0, 'ulong_ptr', 64, _
    'dword', 0x1000, 'dword', 0x40)
Local $scAddr = Int($mem[0])
Local $dataAddr = $scAddr + 32

; MouseDown then MouseUp
For $actionState = 6 To 7
    ; Write action data
    Local $ad = DllStructCreate('dword;dword;dword;dword;dword')
    DllStructSetData($ad, 1, $fid)
    DllStructSetData($ad, 2, $co)
    DllStructSetData($ad, 3, $actionState)
    DllStructSetData($ad, 4, 0)
    DllStructSetData($ad, 5, 0)
    DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
        'handle', $ph, 'ptr', Ptr($dataAddr), _
        'ptr', DllStructGetPtr($ad), 'ulong_ptr', 20, 'ulong_ptr*', 0)

    ; Build shellcode: mov ecx,<ctx+0xA8>; push 0; push dataAddr; push 0x31; call sendFunc; ret
    Local $thisPtr = $ctx + 0xA8
    Local $sc = DllStructCreate('byte[24]')
    Local $p = 1
    DllStructSetData($sc, 1, 0xB9, $p)
    $p += 1
    _WriteLE32($sc, $p, $thisPtr)
    $p += 4
    DllStructSetData($sc, 1, 0x6A, $p)
    $p += 1
    DllStructSetData($sc, 1, 0x00, $p)
    $p += 1
    DllStructSetData($sc, 1, 0x68, $p)
    $p += 1
    _WriteLE32($sc, $p, $dataAddr)
    $p += 4
    DllStructSetData($sc, 1, 0x6A, $p)
    $p += 1
    DllStructSetData($sc, 1, 0x31, $p)
    $p += 1
    DllStructSetData($sc, 1, 0xE8, $p)
    $p += 1
    _WriteLE32($sc, $p, $sendFunc - ($scAddr + $p - 1 + 4))
    $p += 4
    DllStructSetData($sc, 1, 0xC3, $p)

    DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
        'handle', $ph, 'ptr', Ptr($scAddr), _
        'ptr', DllStructGetPtr($sc), 'ulong_ptr', $p, 'ulong_ptr*', 0)

    ; Execute via CreateRemoteThread (not rendering hook)
    Local $th = DllCall($kernel_handle, 'handle', 'CreateRemoteThread', _
        'handle', $ph, 'ptr', 0, 'ulong_ptr', 0, _
        'ptr', Ptr($scAddr), 'ptr', 0, 'dword', 0, 'dword*', 0)
    If IsArray($th) And $th[0] <> 0 Then
        DllCall($kernel_handle, 'dword', 'WaitForSingleObject', 'handle', $th[0], 'dword', 3000)
        DllCall($kernel_handle, 'bool', 'CloseHandle', 'handle', $th[0])
    EndIf
    ConsoleWrite("Sent action " & $actionState & @CRLF)
    Sleep(100)
Next

Sleep(5000)
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\remote_native_after.png', $hWnd)
Local $status = MemoryRead($ph, GetLabel('StatusCode'), 'dword')
ConsoleWrite("StatusCode: " & $status & @CRLF)

ConsoleWrite("=== DONE ===" & @CRLF)
