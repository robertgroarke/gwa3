#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== Rendering Hook Debug ===" & @CRLF)

ScanAndUpdateGameClients()
If $game_clients[0][0] = 0 Then
    ConsoleWrite("No GW clients" & @CRLF)
    Exit
EndIf

SelectClient(1)
InitializeGameClientForGWA2(False)
Local $ph = GetProcessHandle()

; Read detour at RenderingMod
Local $renderMod = Int(GetLabel('RenderingMod'))
Local $relBuf = DllStructCreate('int')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $ph, 'ptr', Ptr($renderMod + 1), _
    'ptr', DllStructGetPtr($relBuf), 'ulong_ptr', 4, 'ulong_ptr*', 0)
Local $procAddr = $renderMod + 5 + DllStructGetData($relBuf, 1)
ConsoleWrite("RenderingMod detour: 0x" & Hex($renderMod, 8) & " -> RenderingModProc: 0x" & Hex($procAddr, 8) & @CRLF)
ConsoleWrite("RenderingModReturn: " & GetLabel('RenderingModReturn') & @CRLF)

; Read 96 bytes of RenderingModProc
Local $buf = DllStructCreate('byte[96]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $ph, 'ptr', Ptr($procAddr), _
    'ptr', DllStructGetPtr($buf), 'ulong_ptr', 96, 'ulong_ptr*', 0)

ConsoleWrite("RenderingModProc bytes:" & @CRLF)
For $row = 0 To 5
    Local $line = "  " & Hex($procAddr + $row * 16, 8) & ": "
    For $i = 1 To 16
        $line &= Hex(DllStructGetData($buf, 1, $row * 16 + $i), 2) & " "
    Next
    ConsoleWrite($line & @CRLF)
Next

; Decode key instructions
ConsoleWrite(@CRLF & "=== Decoding ===" & @CRLF)

; Byte 0-6: cmp dword[MapIsLoaded],0
Local $b0 = DllStructGetData($buf, 1, 1)
ConsoleWrite("Byte 0: 0x" & Hex($b0, 2) & " (expect 0x83 = cmp)" & @CRLF)

; Byte 7-8: jnz +offset
Local $jnzOpcode = DllStructGetData($buf, 1, 8)
Local $jnzOffset = DllStructGetData($buf, 1, 9)
ConsoleWrite("Byte 7-8: 0x" & Hex($jnzOpcode, 2) & " 0x" & Hex($jnzOffset, 2) & " (expect 75 3D = jnz +61)" & @CRLF)

; What's at the jnz target (offset 9 + jnzOffset)?
Local $targetOff = 9 + $jnzOffset
ConsoleWrite("jnz target offset: " & $targetOff & " (expect 70 = add esp,4)" & @CRLF)
Local $targetByte1 = DllStructGetData($buf, 1, $targetOff + 1)
Local $targetByte2 = DllStructGetData($buf, 1, $targetOff + 2)
Local $targetByte3 = DllStructGetData($buf, 1, $targetOff + 3)
ConsoleWrite("Bytes at target: " & Hex($targetByte1, 2) & " " & Hex($targetByte2, 2) & " " & Hex($targetByte3, 2) & " (expect 83 C4 04 = add esp,4)" & @CRLF)

; Check MapIsLoaded address used in the cmp
; cmp dword[addr],0 = 83 3D <addr32> 00
Local $mapLoadedAddr = 0
For $b = 3 To 6
    $mapLoadedAddr += DllStructGetData($buf, 1, $b) * (256 ^ ($b - 3))
Next
ConsoleWrite("MapIsLoaded addr in cmp: 0x" & Hex($mapLoadedAddr, 8) & @CRLF)
ConsoleWrite("MapIsLoaded label:       " & GetLabel('MapIsLoaded') & @CRLF)

; Check MapIsLoaded value
Local $mlVal = MemoryRead($ph, GetLabel('MapIsLoaded'), 'dword')
ConsoleWrite("MapIsLoaded value: " & $mlVal & @CRLF)

; Also check what the second init would produce
ConsoleWrite(@CRLF & "=== Second Init Check ===" & @CRLF)
; Re-run the assembler to get labels
$asm_injection_size = 0
$asm_code_offset = 0
$asm_injection_string = ''
AssemblerCreateData()
AssemblerCreateMain()
AssemblerCreateRenderingMod()
; Don't need to complete — just check the size
ConsoleWrite("Injection size after RenderingMod: " & $asm_injection_size & @CRLF)

ConsoleWrite("=== DONE ===" & @CRLF)
