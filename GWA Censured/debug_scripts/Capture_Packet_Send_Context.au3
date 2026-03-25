#include "lib/GWA2.au3"

Func CapturePacketSendContext()
    ScanAndUpdateGameClients()
    Local $targetName = "L I L B I S C U I T"
    Local $index = FindClientIndexByCharacterName($targetName)
    If $index == -1 Then Return
    SelectClient($index)
    InitializeGameClientData(True, False)
    
    Local $logFile = @ScriptDir & "\packet_send_context.txt"
    Local $hFile = FileOpen($logFile, 2)
    
    Local $packetSendAddr = GetScannedAddress('ScanPacketSendFunction', -0x5)
    If $packetSendAddr == 0 Then
        FileWrite($hFile, "PacketSendFunction not found via Scan." & @CRLF)
        FileClose($hFile)
        Return
    EndIf
    
    FileWrite($hFile, "PacketSendFunction Address: " & Hex($packetSendAddr) & @CRLF)
    
    ; We want to find who CALLS this function.
    ; In x86, a CALL is E8 <rel32>.
    ; So we are looking for the byte sequence E8 ?? ?? ?? ?? where the target is packetSendAddr.
    ; Destination = Source + 5 + Rel32
    ; Rel32 = Destination - Source - 5
    
    ; Since we can't easily scan for "E8 <variable>", we might just have to dump a known area if we had one.
    ; But we don't.
    
    ; Plan B:
    ; The Trader code usually sends a packet.
    ; If we can trigger the "Quote Request" manually in game, and breakpoint it ... we can't breakpoint.
    
    ; Let's just dump the region around where we *thought* the Trader Function was, based on the *original* (broken) Offset.
    ; This might show us if we are just "slightly off".
    
    Local $brokenTraderAddr = GetScannedAddress('ScanTraderFunction', -0x1E) ; Original Botshub offset
    FileWrite($hFile, "Original Broken Trader Address via Scan: " & Hex($brokenTraderAddr) & @CRLF)
    
    If $brokenTraderAddr > 0 Then
         Local $bytes = MemoryRead($brokenTraderAddr - 50, 'byte[200]')
         FileWrite($hFile, "Bytes around Broken Address (-50 to +150): " & Hex($bytes) & @CRLF)
    EndIf
    
    FileClose($hFile)
EndFunc

CapturePacketSendContext()
