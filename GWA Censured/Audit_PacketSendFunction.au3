#RequireAdmin
#include "lib\GWA2_Headers.au3"
#include "lib\GWA2.au3"
#include "lib\GWA2_ID.au3"
#include "lib\Utils.au3"
#include <GUIConstantsEx.au3>
#include "lib\GUI_Functions.au3"

Global Const $BOTNAME = "Transaction Auditor"
Global Const $VERSION = "0.2"
Global Const $AUTHORS[1] = ["Antigravity"]

Global $BotRunning = False

Main()

Func Main()
    ; Check if currently in-game and auto-launch
    ScanAndUpdateGameClients()
    Local $targetChar = "L I L B I S C U I T"
    Local $clientIndex = FindClientIndexByCharacterName($targetChar)
    
    If $clientIndex > 0 Then
        SelectClient($clientIndex)
        InitializeGameClientData(True, False)
        WinSetTitle(GetWindowHandle(), '', 'Guild Wars - ' & GetCharacterName())
    Else
        MsgBox(48, "Error", "Character '" & $targetChar & "' not found! Please log in.")
        Exit
    EndIf
    
    ; Create the GUI for logging
    GUI_Create()
    GUI_SetOnStartFunc("StartBot")
    GUI_SetOnStopFunc("StopBot")
    
    Out("=== Transaction Auditor ===")
    Out("Character: " & $targetChar)
    Out("Ready. Press Start to audit TransactionFunction.")
    Out("")
    
    ; Main Event Loop
    While 1
        If $BotRunning Then
            AuditFunction()
            $BotRunning = False ; Run once then stop
            GUI_HideButton($GUI_idButtonStart, False)
            GUI_HideButton($GUI_idButtonStop, True)
        Else
            Sleep(100)
        EndIf
    WEnd
EndFunc

Func StartBot()
    $BotRunning = True
    Out("Running audit...")
EndFunc

Func StopBot()
    $BotRunning = False
    Out("Stopped.")
EndFunc

Func AuditFunction()
    Local $addr = GetValue('PacketSendFunction')
    Out("PacketSendFunction Address: " & Hex($addr))
    
    If $addr == 0 Then
        Out("Error: PacketSendFunction address not found.")
    Else
        ; MemoryRead with 'byte[16]' returns the binary data directly
        Local $bytes = MemoryRead($addr, 'byte[16]')
        Out("First 16 bytes of PacketSendFunction:")
        Out(String($bytes)) ; Implicitly converts 0x... to string
    EndIf
EndFunc
