#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

; =============================================================================
; test_gwca_combined.au3
;
; Combined: inject gwca.dll into BEASTRIT, init, then try all click approaches.
; Single script ensures we stay connected to the same PID throughout.
; =============================================================================

ConsoleWrite("=== GWCA Combined Inject + Click Test ===" & @CRLF)

; --- Find BEASTRIT (or client with gwca.dll already injected) ---
ScanAndUpdateGameClients()
Local $targetIdx = -1
For $i = 1 To $game_clients[0][0]
	Local $clientPID = $game_clients[$i][0]
	Local $clientTitle = $game_clients[$i][1]
	ConsoleWrite("  Client " & $i & ": PID=" & $clientPID & " title='" & $clientTitle & "'")

	If StringInStr($clientTitle, "B E A S T R I T") Or StringInStr($clientTitle, "BEASTRIT") Then
		$targetIdx = $i
		ConsoleWrite(" <- BEASTRIT")
	EndIf

	; Check if this client has gwca.dll loaded
	Local $hasGWCA = _ProcessHasModule($clientPID, "gwca.dll")
	If $hasGWCA Then
		ConsoleWrite(" [gwca.dll]")
		If $targetIdx = -1 Then $targetIdx = $i
	EndIf
	ConsoleWrite(@CRLF)
Next

If $targetIdx = -1 Then
	ConsoleWrite("No suitable client, launching BEASTRIT..." & @CRLF)
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
	ConsoleWrite("Launched PID=" & $result[0] & ", waiting 40s..." & @CRLF)
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
		ConsoleWrite("WARNING: Using client " & $targetIdx & " as fallback" & @CRLF)
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
ConsoleWrite("Connected: client=" & $targetIdx & " PID=" & $gwPID & @CRLF)

; Activate GW window to ensure rendering is active
WinActivate($gwHWnd)
Sleep(2000)

ConsoleWrite("IsAtCharSelect: " & IsAtCharSelect() & @CRLF)

; --- Find Play button ---
Local $playFrame = GetFrameByHash($FRAME_HASH_PLAY_BUTTON)
If $playFrame[0] = 0 Then
	ConsoleWrite("ERROR: Play button not found" & @CRLF)
	Exit 1
EndIf
Local $framePtr = Int($playFrame[0])
Local $frameId = $playFrame[1]
Local $childOffsetId = MemoryRead($processHandle, $framePtr + $FRAME_OFFSET_CHILD_OFFSET_ID, 'dword')
ConsoleWrite("Play button: ptr=0x" & Hex($framePtr) & " id=" & $frameId & " childOff=" & $childOffsetId & @CRLF)

; --- Verify rendering hook first ---
ConsoleWrite("Testing rendering hook..." & @CRLF)
Local $hookOK = _TestRenderingHook()
If Not $hookOK Then
	; Retry after focusing window
	ConsoleWrite("Retrying after focus..." & @CRLF)
	WinActivate($gwHWnd)
	Sleep(3000)
	$hookOK = _TestRenderingHook()
EndIf
If Not $hookOK Then
	ConsoleWrite("ERROR: Rendering hook not active. Cannot execute shellcode." & @CRLF)
	Exit 1
EndIf

; --- Check/inject GWCA ---
Local $gwcaBase = _FindModuleBase($gwPID, "gwca.dll")

If $gwcaBase = 0 Then
	ConsoleWrite("Injecting gwca.dll..." & @CRLF)
	$gwcaBase = _InjectDLL("c:\Users\Robert\Documents\GWA Censured X BotsHub\toolbox\GWToolboxpp-master\Dependencies\GWCA\bin\gwca.dll")
	If $gwcaBase = 0 Then
		ConsoleWrite("ERROR: injection failed" & @CRLF)
		Exit 1
	EndIf
	ConsoleWrite("gwca.dll at 0x" & Hex($gwcaBase) & @CRLF)

	; Scanner::Initialize via remote thread
	Local $gwImageBase = $pe_sections_ranges[0][0] - 0x1000
	_RemoteCall($gwcaBase + 0x21850, $gwImageBase, 10000)
	ConsoleWrite("Scanner initialized" & @CRLF)

	; GW::Initialize on GAME THREAD via rendering hook
	ConsoleWrite("GW::Initialize on game thread..." & @CRLF)
	_QueueCall($gwcaBase + 0x18E90)
	Sleep(5000)
Else
	ConsoleWrite("gwca.dll already at 0x" & Hex($gwcaBase) & @CRLF)
EndIf

; Verify GWCA init
Local $origSendFrame = MemoryRead($processHandle, $gwcaBase + 0x8A39C, 'dword')
ConsoleWrite("GWCA SendFrameUIMsg orig=0x" & Hex($origSendFrame) & @CRLF)
If $origSendFrame = 0 Then
	; Try remote thread init as fallback
	ConsoleWrite("GWCA not initialized. Trying remote thread init..." & @CRLF)
	_RemoteCall($gwcaBase + 0x18E90, 0, 15000)
	Sleep(2000)
	$origSendFrame = MemoryRead($processHandle, $gwcaBase + 0x8A39C, 'dword')
	ConsoleWrite("After remote init: 0x" & Hex($origSendFrame) & @CRLF)
EndIf

; Screenshot before
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\gwca_combined_before.png', $gwHWnd)

; --- GWCA function addresses ---
Local $GWCA_ButtonClick = $gwcaBase + 0x255E0
Local $GWCA_Click = $gwcaBase + 0x16660
Local $GWCA_MouseAction = $gwcaBase + 0x173D0
Local $GWCA_SendFrame = $gwcaBase + 0x274D0

ConsoleWrite("GWCA ButtonClick=0x" & Hex($GWCA_ButtonClick) & " Click=0x" & Hex($GWCA_Click) & @CRLF)

; =============================================================================
; APPROACH 1: GWCA ButtonClick(frame_ptr)
; =============================================================================
ConsoleWrite(@CRLF & "=== APPROACH 1: ButtonClick(frame_ptr) ===" & @CRLF)
Local $sc1 = _AllocRWX(32)
Local $b1 = DllStructCreate('byte[32]')
Local $p = 1
; push frame_ptr
DllStructSetData($b1, 1, 0x68, $p)
$p += 1
_WriteLE32($b1, $p, $framePtr)
$p += 4
; call ButtonClick
DllStructSetData($b1, 1, 0xE8, $p)
$p += 1
_WriteLE32($b1, $p, $GWCA_ButtonClick - ($sc1 + $p - 1 + 4))
$p += 4
; add esp, 4
DllStructSetData($b1, 1, 0x83, $p)
$p += 1
DllStructSetData($b1, 1, 0xC4, $p)
$p += 1
DllStructSetData($b1, 1, 0x04, $p)
$p += 1
; ret
DllStructSetData($b1, 1, 0xC3, $p)
_WriteAndQueue($sc1, $b1, $p)
If _CheckResult("ButtonClick(frame_ptr)") Then Exit 0

; =============================================================================
; APPROACH 2: ButtonFrame::Click (thiscall ecx=frame_ptr)
; =============================================================================
ConsoleWrite(@CRLF & "=== APPROACH 2: ButtonFrame::Click ===" & @CRLF)
Local $sc2 = _AllocRWX(16)
Local $b2 = DllStructCreate('byte[16]')
$p = 1
DllStructSetData($b2, 1, 0xB9, $p)
$p += 1
_WriteLE32($b2, $p, $framePtr)
$p += 4
DllStructSetData($b2, 1, 0xE8, $p)
$p += 1
_WriteLE32($b2, $p, $GWCA_Click - ($sc2 + $p - 1 + 4))
$p += 4
DllStructSetData($b2, 1, 0xC3, $p)
_WriteAndQueue($sc2, $b2, $p)
If _CheckResult("ButtonFrame::Click") Then Exit 0

; =============================================================================
; APPROACH 3: MouseAction(6) + MouseAction(7)
; =============================================================================
ConsoleWrite(@CRLF & "=== APPROACH 3: MouseAction(6+7) ===" & @CRLF)
For $act = 6 To 7
	Local $sc3 = _AllocRWX(16)
	Local $b3 = DllStructCreate('byte[16]')
	$p = 1
	DllStructSetData($b3, 1, 0xB9, $p)
	$p += 1
	_WriteLE32($b3, $p, $framePtr)
	$p += 4
	DllStructSetData($b3, 1, 0x6A, $p)
	$p += 1
	DllStructSetData($b3, 1, $act, $p)
	$p += 1
	DllStructSetData($b3, 1, 0xE8, $p)
	$p += 1
	_WriteLE32($b3, $p, $GWCA_MouseAction - ($sc3 + $p - 1 + 4))
	$p += 4
	DllStructSetData($b3, 1, 0xC3, $p)
	_WriteAndQueue($sc3, $b3, $p)
	Sleep(100)
Next
If _CheckResult("MouseAction(6+7)") Then Exit 0

; =============================================================================
; APPROACH 4: GWCA SendFrameUIMessage(frame, 0x2F, &action, 0)
; =============================================================================
ConsoleWrite(@CRLF & "=== APPROACH 4: SendFrameUIMsg(0x2F, action) ===" & @CRLF)
Local $sc4 = _AllocRWX(128)
Local $adAddr = $sc4 + 64
Local $ad = DllStructCreate('dword[8]')
DllStructSetData($ad, 1, $frameId, 1)
DllStructSetData($ad, 1, $childOffsetId, 2)
DllStructSetData($ad, 1, 0x8, 3)
DllStructSetData($ad, 1, 0, 4)
DllStructSetData($ad, 1, 0, 5)
DllStructSetData($ad, 1, 0, 6)
DllStructSetData($ad, 1, MemoryRead($processHandle, $framePtr + 0x1C4, 'dword'), 7)
DllStructSetData($ad, 1, 0, 8)
DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
	'handle', $processHandle, 'ptr', Ptr($adAddr), _
	'ptr', DllStructGetPtr($ad), 'ulong_ptr', 32, 'ulong_ptr*', 0)

Local $b4 = DllStructCreate('byte[48]')
$p = 1
; push 0
DllStructSetData($b4, 1, 0x6A, $p)
$p += 1
DllStructSetData($b4, 1, 0x00, $p)
$p += 1
; push adAddr
DllStructSetData($b4, 1, 0x68, $p)
$p += 1
_WriteLE32($b4, $p, $adAddr)
$p += 4
; push 0x2F
DllStructSetData($b4, 1, 0x6A, $p)
$p += 1
DllStructSetData($b4, 1, 0x2F, $p)
$p += 1
; push frame_ptr
DllStructSetData($b4, 1, 0x68, $p)
$p += 1
_WriteLE32($b4, $p, $framePtr)
$p += 4
; call
DllStructSetData($b4, 1, 0xE8, $p)
$p += 1
_WriteLE32($b4, $p, $GWCA_SendFrame - ($sc4 + $p - 1 + 4))
$p += 4
; add esp, 16
DllStructSetData($b4, 1, 0x83, $p)
$p += 1
DllStructSetData($b4, 1, 0xC4, $p)
$p += 1
DllStructSetData($b4, 1, 0x10, $p)
$p += 1
; ret
DllStructSetData($b4, 1, 0xC3, $p)
_WriteAndQueue($sc4, $b4, $p)
If _CheckResult("SendFrameUIMsg(0x2F)") Then Exit 0

; =============================================================================
; APPROACH 5: GetFrameById + ButtonFrame::Click
; =============================================================================
ConsoleWrite(@CRLF & "=== APPROACH 5: GetFrameById(" & $frameId & ") + Click ===" & @CRLF)
Local $sc5 = _AllocRWX(32)
Local $GWCA_GetFrame = $gwcaBase + 0x25CC0
Local $b5 = DllStructCreate('byte[32]')
$p = 1
; push frame_id
DllStructSetData($b5, 1, 0x6A, $p)
$p += 1
DllStructSetData($b5, 1, $frameId, $p)
$p += 1
; call GetFrameById
DllStructSetData($b5, 1, 0xE8, $p)
$p += 1
_WriteLE32($b5, $p, $GWCA_GetFrame - ($sc5 + $p - 1 + 4))
$p += 4
; add esp, 4
DllStructSetData($b5, 1, 0x83, $p)
$p += 1
DllStructSetData($b5, 1, 0xC4, $p)
$p += 1
DllStructSetData($b5, 1, 0x04, $p)
$p += 1
; test eax,eax
DllStructSetData($b5, 1, 0x85, $p)
$p += 1
DllStructSetData($b5, 1, 0xC0, $p)
$p += 1
; jz +7
DllStructSetData($b5, 1, 0x74, $p)
$p += 1
DllStructSetData($b5, 1, 0x07, $p)
$p += 1
; mov ecx, eax
DllStructSetData($b5, 1, 0x8B, $p)
$p += 1
DllStructSetData($b5, 1, 0xC8, $p)
$p += 1
; call Click
DllStructSetData($b5, 1, 0xE8, $p)
$p += 1
_WriteLE32($b5, $p, $GWCA_Click - ($sc5 + $p - 1 + 4))
$p += 4
; ret
DllStructSetData($b5, 1, 0xC3, $p)
_WriteAndQueue($sc5, $b5, $p)
If _CheckResult("GetFrameById+Click") Then Exit 0

; =============================================================================
; APPROACH 6: Direct game SendFrameUIMsg (__thiscall, 0x2F)
; =============================================================================
ConsoleWrite(@CRLF & "=== APPROACH 6: Direct game func (0x" & Hex($origSendFrame) & ") ===" & @CRLF)
If $origSendFrame > 0x10000 Then
	For $actionState = 6 To 7
		Local $sc6 = _AllocRWX(128)
		Local $ad6a = $sc6 + 64
		Local $funcSt = $sc6 + 52
		; Write action
		Local $ad6 = DllStructCreate('dword[5]')
		DllStructSetData($ad6, 1, $frameId, 1)
		DllStructSetData($ad6, 1, $childOffsetId, 2)
		DllStructSetData($ad6, 1, $actionState, 3)
		DllStructSetData($ad6, 1, 0, 4)
		DllStructSetData($ad6, 1, 0, 5)
		DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
			'handle', $processHandle, 'ptr', Ptr($ad6a), _
			'ptr', DllStructGetPtr($ad6), 'ulong_ptr', 20, 'ulong_ptr*', 0)
		; Write func ptr
		MemoryWrite($processHandle, $funcSt, $origSendFrame, 'dword')

		Local $b6 = DllStructCreate('byte[48]')
		$p = 1
		; mov ecx, frame_ptr+0xA8
		DllStructSetData($b6, 1, 0xB9, $p)
		$p += 1
		_WriteLE32($b6, $p, $framePtr + 0xA8)
		$p += 4
		; push 0
		DllStructSetData($b6, 1, 0x6A, $p)
		$p += 1
		DllStructSetData($b6, 1, 0x00, $p)
		$p += 1
		; push ad6a
		DllStructSetData($b6, 1, 0x68, $p)
		$p += 1
		_WriteLE32($b6, $p, $ad6a)
		$p += 4
		; push 0x2F
		DllStructSetData($b6, 1, 0x6A, $p)
		$p += 1
		DllStructSetData($b6, 1, 0x2F, $p)
		$p += 1
		; call [funcSt]
		DllStructSetData($b6, 1, 0xFF, $p)
		$p += 1
		DllStructSetData($b6, 1, 0x15, $p)
		$p += 1
		_WriteLE32($b6, $p, $funcSt)
		$p += 4
		; ret
		DllStructSetData($b6, 1, 0xC3, $p)

		DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
			'handle', $processHandle, 'ptr', Ptr($sc6), _
			'ptr', DllStructGetPtr($b6), 'ulong_ptr', $p, 'ulong_ptr*', 0)

		$queue_counter = MemoryRead($processHandle, GetLabel('QueueCounter'), 'dword')
		Local $cmd6 = DllStructCreate('dword;dword')
		DllStructSetData($cmd6, 1, $sc6)
		DllStructSetData($cmd6, 2, 0)
		Enqueue(DllStructGetPtr($cmd6), DllStructGetSize($cmd6))
		Sleep(100)
	Next
	If _CheckResult("Direct SendFrameUIMsg(0x2F)") Then Exit 0
EndIf

; =============================================================================
; APPROACH 7: Direct game func with msg=0x31
; =============================================================================
ConsoleWrite(@CRLF & "=== APPROACH 7: Direct game func msg=0x31 ===" & @CRLF)
If $origSendFrame > 0x10000 Then
	For $actionState = 6 To 7
		Local $sc7 = _AllocRWX(128)
		Local $ad7a = $sc7 + 64
		Local $funcSt7 = $sc7 + 52
		Local $ad7 = DllStructCreate('dword[5]')
		DllStructSetData($ad7, 1, $frameId, 1)
		DllStructSetData($ad7, 1, $childOffsetId, 2)
		DllStructSetData($ad7, 1, $actionState, 3)
		DllStructSetData($ad7, 1, 0, 4)
		DllStructSetData($ad7, 1, 0, 5)
		DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
			'handle', $processHandle, 'ptr', Ptr($ad7a), _
			'ptr', DllStructGetPtr($ad7), 'ulong_ptr', 20, 'ulong_ptr*', 0)
		MemoryWrite($processHandle, $funcSt7, $origSendFrame, 'dword')

		Local $b7 = DllStructCreate('byte[48]')
		$p = 1
		DllStructSetData($b7, 1, 0xB9, $p)
		$p += 1
		_WriteLE32($b7, $p, $framePtr + 0xA8)
		$p += 4
		DllStructSetData($b7, 1, 0x6A, $p)
		$p += 1
		DllStructSetData($b7, 1, 0x00, $p)
		$p += 1
		DllStructSetData($b7, 1, 0x68, $p)
		$p += 1
		_WriteLE32($b7, $p, $ad7a)
		$p += 4
		DllStructSetData($b7, 1, 0x6A, $p)
		$p += 1
		DllStructSetData($b7, 1, 0x31, $p)
		$p += 1
		DllStructSetData($b7, 1, 0xFF, $p)
		$p += 1
		DllStructSetData($b7, 1, 0x15, $p)
		$p += 1
		_WriteLE32($b7, $p, $funcSt7)
		$p += 4
		DllStructSetData($b7, 1, 0xC3, $p)

		DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
			'handle', $processHandle, 'ptr', Ptr($sc7), _
			'ptr', DllStructGetPtr($b7), 'ulong_ptr', $p, 'ulong_ptr*', 0)

		$queue_counter = MemoryRead($processHandle, GetLabel('QueueCounter'), 'dword')
		Local $cmd7 = DllStructCreate('dword;dword')
		DllStructSetData($cmd7, 1, $sc7)
		DllStructSetData($cmd7, 2, 0)
		Enqueue(DllStructGetPtr($cmd7), DllStructGetSize($cmd7))
		Sleep(100)
	Next
	If _CheckResult("Direct SendFrameUIMsg(0x31)") Then Exit 0
EndIf

; --- Final ---
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\gwca_combined_after.png', $gwHWnd)
ConsoleWrite(@CRLF & "=== ALL 7 APPROACHES FAILED ===" & @CRLF)
ConsoleWrite("Game alive: " & _GameAlive() & @CRLF)
ConsoleWrite("=== DONE ===" & @CRLF)

; =============================================================================
; Helper functions
; =============================================================================

Func _GameAlive()
	Local $ec = DllCall($kernel_handle, 'bool', 'GetExitCodeProcess', 'handle', $processHandle, 'dword*', 0)
	Return (IsArray($ec) And $ec[2] = 259)
EndFunc

Func _CheckResult($label)
	Sleep(3000)
	If Not _GameAlive() Then
		ConsoleWrite("  GAME CRASHED during " & $label & @CRLF)
		Exit 1
	EndIf
	Local $status = MemoryRead($processHandle, GetLabel('StatusCode'), 'dword')
	ConsoleWrite("  StatusCode: " & $status & @CRLF)
	If $status <> 0 Then
		ConsoleWrite("  *** SUCCESS: " & $label & " ***" & @CRLF)
		_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\gwca_combined_success.png', $gwHWnd)
		Return True
	EndIf
	ConsoleWrite("  Still at char select" & @CRLF)
	Return False
EndFunc

Func _AllocRWX($size)
	Local $mem = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
		'handle', $processHandle, 'ptr', 0, 'ulong_ptr', $size, _
		'dword', 0x1000, 'dword', 0x40)
	If Not IsArray($mem) Or $mem[0] = 0 Then Return 0
	Return Int($mem[0])
EndFunc

Func _WriteAndQueue($addr, ByRef $buf, $len)
	DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
		'handle', $processHandle, 'ptr', Ptr($addr), _
		'ptr', DllStructGetPtr($buf), 'ulong_ptr', $len, 'ulong_ptr*', 0)
	$queue_counter = MemoryRead($processHandle, GetLabel('QueueCounter'), 'dword')
	Local $cmd = DllStructCreate('dword;dword')
	DllStructSetData($cmd, 1, $addr)
	DllStructSetData($cmd, 2, 0)
	Enqueue(DllStructGetPtr($cmd), DllStructGetSize($cmd))
EndFunc

Func _TestRenderingHook()
	Local $nop = _AllocRWX(4)
	If $nop = 0 Then Return False
	Local $nb = DllStructCreate('byte[1]')
	DllStructSetData($nb, 1, 0xC3, 1)
	DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
		'handle', $processHandle, 'ptr', Ptr($nop), _
		'ptr', DllStructGetPtr($nb), 'ulong_ptr', 1, 'ulong_ptr*', 0)
	Local $pre = MemoryRead($processHandle, GetLabel('QueueCounter'), 'dword')
	$queue_counter = $pre
	Local $nc = DllStructCreate('dword;dword')
	DllStructSetData($nc, 1, $nop)
	DllStructSetData($nc, 2, 0)
	Enqueue(DllStructGetPtr($nc), DllStructGetSize($nc))
	Sleep(1000)
	Local $post = MemoryRead($processHandle, GetLabel('QueueCounter'), 'dword')
	If $post <> $pre Then
		ConsoleWrite("Rendering hook ACTIVE (" & $pre & " -> " & $post & ")" & @CRLF)
		Return True
	EndIf
	ConsoleWrite("Rendering hook NOT active" & @CRLF)
	Return False
EndFunc

Func _ProcessHasModule($pid, $modName)
	Local $snap = DllCall('kernel32.dll', 'handle', 'CreateToolhelp32Snapshot', 'dword', 0x8, 'dword', $pid)
	If Not IsArray($snap) Or $snap[0] = -1 Then Return False
	Local $me = DllStructCreate('dword dwSize;dword th32ModuleID;dword th32ProcessID;dword GlblcntUsage;dword ProccntUsage;ptr modBaseAddr;dword modBaseSize;handle hModule;wchar szModule[256];wchar szExePath[260]')
	DllStructSetData($me, 'dwSize', DllStructGetSize($me))
	Local $r = DllCall('kernel32.dll', 'bool', 'Module32FirstW', 'handle', $snap[0], 'struct*', $me)
	While IsArray($r) And $r[0]
		If StringLower(DllStructGetData($me, 'szModule')) = StringLower($modName) Then
			DllCall('kernel32.dll', 'bool', 'CloseHandle', 'handle', $snap[0])
			Return True
		EndIf
		$r = DllCall('kernel32.dll', 'bool', 'Module32NextW', 'handle', $snap[0], 'struct*', $me)
	WEnd
	DllCall('kernel32.dll', 'bool', 'CloseHandle', 'handle', $snap[0])
	Return False
EndFunc

Func _FindModuleBase($pid, $modName)
	Local $snap = DllCall('kernel32.dll', 'handle', 'CreateToolhelp32Snapshot', 'dword', 0x8, 'dword', $pid)
	If Not IsArray($snap) Or $snap[0] = -1 Then Return 0
	Local $me = DllStructCreate('dword dwSize;dword th32ModuleID;dword th32ProcessID;dword GlblcntUsage;dword ProccntUsage;ptr modBaseAddr;dword modBaseSize;handle hModule;wchar szModule[256];wchar szExePath[260]')
	DllStructSetData($me, 'dwSize', DllStructGetSize($me))
	Local $r = DllCall('kernel32.dll', 'bool', 'Module32FirstW', 'handle', $snap[0], 'struct*', $me)
	While IsArray($r) And $r[0]
		If StringLower(DllStructGetData($me, 'szModule')) = StringLower($modName) Then
			Local $base = Int(DllStructGetData($me, 'modBaseAddr'))
			DllCall('kernel32.dll', 'bool', 'CloseHandle', 'handle', $snap[0])
			Return $base
		EndIf
		$r = DllCall('kernel32.dll', 'bool', 'Module32NextW', 'handle', $snap[0], 'struct*', $me)
	WEnd
	DllCall('kernel32.dll', 'bool', 'CloseHandle', 'handle', $snap[0])
	Return 0
EndFunc

Func _InjectDLL($dllPath)
	Local $dllPathW = StringToBinary($dllPath, 2) & Binary("0x0000")
	Local $pathLen = BinaryLen($dllPathW)
	Local $rp = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
		'handle', $processHandle, 'ptr', 0, 'ulong_ptr', $pathLen, _
		'dword', 0x1000, 'dword', 0x04)
	If Not IsArray($rp) Or $rp[0] = 0 Then Return 0
	Local $pb = DllStructCreate('byte[' & $pathLen & ']')
	DllStructSetData($pb, 1, $dllPathW)
	DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
		'handle', $processHandle, 'ptr', $rp[0], _
		'ptr', DllStructGetPtr($pb), 'ulong_ptr', $pathLen, 'ulong_ptr*', 0)
	Local $k32 = DllCall('kernel32.dll', 'ptr', 'GetModuleHandleW', 'wstr', 'kernel32.dll')
	Local $ll = DllCall('kernel32.dll', 'ptr', 'GetProcAddress', 'ptr', $k32[0], 'str', 'LoadLibraryW')
	Local $th = DllCall($kernel_handle, 'handle', 'CreateRemoteThread', _
		'handle', $processHandle, 'ptr', 0, 'ulong_ptr', 0, _
		'ptr', $ll[0], 'ptr', $rp[0], 'dword', 0, 'dword*', 0)
	If Not IsArray($th) Or $th[0] = 0 Then Return 0
	DllCall($kernel_handle, 'dword', 'WaitForSingleObject', 'handle', $th[0], 'dword', 10000)
	Local $ec = DllCall($kernel_handle, 'bool', 'GetExitCodeThread', 'handle', $th[0], 'dword*', 0)
	DllCall($kernel_handle, 'bool', 'CloseHandle', 'handle', $th[0])
	DllCall($kernel_handle, 'bool', 'VirtualFreeEx', _
		'handle', $processHandle, 'ptr', $rp[0], 'ulong_ptr', 0, 'dword', 0x8000)
	Return $ec[2]
EndFunc

Func _RemoteCall($funcAddr, $arg, $timeout)
	Local $th = DllCall($kernel_handle, 'handle', 'CreateRemoteThread', _
		'handle', $processHandle, 'ptr', 0, 'ulong_ptr', 0, _
		'ptr', Ptr($funcAddr), 'ptr', Ptr($arg), 'dword', 0, 'dword*', 0)
	If Not IsArray($th) Or $th[0] = 0 Then Return False
	DllCall($kernel_handle, 'dword', 'WaitForSingleObject', 'handle', $th[0], 'dword', $timeout)
	DllCall($kernel_handle, 'bool', 'CloseHandle', 'handle', $th[0])
	Return True
EndFunc

Func _QueueCall($funcAddr)
	Local $sc = _AllocRWX(16)
	If $sc = 0 Then Return
	Local $buf = DllStructCreate('byte[16]')
	Local $pp = 1
	DllStructSetData($buf, 1, 0xE8, $pp)
	$pp += 1
	_WriteLE32($buf, $pp, $funcAddr - ($sc + $pp - 1 + 4))
	$pp += 4
	DllStructSetData($buf, 1, 0xC3, $pp)
	_WriteAndQueue($sc, $buf, $pp)
EndFunc
