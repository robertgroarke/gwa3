#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== UISniffer Fresh Launch Test ===" & @CRLF)

; Launch BEASTRIT fresh
Local $accounts = GWLauncher_LoadAccounts()
Local $idx = GWLauncher_FindAccountByCharacter($accounts, "B E A S T R I T")
If $idx = -1 Then
    ConsoleWrite("ERROR: BEASTRIT account not found" & @CRLF)
    Exit 1
EndIf

ConsoleWrite("Launching BEASTRIT (index " & $idx & ")..." & @CRLF)
Local $result = GWLauncher_LaunchAccount($accounts, $idx)
If $result = 0 Then
    ConsoleWrite("ERROR: Failed to launch" & @CRLF)
    Exit 1
EndIf
Local $launchedPID = $result[0]
ConsoleWrite("GW launched PID=" & $launchedPID & @CRLF)

ConsoleWrite("Waiting 35s for char select..." & @CRLF)
Sleep(35000)

; Scan and find our new client by PID
ScanAndUpdateGameClients()
ConsoleWrite("Found " & $game_clients[0][0] & " clients:" & @CRLF)
Local $targetIdx = -1
For $i = 1 To $game_clients[0][0]
    ConsoleWrite("  " & $i & ": " & $game_clients[$i][3] & " (PID=" & $game_clients[$i][0] & ")" & @CRLF)
    If $game_clients[$i][0] = $launchedPID Then
        $targetIdx = $i
    EndIf
Next

If $targetIdx = -1 Then
    ConsoleWrite("ERROR: Could not find launched PID " & $launchedPID & @CRLF)
    Exit 1
EndIf

ConsoleWrite("Connecting to client " & $targetIdx & " (PID=" & $launchedPID & ")" & @CRLF)
SelectClient($targetIdx)
InitializeGameClientData(True, False)
ConsoleWrite("Connected!" & @CRLF)

; Dump UIMessageStart first 8 bytes
Local $processHandle = GetProcessHandle()
Local $uiMsgAddr = GetLabel('UIMessageStart')
Local $checkBytes = DllStructCreate('byte[8]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $processHandle, 'ptr', $uiMsgAddr, _
    'ptr', DllStructGetPtr($checkBytes), 'ulong_ptr', 8, 'ulong_ptr*', 0)

Local $hexLine = ""
For $i = 1 To 8
    $hexLine &= Hex(DllStructGetData($checkBytes, 1, $i), 2) & " "
Next
ConsoleWrite("UIMessageStart (0x" & Hex($uiMsgAddr) & "): " & $hexLine & @CRLF)

Local $firstByte = DllStructGetData($checkBytes, 1, 1)
If $firstByte = 0xE9 Then
    ; UIMessage might be a thunk/stub that starts with JMP
    ; Follow the JMP to find the real entry point
    Local $relBytes = 0
    For $i = 2 To 5
        $relBytes += DllStructGetData($checkBytes, 1, $i) * (256 ^ ($i - 2))
    Next
    If $relBytes >= 0x80000000 Then $relBytes -= 0x100000000
    Local $realEntry = $uiMsgAddr + 5 + $relBytes
    ConsoleWrite("UIMessage is a JMP thunk -> real entry at 0x" & Hex($realEntry) & @CRLF)

    ; Read the real entry bytes
    Local $realBytes = DllStructCreate('byte[8]')
    DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
        'handle', $processHandle, 'ptr', $realEntry, _
        'ptr', DllStructGetPtr($realBytes), 'ulong_ptr', 8, 'ulong_ptr*', 0)

    $hexLine = ""
    For $i = 1 To 8
        $hexLine &= Hex(DllStructGetData($realBytes, 1, $i), 2) & " "
    Next
    ConsoleWrite("Real entry bytes: " & $hexLine & @CRLF)

    ; We still hook at UIMessageStart (the thunk) — we just need to know
    ; the original 5 bytes are E9+offset, and our hook replaces them with our E9+offset
    ConsoleWrite("Proceeding with hook at thunk address..." & @CRLF)
EndIf

; Enable sniffer
ConsoleWrite(@CRLF & "=== Enabling UISniffer ===" & @CRLF)
UISnifferEnable()

; Dump full trampoline
ConsoleWrite(@CRLF & "=== Full trampoline dump ===" & @CRLF)
Local $actualStart = $g_UISniffer_ActualProcAddr
Local $dumpBuf = DllStructCreate('byte[96]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $processHandle, 'ptr', $actualStart, _
    'ptr', DllStructGetPtr($dumpBuf), 'ulong_ptr', 96, 'ulong_ptr*', 0)

For $row = 0 To 5
    $hexLine = "  +" & Hex($row * 16, 2) & ": "
    For $col = 1 To 16
        $hexLine &= Hex(DllStructGetData($dumpBuf, 1, $row * 16 + $col), 2) & " "
    Next
    ConsoleWrite($hexLine & @CRLF)
Next

; Verify hook
ConsoleWrite(@CRLF & "=== Verifying hook ===" & @CRLF)
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $processHandle, 'ptr', $uiMsgAddr, _
    'ptr', DllStructGetPtr($checkBytes), 'ulong_ptr', 8, 'ulong_ptr*', 0)
$hexLine = ""
For $i = 1 To 8
    $hexLine &= Hex(DllStructGetData($checkBytes, 1, $i), 2) & " "
Next
ConsoleWrite("UIMessageStart after hook: " & $hexLine & @CRLF)

; Poll for 30 seconds
ConsoleWrite(@CRLF & "=== Polling for 30 seconds ===" & @CRLF)
Local $lastCounter = 0
Local $counterAddr = GetLabel('UISnifferCounter')
Local $msgIdAddr = GetLabel('UISnifferMsgId')
Local $wParamAddr = GetLabel('UISnifferWParam')
Local $lParamAddr = GetLabel('UISnifferLParam')
Local $captureCount = 0

For $poll = 1 To 300
    Sleep(100)
    Local $counter = MemoryRead($processHandle, $counterAddr, 'dword')
    If $counter <> $lastCounter Then
        Local $msgId = MemoryRead($processHandle, $msgIdAddr, 'dword')
        Local $wParamPtr = MemoryRead($processHandle, $wParamAddr, 'dword')
        Local $lParamVal = MemoryRead($processHandle, $lParamAddr, 'dword')

        Local $wp0 = 0, $wp1 = 0
        If $wParamPtr > 0x10000 Then
            $wp0 = MemoryRead($processHandle, $wParamPtr, 'dword')
            $wp1 = MemoryRead($processHandle, $wParamPtr + 4, 'dword')
        EndIf

        ConsoleWrite("[CAPTURE] #" & $counter & " MsgID=0x" & Hex($msgId, 8) & _
            " wP=0x" & Hex($wParamPtr, 8) & _
            " [" & Hex($wp0, 8) & "," & Hex($wp1, 8) & "]" & _
            " lP=" & $lParamVal & @CRLF)

        $lastCounter = $counter
        $captureCount += 1
        If $captureCount >= 100 Then ExitLoop
    EndIf
Next

ConsoleWrite(@CRLF & "Captured " & $captureCount & " messages in 30 seconds" & @CRLF)

; Cleanup
UISnifferDisable()
ConsoleWrite("=== DONE ===" & @CRLF)
