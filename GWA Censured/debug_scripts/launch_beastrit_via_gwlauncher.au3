#include "..\lib\Froggy_Includes.au3"

Global Const $TARGET_CHARACTER = "B E A S T R I T"
Global Const $ACCOUNTS_PATH = "C:\Users\Robert\Documents\GWA Censured X BotsHub\GWA Censured\Accounts.json"
Global Const $LOG_PATH = "C:\Users\Robert\Documents\GWA Censured X BotsHub\GWA Censured\debug_scripts\launch_beastrit_via_gwlauncher.log"

Func _NormalizeCharacterNameForLaunch($name)
    Return StringLower(StringReplace(StringStripWS($name, 3), " ", ""))
EndFunc

Func _FindClientIndexByTargetCommandLine($expectedCharacter)
    Local $expectedNormalized = _NormalizeCharacterNameForLaunch($expectedCharacter)
    Local $objWMI = ObjGet("winmgmts:\\.\root\cimv2")
    If Not IsObj($objWMI) Then Return -1

    Local $processes = $objWMI.ExecQuery("SELECT ProcessId, CommandLine FROM Win32_Process WHERE Name='Gw.exe'")
    If Not IsObj($processes) Then Return -1

    For $proc In $processes
        Local $cmd = $proc.CommandLine
        If $cmd = "" Then ContinueLoop
        If StringInStr(_NormalizeCharacterNameForLaunch($cmd), $expectedNormalized) = 0 Then ContinueLoop

        Local $pid = Number($proc.ProcessId)
        If IsArray($game_clients) And $game_clients[0][0] > 0 Then
            For $ci = 1 To $game_clients[0][0]
                If $game_clients[$ci][0] = $pid Then Return $ci
            Next
        EndIf
    Next

    Return -1
EndFunc

Func _SelectExpectedCharacterAndPressPlay($expectedCharacter)
    Local $preGameCandidates[2] = [MemRead($pre_game_address), $pre_game_address]
    Local $preGamePtr = 0
    Local $charsPtr = 0
    Local $charsCount = 0

    For $candidateIndex = 0 To 1
        Local $candidate = $preGameCandidates[$candidateIndex]
        If $candidate = 0 Then ContinueLoop
        Local $candidateCharsPtr = MemRead($candidate + 0x148)
        Local $candidateCharsCount = MemRead($candidate + 0x14C)
        FileWrite($LOG_PATH, "PREGAME_CANDIDATE_" & $candidateIndex & "=0x" & Hex($candidate) & @CRLF)
        FileWrite($LOG_PATH, "PREGAME_CANDIDATE_" & $candidateIndex & "_CHAR_PTR=0x" & Hex($candidateCharsPtr) & @CRLF)
        FileWrite($LOG_PATH, "PREGAME_CANDIDATE_" & $candidateIndex & "_CHAR_COUNT=" & $candidateCharsCount & @CRLF)
        If $candidateCharsCount > 0 And $candidateCharsCount <= 28 And $candidateCharsPtr >= 0x10000 Then
            $preGamePtr = $candidate
            $charsPtr = $candidateCharsPtr
            $charsCount = $candidateCharsCount
            ExitLoop
        EndIf
    Next

    If $preGamePtr = 0 Then
        FileWrite($LOG_PATH, "WARN=pre_game_context_unavailable_using_command_line_selection" & @CRLF)
        If Not PressPlayButton() Then
            FileWrite($LOG_PATH, "ERROR=press_play_failed" & @CRLF)
            Return False
        EndIf
        FileWrite($LOG_PATH, "CLICK_PLAY_SENT=1" & @CRLF)
        Return True
    EndIf

    FileWrite($LOG_PATH, "PREGAME_PTR=0x" & Hex($preGamePtr) & @CRLF)
    FileWrite($LOG_PATH, "CHAR_LIST_PTR=0x" & Hex($charsPtr) & @CRLF)
    FileWrite($LOG_PATH, "CHAR_LIST_COUNT=" & $charsCount & @CRLF)

    If $charsCount <= 0 Or $charsCount > 28 Or $charsPtr < 0x10000 Then
        FileWrite($LOG_PATH, "ERROR=invalid_character_list" & @CRLF)
        Return False
    EndIf

    Local $expectedNormalized = _NormalizeCharacterNameForLaunch($expectedCharacter)
    Local $selectedIndex = -1
    Local $charSize = 44
    For $c = 0 To $charsCount - 1
        Local $nameAddr = $charsPtr + ($c * $charSize) + 4
        Local $charName = StringStripWS(MemRead($nameAddr, 'wchar[20]'), 3)
        FileWrite($LOG_PATH, "CHAR_" & $c & "=" & $charName & @CRLF)
        If _NormalizeCharacterNameForLaunch($charName) = $expectedNormalized Then
            $selectedIndex = $c
            ExitLoop
        EndIf
    Next

    If $selectedIndex < 0 Then
        FileWrite($LOG_PATH, "ERROR=target_character_not_in_pregame_list" & @CRLF)
        Return False
    EndIf

    MemWrite($preGamePtr + 0x124, $selectedIndex, 'dword')
    Sleep(500)
    Local $chosen = MemRead($preGamePtr + 0x124)
    FileWrite($LOG_PATH, "SELECTED_INDEX=" & $selectedIndex & @CRLF)
    FileWrite($LOG_PATH, "CHOSEN_INDEX_AFTER_WRITE=" & $chosen & @CRLF)
    If $chosen <> $selectedIndex Then
        FileWrite($LOG_PATH, "ERROR=chosen_index_write_failed" & @CRLF)
        Return False
    EndIf

    If Not PressPlayButton() Then
        FileWrite($LOG_PATH, "ERROR=press_play_failed" & @CRLF)
        Return False
    EndIf

    FileWrite($LOG_PATH, "CLICK_PLAY_SENT=1" & @CRLF)
    Return True
EndFunc

ConsoleWrite("=== Launch BEASTRIT via GWLauncher ===" & @CRLF)
FileDelete($LOG_PATH)
FileWrite($LOG_PATH, "=== Launch BEASTRIT via GWLauncher ===" & @CRLF)

Local $accounts = GWLauncher_LoadAccounts($ACCOUNTS_PATH)
If UBound($accounts) = 0 Then
    ConsoleWrite("ERROR: no accounts loaded from " & $ACCOUNTS_PATH & @CRLF)
    FileWrite($LOG_PATH, "ERROR: no accounts loaded from " & $ACCOUNTS_PATH & @CRLF)
    Exit 1
EndIf

Local $idx = GWLauncher_FindAccountByCharacter($accounts, $TARGET_CHARACTER)
If $idx < 0 Then
    ConsoleWrite("ERROR: character not found: " & $TARGET_CHARACTER & @CRLF)
    FileWrite($LOG_PATH, "ERROR: character not found: " & $TARGET_CHARACTER & @CRLF)
    Exit 2
EndIf

FileWrite($LOG_PATH, "ACCOUNT_INDEX=" & $idx & @CRLF)
Local $launchAccounts = $accounts
Local $launchAcct = $launchAccounts[$idx]
Local $accountPassword = ''
If IsMap($launchAcct) Then
    If MapExists($launchAcct, 'password') Then $accountPassword = $launchAcct['password']
    $launchAccounts[$idx] = $launchAcct
    FileWrite($LOG_PATH, "COMMAND_LINE_CHARACTER_SELECTION_ENABLED=1" & @CRLF)
EndIf
Local $result = GWLauncher_LaunchAccount($launchAccounts, $idx)
If $result = 0 Then
    ConsoleWrite("ERROR: GWLauncher_LaunchAccount failed" & @CRLF)
    FileWrite($LOG_PATH, "ERROR: GWLauncher_LaunchAccount failed" & @CRLF)
    Exit 3
EndIf

ConsoleWrite("GWLAUNCHER_PID=" & $result[0] & @CRLF)
FileWrite($LOG_PATH, "GWLAUNCHER_PID=" & $result[0] & @CRLF)

Local $launchedPID = $result[0]
Local $clientIdx = -1
Local $waitScan = TimerInit()
While TimerDiff($waitScan) < 30000
    Sleep(1000)
    ScanAndUpdateGameClients()
    If IsArray($game_clients) And $game_clients[0][0] > 0 Then
        For $ci = 1 To $game_clients[0][0]
            If $game_clients[$ci][0] = $launchedPID Then
                $clientIdx = $ci
                ExitLoop 2
            EndIf
        Next
        Local $targetCommandLineIdx = _FindClientIndexByTargetCommandLine($TARGET_CHARACTER)
        If $targetCommandLineIdx > 0 Then
            $clientIdx = $targetCommandLineIdx
            FileWrite($LOG_PATH, "CLIENT_INDEX_RESOLVED_BY_TARGET_COMMAND_LINE=" & $clientIdx & @CRLF)
            ExitLoop
        EndIf
    EndIf
WEnd

If $clientIdx < 0 Then
    ConsoleWrite("WARN: launched PID did not appear in client scan for char selection" & @CRLF)
    FileWrite($LOG_PATH, "WARN: launched PID did not appear in client scan for char selection" & @CRLF)
    Exit 0
EndIf

SelectClient($clientIdx)
InitializeGameClientForGWA2(False)
FileWrite($LOG_PATH, "CLIENT_INDEX=" & $clientIdx & @CRLF)

Local $waitCharSelect = TimerInit()
While Not IsAtCharSelect() And TimerDiff($waitCharSelect) < 15000
    Sleep(500)
WEnd

If Not IsAtCharSelect() And $accountPassword <> '' Then
    FileWrite($LOG_PATH, "LOGIN_FALLBACK_ATTEMPT=1" & @CRLF)
    WinActivate($game_clients[$clientIdx][2])
    Sleep(500)
    Send($accountPassword, 1)
    Sleep(250)
    Send("{ENTER}")
    Local $waitLoginFallback = TimerInit()
    While Not IsAtCharSelect() And TimerDiff($waitLoginFallback) < 45000
        Sleep(1000)
    WEnd
EndIf

If IsAtCharSelect() Then
    If IsReconnectDialogShowing() Then
        ConsoleWrite("Reconnect dialog detected; dismissing before selecting BEASTRIT" & @CRLF)
        FileWrite($LOG_PATH, "RECONNECT_DIALOG_DISMISSED=1" & @CRLF)
        DismissReconnectDialog('no')
        Sleep(2000)
    EndIf

    ConsoleWrite("Selecting character and pressing Play: " & $TARGET_CHARACTER & @CRLF)
    FileWrite($LOG_PATH, "SELECT_CHARACTER=" & $TARGET_CHARACTER & @CRLF)
    If Not _SelectExpectedCharacterAndPressPlay($TARGET_CHARACTER) Then Exit 4
Else
    ConsoleWrite("Client was not at character select before play selection" & @CRLF)
    FileWrite($LOG_PATH, "NOT_AT_CHAR_SELECT=1" & @CRLF)
    Exit 5
EndIf

Local $verifyTimer = TimerInit()
Local $lastRetryMs = 0
Local $lastScanLogMs = 0
While TimerDiff($verifyTimer) < 90000
    Sleep(2000)
    ScanAndUpdateGameClients()
    If IsArray($game_clients) And $game_clients[0][0] > 0 Then
        Local $verifyClientIdx = -1
        For $ci = 1 To $game_clients[0][0]
            If $game_clients[$ci][0] = $launchedPID Then
                $verifyClientIdx = $ci
                ExitLoop
            EndIf
        Next
        If $verifyClientIdx < 0 Then
            $verifyClientIdx = _FindClientIndexByTargetCommandLine($TARGET_CHARACTER)
            If $verifyClientIdx > 0 Then
                FileWrite($LOG_PATH, "VERIFY_CLIENT_RESOLVED_BY_TARGET_COMMAND_LINE=" & $verifyClientIdx & @CRLF)
            EndIf
        EndIf

        If $verifyClientIdx > 0 Then
            For $ci = $verifyClientIdx To $verifyClientIdx
                Local $loadedCharacter = $game_clients[$ci][3]
                If $loadedCharacter = "" And $game_clients[$ci][1] <> 0 Then
                    $loadedCharacter = StringStripWS(ScanForCharname($game_clients[$ci][1]), 3)
                    If $loadedCharacter <> "" Then
                        $game_clients[$ci][3] = $loadedCharacter
                        FileWrite($LOG_PATH, "REFRESHED_LOADED_CHARACTER=" & $loadedCharacter & @CRLF)
                    EndIf
                EndIf
                If TimerDiff($verifyTimer) - $lastScanLogMs > 10000 Then
                    FileWrite($LOG_PATH, "VERIFY_CLIENT pid=" & $game_clients[$ci][0] & " hwnd=0x" & Hex($game_clients[$ci][2]) & " char=" & $loadedCharacter & @CRLF)
                    $lastScanLogMs = TimerDiff($verifyTimer)
                EndIf
                If $loadedCharacter <> "" Then
                    FileWrite($LOG_PATH, "LOADED_CHARACTER=" & $loadedCharacter & @CRLF)
                    If _NormalizeCharacterNameForLaunch($loadedCharacter) = _NormalizeCharacterNameForLaunch($TARGET_CHARACTER) Then
                        FileWrite($LOG_PATH, "GWLAUNCHER_PID=" & $game_clients[$verifyClientIdx][0] & @CRLF)
                        FileWrite($LOG_PATH, "LAUNCH_VERIFIED=1" & @CRLF)
                        Exit 0
                    EndIf

                    FileWrite($LOG_PATH, "ERROR=loaded_wrong_character" & @CRLF)
                    ProcessClose($game_clients[$ci][0])
                    Exit 6
                EndIf

                If TimerDiff($verifyTimer) - $lastRetryMs > 10000 Then
                    SelectClient($ci)
                    If IsAtCharSelect() Then
                        FileWrite($LOG_PATH, "VERIFY_RETRY_PRESS_PLAY=1" & @CRLF)
                        If IsReconnectDialogShowing() Then
                            FileWrite($LOG_PATH, "VERIFY_RECONNECT_DIALOG_DISMISSED=1" & @CRLF)
                            DismissReconnectDialog('no')
                            Sleep(1000)
                        EndIf
                        If Not PressPlayButton() Then
                            FileWrite($LOG_PATH, "VERIFY_RETRY_PRESS_PLAY_FRAME_FAILED=1" & @CRLF)
                            PressPlayButton_MOUSE()
                        EndIf
                    EndIf
                    $lastRetryMs = TimerDiff($verifyTimer)
                EndIf
            Next
        EndIf
    EndIf
WEnd

FileWrite($LOG_PATH, "ERROR=launch_verification_timeout" & @CRLF)
Local $targetCleanupIdx = _FindClientIndexByTargetCommandLine($TARGET_CHARACTER)
If $targetCleanupIdx > 0 Then
    ProcessClose($game_clients[$targetCleanupIdx][0])
Else
    ProcessClose($launchedPID)
EndIf
Exit 7

Exit 0
