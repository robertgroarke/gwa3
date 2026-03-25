#include "lib/GWA2.au3"

Func DebugHookMismatch()
    ScanAndUpdateGameClients()
    Local $targetName = "L I L B I S C U I T"
    Local $index = FindClientIndexByCharacterName($targetName)
    If $index == -1 Then Return
    SelectClient($index)
    InitializeGameClientData(True, False)
    
    Local $logFile = @ScriptDir & "\hook_debug.txt"
    Local $hFile = FileOpen($logFile, 2)

    ; 1. Check current ScanTraderHook bytes
    Local $scanAddr = GetScannedAddress('ScanTraderHook', 0)
    Local $bytesAtScan = MemoryRead($scanAddr, 'byte[10]')
    FileWrite($hFile, "ScanTraderHook Address: " & Hex($scanAddr) & @CRLF)
    FileWrite($hFile, "Bytes at ScanTraderHook: " & Hex($bytesAtScan) & @CRLF)
    
    ; 2. Scan for the expected pattern "8B5D0C8BF0"
    ; This corresponds to: mov ebx, [ebp+0xC]; mov esi, eax
    Local $processHandle = GetProcessHandle()
    Local $found = ScanMemoryForPattern($processHandle, Binary("0x8B5D0C8BF0"))
    
    If IsArray($found) Then
        FileWrite($hFile, "Found Expected Pattern at: " & Hex($found[1]) & @CRLF)
        FileWrite($hFile, "Match Offset: " & $found[2] & @CRLF)
        
        ; Verify surrounding bytes
        Local $foundAddr = $found[1] + $found[2] - 1
        Local $bytesAtFound = MemoryRead($foundAddr, 'byte[10]')
        FileWrite($hFile, "Bytes at Found Pattern: " & Hex($bytesAtFound) & @CRLF)
        
        ; Check if it's executable code
        FileWrite($hFile, "This looks like the correct hook location!" & @CRLF)
    Else
        FileWrite($hFile, "Could not find pattern 8B5D0C8BF0 in memory." & @CRLF)
    EndIf
    
    FileClose($hFile)
EndFunc

DebugHookMismatch()
