#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== HandleCase Queue Test ===" & @CRLF)

ScanAndUpdateGameClients()
SelectClient(1)
InitializeGameClientForGWA2(False)
Local $ph = GetProcessHandle()

ConsoleWrite("At char select: " & IsAtCharSelect() & @CRLF)
ConsoleWrite("MapIsLoaded: " & MemoryRead($ph, GetLabel('MapIsLoaded'), 'dword') & @CRLF)

; Create a NOP command that just jumps to CommandReturn
Local $cmdRetAddr = Int(GetLabel('CommandReturn'))
ConsoleWrite("CommandReturn: 0x" & Hex($cmdRetAddr) & @CRLF)

Local $mem = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
    'handle', $ph, 'ptr', 0, 'ulong_ptr', 8, _
    'dword', 0x1000, 'dword', 0x40)
Local $nopAddr = Int($mem[0])

; Write: E9 <rel32 to CommandReturn>
Local $nopSc = DllStructCreate('byte[5]')
DllStructSetData($nopSc, 1, 0xE9, 1)
_WriteLE32($nopSc, 2, $cmdRetAddr - ($nopAddr + 5))
DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
    'handle', $ph, 'ptr', Ptr($nopAddr), _
    'ptr', DllStructGetPtr($nopSc), 'ulong_ptr', 5, 'ulong_ptr*', 0)
ConsoleWrite("NOP shellcode at 0x" & Hex($nopAddr) & " (jmp CommandReturn)" & @CRLF)

; Queue it
Local $cmd = DllStructCreate('dword;dword')
DllStructSetData($cmd, 1, $nopAddr)
DllStructSetData($cmd, 2, 0)
Enqueue(DllStructGetPtr($cmd), DllStructGetSize($cmd))

; Monitor queue consumption
Local $qBase = Int(GetLabel('QueueBase'))
For $i = 1 To 20
    Sleep(100)
    Local $qEntry = MemoryRead($ph, Ptr($qBase), 'dword')
    Local $qc = MemoryRead($ph, GetLabel('QueueCounter'), 'dword')
    If $qEntry = 0 Then
        ConsoleWrite("  Queue consumed after " & ($i * 100) & "ms! QueueCounter=" & $qc & @CRLF)
        ExitLoop
    EndIf
Next

If MemoryRead($ph, Ptr($qBase), 'dword') <> 0 Then
    ConsoleWrite("  Queue NOT consumed after 2s — MainProc HandleCase not firing!" & @CRLF)

    ; Let's check BasePointer chain to see why
    Local $bp = MemoryRead($ph, GetLabel('BasePointer'), 'ptr')
    ConsoleWrite("  BasePointer addr: " & GetLabel('BasePointer') & @CRLF)
    ConsoleWrite("  BasePointer value: 0x" & Hex(Int($bp)) & @CRLF)
    If $bp <> 0 Then
        Local $v1 = MemoryRead($ph, $bp, 'ptr')
        ConsoleWrite("  [BasePointer]: 0x" & Hex(Int($v1)) & @CRLF)
        If $v1 <> 0 Then
            Local $v2 = MemoryRead($ph, Int($v1) + 0x18, 'ptr')
            ConsoleWrite("  [[BP]+18]: 0x" & Hex(Int($v2)) & @CRLF)
            If $v2 <> 0 Then
                Local $v3 = MemoryRead($ph, Int($v2) + 0x44, 'ptr')
                ConsoleWrite("  [[[BP]+18]+44]: 0x" & Hex(Int($v3)) & @CRLF)
                If $v3 <> 0 Then
                    Local $v198 = MemoryRead($ph, Int($v3) + 0x198, 'dword')
                    Local $v19c = MemoryRead($ph, Int($v3) + 0x19C, 'dword')
                    ConsoleWrite("  [[...]+198] (map): " & $v198 & @CRLF)
                    ConsoleWrite("  [[...]+19C]: " & $v19c & @CRLF)
                EndIf
            EndIf
        EndIf
    EndIf
EndIf

ConsoleWrite("=== DONE ===" & @CRLF)
