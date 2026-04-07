#RequireAdmin
#include "lib\Froggy_Includes.au3"

; =============================================================================
; test_no_gwca.au3
;
; Phase 0 go/no-go harness for the native FrameUI path.
; Verifies that BotsHub/GWA2 scanning and label publication succeed with
; NO gwca.dll loaded, then writes a compact health report to tests/.
; =============================================================================

Global Const $REPORT_PATH = @ScriptDir & '\..\tests\no_gwca_scan_report.md'
Global Const $TARGET_CHARACTER = 'B E A S T R I T'

ConsoleWrite('=== Native No-GWCA Scan Harness ===' & @CRLF)
DirCreate(@ScriptDir & '\..\tests')
FileDelete($REPORT_PATH)

_Report('# Native No-GWCA Scan Report')
_Report('')
_Report('- Generated: ' & @YEAR & '-' & StringFormat('%02d', @MON) & '-' & StringFormat('%02d', @MDAY) & ' ' & _
    StringFormat('%02d', @HOUR) & ':' & StringFormat('%02d', @MIN) & ':' & StringFormat('%02d', @SEC))
_Report('- Script: `debug_scripts/test_no_gwca.au3`')
_Report('- Goal: verify native scanning and FrameUI bootstrap with no `gwca.dll` present')
_Report('')

ScanAndUpdateGameClients()
Local $targetIdx = _FindTargetClient()
If $targetIdx = -1 Then
    ConsoleWrite('No GW client found. Launching ' & $TARGET_CHARACTER & '...' & @CRLF)
    Local $accounts = GWLauncher_LoadAccounts()
    Local $accIdx = GWLauncher_FindAccountByCharacter($accounts, $TARGET_CHARACTER)
    If $accIdx < 0 Then
        _Report('## Result')
        _Report('')
        _Report('- FAIL: account `' & $TARGET_CHARACTER & '` not found in launcher config')
        Exit 1
    EndIf
    Local $launchResult = GWLauncher_LaunchAccount($accounts, $accIdx)
    ConsoleWrite('PID=' & $launchResult[0] & ', waiting 40s...' & @CRLF)
    Sleep(40000)
    ScanAndUpdateGameClients()
    $targetIdx = _FindTargetClient($launchResult[0])
EndIf

If $targetIdx = -1 Then
    _Report('## Result')
    _Report('')
    _Report('- FAIL: unable to locate a target client after launch/scan')
    Exit 1
EndIf

SelectClient($targetIdx)
Local $gwPID = $game_clients[$targetIdx][0]
Local $gwTitle = $game_clients[$targetIdx][1]
_Report('## Client')
_Report('')
_Report('- PID: `' & $gwPID & '`')
_Report('- Window: `' & $gwTitle & '`')

Local $gwcaBefore = _FindModule($gwPID, 'gwca.dll')
_Report('- `gwca.dll` before init: ' & _FmtPtr($gwcaBefore))
If $gwcaBefore <> 0 Then
    _Report('')
    _Report('## Result')
    _Report('')
    _Report('- FAIL: `gwca.dll` is already loaded before native validation')
    Exit 1
EndIf

InitializeGameClientForGWA2(False)
Local $processHandle = GetProcessHandle()
Local $gwcaAfter = _FindModule($gwPID, 'gwca.dll')
_Report('- `gwca.dll` after init: ' & _FmtPtr($gwcaAfter))
_Report('')

Local $failures = 0
If $gwcaAfter <> 0 Then
    $failures += 1
EndIf

_Report('## Pattern Health')
_Report('')

Local $criticalLabels[14] = [ _
    'BasePointer', 'PacketSend', 'AgentBase', 'Move', 'UIMessage', 'FrameArray', _
    'RenderingMod', 'MainStart', 'LoadFinishedStart', 'TraderStart', 'QueueBase', _
    'QueueCounter', 'CommandUIMsg', 'SendFrameUIMsg' _
]

For $i = 0 To UBound($criticalLabels) - 1
    If Not _CheckLabel($criticalLabels[$i]) Then $failures += 1
Next

_Report('')
_Report('## FrameUI Checks')
_Report('')
If Not _CheckFrameHash($FRAME_HASH_PLAY_BUTTON, 'Play button') Then
    _Report('- Play button: not present in current UI state')
EndIf
If Not _CheckFrameHash($FRAME_HASH_RECONNECT_YES, 'Reconnect YES button') Then
    _Report('- Reconnect YES button: not present in current UI state')
EndIf
If Not _CheckFrameHash($FRAME_HASH_RECONNECT_NO, 'Reconnect NO button') Then
    _Report('- Reconnect NO button: not present in current UI state')
EndIf

_Report('')
_Report('## Summary')
_Report('')
If $failures = 0 Then
    _Report('- PASS: native scan/bootstrap completed with no `gwca.dll` dependency detected')
    ConsoleWrite('PASS: native scan/bootstrap completed with no gwca.dll dependency detected' & @CRLF)
    Exit 0
EndIf

_Report('- FAIL: ' & $failures & ' validation issue(s) detected')
ConsoleWrite('FAIL: ' & $failures & ' validation issue(s) detected' & @CRLF)
Exit 1

Func _FindTargetClient($preferredPid = 0)
    If $preferredPid <> 0 Then
        For $i = 1 To $game_clients[0][0]
            If $game_clients[$i][0] = $preferredPid Then Return $i
        Next
    EndIf

    For $i = 1 To $game_clients[0][0]
        Local $title = $game_clients[$i][1]
        If StringInStr($title, $TARGET_CHARACTER) Or StringInStr($title, 'BEASTRIT') Then Return $i
    Next

    If $game_clients[0][0] > 0 Then Return $game_clients[0][0]
    Return -1
EndFunc

Func _FmtPtr($value)
    If $value = 0 Or $value = -1 Then Return '`missing`'
    Return '`0x' & Hex($value) & '`'
EndFunc

Func _CheckLabel($name)
    Local $value = Int(GetLabel($name))
    Local $ok = ($value <> 0 And $value <> -1)
    Local $status = ' FAIL'
    If $ok Then $status = ' OK'
    _Report('- ' & $name & ': ' & _FmtPtr($value) & $status)
    Return $ok
EndFunc

Func _CheckFrameHash($hash, $label)
    Local $result = GetFrameByHash($hash)
    If Not IsArray($result) Or $result[0] = 0 Then Return False

    Local $state = MemoryRead(GetProcessHandle(), $result[0] + $FRAME_OFFSET_STATE, 'dword')
    _Report('- ' & $label & ': frame=`0x' & Hex($result[0]) & '` id=`' & $result[1] & '` state=`0x' & Hex($state) & '`')
    Return True
EndFunc

Func _Report($line)
    ConsoleWrite($line & @CRLF)
    FileWrite($REPORT_PATH, $line & @CRLF)
EndFunc
