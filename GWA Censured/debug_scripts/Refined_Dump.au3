#include "lib/GWA2.au3"

Func RefinedDump()
    ScanAndUpdateGameClients()
    Local $targetName = "L I L B I S C U I T"
    Local $index = FindClientIndexByCharacterName($targetName)
    If $index == -1 Then
         ConsoleWrite("Character not found" & @CRLF)
         Return
    EndIf
    SelectClient($index)
    InitializeGameClientData(True, False)
    
    Local $logFile = @ScriptDir & "\refined_dump.txt"
    Local $hFile = FileOpen($logFile, 2)
    
    Local $processHandle = GetProcessHandle()
    
    ; 1. Try to find the start of the function: 83 FF 10 76 14 68
    ; CMP EDI, 10; JBE +14; PUSH 68...
    Local $pattern1 = Binary("0x83FF10761468")
    FileWrite($hFile, "Scanning for 83FF10761468..." & @CRLF)
    Local $found1 = ScanMemoryForPattern($processHandle, $pattern1)
    
    If IsArray($found1) Then
        Local $addr1 = $found1[1] + $found1[2] - 1 ; ScanMemoryForPattern returns offset 1-based index? standard GWA use seems to need adjustment?
        ; Actually ScanMemoryForPattern returns [Base, RegionStart, Offset]
        ; The function returns offset where the string occurs.
        ; Let's verify usage in GWA2.au3
        ; Local $match[3] = [$memoryBaseAddress, $currentSearchAddress, $matchOffset]
        
        Local $realAddr1 = $found1[1] + $found1[2] - 1 ; StringInStr is 1-based
        FileWrite($hFile, "Found Function Start at: " & Hex($realAddr1) & @CRLF)
        
        ; Dump from there
        Local $bytes = MemoryRead($realAddr1, 'byte[100]')
        FileWrite($hFile, "Bytes at Function Start: " & Hex($bytes) & @CRLF)
    Else
        FileWrite($hFile, "Function Start Pattern NOT found." & @CRLF)
    EndIf
    
    ; 2. Try to find the inner hook part: 8B 43 28 8B 00
    ; MOV EAX, [EBX+28]; MOV EAX, [EAX]
    Local $pattern2 = Binary("0x8B43288B00")
    FileWrite($hFile, "Scanning for 8B43288B00..." & @CRLF)
    Local $found2 = ScanMemoryForPattern($processHandle, $pattern2)
    
    If IsArray($found2) Then
        Local $realAddr2 = $found2[1] + $found2[2] - 1
        FileWrite($hFile, "Found Hook Candidate at: " & Hex($realAddr2) & @CRLF)
        
        Local $bytes2 = MemoryRead($realAddr2 - 20, 'byte[60]')
        FileWrite($hFile, "Bytes around Hook Candidate (-20 to +40): " & Hex($bytes2) & @CRLF)
    Else
        FileWrite($hFile, "Hook Candidate Pattern NOT found." & @CRLF)
    EndIf

    FileClose($hFile)
EndFunc

RefinedDump()
