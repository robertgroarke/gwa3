#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== Find Parent Offset ===" & @CRLF)

ScanAndUpdateGameClients()
If $game_clients[0][0] = 0 Then Exit
SelectClient(1)
InitializeGameClientForGWA2(False)
Local $ph = GetProcessHandle()

; Find Merchant frame
Local $mf = GetFrameByHash(3613855137)
If $mf[0] = 0 Then
    ConsoleWrite("Merchant not found" & @CRLF)
    Exit
EndIf
Local $merchantPtr = Int($mf[0])
ConsoleWrite("Merchant: 0x" & Hex($merchantPtr, 8) & " fid=" & $mf[1] & @CRLF)

; Read ALL frames, check EVERY dword in the struct for pointers back to merchantPtr
Local $faAddr = Int(GetLabel('FrameArray'))
Local $faBase = MemoryRead($ph, $faAddr, 'dword')
Local $faCount = MemoryRead($ph, $faAddr + 4, 'dword')

; For speed, only check frames near the merchant (within 0x10000 address range)
ConsoleWrite("Scanning " & $faCount & " frames for any that reference merchant..." & @CRLF)

Local $found = 0
For $i = 0 To $faCount - 1
    Local $fpBytes = DllStructCreate('byte[4]')
    DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
        'handle', $ph, 'ptr', Ptr($faBase + $i * 4), _
        'ptr', DllStructGetPtr($fpBytes), 'ulong_ptr', 4, 'ulong_ptr*', 0)
    Local $fp = 0
    For $b = 0 To 3
        $fp += DllStructGetData($fpBytes, 1, $b + 1) * (2 ^ ($b * 8))
    Next
    If $fp = 0 Or $fp < 0x10000 Or $fp = $merchantPtr Then ContinueLoop

    ; Read the full frame struct and search for merchantPtr as any dword value
    Local $frameBuf = DllStructCreate('byte[456]')
    DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
        'handle', $ph, 'ptr', Ptr($fp), _
        'ptr', DllStructGetPtr($frameBuf), 'ulong_ptr', 456, 'ulong_ptr*', 0)

    For $off = 0 To 452 Step 4
        Local $val = 0
        For $b = 0 To 3
            $val += DllStructGetData($frameBuf, 1, $off + $b + 1) * (2 ^ ($b * 8))
        Next
        ; Check for merchantPtr OR merchantRelation (ptr+0x128)
        If $val = $merchantPtr Or $val = ($merchantPtr + 0x128) Then
            Local $fid = 0
            For $b = 0 To 3
                $fid += DllStructGetData($frameBuf, 1, 0xBC + $b + 1) * (2 ^ ($b * 8))
            Next
            Local $hash = 0
            For $b = 0 To 3
                $hash += DllStructGetData($frameBuf, 1, 0x134 + $b + 1) * (2 ^ ($b * 8))
            Next
            Local $childOff = 0
            For $b = 0 To 3
                $childOff += DllStructGetData($frameBuf, 1, 0xB8 + $b + 1) * (2 ^ ($b * 8))
            Next
            ConsoleWrite("  Frame 0x" & Hex($fp, 8) & " fid=" & $fid & " hash=" & $hash & _
                " childOff=" & $childOff & " has merchantPtr at offset +0x" & Hex($off, 3) & @CRLF)
            $found += 1
        EndIf
    Next

    If $found >= 20 Then ExitLoop  ; Enough
Next

ConsoleWrite("Found " & $found & " references" & @CRLF)
ConsoleWrite("=== DONE ===" & @CRLF)
