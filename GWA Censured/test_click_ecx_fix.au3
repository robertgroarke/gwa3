#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

; =============================================================================
; test_click_ecx_fix.au3
;
; HYPOTHESIS: The ECX value for SendFrameUIMsg is wrong.
;
; Scan pattern: 83 C1 DC E8 = "add ecx,-0x24; call SendFrameUIMsg"
; This means the GAME caller adjusts ECX before calling:
;   ECX_at_call = frame+0xA8 - 0x24 = frame+0x84
;
; But our code was passing ECX = frame+0xA8 (wrong!).
;
; This test tries multiple ECX values with the game's own SendFrameUIMsg
; on a fresh client without GWCA (to avoid hook conflicts).
; =============================================================================

ConsoleWrite("=== ECX Fix Click Test ===" & @CRLF)

; --- Launch fresh BEASTRIT or find running one ---
ScanAndUpdateGameClients()
Local $targetIdx = -1
For $i = 1 To $game_clients[0][0]
	ConsoleWrite("  Client " & $i & ": PID=" & $game_clients[$i][0] & " title='" & $game_clients[$i][1] & "'" & @CRLF)
	If StringInStr($game_clients[$i][1], "B E A S T R I T") Or StringInStr($game_clients[$i][1], "BEASTRIT") Then
		$targetIdx = $i
	EndIf
Next

If $targetIdx = -1 And $game_clients[0][0] > 0 Then
	; Use first client without gwca.dll
	For $i = 1 To $game_clients[0][0]
		Local $hasGWCA = _CheckModule($game_clients[$i][0], "gwca.dll")
		If Not $hasGWCA Then
			$targetIdx = $i
			ConsoleWrite("Using client " & $i & " (no gwca.dll)" & @CRLF)
			ExitLoop
		EndIf
	Next
EndIf

If $targetIdx = -1 Then
	ConsoleWrite("Launching fresh BEASTRIT..." & @CRLF)
	Local $accounts = GWLauncher_LoadAccounts()
	Local $accIdx = GWLauncher_FindAccountByCharacter($accounts, "B E A S T R I T")
	If $accIdx = -1 Then
		ConsoleWrite("ERROR: Account not found" & @CRLF)
		Exit 1
	EndIf
	Local $result = GWLauncher_LaunchAccount($accounts, $accIdx)
	If $result = 0 Then
		ConsoleWrite("ERROR: Launch failed" & @CRLF)
		Exit 1
	EndIf
	ConsoleWrite("PID=" & $result[0] & ", waiting 40s..." & @CRLF)
	Sleep(40000)
	ScanAndUpdateGameClients()
	For $i = 1 To $game_clients[0][0]
		If $game_clients[$i][0] = $result[0] Then
			$targetIdx = $i
			ExitLoop
		EndIf
	Next
	If $targetIdx = -1 And $game_clients[0][0] > 0 Then
		$targetIdx = $game_clients[0][0]
	EndIf
	If $targetIdx = -1 Then
		ConsoleWrite("ERROR: No clients" & @CRLF)
		Exit 1
	EndIf
EndIf

SelectClient($targetIdx)
InitializeGameClientForGWA2(False)
Local $processHandle = GetProcessHandle()
Local $gwPID = $game_clients[$targetIdx][0]
Local $gwHWnd = $game_clients[$targetIdx][2]
ConsoleWrite("Connected PID=" & $gwPID & @CRLF)

; Activate window for rendering
WinActivate($gwHWnd)
Sleep(2000)

; Verify rendering hook with diagnostic shellcode
; Write shellcode that sets a marker value, not just RET
Local $marker = _Alloc(4)
MemoryWrite($processHandle, $marker, 0xDEAD, 'dword')

Local $diagSc = _Alloc(32)
Local $diagBuf = DllStructCreate('byte[32]')
Local $dp = 1
; mov dword [marker], 0xBEEF
DllStructSetData($diagBuf, 1, 0xC7, $dp)
$dp += 1
DllStructSetData($diagBuf, 1, 0x05, $dp)
$dp += 1
_WriteLE32($diagBuf, $dp, $marker)
$dp += 4
_WriteLE32($diagBuf, $dp, 0xBEEF)
$dp += 4
; ret
DllStructSetData($diagBuf, 1, 0xC3, $dp)
DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
	'handle', $processHandle, 'ptr', Ptr($diagSc), _
	'ptr', DllStructGetPtr($diagBuf), 'ulong_ptr', $dp, 'ulong_ptr*', 0)

; Sync queue counter and enqueue
Local $pre = MemoryRead($processHandle, GetLabel('QueueCounter'), 'dword')
$queue_counter = $pre
ConsoleWrite("QueueCounter=" & $pre & " QueueBase=0x" & Hex(Int(GetLabel('QueueBase'))) & @CRLF)

Local $diagCmd = DllStructCreate('dword;dword')
DllStructSetData($diagCmd, 1, $diagSc)
DllStructSetData($diagCmd, 2, 0)
Enqueue(DllStructGetPtr($diagCmd), DllStructGetSize($diagCmd))

; Wait and check multiple times
Local $hookWorked = False
For $wait = 1 To 5
	Sleep(1000)
	Local $markerVal = MemoryRead($processHandle, $marker, 'dword')
	Local $post = MemoryRead($processHandle, GetLabel('QueueCounter'), 'dword')
	ConsoleWrite("  Check " & $wait & ": marker=0x" & Hex($markerVal) & " counter=" & $post & @CRLF)
	If $markerVal = 0xBEEF Or $post <> $pre Then
		$hookWorked = True
		ExitLoop
	EndIf
	; Re-activate window each try
	WinActivate($gwHWnd)
Next

If Not $hookWorked Then
	; Try MainProc path too — maybe HandleCase processes the queue
	ConsoleWrite("Rendering hook not active. Checking MainProc..." & @CRLF)
	; Read the queue slot to see if our command is still there
	Local $slotAddr = Int(GetLabel('QueueBase')) + (256 * $pre)
	Local $slotVal = MemoryRead($processHandle, $slotAddr, 'dword')
	ConsoleWrite("Queue slot " & $pre & " at 0x" & Hex($slotAddr) & " = 0x" & Hex($slotVal) & @CRLF)
	ConsoleWrite("ERROR: No hook is processing the queue" & @CRLF)
	Exit 1
EndIf
ConsoleWrite("Rendering hook ACTIVE" & @CRLF)

; Verify char select
ConsoleWrite("IsAtCharSelect: " & IsAtCharSelect() & @CRLF)

; Get Play button info
Local $playFrame = GetFrameByHash($FRAME_HASH_PLAY_BUTTON)
If $playFrame[0] = 0 Then
	ConsoleWrite("ERROR: Play button not found" & @CRLF)
	Exit 1
EndIf
Local $fp = Int($playFrame[0])
Local $fid = $playFrame[1]
Local $coff = MemoryRead($processHandle, $fp + $FRAME_OFFSET_CHILD_OFFSET_ID, 'dword')
Local $sendFunc = Int(GetLabel('SendFrameUIMsg'))
ConsoleWrite("Play: ptr=0x" & Hex($fp) & " id=" & $fid & " childOff=" & $coff & @CRLF)
ConsoleWrite("SendFrameUIMsg: 0x" & Hex($sendFunc) & @CRLF)

; Read the frame's callback array details
Local $cbBuf = MemoryRead($processHandle, $fp + 0xA8, 'dword')
Local $cbSize = MemoryRead($processHandle, $fp + 0xAC, 'dword')
ConsoleWrite("Callbacks: buf=0x" & Hex($cbBuf) & " size=" & $cbSize & @CRLF)

; Dump 16 bytes around the scan hit to confirm the calling convention
; Pattern was found at some offset; let's re-scan to see the calling context
ConsoleWrite(@CRLF & "=== Examining call site ===" & @CRLF)
; Read 32 bytes before and after the pattern location
; The pattern 83 C1 DC E8 is at the call site where the game calls SendFrameUIMsg
; Let's find it
Local $textStart = $pe_sections_ranges[0][0]
Local $textEnd = $pe_sections_ranges[0][1]
Local $callSite = 0
Local $chunkSize = 65536
For $offset = 0 To ($textEnd - $textStart - 8) Step $chunkSize
	Local $readSize = $chunkSize + 8
	If $offset + $readSize > ($textEnd - $textStart) Then $readSize = ($textEnd - $textStart) - $offset
	Local $chunk = DllStructCreate('byte[' & $readSize & ']')
	DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
		'handle', $processHandle, 'ptr', Ptr($textStart + $offset), _
		'ptr', DllStructGetPtr($chunk), 'ulong_ptr', $readSize, 'ulong_ptr*', 0)
	For $j = 1 To $readSize - 7
		If DllStructGetData($chunk, 1, $j) = 0x83 And _
		   DllStructGetData($chunk, 1, $j+1) = 0xC1 And _
		   DllStructGetData($chunk, 1, $j+2) = 0xDC And _
		   DllStructGetData($chunk, 1, $j+3) = 0xE8 Then
			$callSite = $textStart + $offset + ($j - 1)
			ExitLoop 2
		EndIf
	Next
Next

If $callSite <> 0 Then
	ConsoleWrite("Call site at 0x" & Hex($callSite) & @CRLF)
	; Read 32 bytes before and 16 after to see full context
	Local $ctx = DllStructCreate('byte[64]')
	DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
		'handle', $processHandle, 'ptr', Ptr($callSite - 32), _
		'ptr', DllStructGetPtr($ctx), 'ulong_ptr', 64, 'ulong_ptr*', 0)
	ConsoleWrite("Context (call site at offset +32):" & @CRLF)
	For $row = 0 To 48 Step 16
		Local $hex = Hex($row, 2) & ": "
		For $b = 1 To 16
			If $row + $b <= 64 Then
				$hex &= Hex(DllStructGetData($ctx, 1, $row + $b), 2) & " "
			EndIf
		Next
		ConsoleWrite("  " & $hex & @CRLF)
	Next
EndIf

; Screenshot before
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\ecx_fix_before.png', $gwHWnd)

; =============================================================================
; Build action data (shared across all approaches)
; =============================================================================
Local $actionMem = _Alloc(64)

; =============================================================================
; TEST A: ECX = frame+0x84 (the -0x24 adjusted value)
; This is what the game actually passes after add ecx,-0x24
; =============================================================================
ConsoleWrite(@CRLF & "=== TEST A: ECX=frame+0x84, msg=0x2F, action(6+7) ===" & @CRLF)
_TrySendFrame($fp + 0x84, 0x2F, $fid, $coff, $actionMem, $sendFunc)
If _Check("ECX=frame+0x84, msg=0x2F") Then Exit 0

; =============================================================================
; TEST B: ECX = frame+0xA8 (original assumption — callbacks array ptr)
; =============================================================================
ConsoleWrite(@CRLF & "=== TEST B: ECX=frame+0xA8, msg=0x2F, action(6+7) ===" & @CRLF)
_TrySendFrame($fp + 0xA8, 0x2F, $fid, $coff, $actionMem, $sendFunc)
If _Check("ECX=frame+0xA8, msg=0x2F") Then Exit 0

; =============================================================================
; TEST C: ECX = callbacks buffer pointer directly
; =============================================================================
ConsoleWrite(@CRLF & "=== TEST C: ECX=cbBuf(0x" & Hex($cbBuf) & "), msg=0x2F ===" & @CRLF)
If $cbBuf > 0x10000 Then
	_TrySendFrame($cbBuf, 0x2F, $fid, $coff, $actionMem, $sendFunc)
	If _Check("ECX=cbBuf, msg=0x2F") Then Exit 0
EndIf

; =============================================================================
; TEST D: ECX = frame+0x84 with msg=0x31 (kMouseClick2)
; =============================================================================
ConsoleWrite(@CRLF & "=== TEST D: ECX=frame+0x84, msg=0x31 ===" & @CRLF)
_TrySendFrame($fp + 0x84, 0x31, $fid, $coff, $actionMem, $sendFunc)
If _Check("ECX=frame+0x84, msg=0x31") Then Exit 0

; =============================================================================
; TEST E: ECX = frame ptr itself (maybe function does its own offset calc)
; =============================================================================
ConsoleWrite(@CRLF & "=== TEST E: ECX=frame_ptr, msg=0x2F ===" & @CRLF)
_TrySendFrame($fp, 0x2F, $fid, $coff, $actionMem, $sendFunc)
If _Check("ECX=frame_ptr, msg=0x2F") Then Exit 0

; =============================================================================
; TEST F: ECX = frame+0x84, msg=0x22 (kMouseClick)
; =============================================================================
ConsoleWrite(@CRLF & "=== TEST F: ECX=frame+0x84, msg=0x22 ===" & @CRLF)
_TrySendFrame($fp + 0x84, 0x22, $fid, $coff, $actionMem, $sendFunc)
If _Check("ECX=frame+0x84, msg=0x22") Then Exit 0

; =============================================================================
; TEST G: ECX = frame+0x84, msg=0x2F, wParam=NULL (no action struct)
; =============================================================================
ConsoleWrite(@CRLF & "=== TEST G: ECX=frame+0x84, msg=0x2F, wParam=NULL ===" & @CRLF)
For $act = 6 To 7
	Local $scG = _Alloc(32)
	Local $bG = DllStructCreate('byte[32]')
	Local $pG = 1
	; mov ecx, frame+0x84
	DllStructSetData($bG, 1, 0xB9, $pG)
	$pG += 1
	_WriteLE32($bG, $pG, $fp + 0x84)
	$pG += 4
	; push 0 (lParam)
	DllStructSetData($bG, 1, 0x6A, $pG)
	$pG += 1
	DllStructSetData($bG, 1, 0x00, $pG)
	$pG += 1
	; push 0 (wParam = NULL)
	DllStructSetData($bG, 1, 0x6A, $pG)
	$pG += 1
	DllStructSetData($bG, 1, 0x00, $pG)
	$pG += 1
	; push 0x2F
	DllStructSetData($bG, 1, 0x6A, $pG)
	$pG += 1
	DllStructSetData($bG, 1, 0x2F, $pG)
	$pG += 1
	; call [sendFuncStore]
	Local $sfG = $scG + 28
	MemoryWrite($processHandle, $sfG, $sendFunc, 'dword')
	DllStructSetData($bG, 1, 0xFF, $pG)
	$pG += 1
	DllStructSetData($bG, 1, 0x15, $pG)
	$pG += 1
	_WriteLE32($bG, $pG, $sfG)
	$pG += 4
	; ret
	DllStructSetData($bG, 1, 0xC3, $pG)
	_WriteQueue($scG, $bG, $pG)
	Sleep(50)
Next
If _Check("ECX=frame+0x84, wParam=NULL") Then Exit 0

; --- Final ---
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\ecx_fix_after.png', $gwHWnd)
ConsoleWrite(@CRLF & "=== ALL TESTS FAILED ===" & @CRLF)

; Check if game crashed
Local $ec = DllCall($kernel_handle, 'bool', 'GetExitCodeProcess', 'handle', $processHandle, 'dword*', 0)
If IsArray($ec) And $ec[2] = 259 Then
	ConsoleWrite("Game still alive" & @CRLF)
Else
	ConsoleWrite("GAME CRASHED (exit=" & $ec[2] & ")" & @CRLF)
EndIf

ConsoleWrite("=== DONE ===" & @CRLF)

; =============================================================================
; Helpers
; =============================================================================

Func _TrySendFrame($ecxVal, $msgId, $frameId, $childOff, $actionMem, $funcAddr)
	; Do MouseDown (6) then MouseUp (7)
	For $act = 6 To 7
		; Write action struct
		Local $ad = DllStructCreate('dword[5]')
		DllStructSetData($ad, 1, $frameId, 1)
		DllStructSetData($ad, 1, $childOff, 2)
		DllStructSetData($ad, 1, $act, 3)
		DllStructSetData($ad, 1, 0, 4)
		DllStructSetData($ad, 1, 0, 5)
		DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
			'handle', $processHandle, 'ptr', Ptr($actionMem), _
			'ptr', DllStructGetPtr($ad), 'ulong_ptr', 20, 'ulong_ptr*', 0)

		Local $sc = _Alloc(64)
		Local $funcStore = $sc + 56
		MemoryWrite($processHandle, $funcStore, $funcAddr, 'dword')

		Local $buf = DllStructCreate('byte[48]')
		Local $pp = 1
		; mov ecx, ecxVal
		DllStructSetData($buf, 1, 0xB9, $pp)
		$pp += 1
		_WriteLE32($buf, $pp, $ecxVal)
		$pp += 4
		; push 0 (lParam)
		DllStructSetData($buf, 1, 0x6A, $pp)
		$pp += 1
		DllStructSetData($buf, 1, 0x00, $pp)
		$pp += 1
		; push actionMem (wParam)
		DllStructSetData($buf, 1, 0x68, $pp)
		$pp += 1
		_WriteLE32($buf, $pp, $actionMem)
		$pp += 4
		; push msgId
		If $msgId < 0x80 Then
			DllStructSetData($buf, 1, 0x6A, $pp)
			$pp += 1
			DllStructSetData($buf, 1, $msgId, $pp)
			$pp += 1
		Else
			DllStructSetData($buf, 1, 0x68, $pp)
			$pp += 1
			_WriteLE32($buf, $pp, $msgId)
			$pp += 4
		EndIf
		; call [funcStore]
		DllStructSetData($buf, 1, 0xFF, $pp)
		$pp += 1
		DllStructSetData($buf, 1, 0x15, $pp)
		$pp += 1
		_WriteLE32($buf, $pp, $funcStore)
		$pp += 4
		; ret
		DllStructSetData($buf, 1, 0xC3, $pp)

		_WriteQueue($sc, $buf, $pp)
		Sleep(50)
	Next
EndFunc

Func _Check($label)
	Sleep(3000)
	Local $ec = DllCall($kernel_handle, 'bool', 'GetExitCodeProcess', 'handle', $processHandle, 'dword*', 0)
	If Not (IsArray($ec) And $ec[2] = 259) Then
		ConsoleWrite("  CRASHED during " & $label & @CRLF)
		Exit 1
	EndIf
	Local $status = MemoryRead($processHandle, GetLabel('StatusCode'), 'dword')
	ConsoleWrite("  StatusCode: " & $status & @CRLF)
	If $status <> 0 Then
		ConsoleWrite("  *** SUCCESS: " & $label & " ***" & @CRLF)
		Return True
	EndIf
	ConsoleWrite("  Still at char select" & @CRLF)
	Return False
EndFunc

Func _Alloc($size)
	Local $mem = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
		'handle', $processHandle, 'ptr', 0, 'ulong_ptr', $size, _
		'dword', 0x1000, 'dword', 0x40)
	If Not IsArray($mem) Or $mem[0] = 0 Then Return 0
	Return Int($mem[0])
EndFunc

Func _WriteByte($addr, $val)
	Local $b = DllStructCreate('byte[1]')
	DllStructSetData($b, 1, $val, 1)
	DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
		'handle', $processHandle, 'ptr', Ptr($addr), _
		'ptr', DllStructGetPtr($b), 'ulong_ptr', 1, 'ulong_ptr*', 0)
EndFunc

Func _Enqueue($addr)
	$queue_counter = MemoryRead($processHandle, GetLabel('QueueCounter'), 'dword')
	Local $cmd = DllStructCreate('dword;dword')
	DllStructSetData($cmd, 1, $addr)
	DllStructSetData($cmd, 2, 0)
	Enqueue(DllStructGetPtr($cmd), DllStructGetSize($cmd))
EndFunc

Func _WriteQueue($addr, ByRef $buf, $len)
	DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
		'handle', $processHandle, 'ptr', Ptr($addr), _
		'ptr', DllStructGetPtr($buf), 'ulong_ptr', $len, 'ulong_ptr*', 0)
	_Enqueue($addr)
EndFunc

Func _CheckModule($pid, $name)
	Local $snap = DllCall('kernel32.dll', 'handle', 'CreateToolhelp32Snapshot', 'dword', 0x8, 'dword', $pid)
	If Not IsArray($snap) Or $snap[0] = -1 Then Return False
	Local $me = DllStructCreate('dword dwSize;dword th32ModuleID;dword th32ProcessID;dword GlblcntUsage;dword ProccntUsage;ptr modBaseAddr;dword modBaseSize;handle hModule;wchar szModule[256];wchar szExePath[260]')
	DllStructSetData($me, 'dwSize', DllStructGetSize($me))
	Local $r = DllCall('kernel32.dll', 'bool', 'Module32FirstW', 'handle', $snap[0], 'struct*', $me)
	While IsArray($r) And $r[0]
		If StringLower(DllStructGetData($me, 'szModule')) = StringLower($name) Then
			DllCall('kernel32.dll', 'bool', 'CloseHandle', 'handle', $snap[0])
			Return True
		EndIf
		$r = DllCall('kernel32.dll', 'bool', 'Module32NextW', 'handle', $snap[0], 'struct*', $me)
	WEnd
	DllCall('kernel32.dll', 'bool', 'CloseHandle', 'handle', $snap[0])
	Return False
EndFunc
