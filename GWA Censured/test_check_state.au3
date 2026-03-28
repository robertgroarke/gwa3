#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>
ScanAndUpdateGameClients()
If $game_clients[0][0] = 0 Then
	ConsoleWrite("No GW" & @CRLF)
	Exit 1
EndIf
SelectClient(1)
InitializeGameClientForGWA2(False)
Local $ph = GetProcessHandle()
ConsoleWrite("IsAtCharSelect: " & IsAtCharSelect() & @CRLF)
ConsoleWrite("StatusCode: " & MemoryRead($ph, GetLabel('StatusCode'), 'dword') & @CRLF)
ConsoleWrite("PreGame: " & MemoryRead($ph, GetLabel('PreGame'), 'dword') & @CRLF)
ConsoleWrite("Environment: " & MemoryRead($ph, GetLabel('Environment'), 'dword') & @CRLF)
ConsoleWrite("Region: " & MemoryRead($ph, GetLabel('Region'), 'dword') & @CRLF)
ConsoleWrite("MyID: " & MemoryRead($ph, GetLabel('MyID'), 'dword') & @CRLF)
; Check Play button visibility
ConsoleWrite("PlayVisible: " & IsFrameVisible($FRAME_HASH_PLAY_BUTTON) & @CRLF)
ConsoleWrite("PlayGreyed: " & IsFrameVisible($FRAME_HASH_PLAY_GREYED) & @CRLF)
_ScreenCapture_CaptureWnd(@ScriptDir & "\tests\post_click_state.png", $game_clients[1][2])
ConsoleWrite("Screenshot saved" & @CRLF)
