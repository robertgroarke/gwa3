#RequireAdmin
#include "lib\Froggy_Includes.au3"

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

; Alloc shellcode that writes DEADBEEF
Local $mem = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
    'handle', $ph, 'ptr', 0, 'ulong_ptr', 64, _
    'dword', 0x1000, 'dword', 0x40)
Local $scAddr = Int($mem[0])
Local $flagAddr = $scAddr + 32
MemoryWrite($ph, $flagAddr, 0, 'dword')

Local $sc = DllStructCreate('byte[16]')
Local $p = 1
DllStructSetData($sc, 1, 0xC7, $p)
$p += 1
DllStructSetData($sc, 1, 0x05, $p)
$p += 1
_WriteLE32($sc, $p, $flagAddr)
$p += 4
_WriteLE32($sc, $p, 0xDEADBEEF)
$p += 4
DllStructSetData($sc, 1, 0xC3, $p)
DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
    'handle', $ph, 'ptr', Ptr($scAddr), _
    'ptr', DllStructGetPtr($sc), 'ulong_ptr', $p, 'ulong_ptr*', 0)

ConsoleWrite("Shellcode at 0x" & Hex($scAddr) & @CRLF)
ConsoleWrite("QueueBase = 0x" & Hex(Int(GetLabel('QueueBase'))) & @CRLF)
ConsoleWrite("QueueCounter = 0x" & Hex(Int(GetLabel('QueueCounter'))) & @CRLF)
ConsoleWrite("RenderCmdPtr = 0x" & Hex(Int(GetLabel('RenderCmdPtr'))) & @CRLF)

; Read current counter
$queue_counter = MemoryRead($ph, GetLabel('QueueCounter'), 'dword')
ConsoleWrite("Counter before: " & $queue_counter & @CRLF)

; Write command directly to the queue slot (bypass Enqueue for testing)
Local $slotAddr = Int(GetLabel('QueueBase')) + ($queue_counter * 256)
ConsoleWrite("Writing 0x" & Hex($scAddr) & " to slot " & $queue_counter & " at 0x" & Hex($slotAddr) & @CRLF)
MemoryWrite($ph, $slotAddr, $scAddr, 'dword')

; Wait and check
For $check = 1 To 10
    Sleep(500)
    Local $qc = MemoryRead($ph, GetLabel('QueueCounter'), 'dword')
    Local $rcp = MemoryRead($ph, GetLabel('RenderCmdPtr'), 'dword')
    Local $sv = MemoryRead($ph, $slotAddr, 'dword')
    Local $flag = MemoryRead($ph, $flagAddr, 'dword')
    ConsoleWrite("  Check " & $check & ": QC=" & $qc & " RenderCmdPtr=0x" & Hex($rcp) & _
        " slot=0x" & Hex($sv) & " flag=0x" & Hex($flag) & @CRLF)
    If $flag = 0xDEADBEEF Then
        ConsoleWrite("*** SHELLCODE EXECUTED! ***" & @CRLF)
        ExitLoop
    EndIf
Next
