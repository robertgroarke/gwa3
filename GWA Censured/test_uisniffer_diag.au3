#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== UISniffer Diagnostic v3 ===" & @CRLF)

; Connect to first available client
ScanAndUpdateGameClients()
If $game_clients[0][0] = 0 Then
    ConsoleWrite("ERROR: No GW clients found" & @CRLF)
    Exit 1
EndIf

ConsoleWrite("Found " & $game_clients[0][0] & " clients" & @CRLF)
For $i = 1 To $game_clients[0][0]
    ConsoleWrite("  " & $i & ": " & $game_clients[$i][3] & @CRLF)
Next

SelectClient(1)
InitializeGameClientData(True, False)
ConsoleWrite("Connected to client 1" & @CRLF)

; Enable sniffer (this does calibration + JMP install)
ConsoleWrite(@CRLF & "=== Enabling UISniffer ===" & @CRLF)
UISnifferEnable()

; Verify the hook
Local $processHandle = GetProcessHandle()
Local $uiMsgAddr = GetLabel('UIMessageStart')

ConsoleWrite(@CRLF & "=== Verifying hook ===" & @CRLF)
Local $hookBytes = DllStructCreate('byte[8]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $processHandle, 'ptr', $uiMsgAddr, _
    'ptr', DllStructGetPtr($hookBytes), 'ulong_ptr', 8, 'ulong_ptr*', 0)

Local $hexLine = ""
For $i = 1 To 8
    $hexLine &= Hex(DllStructGetData($hookBytes, 1, $i), 2) & " "
Next
ConsoleWrite("UIMessageStart bytes: " & $hexLine & @CRLF)

If DllStructGetData($hookBytes, 1, 1) = 0xE9 Then
    Local $relBytes = 0
    For $i = 2 To 5
        $relBytes += DllStructGetData($hookBytes, 1, $i) * (256 ^ ($i - 2))
    Next
    If $relBytes >= 0x80000000 Then $relBytes -= 0x100000000
    Local $jmpTarget = $uiMsgAddr + 5 + $relBytes
    ConsoleWrite("JMP target = 0x" & Hex($jmpTarget) & @CRLF)

    Local $targetCheck = DllStructCreate('byte[2]')
    DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
        'handle', $processHandle, 'ptr', $jmpTarget, _
        'ptr', DllStructGetPtr($targetCheck), 'ulong_ptr', 2, 'ulong_ptr*', 0)
    If DllStructGetData($targetCheck, 1, 1) = 0x9C And DllStructGetData($targetCheck, 1, 2) = 0x60 Then
        ConsoleWrite("OK: JMP lands on pushfd+pushad" & @CRLF)
    Else
        ConsoleWrite("FAIL: bytes at target = " & Hex(DllStructGetData($targetCheck, 1, 1), 2) & " " & _
            Hex(DllStructGetData($targetCheck, 1, 2), 2) & @CRLF)
    EndIf
EndIf

; Dump the full trampoline (from actual start, 128 bytes)
ConsoleWrite(@CRLF & "=== Full trampoline dump ===" & @CRLF)
Local $actualStart = $g_UISniffer_ActualProcAddr
Local $dumpBuf = DllStructCreate('byte[128]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $processHandle, 'ptr', $actualStart, _
    'ptr', DllStructGetPtr($dumpBuf), 'ulong_ptr', 128, 'ulong_ptr*', 0)

For $row = 0 To 7
    $hexLine = "  +" & Hex($row * 16, 2) & ": "
    For $col = 1 To 16
        $hexLine &= Hex(DllStructGetData($dumpBuf, 1, $row * 16 + $col), 2) & " "
    Next
    ConsoleWrite($hexLine & @CRLF)
Next

; Poll for 20 seconds
ConsoleWrite(@CRLF & "=== Polling for 20 seconds ===" & @CRLF)
Local $lastCounter = 0
Local $counterAddr = GetLabel('UISnifferCounter')
Local $msgIdAddr = GetLabel('UISnifferMsgId')
Local $wParamAddr = GetLabel('UISnifferWParam')
Local $lParamAddr = GetLabel('UISnifferLParam')
Local $captureCount = 0

For $poll = 1 To 200
    Sleep(100)
    Local $counter = MemoryRead($processHandle, $counterAddr, 'dword')
    If $counter <> $lastCounter Then
        Local $msgId = MemoryRead($processHandle, $msgIdAddr, 'dword')
        Local $wParamPtr = MemoryRead($processHandle, $wParamAddr, 'dword')
        Local $lParamVal = MemoryRead($processHandle, $lParamAddr, 'dword')

        ConsoleWrite("[CAPTURE] #" & $counter & " MsgID=0x" & Hex($msgId, 8) & _
            " wParam=0x" & Hex($wParamPtr, 8) & " lParam=" & $lParamVal & @CRLF)

        $lastCounter = $counter
        $captureCount += 1
        If $captureCount >= 50 Then ExitLoop  ; cap at 50 messages
    EndIf
Next

ConsoleWrite(@CRLF & "Captured " & $captureCount & " messages" & @CRLF)

; Cleanup
UISnifferDisable()
ConsoleWrite("=== DONE ===" & @CRLF)
