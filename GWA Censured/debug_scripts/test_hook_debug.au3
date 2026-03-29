#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== Hook Debug ===" & @CRLF)

ScanAndUpdateGameClients()
SelectClient(1)
InitializeGameClientForGWA2(False)
Local $ph = GetProcessHandle()

; Check key addresses
Local $renderModAddr = Int(GetLabel('RenderingMod'))
Local $renderModRetAddr = Int(GetLabel('RenderingModReturn'))
ConsoleWrite("RenderingMod: 0x" & Hex($renderModAddr) & @CRLF)
ConsoleWrite("RenderingModReturn: 0x" & Hex($renderModRetAddr) & @CRLF)

; Read bytes at detour site — should be E9 (JMP)
Local $detBuf = DllStructCreate('byte[16]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $ph, 'ptr', Ptr($renderModAddr), _
    'ptr', DllStructGetPtr($detBuf), 'ulong_ptr', 16, 'ulong_ptr*', 0)
Local $dhex = ''
For $i = 1 To 16
    $dhex &= Hex(DllStructGetData($detBuf, 1, $i), 2) & ' '
Next
ConsoleWrite("Bytes at RenderingMod: " & $dhex & @CRLF)

; Decode the JMP target
If DllStructGetData($detBuf, 1, 1) = 0xE9 Then
    Local $jmpRel = 0
    For $b = 2 To 5
        $jmpRel += DllStructGetData($detBuf, 1, $b) * (256 ^ ($b - 2))
    Next
    If $jmpRel >= 0x80000000 Then $jmpRel -= 0x100000000
    Local $jmpTarget = $renderModAddr + 5 + $jmpRel
    ConsoleWrite("JMP target (RenderingModProc): 0x" & Hex($jmpTarget) & @CRLF)

    ; Read RenderingModProc bytes
    Local $procBuf = DllStructCreate('byte[48]')
    DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
        'handle', $ph, 'ptr', Ptr($jmpTarget), _
        'ptr', DllStructGetPtr($procBuf), 'ulong_ptr', 48, 'ulong_ptr*', 0)
    Local $phex = ''
    For $i = 1 To 48
        $phex &= Hex(DllStructGetData($procBuf, 1, $i), 2) & ' '
        If Mod($i, 16) = 0 Then $phex &= @CRLF & "  "
    Next
    ConsoleWrite("RenderingModProc bytes:" & @CRLF & "  " & $phex & @CRLF)

    ; Decode: first bytes should be: 83 C4 04 (add esp,4)
    ;                              83 3D xx xx xx xx 01 (cmp dword[DisableRendering],1)
    ;                              83 3D xx xx xx xx 00 (cmp dword[MapIsLoaded],0)
    ;                              75 3D (jnz +61)
    ConsoleWrite("Expected: 83 C4 04 83 3D .. .. .. .. 01 83 3D .. .. .. .. 00 75 3D ..." & @CRLF)
EndIf

; Check MapIsLoaded value
Local $mapLoaded = MemoryRead($ph, GetLabel('MapIsLoaded'), 'dword')
ConsoleWrite("MapIsLoaded: " & $mapLoaded & @CRLF)

; Check QueueCounter and QueueBase
Local $qcAddr = Int(GetLabel('QueueCounter'))
Local $qbAddr = Int(GetLabel('QueueBase'))
ConsoleWrite("QueueCounter addr: 0x" & Hex($qcAddr) & @CRLF)
ConsoleWrite("QueueBase addr: 0x" & Hex($qbAddr) & @CRLF)
ConsoleWrite("QueueCounter value: " & MemoryRead($ph, GetLabel('QueueCounter'), 'dword') & @CRLF)

; Also check HandleCase in MainProc
Local $mainProcAddr = Int(GetLabel('MainProc'))
ConsoleWrite("MainProc: 0x" & Hex($mainProcAddr) & @CRLF)

ConsoleWrite("=== DONE ===" & @CRLF)
