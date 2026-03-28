#RequireAdmin
#include "lib\Froggy_Includes.au3"

; =============================================================================
; test_gwca_retval.au3
; Capture ButtonClick return value and intermediate state to diagnose failures.
; Uses the running client from test_gwca_manual_ptrs (PID=33260).
; =============================================================================

ConsoleWrite("=== GWCA Return Value Diagnostic ===" & @CRLF)

ScanAndUpdateGameClients()
If $game_clients[0][0] = 0 Then
	ConsoleWrite("ERROR: No GW" & @CRLF)
	Exit 1
EndIf
SelectClient(1)
; Don't reinit — use existing injection
Local $processHandle = GetProcessHandle()
ConsoleWrite("PID=" & $game_clients[1][0] & @CRLF)

WinActivate($game_clients[1][2])
Sleep(1000)

; Find gwca
Local $gwcaBase = 0
Local $gwPID = $game_clients[1][0]
Local $sn = DllCall('kernel32.dll', 'handle', 'CreateToolhelp32Snapshot', 'dword', 0x8, 'dword', $gwPID)
If IsArray($sn) And $sn[0] <> -1 Then
	Local $me = DllStructCreate('dword dwSize;dword th32ModuleID;dword th32ProcessID;dword GlblcntUsage;dword ProccntUsage;ptr modBaseAddr;dword modBaseSize;handle hModule;wchar szModule[256];wchar szExePath[260]')
	DllStructSetData($me, 'dwSize', DllStructGetSize($me))
	Local $r = DllCall('kernel32.dll', 'bool', 'Module32FirstW', 'handle', $sn[0], 'struct*', $me)
	While IsArray($r) And $r[0]
		If StringLower(DllStructGetData($me, 'szModule')) = "gwca.dll" Then
			$gwcaBase = Int(DllStructGetData($me, 'modBaseAddr'))
			ExitLoop
		EndIf
		$r = DllCall('kernel32.dll', 'bool', 'Module32NextW', 'handle', $sn[0], 'struct*', $me)
	WEnd
	DllCall('kernel32.dll', 'bool', 'CloseHandle', 'handle', $sn[0])
EndIf

If $gwcaBase = 0 Then
	ConsoleWrite("ERROR: gwca.dll not loaded" & @CRLF)
	Exit 1
EndIf
ConsoleWrite("gwca at 0x" & Hex($gwcaBase) & @CRLF)

; Verify +0x8A3A0 is populated
Local $hookPtr = MemoryRead($processHandle, $gwcaBase + 0x8A3A0, 'dword')
ConsoleWrite("+0x8A3A0 = 0x" & Hex($hookPtr) & @CRLF)
If $hookPtr = 0 Then
	; Populate it
	Local $gwBase = $pe_sections_ranges[0][0] - 0x1000
	Local $sendFunc = $gwBase + 0x2286D0
	ConsoleWrite("Populating +0x8A3A0 with 0x" & Hex($sendFunc) & @CRLF)
	MemoryWrite($processHandle, $gwcaBase + 0x8A3A0, $sendFunc, 'dword')
	MemoryWrite($processHandle, $gwcaBase + 0x8A39C, $sendFunc, 'dword')
EndIf

; Get play button
Local $pf = GetFrameByHash($FRAME_HASH_PLAY_BUTTON)
If $pf[0] = 0 Then
	ConsoleWrite("ERROR: Play not found" & @CRLF)
	Exit 1
EndIf
Local $fp = Int($pf[0])
ConsoleWrite("Play: 0x" & Hex($fp) & " id=" & $pf[1] & @CRLF)

; Verify rendering hook
Local $marker = _Alloc(8)
MemoryWrite($processHandle, $marker, 0x0000, 'dword')
MemoryWrite($processHandle, $marker + 4, 0x0000, 'dword')

; Build shellcode that:
; 1. Calls ButtonClick(frame_ptr)
; 2. Stores return value (AL) to marker
; 3. Stores EAX to marker+4 for more info
; 4. RET
Local $sc = _Alloc(64)
Local $buf = DllStructCreate('byte[48]')
Local $p = 1

; push frame_ptr
DllStructSetData($buf, 1, 0x68, $p)
$p += 1
_WriteLE32($buf, $p, $fp)
$p += 4

; call ButtonClick (cdecl, frame_ptr on stack)
DllStructSetData($buf, 1, 0xE8, $p)
$p += 1
_WriteLE32($buf, $p, ($gwcaBase + 0x255E0) - ($sc + $p - 1 + 4))
$p += 4

; add esp, 4 (cdecl cleanup)
DllStructSetData($buf, 1, 0x83, $p)
$p += 1
DllStructSetData($buf, 1, 0xC4, $p)
$p += 1
DllStructSetData($buf, 1, 0x04, $p)
$p += 1

; movzx eax, al (zero-extend return value)
DllStructSetData($buf, 1, 0x0F, $p)
$p += 1
DllStructSetData($buf, 1, 0xB6, $p)
$p += 1
DllStructSetData($buf, 1, 0xC0, $p)
$p += 1

; mov [marker], eax (store return value)
DllStructSetData($buf, 1, 0xA3, $p)
$p += 1
_WriteLE32($buf, $p, $marker)
$p += 4

; Also check frame_state for debug: mov eax, [frame+0x18C]; mov [marker+4], eax
DllStructSetData($buf, 1, 0xB8, $p)
$p += 1
_WriteLE32($buf, $p, $fp + 0x18C)
$p += 4
; Actually dereference it: mov eax, [imm32] uses A1
; Let me use: mov ecx, frame+0x18C; mov eax, [ecx]; mov [marker+4], eax
; Simpler: just store a known value to confirm execution
DllStructSetData($buf, 1, 0xC7, $p)
$p += 1
DllStructSetData($buf, 1, 0x05, $p)
$p += 1
_WriteLE32($buf, $p, $marker + 4)
$p += 4
_WriteLE32($buf, $p, 0xDEAD)
$p += 4

; ret
DllStructSetData($buf, 1, 0xC3, $p)

; Write and queue
DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
	'handle', $processHandle, 'ptr', Ptr($sc), _
	'ptr', DllStructGetPtr($buf), 'ulong_ptr', $p, 'ulong_ptr*', 0)
$queue_counter = MemoryRead($processHandle, GetLabel('QueueCounter'), 'dword')
Local $cmd = DllStructCreate('dword;dword')
DllStructSetData($cmd, 1, $sc)
DllStructSetData($cmd, 2, 0)
Enqueue(DllStructGetPtr($cmd), DllStructGetSize($cmd))

ConsoleWrite("Waiting 3s..." & @CRLF)
Sleep(3000)

Local $retVal = MemoryRead($processHandle, $marker, 'dword')
Local $execMarker = MemoryRead($processHandle, $marker + 4, 'dword')
ConsoleWrite("ButtonClick return: " & $retVal & " (0=false, 1=true)" & @CRLF)
ConsoleWrite("Execution marker: 0x" & Hex($execMarker) & " (want 0xDEAD)" & @CRLF)

If $execMarker <> 0xDEAD Then
	ConsoleWrite("Shellcode DID NOT EXECUTE (rendering hook issue)" & @CRLF)
ElseIf $retVal = 0 Then
	ConsoleWrite("ButtonClick returned FALSE — some internal check failed" & @CRLF)
	ConsoleWrite("Possible causes:" & @CRLF)
	ConsoleWrite("  - frame_state not created/visible" & @CRLF)
	ConsoleWrite("  - GetFrameContext returned null" & @CRLF)
	ConsoleWrite("  - Context frame_state not created" & @CRLF)
	ConsoleWrite("  - GWCA SendFrameUIMessage hook dispatch failed" & @CRLF)

	; Read frame state
	Local $fState = MemoryRead($processHandle, $fp + 0x18C, 'dword')
	ConsoleWrite("Frame state: 0x" & Hex($fState) & @CRLF)
	ConsoleWrite("  Created: " & (BitAND($fState, 0x4) <> 0) & @CRLF)
	ConsoleWrite("  Hidden: " & (BitAND($fState, 0x200) <> 0) & @CRLF)
	ConsoleWrite("  Disabled: " & (BitAND($fState, 0x10) <> 0) & @CRLF)

	; Read relation/context
	Local $relation = MemoryRead($processHandle, $fp + 0x128, 'dword')
	ConsoleWrite("Relation ptr: 0x" & Hex($relation) & @CRLF)
	If $relation > 0x10000 Then
		Local $contextState = MemoryRead($processHandle, $relation + 0x18C, 'dword')
		ConsoleWrite("Context frame_state: 0x" & Hex($contextState) & @CRLF)
	EndIf
Else
	ConsoleWrite("ButtonClick returned TRUE!" & @CRLF)
	Local $status = MemoryRead($processHandle, GetLabel('StatusCode'), 'dword')
	ConsoleWrite("StatusCode: " & $status & @CRLF)
EndIf

ConsoleWrite("=== DONE ===" & @CRLF)

Func _Alloc($sz)
	Local $m = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
		'handle', $processHandle, 'ptr', 0, 'ulong_ptr', $sz, _
		'dword', 0x1000, 'dword', 0x40)
	If Not IsArray($m) Or $m[0] = 0 Then Return 0
	Return Int($m[0])
EndFunc
