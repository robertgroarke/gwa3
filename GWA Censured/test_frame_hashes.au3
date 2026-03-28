#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== Frame Hash Scanner ===" & @CRLF)

; Launch DISCO PANIC if no GW running
ScanAndUpdateGameClients()
If $game_clients[0][0] = 0 Then
    ConsoleWrite("No GW clients. Launching DISCO PANIC..." & @CRLF)
    Local $accounts = GWLauncher_LoadAccounts()
    Local $idx = GWLauncher_FindAccountByCharacter($accounts, "D I S C O P A N I C")
    Local $result = GWLauncher_LaunchAccount($accounts, $idx)
    ConsoleWrite("Launched PID=" & $result[0] & ". Waiting 20s..." & @CRLF)
    Sleep(20000)
    ScanAndUpdateGameClients()
EndIf

If $game_clients[0][0] = 0 Then
    ConsoleWrite("ERROR: No clients found" & @CRLF)
    Exit 1
EndIf

; Find the launched client by PID or use last
Local $targetIdx = $game_clients[0][0]
ConsoleWrite("Connecting to client " & $targetIdx & " (" & $game_clients[$targetIdx][3] & ")" & @CRLF)
SelectClient($targetIdx)
InitializeGameClientForGWA2(False)
ConsoleWrite("Connected!" & @CRLF)

Local $processHandle = GetProcessHandle()
Local $frameArrayAddr = GetLabel('FrameArray')

; Read frame array
Local $bufferPtr = MemoryRead($processHandle, $frameArrayAddr, 'dword')
Local $arraySize = MemoryRead($processHandle, $frameArrayAddr + 4, 'dword')
ConsoleWrite("Frame array: " & $arraySize & " frames at 0x" & Hex($bufferPtr) & @CRLF)

If $arraySize > 5000 Or $arraySize <= 0 Or $bufferPtr < 0x10000 Then
    ConsoleWrite("Invalid frame array. Exiting." & @CRLF)
    Exit 1
EndIf

; Scan ALL frames for non-zero hashes
ConsoleWrite(@CRLF & "=== Visible frames with hashes ===" & @CRLF)
Local $hashCount = 0

For $i = 0 To $arraySize - 1
    Local $framePtr = MemoryRead($processHandle, $bufferPtr + ($i * 4), 'dword')
    If $framePtr = 0 Or $framePtr < 0x10000 Then ContinueLoop

    Local $frameHash = MemoryRead($processHandle, $framePtr + 0x134, 'dword')
    If $frameHash = 0 Then ContinueLoop

    Local $frameId = MemoryRead($processHandle, $framePtr + 0xBC, 'dword')
    Local $frameState = MemoryRead($processHandle, $framePtr + 0x18C, 'dword')
    Local $isCreated = BitAND($frameState, 0x4) <> 0
    Local $isHidden = BitAND($frameState, 0x200) <> 0

    $hashCount += 1

    If $isCreated And Not $isHidden Then
        ConsoleWrite("  [" & $i & "] hash=" & $frameHash & " id=" & $frameId & _
            " state=0x" & Hex($frameState) & @CRLF)
    EndIf
Next

ConsoleWrite(@CRLF & "Total hashed frames: " & $hashCount & "/" & $arraySize & @CRLF)
ConsoleWrite("=== DONE ===" & @CRLF)
