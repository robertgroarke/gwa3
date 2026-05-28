#RequireAdmin
#include "..\lib\Froggy_Includes.au3"

Global Const $TARGET_CHARACTER = "Starvin M A R V I N"
Global Const $ACCOUNTS_PATH = "C:\Users\Robert\Documents\GWA Censured X BotsHub\GWA Censured\Accounts.json"
Global Const $LOG_PATH = "C:\Users\Robert\Documents\GWA Censured X BotsHub\GWA Censured\debug_scripts\launch_marvin_via_gwlauncher.log"

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

Func _CommandLinePidMatchesTarget($pid, $expectedCharacter)
    Local $expectedNormalized = _NormalizeCharacterNameForLaunch($expectedCharacter)
    Local $objWMI = ObjGet("winmgmts:\\.\root\cimv2")
    If Not IsObj($objWMI) Then Return False

    Local $processes = $objWMI.ExecQuery("SELECT ProcessId, CommandLine FROM Win32_Process WHERE ProcessId=" & Number($pid))
    If Not IsObj($processes) Then Return False

    For $proc In $processes
        Local $cmd = $proc.CommandLine
        If $cmd = "" Then ContinueLoop
        Return StringInStr(_NormalizeCharacterNameForLaunch($cmd), $expectedNormalized) > 0
    Next

    Return False
EndFunc

Func _FindWindowForLaunchedPid($pid)
    Local $hWnd = _FindWindowByPID($pid)
    If $hWnd <> 0 Then Return $hWnd

    If IsArray($game_clients) And $game_clients[0][0] > 0 Then
        For $ci = 1 To $game_clients[0][0]
            If $game_clients[$ci][0] = $pid And $game_clients[$ci][2] > 0 Then
                FileWrite($LOG_PATH, "WINDOW_RESOLVED_FROM_CLIENT_SCAN pid=" & $pid & " hwnd=0x" & Hex($game_clients[$ci][2]) & @CRLF)
                Return $game_clients[$ci][2]
            EndIf
        Next
    EndIf

    Return 0
EndFunc

Func _EnsureGwWindowVisibleForPid($pid, $context)
    Local $hWnd = _FindWindowForLaunchedPid($pid)
    If $hWnd = 0 Then
        FileWrite($LOG_PATH, "WARN=window_not_found context=" & $context & " pid=" & $pid & @CRLF)
        Return False
    EndIf

    Local $pos = WinGetPos($hWnd)
    If IsArray($pos) Then
        FileWrite($LOG_PATH, "WINDOW_BEFORE context=" & $context & " hwnd=0x" & Hex($hWnd) & _
            " x=" & $pos[0] & " y=" & $pos[1] & " w=" & $pos[2] & " h=" & $pos[3] & @CRLF)
    EndIf

    WinSetState($hWnd, '', @SW_RESTORE)
    WinMove($hWnd, '', 40, 40, 1280, 900)
    WinActivate($hWnd)
    WinWaitActive($hWnd, '', 3)
    Sleep(500)

    $pos = WinGetPos($hWnd)
    If IsArray($pos) Then
        FileWrite($LOG_PATH, "WINDOW_AFTER context=" & $context & " hwnd=0x" & Hex($hWnd) & _
            " x=" & $pos[0] & " y=" & $pos[1] & " w=" & $pos[2] & " h=" & $pos[3] & @CRLF)
        If $pos[0] < 0 Or $pos[1] < 0 Or ($pos[0] + $pos[2]) > @DesktopWidth Or ($pos[1] + $pos[3]) > @DesktopHeight Then
            FileWrite($LOG_PATH, "ERROR=window_not_fully_clickable context=" & $context & @CRLF)
            Return False
        EndIf
    EndIf
    Return True
EndFunc

Func _PressPlayForGwLauncherSelectedCharacter($launchedPid, $expectedCharacter)
    If $launchedPid <= 0 Or Not _CommandLinePidMatchesTarget($launchedPid, $expectedCharacter) Then
        FileWrite($LOG_PATH, "ERROR=command_line_target_not_confirmed_for_autoselect_play" & @CRLF)
        Return False
    EndIf

    FileWrite($LOG_PATH, "GWLAUNCHER_AUTOSELECT_CONFIRMED=1" & @CRLF)
    If Not _EnsureGwWindowVisibleForPid($launchedPid, "press-play") Then Return False
    Sleep(5000)
    If PressPlayButton() Then
        FileWrite($LOG_PATH, "CLICK_PLAY_SENT=1" & @CRLF)
        Return True
    EndIf

    FileWrite($LOG_PATH, "WARN=press_play_frame_failed_using_mouse" & @CRLF)
    If Not _EnsureGwWindowVisibleForPid($launchedPid, "press-play-mouse-fallback") Then Return False
    If PressPlayButton_MOUSE() Then
        FileWrite($LOG_PATH, "CLICK_PLAY_MOUSE_SENT=1" & @CRLF)
        Return True
    EndIf

    FileWrite($LOG_PATH, "ERROR=press_play_failed" & @CRLF)
    Return False
EndFunc

Func _SelectExpectedCharacterAndPressPlay($expectedCharacter, $launchedPid = 0)
    Local $preGamePtr = 0
    Local $charsPtr = 0
    Local $charsCount = 0

    Local $pregameWait = TimerInit()
    Local $attempt = 0
    While TimerDiff($pregameWait) < 20000
        Local $preGameCandidates[2] = [MemRead($pre_game_address), $pre_game_address]
        For $candidateIndex = 0 To 1
            Local $candidate = $preGameCandidates[$candidateIndex]
            If $candidate = 0 Then ContinueLoop
            Local $candidateCharsPtr = MemRead($candidate + 0x148)
            Local $candidateCharsCount = MemRead($candidate + 0x14C)
            FileWrite($LOG_PATH, "PREGAME_ATTEMPT_" & $attempt & "_CANDIDATE_" & $candidateIndex & "=0x" & Hex($candidate) & @CRLF)
            FileWrite($LOG_PATH, "PREGAME_ATTEMPT_" & $attempt & "_CANDIDATE_" & $candidateIndex & "_CHAR_PTR=0x" & Hex($candidateCharsPtr) & @CRLF)
            FileWrite($LOG_PATH, "PREGAME_ATTEMPT_" & $attempt & "_CANDIDATE_" & $candidateIndex & "_CHAR_COUNT=" & $candidateCharsCount & @CRLF)
            If $candidateCharsCount > 0 And $candidateCharsCount <= 28 And $candidateCharsPtr >= 0x10000 Then
                $preGamePtr = $candidate
                $charsPtr = $candidateCharsPtr
                $charsCount = $candidateCharsCount
                ExitLoop 2
            EndIf
        Next
        $attempt += 1
        Sleep(1000)
    WEnd

    If $preGamePtr = 0 Then
        FileWrite($LOG_PATH, "WARN=pre_game_context_unavailable_using_gwlauncher_autoselect" & @CRLF)
        Return _PressPlayForGwLauncherSelectedCharacter($launchedPid, $expectedCharacter)
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

    Return _PressPlayForGwLauncherSelectedCharacter($launchedPid, $expectedCharacter)
EndFunc

ConsoleWrite("=== Launch MARVIN via GWLauncher ===" & @CRLF)
FileDelete($LOG_PATH)
FileWrite($LOG_PATH, "=== Launch MARVIN via GWLauncher ===" & @CRLF)

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
Local $result = GWLauncher_LaunchAccount($accounts, $idx)
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
    ConsoleWrite("ERROR: launched PID did not appear in client scan for char selection" & @CRLF)
    FileWrite($LOG_PATH, "ERROR=launched_pid_not_in_client_scan" & @CRLF)
    Exit 8
EndIf

SelectClient($clientIdx)
InitializeGameClientForGWA2(False)
FileWrite($LOG_PATH, "CLIENT_INDEX=" & $clientIdx & @CRLF)
_EnsureGwWindowVisibleForPid($launchedPID, "char-select")

Local $waitCharSelect = TimerInit()
While Not IsAtCharSelect() And TimerDiff($waitCharSelect) < 30000
    Sleep(500)
WEnd

If IsAtCharSelect() Then
    If IsReconnectDialogShowing() Then
        ConsoleWrite("Reconnect dialog detected; dismissing before selecting MARVIN" & @CRLF)
        FileWrite($LOG_PATH, "RECONNECT_DIALOG_DISMISSED=1" & @CRLF)
        _EnsureGwWindowVisibleForPid($launchedPID, "reconnect-dialog")
        DismissReconnectDialog('no')
        Sleep(2000)
    EndIf

    ConsoleWrite("Selecting character and pressing Play: " & $TARGET_CHARACTER & @CRLF)
    FileWrite($LOG_PATH, "SELECT_CHARACTER=" & $TARGET_CHARACTER & @CRLF)
    If Not _SelectExpectedCharacterAndPressPlay($TARGET_CHARACTER, $launchedPID) Then Exit 4
Else
    ConsoleWrite("Client was not at character select before play selection; continuing verification" & @CRLF)
    FileWrite($LOG_PATH, "NOT_AT_CHAR_SELECT=1" & @CRLF)
    FileWrite($LOG_PATH, "NOT_AT_CHAR_SELECT_CONTINUE_VERIFY=1" & @CRLF)
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
            Local $loadedCharacter = $game_clients[$verifyClientIdx][3]
            If $loadedCharacter = "" And $game_clients[$verifyClientIdx][1] <> 0 Then
                $loadedCharacter = StringStripWS(ScanForCharname($game_clients[$verifyClientIdx][1]), 3)
                If $loadedCharacter <> "" Then
                    $game_clients[$verifyClientIdx][3] = $loadedCharacter
                    FileWrite($LOG_PATH, "REFRESHED_LOADED_CHARACTER=" & $loadedCharacter & @CRLF)
                EndIf
            EndIf
            If TimerDiff($verifyTimer) - $lastScanLogMs > 10000 Then
                FileWrite($LOG_PATH, "VERIFY_CLIENT pid=" & $game_clients[$verifyClientIdx][0] & " hwnd=0x" & Hex($game_clients[$verifyClientIdx][2]) & " char=" & $loadedCharacter & @CRLF)
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
                ProcessClose($game_clients[$verifyClientIdx][0])
                Exit 6
            EndIf

            If TimerDiff($verifyTimer) - $lastRetryMs > 10000 Then
                SelectClient($verifyClientIdx)
                _EnsureGwWindowVisibleForPid($launchedPID, "verify-retry")
                If IsAtCharSelect() Then
                    FileWrite($LOG_PATH, "VERIFY_RETRY_SELECT_AND_PRESS_PLAY=1" & @CRLF)
                    If IsReconnectDialogShowing() Then
                        FileWrite($LOG_PATH, "VERIFY_RECONNECT_DIALOG_DISMISSED=1" & @CRLF)
                        DismissReconnectDialog('no')
                        Sleep(1000)
                    EndIf
                    If Not _SelectExpectedCharacterAndPressPlay($TARGET_CHARACTER, $launchedPID) Then
                        FileWrite($LOG_PATH, "VERIFY_RETRY_SELECT_FAILED=1" & @CRLF)
                    EndIf
                EndIf
                $lastRetryMs = TimerDiff($verifyTimer)
            EndIf
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
