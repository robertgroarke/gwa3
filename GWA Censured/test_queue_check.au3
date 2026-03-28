#RequireAdmin
#include "lib\Froggy_Includes.au3"

; Quick queue diagnostic — tries to use MoveTo to verify hooks work
ScanAndUpdateGameClients()
ConsoleWrite("Clients: " & $game_clients[0][0] & @CRLF)
For $i = 1 To $game_clients[0][0]
	ConsoleWrite("  " & $i & ": PID=" & $game_clients[$i][0] & " title='" & $game_clients[$i][1] & "'" & @CRLF)
Next

If $game_clients[0][0] = 0 Then Exit 1

; Try each client
For $ci = 1 To $game_clients[0][0]
	ConsoleWrite(@CRLF & "=== Testing client " & $ci & " PID=" & $game_clients[$ci][0] & " ===" & @CRLF)
	SelectClient($ci)
	InitializeGameClientForGWA2(False)
	Local $ph = GetProcessHandle()
	ConsoleWrite("MyID=" & GetMyID() & " MapID=" & GetMapID() & @CRLF)
	ConsoleWrite("IsAtCharSelect=" & IsAtCharSelect() & @CRLF)

	; Test queue with marker
	Local $m = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
		'handle', $ph, 'ptr', 0, 'ulong_ptr', 4, 'dword', 0x1000, 'dword', 0x40)
	If Not IsArray($m) Or $m[0] = 0 Then ContinueLoop
	Local $ma = Int($m[0])
	MemoryWrite($ph, $ma, 0xDEAD, 'dword')

	Local $sc = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
		'handle', $ph, 'ptr', 0, 'ulong_ptr', 16, 'dword', 0x1000, 'dword', 0x40)
	Local $sa = Int($sc[0])
	Local $b = DllStructCreate('byte[16]')
	Local $p = 1
	DllStructSetData($b, 1, 0xC7, $p)
	$p += 1
	DllStructSetData($b, 1, 0x05, $p)
	$p += 1
	_WriteLE32($b, $p, $ma)
	$p += 4
	_WriteLE32($b, $p, 0xBEEF)
	$p += 4
	DllStructSetData($b, 1, 0xC3, $p)
	DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
		'handle', $ph, 'ptr', Ptr($sa), 'ptr', DllStructGetPtr($b), 'ulong_ptr', $p, 'ulong_ptr*', 0)

	$queue_counter = MemoryRead($ph, GetLabel('QueueCounter'), 'dword')
	ConsoleWrite("QueueCounter before: " & $queue_counter & @CRLF)
	Local $c = DllStructCreate('dword;dword')
	DllStructSetData($c, 1, $sa)
	DllStructSetData($c, 2, 0)
	Enqueue(DllStructGetPtr($c), DllStructGetSize($c))

	Sleep(3000)
	Local $qc2 = MemoryRead($ph, GetLabel('QueueCounter'), 'dword')
	Local $mv = MemoryRead($ph, $ma, 'dword')
	ConsoleWrite("QueueCounter after: " & $qc2 & @CRLF)
	ConsoleWrite("Marker: 0x" & Hex($mv) & " (want 0xBEEF)" & @CRLF)

	; Check for gwca.dll
	Local $hasGwca = False
	Local $sn = DllCall('kernel32.dll', 'handle', 'CreateToolhelp32Snapshot', 'dword', 0x8, 'dword', $game_clients[$ci][0])
	If IsArray($sn) And $sn[0] <> -1 Then
		Local $me = DllStructCreate('dword dwSize;dword th32ModuleID;dword th32ProcessID;dword GlblcntUsage;dword ProccntUsage;ptr modBaseAddr;dword modBaseSize;handle hModule;wchar szModule[256];wchar szExePath[260]')
		DllStructSetData($me, 'dwSize', DllStructGetSize($me))
		Local $r = DllCall('kernel32.dll', 'bool', 'Module32FirstW', 'handle', $sn[0], 'struct*', $me)
		While IsArray($r) And $r[0]
			If StringLower(DllStructGetData($me, 'szModule')) = "gwca.dll" Then
				$hasGwca = True
				ExitLoop
			EndIf
			$r = DllCall('kernel32.dll', 'bool', 'Module32NextW', 'handle', $sn[0], 'struct*', $me)
		WEnd
		DllCall('kernel32.dll', 'bool', 'CloseHandle', 'handle', $sn[0])
	EndIf
	ConsoleWrite("gwca.dll loaded: " & $hasGwca & @CRLF)
	ConsoleWrite("RESULT: " & ($mv = 0xBEEF ? "QUEUE WORKS" : "QUEUE BROKEN") & @CRLF)
Next
