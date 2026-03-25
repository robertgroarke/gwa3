#include "lib/GWA2.au3"

Func DumpPrologue()
    ScanAndUpdateGameClients()
    Local $targetName = "L I L B I S C U I T"
    Local $index = FindClientIndexByCharacterName($targetName)
    
    If $index == -1 Then
         Local $hFile = FileOpen("prologue_dump.txt", 2)
        FileWrite($hFile, "Error: Character '" & $targetName & "' not found." & @CRLF)
        FileClose($hFile)
        Return
    EndIf
    
    SelectClient($index)
    InitializeGameClientData(True, False)
    
    Local $packetSendFunc = GetValue('PacketSendFunction')
    
    Local $hFile = FileOpen("prologue_dump.txt", 2)
    FileWrite($hFile, "PacketSendFunction Address: " & Hex($packetSendFunc) & @CRLF)
    
    If $packetSendFunc == 0 Then
        FileWrite($hFile, "Error: PacketSendFunction is 0" & @CRLF)
    Else
        Local $b1 = Hex(MemoryRead($packetSendFunc, 'dword'), 8)
        Local $b2 = Hex(MemoryRead($packetSendFunc + 4, 'dword'), 8)
        Local $b3 = Hex(MemoryRead($packetSendFunc + 8, 'dword'), 8)
        Local $b4 = Hex(MemoryRead($packetSendFunc + 12, 'dword'), 8)
        FileWrite($hFile, "Prologue Bytes: " & $b1 & " " & $b2 & " " & $b3 & " " & $b4 & @CRLF)
    EndIf
    
    FileClose($hFile)
EndFunc

DumpPrologue()
