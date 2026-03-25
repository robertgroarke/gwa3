#include "lib/GWA2.au3"

Func DumpPacketSendPrologue()
    Local $logFile = @ScriptDir & "\packet_send_prologue.txt"
    Local $hFile = FileOpen($logFile, 2)
    FileWrite($hFile, "Script Started." & @CRLF)

    ScanAndUpdateGameClients()
    FileWrite($hFile, "Clients Scanned." & @CRLF)

    Local $targetName = "L I L B I S C U I T"
    Local $index = FindClientIndexByCharacterName($targetName)
    FileWrite($hFile, "Client Index: " & $index & @CRLF)
    
    If $index == -1 Then 
        FileWrite($hFile, "Character not found!" & @CRLF)
        FileClose($hFile)
        Return
    EndIf
    SelectClient($index)
    InitializeGameClientData(True, False)
    
    Local $packetSendAddr = GetScannedAddress('ScanPacketSendFunction', -0x5)
    FileWrite($hFile, "PacketSendFunction Address: " & Hex($packetSendAddr) & @CRLF)
    
    If $packetSendAddr > 0 Then
        Local $bytes = MemoryRead($packetSendAddr, 'byte[16]')
        FileWrite($hFile, "Prologue Bytes: " & Hex($bytes) & @CRLF)
    EndIf
    
    FileClose($hFile)
EndFunc

DumpPacketSendPrologue()
