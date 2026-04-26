#RequireAdmin
#include "..\lib\Froggy_Includes.au3"
Global Const $ACCOUNTS_PATH = "C:\Users\Robert\Documents\GWA Censured X BotsHub\GWA Censured\Accounts.json"
Global Const $TARGET_CHARACTER = "B E A S T R I T"
Global Const $LOG_PATH = "C:\Users\Robert\Documents\GWA Censured X BotsHub\gwa3\..\GWA Censured\debug_scripts\launch_beastrit_override_via_gwlauncher.log"
Local $accounts = GWLauncher_LoadAccounts($ACCOUNTS_PATH)
Local $idx = GWLauncher_FindAccountByCharacter($accounts, $TARGET_CHARACTER)
If $idx < 0 Then
    FileDelete($LOG_PATH)
    FileWrite($LOG_PATH, "GWLAUNCHER_ERROR=account_not_found" & @CRLF)
    Exit 2
EndIf
Local $overrideGwPath = "C:\Program Files (x86)\Guild Wars - Copy\Gw.exe"
If $overrideGwPath <> "" Then
    Local $acct = $accounts[$idx]
    If IsMap($acct) Then
        $acct['gwpath'] = $overrideGwPath
        $accounts[$idx] = $acct
    EndIf
EndIf
Local $result = GWLauncher_LaunchAccount($accounts, $idx)
If $result = 0 Then
    FileDelete($LOG_PATH)
    FileWrite($LOG_PATH, "GWLAUNCHER_ERROR=launch_failed" & @CRLF)
    Exit 3
EndIf
FileDelete($LOG_PATH)
FileWrite($LOG_PATH, "GWLAUNCHER_PID=" & $result[0] & @CRLF)
ConsoleWrite("GWLAUNCHER_PID=" & $result[0] & @CRLF)
Exit 0
