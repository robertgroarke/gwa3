#RequireAdmin
#include "lib\Froggy_Includes.au3"

; =============================================================================
; test_disasm_gwca.au3
; Dump raw bytes of GWCA ButtonClick, Click, MouseAction, SendFrameUIMessage
; to understand what data pointers they read and their full call chain.
; =============================================================================

ConsoleWrite("=== GWCA Function Disassembly ===" & @CRLF)

ScanAndUpdateGameClients()
If $game_clients[0][0] = 0 Then
	ConsoleWrite("ERROR: No GW clients" & @CRLF)
	Exit 1
EndIf

SelectClient(1)
InitializeGameClientForGWA2(False)
Local $processHandle = GetProcessHandle()
ConsoleWrite("PID=" & $game_clients[1][0] & @CRLF)

; Find gwca.dll
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
ConsoleWrite("gwca.dll at 0x" & Hex($gwcaBase) & @CRLF)

; Helper to dump function bytes
Func _DumpFunc($name, $addr, $len)
	ConsoleWrite(@CRLF & "=== " & $name & " @ 0x" & Hex($addr) & " ===" & @CRLF)
	Local $buf = DllStructCreate('byte[' & $len & ']')
	DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
		'handle', $processHandle, 'ptr', Ptr($addr), _
		'ptr', DllStructGetPtr($buf), 'ulong_ptr', $len, 'ulong_ptr*', 0)
	For $row = 0 To $len - 1 Step 16
		Local $hex = Hex($row, 4) & ": "
		Local $asm = ""
		For $b = 1 To 16
			If $row + $b <= $len Then
				Local $byte = DllStructGetData($buf, 1, $row + $b)
				$hex &= Hex($byte, 2) & " "
			EndIf
		Next
		ConsoleWrite($hex & @CRLF)
	Next
	Return $buf
EndFunc

; Dump all key functions (128 bytes each)
_DumpFunc("ButtonClick", $gwcaBase + 0x255E0, 128)
_DumpFunc("ButtonFrame::Click", $gwcaBase + 0x16660, 64)
_DumpFunc("ButtonFrame::MouseAction", $gwcaBase + 0x173D0, 128)
_DumpFunc("SendFrameUIMessage (GWCA wrapper)", $gwcaBase + 0x274D0, 128)
_DumpFunc("GetFrameById", $gwcaBase + 0x25CC0, 64)

; Also dump the game's SendFrameUIMsg for comparison
Local $gameSendFrame = Int(GetLabel('SendFrameUIMsg'))
_DumpFunc("Game SendFrameUIMsg", $gameSendFrame, 128)

; Read GWCA data section to see what's populated
ConsoleWrite(@CRLF & "=== GWCA Data Section ===" & @CRLF)
Local $dataOffsets[15][2] = [ _
	[0x8A39C, "SendFrameUIMsg_Orig"], _
	[0x8A3A0, "SendFrameUIMsg_Hook"], _
	[0x8A37C, "GetChildFrame"], _
	[0x8A380, "+0x8A380"], _
	[0x8A384, "+0x8A384"], _
	[0x8A388, "+0x8A388"], _
	[0x8A38C, "+0x8A38C"], _
	[0x8A390, "+0x8A390"], _
	[0x8A394, "+0x8A394"], _
	[0x8A398, "+0x8A398"], _
	[0x8A3A4, "+0x8A3A4"], _
	[0x8A3A8, "+0x8A3A8"], _
	[0x8A410, "RootFrame"], _
	[0x8A3B0, "FrameHashTable"], _
	[0x8A3D0, "SetWindowVisible"] _
]
For $i = 0 To 14
	Local $val = MemoryRead($processHandle, $gwcaBase + $dataOffsets[$i][0], 'dword')
	ConsoleWrite("  " & $dataOffsets[$i][1] & " [+" & Hex($dataOffsets[$i][0]) & "]: 0x" & Hex($val) & @CRLF)
Next

; Dump a wider range of GWCA data to find all needed pointers
ConsoleWrite(@CRLF & "=== GWCA Data Section Full Dump (0x8A370 - 0x8A420) ===" & @CRLF)
Local $dataBuf = DllStructCreate('byte[176]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
	'handle', $processHandle, 'ptr', Ptr($gwcaBase + 0x8A370), _
	'ptr', DllStructGetPtr($dataBuf), 'ulong_ptr', 176, 'ulong_ptr*', 0)
For $row = 0 To 160 Step 16
	Local $hex = Hex(0x8A370 + $row, 5) & ": "
	For $b = 1 To 16
		If $row + $b <= 176 Then
			$hex &= Hex(DllStructGetData($dataBuf, 1, $row + $b), 2) & " "
		EndIf
	Next
	ConsoleWrite($hex & @CRLF)
Next

ConsoleWrite(@CRLF & "=== DONE ===" & @CRLF)
