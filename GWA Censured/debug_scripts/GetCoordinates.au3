#RequireAdmin
#include "lib\GWA2_Headers.au3"
#include "lib\GWA2.au3"
#include <GUIConstantsEx.au3>
#include <WindowsConstants.au3>
#include <EditConstants.au3>
#include <ComboConstants.au3>

; --- Client Selection ---

; Scan for running Guild Wars clients
ScanAndUpdateGameClients()

If Not IsArray($game_clients) Or $game_clients[0][0] = 0 Then
    MsgBox(48, "Error", "No Guild Wars clients found." & @CRLF & "Please start Guild Wars and log in to a character.")
    Exit
EndIf

Global $hSelectGUI = GUICreate("Select Character", 300, 150)
GUICtrlCreateLabel("Select Character to Monitor:", 10, 10, 280, 20)
Global $cmbChars = GUICtrlCreateCombo("", 10, 35, 280, 25, $CBS_DROPDOWNLIST)
Global $btnLaunch = GUICtrlCreateButton("Start Logger", 100, 80, 100, 30)

; Populate combo box
Local $sData = ""
For $i = 1 To $game_clients[0][0]
    $sData &= $game_clients[$i][3] & "|"
Next
GUICtrlSetData($cmbChars, StringTrimRight($sData, 1), $game_clients[1][3])

GUISetState(@SW_SHOW, $hSelectGUI)

Global $bSelected = False

While True
    Local $msg = GUIGetMsg()
    Switch $msg
        Case $GUI_EVENT_CLOSE
            Exit
        Case $btnLaunch
            Local $charName = GUICtrlRead($cmbChars)
            For $i = 1 To $game_clients[0][0]
                If $game_clients[$i][3] = $charName Then
                    ; Set the global index for GWA2 to use
                    $selected_client_index = $i
                    $bSelected = True
                    ExitLoop 2
                EndIf
            Next
    EndSwitch
WEnd

GUIDelete($hSelectGUI)

If Not $bSelected Then Exit

; Initialize GWA2 with the selected client
InitializeGameClientData(True, False)

; --- Main Logging GUI ---

Global $hGUI = GUICreate("Coordinate Logger - " & GetCharacterName(), 400, 400, -1, -1, -1, $WS_EX_TOPMOST)

; Current Position Display
GUICtrlCreateLabel("Current Position:", 10, 10, 380, 20)
Global $lblCurrent = GUICtrlCreateLabel("Waiting...", 10, 30, 380, 30)
GUICtrlSetFont(-1, 14, 800)

; Controls
Global $btnLog = GUICtrlCreateButton("Log Current Position", 10, 70, 140, 30)
Global $btnCopy = GUICtrlCreateButton("Copy Log", 160, 70, 100, 30)
Global $btnClear = GUICtrlCreateButton("Clear", 270, 70, 100, 30)
Global $cbAuto = GUICtrlCreateCheckbox("Auto-Log (every 1s)", 10, 110, 150, 20)

; Scrolling Log
Global $txtLog = GUICtrlCreateEdit("", 10, 140, 380, 250, BitOR($ES_READONLY, $WS_VSCROLL, $ES_AUTOVSCROLL))

GUISetState(@SW_SHOW, $hGUI)

Global $guiTimer = TimerInit()
Global $autoLogTimer = TimerInit()

While 1
    Local $nMsg = GUIGetMsg()
    Switch $nMsg
        Case $GUI_EVENT_CLOSE
            Exit
        Case $btnLog
            LogCurrentPos()
        Case $btnClear
            GUICtrlSetData($txtLog, "")
        Case $btnCopy
            ClipPut(GUICtrlRead($txtLog))
            MsgBox(0, "Copied", "Log copied to clipboard!")
    EndSwitch
    
    ; Update UI every 100ms
    If TimerDiff($guiTimer) > 100 Then
        UpdateCurrentDisplay()
        $guiTimer = TimerInit()
    EndIf
    
    ; Auto Log every 1000ms
    If GUICtrlRead($cbAuto) = $GUI_CHECKED And TimerDiff($autoLogTimer) > 1000 Then
        LogCurrentPos()
        $autoLogTimer = TimerInit()
    EndIf
WEnd

Func UpdateCurrentDisplay()
    Local $coords = GetCoordsString()
    If $coords <> "" Then
        GUICtrlSetData($lblCurrent, $coords)
    Else
        GUICtrlSetData($lblCurrent, "Character not found")
    EndIf
EndFunc

Func LogCurrentPos()
    Local $coords = GetCoordsString()
    If $coords <> "" Then
        GUICtrlSetData($txtLog, $coords & @CRLF, 1) ; Append
    EndIf
EndFunc

Func GetCoordsString()
    Local $myID = GetMyID()
    If $myID = 0 Then Return ""
    
    Local $myAgent = GetAgentByID($myID)
    If Not IsDllStruct($myAgent) Then Return ""
    
    Local $x = Round(DllStructGetData($myAgent, 'X'), 2)
    Local $y = Round(DllStructGetData($myAgent, 'Y'), 2)
    
    Return "X: " & $x & ", Y: " & $y
EndFunc
