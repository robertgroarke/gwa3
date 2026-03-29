#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

ConsoleWrite("=== Character Select Debug ===" & @CRLF)

; Launch DISCO PANIC
Local $accounts = GWLauncher_LoadAccounts()
Local $idx = GWLauncher_FindAccountByCharacter($accounts, "D I S C O P A N I C")
Local $result = GWLauncher_LaunchAccount($accounts, $idx)
Local $pid = $result[0]
ConsoleWrite("Launched PID=" & $pid & @CRLF)

; Wait for char select
ConsoleWrite("Waiting 20s..." & @CRLF)
Sleep(20000)

; Connect to the process
ScanAndUpdateGameClients()
Local $targetIdx = -1
For $i = 1 To $game_clients[0][0]
    ConsoleWrite("  " & $i & ": '" & $game_clients[$i][3] & "' PID=" & $game_clients[$i][0] & @CRLF)
    If $game_clients[$i][0] = $pid Then $targetIdx = $i
Next

If $targetIdx = -1 Then
    ConsoleWrite("PID not found, using last client" & @CRLF)
    $targetIdx = $game_clients[0][0]
EndIf

SelectClient($targetIdx)
InitializeGameClientForGWA2(False)
ConsoleWrite("Connected. pre_game_address = 0x" & Hex($pre_game_address) & @CRLF)

; Read PreGameContext
Local $processHandle = GetProcessHandle()
Local $preGamePtr = MemoryRead($processHandle, $pre_game_address, 'dword')
ConsoleWrite("PreGameContext ptr = 0x" & Hex($preGamePtr) & @CRLF)

If $preGamePtr = 0 Then
    ; Wait and retry
    ConsoleWrite("PreGameContext NULL, waiting 10s and retrying..." & @CRLF)
    Sleep(10000)
    $preGamePtr = MemoryRead($processHandle, $pre_game_address, 'dword')
    ConsoleWrite("PreGameContext ptr (retry) = 0x" & Hex($preGamePtr) & @CRLF)
EndIf

If $preGamePtr <> 0 Then
    ; Read chosen_character_index
    Local $chosenIdx = MemoryRead($processHandle, $preGamePtr + 0x124, 'dword')
    ConsoleWrite("chosen_character_index = " & $chosenIdx & @CRLF)

    ; Read chars array
    Local $charsPtr = MemoryRead($processHandle, $preGamePtr + 0x148, 'dword')
    Local $charsCount = MemoryRead($processHandle, $preGamePtr + 0x14C, 'dword')
    ConsoleWrite("chars: count=" & $charsCount & " ptr=0x" & Hex($charsPtr) & @CRLF)

    If $charsCount > 0 And $charsCount <= 28 And $charsPtr > 0x10000 Then
        Local $charSize = 44
        For $c = 0 To $charsCount - 1
            Local $nameAddr = $charsPtr + ($c * $charSize) + 4
            Local $charName = MemoryRead($processHandle, $nameAddr, 'wchar[20]')
            $charName = StringStripWS($charName, 3)
            Local $marker = ""
            If $c = $chosenIdx Then $marker = " <-- SELECTED"
            ConsoleWrite("  [" & $c & "] '" & $charName & "'" & $marker & @CRLF)
        Next

        ; Find and select DISCO PANIC
        For $c = 0 To $charsCount - 1
            Local $nameAddr2 = $charsPtr + ($c * $charSize) + 4
            Local $cn = MemoryRead($processHandle, $nameAddr2, 'wchar[20]')
            $cn = StringStripWS($cn, 3)
            If $cn = "D I S C O P A N I C" Then
                ConsoleWrite("Setting chosen_character_index to " & $c & @CRLF)
                MemoryWrite($processHandle, $preGamePtr + 0x124, $c, 'dword')
                Sleep(1000)
                ; Verify
                Local $verify = MemoryRead($processHandle, $preGamePtr + 0x124, 'dword')
                ConsoleWrite("Verified: chosen_character_index = " & $verify & @CRLF)
                ExitLoop
            EndIf
        Next
    EndIf
EndIf

; Take screenshot
Local $hWnd = $game_clients[$targetIdx][2]
If $hWnd Then
    _ScreenCapture_CaptureWnd(@ScriptDir & '\tests\disco_charselect.png', $hWnd)
    ConsoleWrite("Screenshot saved" & @CRLF)
EndIf

; Press Play
ConsoleWrite("Pressing Play via Send..." & @CRLF)
If $hWnd Then
    WinActivate($hWnd)
    Sleep(500)
    Send('{ENTER}')
EndIf

Sleep(5000)
ConsoleWrite("Done" & @CRLF)
