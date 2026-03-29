#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== UIMessage Sniffer Test (Polling Mode) ===" & @CRLF)

; Launch BEASTRIT
Local $accounts = GWLauncher_LoadAccounts()
Local $idx = GWLauncher_FindAccountByCharacter($accounts, "B E A S T R I T")
Local $result = GWLauncher_LaunchAccount($accounts, $idx)
If $result = 0 Then
    ConsoleWrite("ERROR: Failed to launch" & @CRLF)
    Exit 1
EndIf
ConsoleWrite("GW launched PID=" & $result[0] & @CRLF)

ConsoleWrite("Waiting 35s for char select..." & @CRLF)
Sleep(35000)

ScanAndUpdateGameClients()
ConsoleWrite("Clients: " & $game_clients[0][0] & @CRLF)
Local $targetClient = $game_clients[0][0]
For $i = 1 To $game_clients[0][0]
    ConsoleWrite("  " & $i & ": " & $game_clients[$i][3] & @CRLF)
Next

SelectClient($targetClient)
InitializeGameClientData(True, False)
ConsoleWrite("Connected to client " & $targetClient & @CRLF)

; Debug: print label addresses before enabling
ConsoleWrite("UISnifferProc label = " & GetLabel('UISnifferProc') & @CRLF)
ConsoleWrite("UISnifferOriginal label = " & GetLabel('UISnifferOriginal') & @CRLF)
ConsoleWrite("UISnifferEnabled label = " & GetLabel('UISnifferEnabled') & @CRLF)
ConsoleWrite("UISnifferCounter label = " & GetLabel('UISnifferCounter') & @CRLF)
ConsoleWrite("UISnifferMsgId label = " & GetLabel('UISnifferMsgId') & @CRLF)
ConsoleWrite("UISnifferWParam label = " & GetLabel('UISnifferWParam') & @CRLF)
ConsoleWrite("UIMessageStart label = " & GetLabel('UIMessageStart') & @CRLF)

; Enable sniffer
UISnifferEnable()
ConsoleWrite("=== SNIFFER ENABLED ===" & @CRLF)
ConsoleWrite("POLLING_ACTIVE" & @CRLF)

; Poll the shared memory buffer for captured messages
Local $lastCounter = 0
Local $processHandle = GetProcessHandle()
Local $counterAddr = GetLabel('UISnifferCounter')
Local $msgIdAddr = GetLabel('UISnifferMsgId')
Local $wParamAddr = GetLabel('UISnifferWParam')
Local $lParamAddr = GetLabel('UISnifferLParam')

For $poll = 1 To 900  ; poll for 90 seconds (100ms intervals)
    Sleep(100)

    Local $counter = MemoryRead($processHandle, $counterAddr, 'dword')
    If $counter <> $lastCounter Then
        Local $msgId = MemoryRead($processHandle, $msgIdAddr, 'dword')
        Local $wParamPtr = MemoryRead($processHandle, $wParamAddr, 'dword')
        Local $lParamVal = MemoryRead($processHandle, $lParamAddr, 'dword')

        ; Try to dereference wParam if it looks like a pointer
        Local $wp0 = 0
        Local $wp1 = 0
        If $wParamPtr > 0x10000 Then
            $wp0 = MemoryRead($processHandle, $wParamPtr, 'dword')
            $wp1 = MemoryRead($processHandle, $wParamPtr + 4, 'dword')
        EndIf

        ConsoleWrite("[CAPTURE] #" & $counter & _
            " MsgID=0x" & Hex($msgId, 8) & _
            " wParam=0x" & Hex($wParamPtr, 8) & _
            " [" & Hex($wp0, 8) & "," & Hex($wp1, 8) & "]" & _
            " lParam=" & $lParamVal & @CRLF)

        $lastCounter = $counter
    EndIf
Next

UISnifferDisable()
ConsoleWrite("=== DONE ===" & @CRLF)
