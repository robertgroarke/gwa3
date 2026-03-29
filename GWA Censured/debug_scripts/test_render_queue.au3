#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== Rendering Hook Queue Test ===" & @CRLF)

ScanAndUpdateGameClients()
SelectClient(1)
InitializeGameClientForGWA2(False)
Local $ph = GetProcessHandle()

ConsoleWrite("MapIsLoaded: " & MemoryRead($ph, GetLabel('MapIsLoaded'), 'dword') & @CRLF)
ConsoleWrite("At char select: " & IsAtCharSelect() & @CRLF)

; First test: queue a NOP command (just RET) to verify rendering hook processes queue
ConsoleWrite(@CRLF & "--- Test 1: Verify queue processing with NOP command ---" & @CRLF)
Local $nopMem = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
    'handle', $ph, 'ptr', 0, 'ulong_ptr', 8, _
    'dword', 0x1000, 'dword', 0x40)
Local $nopAddr = Int($nopMem[0])
; Write just a RET instruction
Local $retInstr = DllStructCreate('byte')
DllStructSetData($retInstr, 1, 0xC3)
DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
    'handle', $ph, 'ptr', Ptr($nopAddr), _
    'ptr', DllStructGetPtr($retInstr), 'ulong_ptr', 1, 'ulong_ptr*', 0)

; Queue it
Local $cmd = DllStructCreate('dword;dword')
DllStructSetData($cmd, 1, $nopAddr)
DllStructSetData($cmd, 2, 0)
Enqueue(DllStructGetPtr($cmd), DllStructGetSize($cmd))
ConsoleWrite("Queued NOP at 0x" & Hex($nopAddr) & @CRLF)

; Check queue entry
Local $qBase = Int(GetLabel('QueueBase'))
Local $qc = MemoryRead($ph, GetLabel('QueueCounter'), 'dword')
Local $qEntry = MemoryRead($ph, Ptr($qBase), 'dword')
ConsoleWrite("QueueCounter=" & $qc & " entry[0]=0x" & Hex($qEntry) & @CRLF)

For $i = 1 To 10
    Sleep(200)
    $qEntry = MemoryRead($ph, Ptr($qBase), 'dword')
    If $qEntry = 0 Then
        ConsoleWrite("  NOP consumed after " & ($i * 200) & "ms - rendering hook works!" & @CRLF)
        ExitLoop
    EndIf
Next
If $qEntry <> 0 Then
    ConsoleWrite("  NOP NOT consumed - rendering hook not processing queue!" & @CRLF)
    Exit
EndIf

; Test 2: Try clicking Play with ECX = context + 0xA8 (GWCA style)
ConsoleWrite(@CRLF & "--- Test 2: Click Play with ECX=context+0xA8 ---" & @CRLF)

Local $result = GetFrameByHash($FRAME_HASH_PLAY_BUTTON)
If $result[0] = 0 Then
    ConsoleWrite("Play button not found" & @CRLF)
    Exit
EndIf

Local $framePtr = Int($result[0])
Local $frameId = $result[1]
Local $childOffsetId = MemoryRead($ph, $framePtr + 0xB8, 'dword')
Local $context = _GetFrameContext($framePtr)
Local $sendFunc = Int(GetLabel('SendFrameUIMsg'))

ConsoleWrite("Frame id=" & $frameId & " childOffset=" & $childOffsetId & @CRLF)
ConsoleWrite("Context=0x" & Hex($context) & @CRLF)

; Allocate shellcode + data
Local $mem = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
    'handle', $ph, 'ptr', 0, 'ulong_ptr', 64, _
    'dword', 0x1000, 'dword', 0x40)
Local $scAddr = Int($mem[0])
Local $dataAddr = $scAddr + 32

; Try ECX = context + 0xA8 (no adjustment)
Local $thisPtr = $context + 0xA8
ConsoleWrite("thisPtr (ECX) = 0x" & Hex($thisPtr) & @CRLF)

; Write action data
Local $ad = DllStructCreate('dword;dword;dword;dword;dword')
DllStructSetData($ad, 1, $frameId)
DllStructSetData($ad, 2, $childOffsetId)
DllStructSetData($ad, 3, 0x7)  ; MouseUp only
DllStructSetData($ad, 4, 0)
DllStructSetData($ad, 5, 0)
DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
    'handle', $ph, 'ptr', Ptr($dataAddr), _
    'ptr', DllStructGetPtr($ad), 'ulong_ptr', 20, 'ulong_ptr*', 0)

; Build shellcode
Local $sc = DllStructCreate('byte[24]')
Local $p = 1
DllStructSetData($sc, 1, 0xB9, $p) : $p += 1
_WriteLE32($sc, $p, $thisPtr) : $p += 4
DllStructSetData($sc, 1, 0x6A, $p) : $p += 1
DllStructSetData($sc, 1, 0x00, $p) : $p += 1
DllStructSetData($sc, 1, 0x68, $p) : $p += 1
_WriteLE32($sc, $p, $dataAddr) : $p += 4
DllStructSetData($sc, 1, 0x6A, $p) : $p += 1
DllStructSetData($sc, 1, 0x31, $p) : $p += 1
DllStructSetData($sc, 1, 0xE8, $p) : $p += 1
_WriteLE32($sc, $p, $sendFunc - ($scAddr + $p - 1 + 4)) : $p += 4
DllStructSetData($sc, 1, 0xC3, $p)

; Dump shellcode
Local $hex = ""
For $b = 1 To $p
    $hex &= Hex(DllStructGetData($sc, 1, $b), 2) & " "
Next
ConsoleWrite("Shellcode: " & $hex & @CRLF)

; Verify call target
Local $e8pos = 15
Local $rel32 = $sendFunc - ($scAddr + $e8pos - 1 + 4)
ConsoleWrite("Call target: 0x" & Hex($scAddr + $e8pos - 1 + 4 + $rel32) & " (expected 0x" & Hex($sendFunc) & ")" & @CRLF)

; Write shellcode
DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
    'handle', $ph, 'ptr', Ptr($scAddr), _
    'ptr', DllStructGetPtr($sc), 'ulong_ptr', $p, 'ulong_ptr*', 0)

; Queue via Enqueue
Local $cmd2 = DllStructCreate('dword;dword')
DllStructSetData($cmd2, 1, $scAddr)
DllStructSetData($cmd2, 2, 0)
Enqueue(DllStructGetPtr($cmd2), DllStructGetSize($cmd2))
ConsoleWrite("Queued shellcode" & @CRLF)

Sleep(2000)
ConsoleWrite("Still at char select: " & IsAtCharSelect() & @CRLF)

If Not IsAtCharSelect() Then
    ConsoleWrite("SUCCESS with ECX=context+0xA8!" & @CRLF)
Else
    ConsoleWrite("No transition with ECX=context+0xA8" & @CRLF)
EndIf

ConsoleWrite("=== DONE ===" & @CRLF)
