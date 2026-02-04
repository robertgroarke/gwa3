#requireadmin
#include "lib\GWA2_Headers.au3"
#include "lib\GWA2.au3"
#include "lib\Utils.au3"
#include "lib\Utils-Debugger.au3"
#include "lib\GUI_Functions.au3"
#include <GUIConstantsEx.au3>

; ==========================================
; MapLoading Test v2
; Tests the updated GetMapLoading() function
; ==========================================

Global Const $BOTNAME = "MapLoading Test v2"
Global Const $VERSION = "2.0"
Global Const $AUTHORS[1] = ["Test"]

Global $BotRunning = False
Global $mBasePointer

ScanAndUpdateGameClients()

Global $Form1 = GUICreate("MapLoading Test v2", 250, 150)

If Not IsArray($game_clients) Or $game_clients[0][0] = 0 Then
	MsgBox(48, "Error", "No Guild Wars clients found.")
	Exit
EndIf

Global $Combo_Label = GUICtrlCreateLabel("Select Character:", 20, 20, 100, 20)
Global $Char_Combo = GUICtrlCreateCombo("", 20, 40, 210, 25)
Global $Launch_Button = GUICtrlCreateButton("Launch", 85, 90, 80, 30)

Local $comboList = ""
For $i = 1 To $game_clients[0][0]
	$comboList &= $game_clients[$i][3] & "|"
Next
$comboList = StringTrimRight($comboList, 1)
GUICtrlSetData($Char_Combo, $comboList, $game_clients[1][3])

GUICtrlSetOnEvent($Launch_Button, "LaunchEvent")
GUISetOnEvent($GUI_EVENT_CLOSE, "CloseEvent")
GUISetState(@SW_SHOW)

Global $g_BotHasLaunched = False
While Not $g_BotHasLaunched
	Sleep(100)
WEnd

Func CloseEvent()
	Exit
EndFunc

Func LaunchEvent()
	GUICtrlSetData($Combo_Label, "Launching...")
	Global $Character_Select = GUICtrlRead($Char_Combo)
	Local $clientIndex = FindClientIndexByCharacterName($Character_Select)

	If $clientIndex > 0 Then
		SelectClient($clientIndex)
		InitializeGameClientData(True, False)
		$mBasePointer = MemoryRead(GetScannedAddress('ScanBasePointer', 8))
		WinSetTitle(GetWindowHandle(), '', 'Guild Wars - ' & GetCharacterName())
		GUIDelete($Form1)
		$g_BotHasLaunched = True
	Else
		MsgBox(48, "Error", "Could not find a GW client with character: '" & $Character_Select & "'")
		GUICtrlSetData($Combo_Label, "Select Character:")
	EndIf
EndFunc

GUI_SetOnStartFunc("onStart")
GUI_SetOnStopFunc("onStop")
GUI_SetOnResumeFunc("onResume")
GUI_Create()

While 1
	Sleep(200)
	While $BotRunning
		TestMapLoading()
		Sleep(100) ; Poll every 100ms to catch loading screen
	WEnd
WEnd

Func onStart()
	$BotRunning = True
	Out("=== MapLoading Test v2 Started ===")
	Out("Now using $instance_info_ptr approach")
	Out("Expected: 0=Outpost, 1=Explorable, ?=Loading")
	Out("================================")
EndFunc

Func onStop()
	$BotRunning = False
	Out("Test Stopped")
EndFunc

Func onResume()
	$BotRunning = True
EndFunc

Func TestMapLoading()
	Local $timestamp = @HOUR & ":" & @MIN & ":" & @SEC
	Local $mapLoadingVal = GetMapLoading()
	Local $mapID = GetMapID()
	Local $agentExists = GetAgentExists(GetMyID())
	
	Local $status = ""
	Switch $mapLoadingVal
		Case 0
			$status = "OUTPOST"
		Case 1
			$status = "EXPLORABLE"
		Case 2
			$status = "LOADING?"
		Case Else
			$status = "UNKNOWN(" & $mapLoadingVal & ")"
	EndSwitch
	
	Out("[" & $timestamp & "] MapLoading=" & $mapLoadingVal & " (" & $status & ") | MapID=" & $mapID & " | AgentExists=" & $agentExists)
EndFunc
