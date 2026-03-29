#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== Queue Diagnostic ===" & @CRLF)

ScanAndUpdateGameClients()
SelectClient(1)
InitializeGameClientForGWA2(False)
Local $ph = GetProcessHandle()

; Check queue state before
Local $qc = MemoryRead($ph, GetLabel('QueueCounter'), 'dword')
ConsoleWrite("QueueCounter before: " & $qc & @CRLF)

; Check MapIsLoaded
Local $mapLoaded = MemoryRead($ph, GetLabel('MapIsLoaded'), 'dword')
ConsoleWrite("MapIsLoaded: " & $mapLoaded & @CRLF)

; Queue a dummy command via ClickFrameButton
ConsoleWrite("Queuing ClickFrameButton..." & @CRLF)
ClickFrameButton($FRAME_HASH_PLAY_BUTTON)

; Check queue immediately after
$qc = MemoryRead($ph, GetLabel('QueueCounter'), 'dword')
ConsoleWrite("QueueCounter after queue: " & $qc & @CRLF)

; Read queue entry at current counter position
Local $qBase = Int(GetLabel('QueueBase'))
Local $qAddr = $qBase + ($qc * 256)
Local $qEntry = MemoryRead($ph, Ptr($qAddr), 'dword')
ConsoleWrite("Queue entry at counter " & $qc & ": 0x" & Hex($qEntry) & @CRLF)

; Wait and check if it gets consumed
For $i = 1 To 10
    Sleep(500)
    $qc = MemoryRead($ph, GetLabel('QueueCounter'), 'dword')
    $qEntry = MemoryRead($ph, Ptr($qAddr), 'dword')
    ConsoleWrite("  t+" & ($i * 500) & "ms: QueueCounter=" & $qc & " entry=0x" & Hex($qEntry) & @CRLF)
    If $qEntry = 0 Then
        ConsoleWrite("  Queue entry consumed! Hook is working." & @CRLF)
        ExitLoop
    EndIf
Next

If $qEntry <> 0 Then
    ConsoleWrite("Queue entry NOT consumed after 5 seconds — hook is NOT firing!" & @CRLF)
EndIf

; Also check: read 5 bytes at GameTickStart to verify detour
Local $detourBuf = DllStructCreate('byte[5]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
    'handle', $ph, 'ptr', GetLabel('GameTickStart'), _
    'ptr', DllStructGetPtr($detourBuf), 'ulong_ptr', 5, 'ulong_ptr*', 0)
Local $dHex = ''
For $i = 1 To 5
    $dHex &= Hex(DllStructGetData($detourBuf, 1, $i), 2) & ' '
Next
ConsoleWrite("Bytes at GameTickStart: " & $dHex & @CRLF)

ConsoleWrite("=== DONE ===" & @CRLF)
