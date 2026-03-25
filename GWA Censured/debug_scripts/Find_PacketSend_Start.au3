#include "lib/GWA2.au3"

Func FindPacketSendStart()
    ScanAndUpdateGameClients()
    Local $targetName = "L I L B I S C U I T"
    Local $index = FindClientIndexByCharacterName($targetName)
    If $index == -1 Then 
        ConsoleWrite("Character not found" & @CRLF)
        Return
    EndIf
    SelectClient($index)
    InitializeGameClientData(True, False)
    
    Local $logFile = @ScriptDir & "\packet_send_start.txt"
    Local $hFile = FileOpen($logFile, 2)
    
    Local $packetSendAddr = GetScannedAddress('ScanPacketSendFunction', -0x5)
    If $packetSendAddr == 0 Then
        FileWrite($hFile, "PacketSendFunction not found." & @CRLF)
        FileClose($hFile)
        Return
    EndIf
    
    FileWrite($hFile, "Current PacketSend Address: " & Hex($packetSendAddr) & @CRLF)
    
    ; Scan backwards for common function start signatures or padding (CC)
    ; Common starts:
    ; 55 8B EC (PUSH EBP; MOV EBP, ESP)
    ; 56 8B F1 (PUSH ESI; MOV ESI, ECX - thiscall)
    ; 53 8B DC (PUSH EBX; MOV EBX, ESP)
    
    Local $foundStart = 0
    Local $buffer = MemoryRead($packetSendAddr - 200, 'byte[200]')
    Local $bufferHex = Hex($buffer)
    
    ; We search from the END of the buffer (which is close to the address) to the BEGINNING
    ; Hex string is 400 chars long.
    ; packetSendAddr corresponds to the END of this dump roughly?
    ; No, MemoryRead reads [Addr, Addr+Size].
    ; So we read [Addr-200, Addr].
    ; The byte at Addr is not in the buffer. The last byte is Addr-1.
    
    ; Let's parse bytes backwards.
    For $i = 0 To 199
        Local $offset = 200 - 1 - $i
        Local $b1 = BinaryMid($buffer, $offset + 1, 1) ; 1-based index
        Local $b2 = BinaryMid($buffer, $offset + 2, 1)
        Local $b3 = BinaryMid($buffer, $offset + 3, 1)
        
        Local $h1 = Hex($b1)
        Local $h2 = Hex($b2)
        Local $h3 = Hex($b3)
        
        Local $currAddr = $packetSendAddr - 200 + $offset
        
        ; Check for CC (Int3 - Padding)
        If $h1 == "CC" Then
             ; If we hit padding, the *next* byte is likely the function start
             $foundStart = $currAddr + 1
             FileWrite($hFile, "Found Padding (CC) at: " & Hex($currAddr) & @CRLF)
             FileWrite($hFile, "Probable Function Start: " & Hex($foundStart) & @CRLF)
             
             ; Dump the start bytes
             Local $startBytes = MemoryRead($foundStart, 'byte[10]')
             FileWrite($hFile, "Bytes at Start: " & Hex($startBytes) & @CRLF)
             ExitLoop
        EndIf
        
        ; Check 55 8B EC
        If $h1 == "55" And $h2 == "8B" And $h3 == "EC" Then
             $foundStart = $currAddr
             FileWrite($hFile, "Found Standard Prologue (55 8B EC) at: " & Hex($currAddr) & @CRLF)
             ExitLoop
        EndIf
        
        ; Check 56 8B F1 (thiscall)
        If $h1 == "56" And $h2 == "8B" And $h3 == "F1" Then
             $foundStart = $currAddr
             FileWrite($hFile, "Found ThisCall Prologue (56 8B F1) at: " & Hex($currAddr) & @CRLF)
             ExitLoop
        EndIf
    Next
    
    If $foundStart == 0 Then
         FileWrite($hFile, "Could not find function start in previous 200 bytes." & @CRLF)
    EndIf
    
    FileClose($hFile)
EndFunc

FindPacketSendStart()
