#requireadmin
#include "lib\GWA2_Headers.au3"
#include "lib\GWA2.au3"
#include "lib\Utils.au3"
#include "lib\Utils-Debugger.au3"
#include "lib\GUI_Functions.au3"
#include <GUIConstantsEx.au3>

; ==========================================
; Instance Info Probe Script
; Purpose: Probe different offsets from $instance_info_ptr
; to find where instance_type is stored
; ==========================================

Global Const $BOTNAME = "Instance Probe"
Global Const $VERSION = "1.0"
Global Const $AUTHORS[1] = ["Test"]

Global $BotRunning = False
Global $mBasePointer

; Scan for game clients
ScanAndUpdateGameClients()

; Create character selection GUI
Global $Form1 = GUICreate("Instance Probe", 250, 150)

If Not IsArray($game_clients) Or $game_clients[0][0] = 0 Then
	MsgBox(48, "Error", "No Guild Wars clients found." & @CRLF & "Please start Guild Wars and log in to a character.")
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

; Setup GUI callbacks
GUI_SetOnStartFunc("onStart")
GUI_SetOnStopFunc("onStop")
GUI_SetOnResumeFunc("onResume")
GUI_Create()

; Main loop
While 1
	Sleep(200)
	While $BotRunning
		ProbeInstanceInfo()
		Sleep(100) ; Check every 100ms to catch loading screen
	WEnd
WEnd

Func onStart()
	$BotRunning = True
	Out("=== Instance Info Probe Started ===")
	Out("This script will probe memory around $instance_info_ptr")
	Out("Look for values that change between:")
	Out("  - When in outpost (loaded)")
	Out("  - When zoning (loading)")
	Out("================================")
	Out("")
	Out("$instance_info_ptr = 0x" & Hex($instance_info_ptr, 8))
	Out("")
EndFunc

Func onStop()
	$BotRunning = False
	Out("Instance Probe Stopped")
EndFunc

Func onResume()
	$BotRunning = True
	Out("Instance Probe Resumed")
EndFunc

Func ProbeInstanceInfo()
	Local $timestamp = @HOUR & ":" & @MIN & ":" & @SEC
	
	; Get agent exists as reference (known working indicator)
	Local $agentExists = GetAgentExists(GetMyID())
	Local $mapID = GetMapID()
	
	Out("--- [" & $timestamp & "] MapID=" & $mapID & " AgentExists=" & $agentExists & " ---")
	
	; Read from the instance_info_ptr directly
	Out("$instance_info_ptr = 0x" & Hex($instance_info_ptr, 8))
	
	; Try reading the pointer value first
	Local $ptrValue = MemoryRead($instance_info_ptr, 'ptr')
	Out("  [ptr] @ instance_info_ptr = 0x" & Hex($ptrValue, 8))
	
	; If we got a valid pointer, try reading from it
	If $ptrValue <> 0 Then
		Out("  Reading from dereferenced pointer:")
		For $offset = 0 To 32 Step 4
			Local $val = MemoryRead($ptrValue + $offset, 'dword')
			Out("    [+" & StringFormat("%02d", $offset) & "] = " & $val & " (0x" & Hex($val, 8) & ")")
		Next
	EndIf
	
	; Also try reading directly from instance_info_ptr with offsets
	Out("  Reading directly from $instance_info_ptr:")
	For $offset = 0 To 32 Step 4
		Local $val = MemoryRead($instance_info_ptr + $offset, 'dword')
		Out("    [+" & StringFormat("%02d", $offset) & "] = " & $val & " (0x" & Hex($val, 8) & ")")
	Next
	
	; Also test the old map_loading variable
	Out("  Old $map_loading ptr = 0x" & Hex($map_loading, 8))
	Local $oldMapLoadingVal = MemoryRead($map_loading)
	Out("    Value = " & $oldMapLoadingVal)
	
	; Try reading from base_address_ptr with common offsets that might store instance type
	Out("  Probing from $base_address_ptr:")
	Local $offsets[5] = [0x18, 0x1C, 0x20, 0x24, 0x28]
	For $i = 0 To UBound($offsets) - 1
		Local $tempOffset[3] = [0, $offsets[$i], 0]
		Local $result = MemoryReadPtr($base_address_ptr, $tempOffset)
		If $result[0] <> 0 Then
			Out("    [0x" & Hex($offsets[$i], 2) & ", 0x00] = " & $result[1] & " (0x" & Hex($result[1], 8) & ")")
		EndIf
	Next
	
	Out("")
EndFunc
