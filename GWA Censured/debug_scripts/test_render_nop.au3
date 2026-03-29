#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== Rendering Hook NOP Test ===" & @CRLF)

ScanAndUpdateGameClients()
SelectClient(1)
InitializeGameClientForGWA2(False)
Local $ph = GetProcessHandle()

ConsoleWrite("At char select: " & IsAtCharSelect() & @CRLF)
ConsoleWrite("MapIsLoaded: " & MemoryRead($ph, GetLabel('MapIsLoaded'), 'dword') & @CRLF)

; Create NOP (just RET)
Local $mem = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
    'handle', $ph, 'ptr', 0, 'ulong_ptr', 8, _
    'dword', 0x1000, 'dword', 0x40)
Local $nopAddr = Int($mem[0])
Local $retByte = DllStructCreate('byte')
DllStructSetData($retByte, 1, 0xC3)
DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
    'handle', $ph, 'ptr', Ptr($nopAddr), _
    'ptr', DllStructGetPtr($retByte), 'ulong_ptr', 1, 'ulong_ptr*', 0)

; Also dump RenderingModProc bytes to verify structure
Local $renderMod = Int(GetLabel('RenderingMod'))
Local $detBuf = DllStructCreate('byte[5]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $ph, 'ptr', Ptr($renderMod), _
    'ptr', DllStructGetPtr($detBuf), 'ulong_ptr', 5, 'ulong_ptr*', 0)
Local $jmpRel = 0
For $b = 2 To 5
    $jmpRel += DllStructGetData($detBuf, 1, $b) * (256 ^ ($b - 2))
Next
If $jmpRel >= 0x80000000 Then $jmpRel -= 0x100000000
Local $procAddr = $renderMod + 5 + $jmpRel
ConsoleWrite("RenderingModProc at: 0x" & Hex($procAddr) & @CRLF)

Local $procBuf = DllStructCreate('byte[20]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $ph, 'ptr', Ptr($procAddr), _
    'ptr', DllStructGetPtr($procBuf), 'ulong_ptr', 20, 'ulong_ptr*', 0)
Local $phex = ''
For $i = 1 To 20
    $phex &= Hex(DllStructGetData($procBuf, 1, $i), 2) & ' '
Next
ConsoleWrite("First 20 bytes: " & $phex & @CRLF)
ConsoleWrite("Expected: 83 3D xx xx xx xx 00 75 3D 60 9C A1 ..." & @CRLF)

; Queue the NOP
Local $cmd = DllStructCreate('dword;dword')
DllStructSetData($cmd, 1, $nopAddr)
DllStructSetData($cmd, 2, 0)
Enqueue(DllStructGetPtr($cmd), DllStructGetSize($cmd))
ConsoleWrite("Queued NOP (RET) at 0x" & Hex($nopAddr) & @CRLF)

; Monitor
Local $qBase = Int(GetLabel('QueueBase'))
For $i = 1 To 20
    Sleep(100)
    Local $qEntry = MemoryRead($ph, Ptr($qBase), 'dword')
    If $qEntry = 0 Then
        ConsoleWrite("Queue consumed after " & ($i * 100) & "ms!" & @CRLF)
        ExitLoop
    EndIf
Next

If MemoryRead($ph, Ptr($qBase), 'dword') <> 0 Then
    ConsoleWrite("Queue NOT consumed after 2s" & @CRLF)
Else
    ; Queue works! Now try the actual Play button click
    ConsoleWrite(@CRLF & "--- Testing Play button click ---" & @CRLF)
    Local $result = ClickFrameButton($FRAME_HASH_PLAY_BUTTON)
    ConsoleWrite("ClickFrameButton result: " & $result & @CRLF)
    Sleep(3000)
    ConsoleWrite("Still at char select: " & IsAtCharSelect() & @CRLF)
EndIf

ConsoleWrite("=== DONE ===" & @CRLF)
