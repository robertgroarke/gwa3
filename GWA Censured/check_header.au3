#RequireAdmin
#include "lib\GWA2_Headers.au3"
#include "lib\GWA2.au3"
#include "lib\Utils.au3"
#include <GUIConstantsEx.au3>
#include "lib\GUI_Functions.au3"

Global Const $BOTNAME = "Header Checker"
Global Const $VERSION = "0.1"
Global Const $AUTHORS[1] = ["Antigravity"]

Global $BotRunning = False

Main()

Func Main()
    ScanAndUpdateGameClients()
    Local $targetChar = "L I L B I S C U I T"
    Local $clientIndex = FindClientIndexByCharacterName($targetChar)
    
    If $clientIndex > 0 Then
        SelectClient($clientIndex)
        InitializeGameClientData(True, False)
        WinSetTitle(GetWindowHandle(), '', 'Guild Wars - ' & GetCharacterName())
    Else
        MsgBox(48, "Error", "Character '" & $targetChar & "' not found!")
        Exit
    EndIf
    
    GUI_Create()
    GUI_SetOnStartFunc("StartCheck")
    
    Out("=== Header Checker ===")
    Out("Click Start to check PacketSendFunction bytes.")
    
    While 1
        If $BotRunning Then
            CheckHeader()
            $BotRunning = False
        Else
            Sleep(100)
        EndIf
    WEnd
EndFunc

Func StartCheck()
    $BotRunning = True
EndFunc

Func CheckHeader()
    Local $addr = GetValue('PacketSendFunction')
    Out("PacketSendFunction Address: " & Hex($addr))
    
    If $addr == 0 Then
        Out("Error: Address is 0!")
        Return
    EndIf
    
    Local $bytes = MemoryRead($addr, 'byte[10]')
    Out("First 10 Bytes: " & String($bytes))
    
    ; Compare with expected: 0x558BEC83EC50...
    If StringLeft(String($bytes), 14) == "0x558BEC83EC50" Then
        Out("MATCHES STANDARD PROLOGUE (Hook Logic OK)")
    Else
        Out("MISMATCH! Hook logic might be wrong.")
    EndIf
EndFunc
