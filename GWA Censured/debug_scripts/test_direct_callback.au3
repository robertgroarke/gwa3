#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

ConsoleWrite("=== Direct Callback Click Test ===" & @CRLF)

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

; Get Play button frame
Local $result = GetFrameByHash($FRAME_HASH_PLAY_BUTTON)
Local $fp = Int($result[0])
Local $fid = $result[1]
ConsoleWrite("Play: ptr=0x" & Hex($fp) & " id=" & $fid & @CRLF)

; Read callback[0] from frame_callbacks array
Local $cbBuf = MemoryRead($ph, $fp + 0xA8, 'dword')
Local $cbFunc = MemoryRead($ph, $cbBuf, 'dword')
ConsoleWrite("Callback[0] = 0x" & Hex($cbFunc) & @CRLF)

; Build InteractionMessage struct:
; struct InteractionMessage {
;   uint32_t frame_id;        // +0
;   UIMessage message_id;     // +4  (0x31 = kMouseClick2, or 0x22 = kMouseClick)
;   void** wParam;            // +8
; };

; Alloc memory for shellcode + data
Local $mem = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
    'handle', $ph, 'ptr', 0, 'ulong_ptr', 128, _
    'dword', 0x1000, 'dword', 0x40)
Local $scAddr = Int($mem[0])
Local $msgAddr = $scAddr + 48   ; InteractionMessage
Local $wpAddr = $scAddr + 64    ; wParam data (MouseClick struct)

; Write InteractionMessage
; Try kMouseClick (0x22) first — simpler than kMouseClick2
Local $msg = DllStructCreate('dword;dword;dword')
DllStructSetData($msg, 1, $fid)     ; frame_id
DllStructSetData($msg, 2, 0x22)     ; kMouseClick
DllStructSetData($msg, 3, $wpAddr)  ; wParam pointer
DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
    'handle', $ph, 'ptr', Ptr($msgAddr), _
    'ptr', DllStructGetPtr($msg), 'ulong_ptr', 12, 'ulong_ptr*', 0)

; Write wParam: kMouseClick = {mouse_button=0 (left), is_doubleclick=0}
Local $wp = DllStructCreate('dword;dword')
DllStructSetData($wp, 1, 0)  ; left button
DllStructSetData($wp, 2, 0)  ; not double click
DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
    'handle', $ph, 'ptr', Ptr($wpAddr), _
    'ptr', DllStructGetPtr($wp), 'ulong_ptr', 8, 'ulong_ptr*', 0)

; Shellcode: call callback(msg, wParam, lParam=0) — __cdecl
; push 0            ; lParam
; push <wpAddr>     ; wParam
; push <msgAddr>    ; InteractionMessage*
; call <cbFunc>     ; callback function
; add esp, 12       ; cdecl cleanup
; ret
Local $sc = DllStructCreate('byte[32]')
Local $p = 1

; push 0
DllStructSetData($sc, 1, 0x6A, $p)
$p += 1
DllStructSetData($sc, 1, 0x00, $p)
$p += 1

; push wpAddr
DllStructSetData($sc, 1, 0x68, $p)
$p += 1
_WriteLE32($sc, $p, $wpAddr)
$p += 4

; push msgAddr
DllStructSetData($sc, 1, 0x68, $p)
$p += 1
_WriteLE32($sc, $p, $msgAddr)
$p += 4

; call cbFunc
DllStructSetData($sc, 1, 0xE8, $p)
$p += 1
_WriteLE32($sc, $p, $cbFunc - ($scAddr + $p - 1 + 4))
$p += 4

; add esp, 12
DllStructSetData($sc, 1, 0x83, $p)
$p += 1
DllStructSetData($sc, 1, 0xC4, $p)
$p += 1
DllStructSetData($sc, 1, 0x0C, $p)
$p += 1

; ret
DllStructSetData($sc, 1, 0xC3, $p)

DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
    'handle', $ph, 'ptr', Ptr($scAddr), _
    'ptr', DllStructGetPtr($sc), 'ulong_ptr', $p, 'ulong_ptr*', 0)

ConsoleWrite("Shellcode at 0x" & Hex($scAddr) & @CRLF)

; Screenshot before
Local $hWnd = $game_clients[$game_clients[0][0]][2]
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\callback_before.png', $hWnd)

; Queue via rendering hook
$queue_counter = MemoryRead($ph, GetLabel('QueueCounter'), 'dword')
Local $cmd = DllStructCreate('dword;dword')
DllStructSetData($cmd, 1, $scAddr)
DllStructSetData($cmd, 2, 0)
Enqueue(DllStructGetPtr($cmd), DllStructGetSize($cmd))
ConsoleWrite("Command queued (counter=" & $queue_counter & ")" & @CRLF)

Sleep(5000)

_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\callback_after.png', $hWnd)
Local $status = MemoryRead($ph, GetLabel('StatusCode'), 'dword')
ConsoleWrite("StatusCode: " & $status & @CRLF)

If $status <> 0 Then
    ConsoleWrite("*** PLAY CLICKED! ***" & @CRLF)
Else
    ConsoleWrite("Still at char select. Trying kMouseAction (0x2F)..." & @CRLF)
    ; Try with kMouseAction and ActionState::MouseClick = 0x8
    DllStructSetData($msg, 2, 0x2F)  ; kMouseAction
    DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
        'handle', $ph, 'ptr', Ptr($msgAddr), _
        'ptr', DllStructGetPtr($msg), 'ulong_ptr', 12, 'ulong_ptr*', 0)
    ; wParam for kMouseAction = {frame_id, child_offset_id, action_state, wp, lp}
    Local $co = MemoryRead($ph, $fp + 0xB8, 'dword')
    Local $wp2 = DllStructCreate('dword;dword;dword;dword;dword')
    DllStructSetData($wp2, 1, $fid)
    DllStructSetData($wp2, 2, $co)
    DllStructSetData($wp2, 3, 0x8)  ; MouseClick
    DllStructSetData($wp2, 4, 0)
    DllStructSetData($wp2, 5, 0)
    DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
        'handle', $ph, 'ptr', Ptr($wpAddr), _
        'ptr', DllStructGetPtr($wp2), 'ulong_ptr', 20, 'ulong_ptr*', 0)

    $queue_counter = MemoryRead($ph, GetLabel('QueueCounter'), 'dword')
    Enqueue(DllStructGetPtr($cmd), DllStructGetSize($cmd))
    Sleep(5000)

    $status = MemoryRead($ph, GetLabel('StatusCode'), 'dword')
    ConsoleWrite("StatusCode after kMouseAction: " & $status & @CRLF)
EndIf

ConsoleWrite("=== DONE ===" & @CRLF)
