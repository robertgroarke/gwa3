#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== UISniffer In-Game Test ===" & @CRLF)

; Connect to an existing in-game client (don't launch new one)
ScanAndUpdateGameClients()
If $game_clients[0][0] = 0 Then
    ConsoleWrite("ERROR: No GW clients found" & @CRLF)
    Exit 1
EndIf

ConsoleWrite("Found " & $game_clients[0][0] & " clients:" & @CRLF)
For $i = 1 To $game_clients[0][0]
    ConsoleWrite("  " & $i & ": " & $game_clients[$i][3] & " (PID=" & $game_clients[$i][0] & ")" & @CRLF)
Next

; Use the last client (least likely to be the one with stale hooks from diag tests)
Local $targetIdx = $game_clients[0][0]
ConsoleWrite("Connecting to client " & $targetIdx & " (" & $game_clients[$targetIdx][3] & ")" & @CRLF)
SelectClient($targetIdx)
InitializeGameClientData(True, False)
ConsoleWrite("Connected!" & @CRLF)

; Check if UIMessageStart is clean
Local $processHandle = GetProcessHandle()
Local $uiMsgAddr = GetLabel('UIMessageStart')
Local $checkByte = DllStructCreate('byte[1]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $processHandle, 'ptr', $uiMsgAddr, _
    'ptr', DllStructGetPtr($checkByte), 'ulong_ptr', 1, 'ulong_ptr*', 0)

Local $firstByte = DllStructGetData($checkByte, 1, 1)
ConsoleWrite("UIMessageStart first byte: 0x" & Hex($firstByte, 2) & @CRLF)
If $firstByte = 0xE9 Then
    ConsoleWrite("UIMessageStart already hooked (stale from previous injection). Skipping." & @CRLF)
    ConsoleWrite("This client was already initialized by the Froggy bot." & @CRLF)
    ConsoleWrite("The framework reuses existing injection - UIMessage may or may not be hooked." & @CRLF)
EndIf

; Enable sniffer
ConsoleWrite(@CRLF & "=== Enabling UISniffer ===" & @CRLF)
UISnifferEnable()

; Poll for 20 seconds
ConsoleWrite(@CRLF & "=== Polling for 20 seconds ===" & @CRLF)
Local $lastCounter = 0
Local $counterAddr = GetLabel('UISnifferCounter')
Local $msgIdAddr = GetLabel('UISnifferMsgId')
Local $wParamAddr = GetLabel('UISnifferWParam')
Local $lParamAddr = GetLabel('UISnifferLParam')
Local $captureCount = 0

; Track unique message IDs
Local $uniqueMsgs[]

For $poll = 1 To 200
    Sleep(100)
    Local $counter = MemoryRead($processHandle, $counterAddr, 'dword')
    If $counter <> $lastCounter Then
        Local $msgId = MemoryRead($processHandle, $msgIdAddr, 'dword')
        Local $wParamPtr = MemoryRead($processHandle, $wParamAddr, 'dword')
        Local $lParamVal = MemoryRead($processHandle, $lParamAddr, 'dword')

        Local $msgHex = "0x" & Hex($msgId, 8)
        If Not MapExists($uniqueMsgs, $msgHex) Then
            $uniqueMsgs[$msgHex] = 0
        EndIf
        $uniqueMsgs[$msgHex] += 1

        ; Only print first 50 messages to avoid flood
        If $captureCount < 50 Then
            ConsoleWrite("[CAPTURE] #" & $counter & " MsgID=" & $msgHex & _
                " wP=0x" & Hex($wParamPtr, 8) & " lP=" & $lParamVal & @CRLF)
        EndIf

        $lastCounter = $counter
        $captureCount += 1
    EndIf
Next

ConsoleWrite(@CRLF & "=== Results ===" & @CRLF)
ConsoleWrite("Total captured: " & $captureCount & " messages" & @CRLF)
ConsoleWrite("Unique message IDs:" & @CRLF)
Local $keys = MapKeys($uniqueMsgs)
For $k In $keys
    ConsoleWrite("  " & $k & " x" & $uniqueMsgs[$k] & @CRLF)
Next

; Cleanup
UISnifferDisable()
ConsoleWrite("=== DONE ===" & @CRLF)
