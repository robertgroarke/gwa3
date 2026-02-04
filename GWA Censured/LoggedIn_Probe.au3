#requireadmin
#include "lib\GWA2_Headers.au3"
#include "lib\GWA2.au3"
#include "lib\Utils.au3"
#include "lib\Utils-Debugger.au3"
#include "lib\GUI_Functions.au3"
#include <GUIConstantsEx.au3>

; ==========================================
; LoggedIn Probe Script
; Purpose: Find where the logged in state is stored
; Run this at character select AND when logged in
; ==========================================

Global Const $BOTNAME = "LoggedIn Probe"
Global Const $VERSION = "1.0"
Global Const $AUTHORS[1] = ["Test"]

Global $BotRunning = False
Global $mBasePointer

ScanAndUpdateGameClients()

Global $Form1 = GUICreate("LoggedIn Probe", 250, 150)

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
		ProbeLoggedIn()
		Sleep(500)
	WEnd
WEnd

Func onStart()
	$BotRunning = True
	Out("=== LoggedIn Probe Started ===")
	Out("Compare values when:")
	Out("  1. At character select screen")
	Out("  2. Logged into game")
	Out("================================")
EndFunc

Func onStop()
	$BotRunning = False
	Out("Probe Stopped")
EndFunc

Func onResume()
	$BotRunning = True
EndFunc

Func ProbeLoggedIn()
	Local $timestamp = @HOUR & ":" & @MIN & ":" & @SEC
	
	; Reference values we already know work
	Local $instanceType = GetMapLoading()
	Local $agentExists = GetAgentExists(GetMyID())
	Local $myID = GetMyID()
	
	Out("--- [" & $timestamp & "] ---")
	Out("Known working: InstanceType=" & $instanceType & " | AgentExists=" & $agentExists & " | MyID=" & $myID)
	
	; Read the old $is_logged_in value (may be broken)
	Local $oldLoggedInVal = GetLoggedIn()
	Out("Old GetLoggedIn() = " & $oldLoggedInVal)
	
	; Probe $instance_info_ptr values
	Out("Probing $instance_info_ptr (0x" & Hex($instance_info_ptr, 8) & "):")
	For $offset = 0 To 0x30 Step 4
		Local $val = MemoryRead($instance_info_ptr + $offset)
		If $val <> 0 Then ; Only show non-zero values to reduce noise
			Out("  [+" & Hex($offset, 2) & "] = " & $val & " (0x" & Hex($val, 8) & ")")
		EndIf
	Next
	
	; Probe $base_address_ptr with common offsets
	Out("Probing from $base_address_ptr (0x" & Hex($base_address_ptr, 8) & "):")
	
	; Try various pointer chains that might hold login state
	Local $testOffsets[10][2] = [ _
		[0x18, 0x00], _
		[0x1C, 0x00], _
		[0x20, 0x00], _
		[0x24, 0x00], _
		[0x28, 0x00], _
		[0x30, 0x00], _
		[0x34, 0x00], _
		[0x38, 0x00], _
		[0x3C, 0x00], _
		[0x40, 0x00]]
	
	For $i = 0 To 9
		Local $offset[3] = [0, $testOffsets[$i][0], 0]
		Local $result = MemoryReadPtr($base_address_ptr, $offset)
		If $result[0] <> 0 And $result[1] <> 0 Then
			Out("  [0x" & Hex($testOffsets[$i][0], 2) & ", 0x00] = " & $result[1] & " (0x" & Hex($result[1], 8) & ")")
		EndIf
	Next
	
	; Check if character name is readable (another indicator of logged in)
	Local $charName = GetCharacterName()
	Out("CharacterName = '" & $charName & "' (length=" & StringLen($charName) & ")")
	
	Out("")
EndFunc
