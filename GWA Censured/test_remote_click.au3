#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

ConsoleWrite("=== Remote Thread Click Test ===" & @CRLF)

ScanAndUpdateGameClients()
SelectClient(1)
InitializeGameClientForGWA2(False)
Local $ph = GetProcessHandle()
Local $hWnd = $game_clients[$game_clients[0][0]][2]
ConsoleWrite("At char select: " & IsAtCharSelect() & @CRLF)

; Get Play button frame
Local $result = GetFrameByHash($FRAME_HASH_PLAY_BUTTON)
Local $fp = Int($result[0])
Local $fid = $result[1]
Local $co = MemoryRead($ph, $fp + 0xB8, 'dword')
ConsoleWrite("Play: ptr=0x" & Hex($fp) & " id=" & $fid & " child_off=" & $co & @CRLF)

; Get SendFrameUIMsg
Local $sfFunc = Int(GetLabel('SendFrameUIMsg'))
ConsoleWrite("SendFrameUIMsg: 0x" & Hex($sfFunc) & @CRLF)

; Allocate memory for shellcode + data
Local $mem = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
    'handle', $ph, 'ptr', 0, 'ulong_ptr', 128, _
    'dword', 0x1000, 'dword', 0x40)
Local $codeAddr = Int($mem[0])
Local $dataAddr = $codeAddr + 48

; Try MULTIPLE parameter combinations
Local $attempts[][3] = [ _
    [0x31, $fp + 0xA8, "ECX=frame+0xA8, msg=0x31"], _
    [0x22, $fp + 0xA8, "ECX=frame+0xA8, msg=0x22 (kMouseClick)"], _
    [0x2F, $fp + 0xA8, "ECX=frame+0xA8, msg=0x2F (kMouseAction)"], _
    [0x31, $fp + 0x84, "ECX=frame+0x84, msg=0x31"], _
    [0x31, $fp,        "ECX=frame, msg=0x31"] _
]

For $a = 0 To UBound($attempts) - 1
    Local $msgId = $attempts[$a][0]
    Local $thisPtr = $attempts[$a][1]
    Local $desc = $attempts[$a][2]

    ConsoleWrite(@CRLF & "=== Attempt " & $a & ": " & $desc & " ===" & @CRLF)

    ; Write action data: {frame_id, child_offset_id, 0x6 (MouseDown), 0, 0}
    Local $ad = DllStructCreate('dword;dword;dword;dword;dword')
    DllStructSetData($ad, 1, $fid)
    DllStructSetData($ad, 2, $co)
    DllStructSetData($ad, 3, 0x6)  ; MouseDown
    DllStructSetData($ad, 4, 0)
    DllStructSetData($ad, 5, 0)
    DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
        'handle', $ph, 'ptr', Ptr($dataAddr), _
        'ptr', DllStructGetPtr($ad), 'ulong_ptr', 20, 'ulong_ptr*', 0)

    ; Build shellcode: mov ecx,thisPtr; push 0; push dataAddr; push msgId; call sfFunc; ret
    Local $sc = DllStructCreate('byte[32]')
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
    DllStructSetData($sc, 1, $msgId, $p)
    $p += 1
    DllStructSetData($sc, 1, 0xE8, $p)
    $p += 1
    _WriteLE32($sc, $p, $sfFunc - ($codeAddr + $p - 1 + 4))
    $p += 4
    DllStructSetData($sc, 1, 0xC3, $p)

    DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
        'handle', $ph, 'ptr', Ptr($codeAddr), _
        'ptr', DllStructGetPtr($sc), 'ulong_ptr', $p, 'ulong_ptr*', 0)

    ; Execute via CreateRemoteThread
    Local $thread = DllCall($kernel_handle, 'handle', 'CreateRemoteThread', _
        'handle', $ph, 'ptr', 0, 'ulong_ptr', 0, _
        'ptr', Ptr($codeAddr), 'ptr', 0, 'dword', 0, 'dword*', 0)
    If IsArray($thread) And $thread[0] <> 0 Then
        DllCall($kernel_handle, 'dword', 'WaitForSingleObject', 'handle', $thread[0], 'dword', 3000)
        DllCall($kernel_handle, 'bool', 'CloseHandle', 'handle', $thread[0])
    EndIf

    Sleep(1000)

    ; Also send MouseUp
    DllStructSetData($ad, 3, 0x7)  ; MouseUp
    DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
        'handle', $ph, 'ptr', Ptr($dataAddr), _
        'ptr', DllStructGetPtr($ad), 'ulong_ptr', 20, 'ulong_ptr*', 0)
    Local $thread2 = DllCall($kernel_handle, 'handle', 'CreateRemoteThread', _
        'handle', $ph, 'ptr', 0, 'ulong_ptr', 0, _
        'ptr', Ptr($codeAddr), 'ptr', 0, 'dword', 0, 'dword*', 0)
    If IsArray($thread2) And $thread2[0] <> 0 Then
        DllCall($kernel_handle, 'dword', 'WaitForSingleObject', 'handle', $thread2[0], 'dword', 3000)
        DllCall($kernel_handle, 'bool', 'CloseHandle', 'handle', $thread2[0])
    EndIf

    Sleep(2000)
    Local $status = MemoryRead($ph, GetLabel('StatusCode'), 'dword')
    ConsoleWrite("  StatusCode: " & $status & @CRLF)

    If $status <> 0 Then
        ConsoleWrite("*** PLAY CLICKED! ***" & @CRLF)
        ExitLoop
    EndIf

    ; Check if game crashed
    Local $proc = ProcessExists($game_clients[$game_clients[0][0]][0])
    If Not $proc Then
        ConsoleWrite("  Game crashed!" & @CRLF)
        ExitLoop
    EndIf
Next

_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\remote_click_result.png', $hWnd)
ConsoleWrite("=== DONE ===" & @CRLF)
