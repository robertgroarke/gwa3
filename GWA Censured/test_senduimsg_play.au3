#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

ConsoleWrite("=== Test SendUIMessage for Play ===" & @CRLF)

ScanAndUpdateGameClients()
If $game_clients[0][0] = 0 Then Exit 1

SelectClient($game_clients[0][0])
InitializeGameClientForGWA2(False)
ConsoleWrite("Connected" & @CRLF)

Local $processHandle = GetProcessHandle()
Local $hWnd = $game_clients[$game_clients[0][0]][2]

; Verify we're at char select
ConsoleWrite("At char select: " & IsAtCharSelect() & @CRLF)

; Try 1: Send kMouseClick (0x22) via global SendUIMessage
; kMouseClick struct: {mouse_button=0, is_doubleclick=0}
ConsoleWrite(@CRLF & "=== Attempt 1: SendUIMessage with kMouseClick (0x22) ===" & @CRLF)
Local $s1 = DllStructCreate('dword;dword;dword;dword')
DllStructSetData($s1, 1, GetLabel('CommandUIMsg'))
DllStructSetData($s1, 2, 0x22)  ; kMouseClick
DllStructSetData($s1, 3, 0)     ; mouse_button = left
DllStructSetData($s1, 4, 0)     ; is_doubleclick = no
Enqueue(DllStructGetPtr($s1), DllStructGetSize($s1))
Sleep(3000)

_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\senduimsg_play_1.png', $hWnd)
Local $status1 = MemoryRead($processHandle, GetLabel('StatusCode'), 'dword')
ConsoleWrite("StatusCode: " & $status1 & @CRLF)

; Try 2: Send kSetPreGameContext_Value0 (0x10000183)
; This sets a value in PreGameContext — maybe we can trigger the Play action
ConsoleWrite(@CRLF & "=== Attempt 2: kSetPreGameContext_Value0 (0x10000183) ===" & @CRLF)
Local $s2 = DllStructCreate('dword;dword;dword')
DllStructSetData($s2, 1, GetLabel('CommandUIMsg'))
DllStructSetData($s2, 2, 0x10000183)  ; kSetPreGameContext_Value0
DllStructSetData($s2, 3, 1)            ; value = 1 (maybe "play" = 1?)
Enqueue(DllStructGetPtr($s2), DllStructGetSize($s2))
Sleep(3000)

$status1 = MemoryRead($processHandle, GetLabel('StatusCode'), 'dword')
ConsoleWrite("StatusCode: " & $status1 & @CRLF)

; Try 3: Send kSetPreGameContext_Value1 (0x10000186)
ConsoleWrite(@CRLF & "=== Attempt 3: kSetPreGameContext_Value1 (0x10000186) ===" & @CRLF)
Local $s3 = DllStructCreate('dword;dword;dword')
DllStructSetData($s3, 1, GetLabel('CommandUIMsg'))
DllStructSetData($s3, 2, 0x10000186)  ; kSetPreGameContext_Value1
DllStructSetData($s3, 3, 1)
Enqueue(DllStructGetPtr($s3), DllStructGetSize($s3))
Sleep(5000)

$status1 = MemoryRead($processHandle, GetLabel('StatusCode'), 'dword')
ConsoleWrite("StatusCode: " & $status1 & @CRLF)

_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\senduimsg_play_2.png', $hWnd)

ConsoleWrite("At char select still: " & IsAtCharSelect() & @CRLF)
ConsoleWrite("=== DONE ===" & @CRLF)
