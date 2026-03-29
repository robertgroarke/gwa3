#RequireAdmin
#include "lib\Froggy_Includes.au3"

ConsoleWrite("=== Verify Rendering Hook Executes Shellcode ===" & @CRLF)

ScanAndUpdateGameClients()
If $game_clients[0][0] = 0 Then
    Local $accounts = GWLauncher_LoadAccounts()
    Local $idx = GWLauncher_FindAccountByCharacter($accounts, "D I S C O P A N I C")
    GWLauncher_LaunchAccount($accounts, $idx)
    Sleep(20000)
    ScanAndUpdateGameClients()
EndIf
SelectClient($game_clients[0][0])
InitializeGameClientForGWA2(False)
Local $ph = GetProcessHandle()

; Alloc a simple test shellcode that just writes 0xDEADBEEF to a known address
Local $mem = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
    'handle', $ph, 'ptr', 0, 'ulong_ptr', 64, _
    'dword', 0x1000, 'dword', 0x40)
Local $scAddr = Int($mem[0])
Local $flagAddr = $scAddr + 32

; Write 0 to flag
MemoryWrite($ph, $flagAddr, 0, 'dword')

; Shellcode: mov dword [flagAddr], 0xDEADBEEF; ret
Local $sc = DllStructCreate('byte[16]')
Local $p = 1
; C7 05 <addr> EF BE AD DE = mov [addr], 0xDEADBEEF
DllStructSetData($sc, 1, 0xC7, $p)
$p += 1
DllStructSetData($sc, 1, 0x05, $p)
$p += 1
_WriteLE32($sc, $p, $flagAddr)
$p += 4
_WriteLE32($sc, $p, 0xDEADBEEF)
$p += 4
; C3 = ret
DllStructSetData($sc, 1, 0xC3, $p)

DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
    'handle', $ph, 'ptr', Ptr($scAddr), _
    'ptr', DllStructGetPtr($sc), 'ulong_ptr', $p, 'ulong_ptr*', 0)

ConsoleWrite("Shellcode at 0x" & Hex($scAddr) & " flag at 0x" & Hex($flagAddr) & @CRLF)

; Sync and queue
$queue_counter = MemoryRead($ph, GetLabel('QueueCounter'), 'dword')
Local $cmd = DllStructCreate('dword;dword')
DllStructSetData($cmd, 1, $scAddr)
DllStructSetData($cmd, 2, 0)
Enqueue(DllStructGetPtr($cmd), DllStructGetSize($cmd))
ConsoleWrite("Queued at counter " & $queue_counter & @CRLF)

Sleep(500)

; Check SavedIndex to see if command addr was saved there
Local $savedIdx = MemoryRead($ph, GetLabel('SavedIndex'), 'dword')
ConsoleWrite("SavedIndex value = 0x" & Hex($savedIdx) & " (expect 0x" & Hex($scAddr) & ")" & @CRLF)

; Check if queue slot was consumed
Local $qcNow = MemoryRead($ph, GetLabel('QueueCounter'), 'dword')
ConsoleWrite("QueueCounter now = " & $qcNow & @CRLF)

; Check the queue slot we wrote to
Local $slotAddr = Int(GetLabel('QueueBase')) + (($queue_counter - 1) * 256)
Local $slotVal = MemoryRead($ph, $slotAddr, 'dword')
ConsoleWrite("Queue slot[" & ($queue_counter-1) & "] = 0x" & Hex($slotVal) & " (0=consumed)" & @CRLF)

Sleep(1500)

Local $flag = MemoryRead($ph, $flagAddr, 'dword')
ConsoleWrite("Flag = 0x" & Hex($flag) & " (expect 0xDEADBEEF if executed)" & @CRLF)

If $flag = 0xDEADBEEF Then
    ConsoleWrite("*** SHELLCODE EXECUTED SUCCESSFULLY! ***" & @CRLF)
Else
    ConsoleWrite("Shellcode did NOT execute" & @CRLF)
EndIf
