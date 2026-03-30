#include-once
; =============================================================================
; WebIPC — File-based IPC for web dashboard monitoring and control
;
; Each bot writes status.json atomically and polls for command.json.
; A Node.js web server reads status files and writes command files.
;
; Usage:
;   WebIPC_Init("B E A S T R I T")   ; Call after connecting to client
;   ; In main loop:
;   WebIPC_WriteStatus()              ; Write current state to status.json
;   WebIPC_CheckCommand()             ; Check for and process commands
;   ; In Out() function:
;   WebIPC_AppendLog($text)           ; Buffer log lines for status.json
; =============================================================================

Global $g_WebIPC_Dir = ''
Global $g_WebIPC_Character = ''
Global $g_WebIPC_Script = 'Froggy_HM_v1.6'
Global $g_WebIPC_StartTime = 0
Global $g_WebIPC_Initialized = False
Global $g_WebIPC_LastWrite = 0
Global Const $g_WebIPC_WriteInterval = 2000  ; ms between status writes
Global Const $g_WebIPC_LogMaxLines = 200

; Log ring buffer
Global $g_WebIPC_Log[1] = ['']
Global $g_WebIPC_LogCount = 0

;~ Initialize the IPC directory and state for this bot instance.
;~ Call after connecting to the GW client.
Func WebIPC_Init($characterName)
    $g_WebIPC_Character = $characterName
    $g_WebIPC_StartTime = TimerInit()

    ; Sanitize character name for directory: strip spaces, lowercase
    Local $dirName = StringReplace($characterName, ' ', '')
    $dirName = StringLower($dirName)

    ; Create ipc directory under the script directory
    $g_WebIPC_Dir = @ScriptDir & '\ipc\' & $dirName
    If Not FileExists(@ScriptDir & '\ipc') Then DirCreate(@ScriptDir & '\ipc')
    If Not FileExists($g_WebIPC_Dir) Then DirCreate($g_WebIPC_Dir)

    $g_WebIPC_Initialized = True
    $g_WebIPC_LastWrite = 0
    ReDim $g_WebIPC_Log[$g_WebIPC_LogMaxLines]
    $g_WebIPC_LogCount = 0

    ; Register periodic timer so status updates even during long farming functions
    AdlibRegister('_WebIPC_Tick', 3000)

    ConsoleWrite('[WebIPC] Initialized: ' & $g_WebIPC_Dir & @CRLF)
    WebIPC_WriteStatus()
EndFunc

;~ Timer callback — called every 3 seconds by AdlibRegister
Func _WebIPC_Tick()
    WebIPC_WriteStatus()
    WebIPC_CheckCommand()
EndFunc

;~ Append a log line to the ring buffer.
;~ Called from Out() to capture bot console output.
Func WebIPC_AppendLog($text)
    If Not $g_WebIPC_Initialized Then Return

    ; Strip leading CRLF/LF that Out() prepends between lines
    $text = StringRegExpReplace($text, '^\s*[\r\n]+', '')
    If $text = '' Then Return

    If $g_WebIPC_LogCount < $g_WebIPC_LogMaxLines Then
        $g_WebIPC_Log[$g_WebIPC_LogCount] = $text
        $g_WebIPC_LogCount += 1
    Else
        ; Shift buffer: drop oldest, append at end
        For $i = 0 To $g_WebIPC_LogMaxLines - 2
            $g_WebIPC_Log[$i] = $g_WebIPC_Log[$i + 1]
        Next
        $g_WebIPC_Log[$g_WebIPC_LogMaxLines - 1] = $text
    EndIf
EndFunc

;~ Write current bot state to status.json (atomic: write .tmp then rename).
;~ Throttled to once per $g_WebIPC_WriteInterval ms.
Func WebIPC_WriteStatus()
    If Not $g_WebIPC_Initialized Then Return

    ; Throttle writes
    If TimerDiff($g_WebIPC_LastWrite) < $g_WebIPC_WriteInterval Then Return
    $g_WebIPC_LastWrite = TimerInit()

    ; Build status JSON manually (faster than Scripting.Dictionary for fixed schema)
    Local $json = '{' & @CRLF
    $json &= '  "character": ' & _JsonStr($g_WebIPC_Character) & ',' & @CRLF
    $json &= '  "script": ' & _JsonStr($g_WebIPC_Script) & ',' & @CRLF
    $json &= '  "pid": ' & @AutoItPID & ',' & @CRLF
    $json &= '  "timestamp": ' & _GetUnixTimestamp() & ',' & @CRLF
    $json &= '  "uptime_seconds": ' & Int(TimerDiff($g_WebIPC_StartTime) / 1000) & ',' & @CRLF
    $json &= '  "bot_running": ' & _JsonBool($BotRunning) & ',' & @CRLF

    ; Game state (safe reads — these return 0 if not connected)
    Local $mapId = 0, $charGold = 0, $storageGold = 0
    If $g_WebIPC_Initialized Then
        $mapId = GetMapID()
        $charGold = GetGoldCharacter()
        $storageGold = GetGoldStorage()
    EndIf
    $json &= '  "map_id": ' & $mapId & ',' & @CRLF
    $json &= '  "gold": { "character": ' & $charGold & ', "storage": ' & $storageGold & ' },' & @CRLF

    ; Determine state label from map
    Local $state = 'idle'
    If $BotRunning Then
        Switch $mapId
            Case 638  ; Gadd's Encampment
                $state = 'setup'
            Case 495  ; Sparkfly Swamp
                $state = 'traveling'
            Case 857  ; Embark Beach
                $state = 'maintenance'
            Case 558, 559  ; Bogroot Growths
                $state = 'farming'
            Case Else
                If $mapId > 0 Then $state = 'farming'
        EndSwitch
    EndIf
    $json &= '  "state": ' & _JsonStr($state) & ',' & @CRLF

    ; Settings from GUI checkboxes
    $json &= '  "settings": {' & @CRLF
    $json &= '    "add_heroes": ' & _JsonBool(GUI_IsAddHeroesChecked()) & ',' & @CRLF
    $json &= '    "consets": ' & _JsonBool(GUI_IsConsetsChecked()) & ',' & @CRLF
    $json &= '    "buy_consets": ' & _JsonBool(GUI_IsBuyConsetsChecked()) & ',' & @CRLF
    $json &= '    "stones": ' & _JsonBool(GUI_IsStonesChecked()) & ',' & @CRLF
    $json &= '    "open_chests": ' & _JsonBool(GUI_IsChestChecked()) & ',' & @CRLF
    $json &= '    "pick_up_golds": ' & _JsonBool(GUI_IsPickUpGoldsChecked()) & ',' & @CRLF
    $json &= '    "auto_salvage": ' & _JsonBool(GUI_IsSalvageChecked()) & ',' & @CRLF
    $json &= '    "hero_config": ' & _JsonStr(GUI_GetSelectedHeroConfig()) & @CRLF
    $json &= '  },' & @CRLF

    ; Run statistics
    Local $bestTime = '---'
    If $nBestRunTime < 999999999 Then $bestTime = _FormatTicks($nBestRunTime)
    Local $avgTime = '---'
    If $GUI_RunCounter > 0 Then $avgTime = _FormatTicks($CumulatedTime / $GUI_RunCounter)
    Local $totalTime = _FormatTicks(TimerDiff($nTotalRunTime))
    Local $currentTime = _FormatTicks(TimerDiff($nCurrentRunTime))

    $json &= '  "stats": {' & @CRLF
    $json &= '    "run_count": ' & $GUI_RunCounter & ',' & @CRLF
    $json &= '    "fail_count": ' & $GUI_FailCounter & ',' & @CRLF
    $json &= '    "best_run_time": ' & _JsonStr($bestTime) & ',' & @CRLF
    $json &= '    "avg_run_time": ' & _JsonStr($avgTime) & ',' & @CRLF
    $json &= '    "current_run_time": ' & _JsonStr($currentTime) & ',' & @CRLF
    $json &= '    "total_time": ' & _JsonStr($totalTime) & @CRLF
    $json &= '  },' & @CRLF

    ; Log buffer
    $json &= '  "log": [' & @CRLF
    For $i = 0 To $g_WebIPC_LogCount - 1
        $json &= '    ' & _JsonStr($g_WebIPC_Log[$i])
        If $i < $g_WebIPC_LogCount - 1 Then $json &= ','
        $json &= @CRLF
    Next
    $json &= '  ]' & @CRLF
    $json &= '}'

    ; Atomic write: write to .tmp, then rename
    Local $tmpFile = $g_WebIPC_Dir & '\status.tmp'
    Local $statusFile = $g_WebIPC_Dir & '\status.json'
    Local $hFile = FileOpen($tmpFile, 2 + 256)  ; overwrite + UTF8 no BOM
    If $hFile = -1 Then Return
    FileWrite($hFile, $json)
    FileClose($hFile)
    FileMove($tmpFile, $statusFile, 1)  ; 1 = overwrite
EndFunc

;~ Check for command.json and process it. Deletes the file after processing.
Func WebIPC_CheckCommand()
    If Not $g_WebIPC_Initialized Then Return

    Local $cmdFile = $g_WebIPC_Dir & '\command.json'
    If Not FileExists($cmdFile) Then Return

    Local $raw = FileRead($cmdFile)
    FileDelete($cmdFile)

    If StringLen($raw) < 5 Then Return

    Local $cmd = _JSON_Parse($raw)
    If Not IsObj($cmd) Then Return

    Local $action = ''
    If $cmd.Exists('action') Then $action = $cmd.Item('action')

    ConsoleWrite('[WebIPC] Command received: ' & $action & @CRLF)

    Switch $action
        Case 'stop'
            $BotRunning = False
            Out('Web command: STOP')

        Case 'start'
            $BotRunning = True
            Out('Web command: START')

        Case 'restart'
            $BotRunning = False
            Sleep(1000)
            $BotRunning = True
            Out('Web command: RESTART')

        Case 'update_settings'
            If $cmd.Exists('settings') Then
                _WebIPC_ApplySettings($cmd.Item('settings'))
            EndIf

        Case 'kill'
            Out('Web command: KILL — shutting down')
            WebIPC_Shutdown()
            Exit
    EndSwitch
EndFunc

;~ Write a final offline status and clean up.
Func WebIPC_Shutdown()
    If Not $g_WebIPC_Initialized Then Return

    Local $json = '{' & @CRLF
    $json &= '  "character": ' & _JsonStr($g_WebIPC_Character) & ',' & @CRLF
    $json &= '  "state": "offline",' & @CRLF
    $json &= '  "bot_running": false,' & @CRLF
    $json &= '  "timestamp": ' & _GetUnixTimestamp() & @CRLF
    $json &= '}'

    Local $hFile = FileOpen($g_WebIPC_Dir & '\status.json', 2 + 256)
    If $hFile <> -1 Then
        FileWrite($hFile, $json)
        FileClose($hFile)
    EndIf
EndFunc

; =============================================================================
; Internal helpers
; =============================================================================

Func _WebIPC_ApplySettings($settings)
    If Not IsObj($settings) Then Return

    If $settings.Exists('consets') Then
        GUICtrlSetState($GUI_GroupSettings_CheckConsets, $settings.Item('consets') ? $GUI_CHECKED : $GUI_UNCHECKED)
    EndIf
    If $settings.Exists('buy_consets') Then
        GUICtrlSetState($GUI_GroupSettings_CheckBuyConsets, $settings.Item('buy_consets') ? $GUI_CHECKED : $GUI_UNCHECKED)
    EndIf
    If $settings.Exists('stones') Then
        GUICtrlSetState($GUI_GroupSettings_CheckStones, $settings.Item('stones') ? $GUI_CHECKED : $GUI_UNCHECKED)
    EndIf
    If $settings.Exists('open_chests') Then
        GUICtrlSetState($GUI_GroupSettings_CheckChests, $settings.Item('open_chests') ? $GUI_CHECKED : $GUI_UNCHECKED)
    EndIf
    If $settings.Exists('pick_up_golds') Then
        GUICtrlSetState($GUI_GroupSettings_CheckPickUpGolds, $settings.Item('pick_up_golds') ? $GUI_CHECKED : $GUI_UNCHECKED)
    EndIf
    If $settings.Exists('auto_salvage') Then
        GUICtrlSetState($GUI_GroupSettings_CheckSalvage, $settings.Item('auto_salvage') ? $GUI_CHECKED : $GUI_UNCHECKED)
    EndIf

    Out('Web command: settings updated')
EndFunc

;~ Escape a string for JSON output
Func _JsonStr($val)
    Local $s = String($val)
    $s = StringReplace($s, '\', '\\')
    $s = StringReplace($s, '"', '\"')
    $s = StringReplace($s, @CR, '')
    $s = StringReplace($s, @LF, '\n')
    $s = StringReplace($s, @TAB, '\t')
    Return '"' & $s & '"'
EndFunc

;~ Convert a boolean to JSON true/false
Func _JsonBool($val)
    If $val Then Return 'true'
    Return 'false'
EndFunc

;~ Get current Unix timestamp (seconds since epoch) using Windows API
Func _GetUnixTimestamp()
    Local $tSys = DllStructCreate('word;word;word;word;word;word;word;word')
    DllCall('kernel32.dll', 'none', 'GetSystemTime', 'struct*', $tSys)
    Local $tFile = DllStructCreate('dword;dword')
    DllCall('kernel32.dll', 'bool', 'SystemTimeToFileTime', 'struct*', $tSys, 'struct*', $tFile)
    ; FILETIME is 100-nanosecond intervals since 1601-01-01
    ; Unix epoch offset: 116444736000000000
    Local $lo = DllStructGetData($tFile, 1)
    Local $hi = DllStructGetData($tFile, 2)
    ; Convert to seconds: (hi * 2^32 + lo) / 10000000 - 11644473600
    Return Int($hi * 4294967296 / 10000000 + $lo / 10000000 - 11644473600)
EndFunc

;~ Format millisecond ticks to HH:MM:SS string
Func _FormatTicks($ticks)
    Local $totalSecs = Int($ticks / 1000)
    Local $h = Int($totalSecs / 3600)
    Local $m = Mod(Int($totalSecs / 60), 60)
    Local $s = Mod($totalSecs, 60)
    Return StringFormat('%02d:%02d:%02d', $h, $m, $s)
EndFunc
