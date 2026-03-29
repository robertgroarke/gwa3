#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== Fixed Rendering Hook Test ===" & @CRLF)

ScanAndUpdateGameClients()
SelectClient(1)
InitializeGameClientForGWA2(False)
Local $ph = GetProcessHandle()

ConsoleWrite("At char select: " & IsAtCharSelect() & @CRLF)
ConsoleWrite("MapIsLoaded: " & MemoryRead($ph, GetLabel('MapIsLoaded'), 'dword') & @CRLF)

; Read the JMP rel32 at RenderingMod as a signed int
Local $renderMod = Int(GetLabel('RenderingMod'))
Local $relBuf = DllStructCreate('int')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $ph, 'ptr', Ptr($renderMod + 1), _
    'ptr', DllStructGetPtr($relBuf), 'ulong_ptr', 4, 'ulong_ptr*', 0)
Local $rel32 = DllStructGetData($relBuf, 1)
Local $procAddr = $renderMod + 5 + $rel32
ConsoleWrite("RenderingMod: 0x" & Hex($renderMod, 8) & @CRLF)
ConsoleWrite("JMP rel32: " & $rel32 & @CRLF)
ConsoleWrite("RenderingModProc: 0x" & Hex($procAddr, 8) & @CRLF)

; Read RenderingModProc bytes
Local $procBuf = DllStructCreate('byte[80]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $ph, 'ptr', Ptr($procAddr), _
    'ptr', DllStructGetPtr($procBuf), 'ulong_ptr', 80, 'ulong_ptr*', 0)

Local $line = ''
For $i = 1 To 80
    $line &= Hex(DllStructGetData($procBuf, 1, $i), 2) & ' '
    If Mod($i, 16) = 0 Then
        ConsoleWrite("  " & $line & @CRLF)
        $line = ''
    EndIf
Next

ConsoleWrite(@CRLF & "Byte 1 should be 83 (cmp), NOT 83 C4 04 (add esp,4)" & @CRLF)
ConsoleWrite("If first bytes are 83 C4 04: OLD layout (queue processing is dead code)" & @CRLF)
ConsoleWrite("If first bytes are 83 3D xx: NEW layout (queue processing first)" & @CRLF)

; Queue a NOP (just C3=ret)
Local $nopMem = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
    'handle', $ph, 'ptr', 0, 'ulong_ptr', 8, _
    'dword', 0x1000, 'dword', 0x40)
Local $nopAddr = Int($nopMem[0])
Local $retByte = DllStructCreate('byte')
DllStructSetData($retByte, 1, 0xC3)
DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
    'handle', $ph, 'ptr', Ptr($nopAddr), _
    'ptr', DllStructGetPtr($retByte), 'ulong_ptr', 1, 'ulong_ptr*', 0)

Local $cmd = DllStructCreate('dword;dword')
DllStructSetData($cmd, 1, $nopAddr)
DllStructSetData($cmd, 2, 0)
Enqueue(DllStructGetPtr($cmd), DllStructGetSize($cmd))
ConsoleWrite("Queued NOP (RET) at 0x" & Hex($nopAddr, 8) & @CRLF)

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
    ConsoleWrite(@CRLF & "--- Testing Play button click ---" & @CRLF)
    ClickFrameButton($FRAME_HASH_PLAY_BUTTON)
    Sleep(3000)
    ConsoleWrite("Still at char select: " & IsAtCharSelect() & @CRLF)
EndIf

ConsoleWrite("=== DONE ===" & @CRLF)
