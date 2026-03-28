#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

; =============================================================================
; test_direct_play.au3
;
; Fresh client, NO GWCA, working rendering hook.
; Try direct callback invocation and the UNHOOKED SendFrameUIMsg address.
;
; Key discovery: BotsHub's "Action" hook is at the SAME address as
; SendFrameUIMsg (0x002786D0 relative). Our calls go through the BotsHub hook
; which may eat them. So we also try the ORIGINAL unhooked function.
; =============================================================================

ConsoleWrite("=== Direct Play Button Test ===" & @CRLF)

; Launch fresh
ConsoleWrite("Launching BEASTRIT..." & @CRLF)
Local $accounts = GWLauncher_LoadAccounts()
Local $accIdx = GWLauncher_FindAccountByCharacter($accounts, "B E A S T R I T")
Local $result = GWLauncher_LaunchAccount($accounts, $accIdx)
ConsoleWrite("PID=" & $result[0] & ", waiting 40s..." & @CRLF)
Sleep(40000)

ScanAndUpdateGameClients()
Local $targetIdx = -1
For $i = 1 To $game_clients[0][0]
	If $game_clients[$i][0] = $result[0] Then
		$targetIdx = $i
		ExitLoop
	EndIf
Next
If $targetIdx = -1 Then $targetIdx = $game_clients[0][0]

SelectClient($targetIdx)
InitializeGameClientForGWA2(False)
Local $processHandle = GetProcessHandle()
Local $gwHWnd = $game_clients[$targetIdx][2]
ConsoleWrite("Connected PID=" & $game_clients[$targetIdx][0] & @CRLF)

WinActivate($gwHWnd)
Sleep(2000)

; Verify rendering hook with marker
Local $marker = _Alloc(4)
MemoryWrite($processHandle, $marker, 0x1111, 'dword')
Local $testSc = _Alloc(16)
Local $tb = DllStructCreate('byte[16]')
Local $p = 1
DllStructSetData($tb, 1, 0xC7, $p)
$p += 1
DllStructSetData($tb, 1, 0x05, $p)
$p += 1
_WriteLE32($tb, $p, $marker)
$p += 4
_WriteLE32($tb, $p, 0x2222)
$p += 4
DllStructSetData($tb, 1, 0xC3, $p)
_WQ($testSc, $tb, $p)
Sleep(1500)
Local $mv = MemoryRead($processHandle, $marker, 'dword')
ConsoleWrite("Hook test: 0x" & Hex($mv) & " (want 0x2222)" & @CRLF)
If $mv <> 0x2222 Then
	ConsoleWrite("ERROR: Rendering hook not working" & @CRLF)
	Exit 1
EndIf

ConsoleWrite("IsAtCharSelect: " & IsAtCharSelect() & @CRLF)

; Get Play button
Local $pf = GetFrameByHash($FRAME_HASH_PLAY_BUTTON)
If $pf[0] = 0 Then
	ConsoleWrite("ERROR: Play button not found" & @CRLF)
	Exit 1
EndIf
Local $fp = Int($pf[0])
Local $fid = $pf[1]
Local $coff = MemoryRead($processHandle, $fp + $FRAME_OFFSET_CHILD_OFFSET_ID, 'dword')
Local $cbBuf = MemoryRead($processHandle, $fp + 0xA8, 'dword')
Local $cbSize = MemoryRead($processHandle, $fp + 0xAC, 'dword')
Local $cb0 = 0
If $cbSize > 0 And $cbBuf > 0x10000 Then
	$cb0 = MemoryRead($processHandle, $cbBuf, 'dword')
EndIf
ConsoleWrite("Play: ptr=0x" & Hex($fp) & " id=" & $fid & " cb[0]=0x" & Hex($cb0) & @CRLF)

; Get the UNHOOKED function address
; BotsHub saves the original bytes before patching. We can read the original
; function by looking at where the hook JMP goes and finding the trampoline.
; OR: read bytes at the Action address to see if there's a JMP to BotsHub code.
Local $sendFunc = Int(GetLabel('SendFrameUIMsg'))
Local $actionAddr = Int(GetLabel('Action'))
ConsoleWrite("SendFrameUIMsg = 0x" & Hex($sendFunc) & @CRLF)
ConsoleWrite("Action = 0x" & Hex($actionAddr) & @CRLF)

; Read first 16 bytes of the function to check for hook
Local $funcHead = DllStructCreate('byte[16]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
	'handle', $processHandle, 'ptr', Ptr($sendFunc), _
	'ptr', DllStructGetPtr($funcHead), 'ulong_ptr', 16, 'ulong_ptr*', 0)
Local $hex = ""
For $b = 1 To 16
	$hex &= Hex(DllStructGetData($funcHead, 1, $b), 2) & " "
Next
ConsoleWrite("SendFrameUIMsg bytes: " & $hex & @CRLF)
; If first byte is E9 (JMP) or FF 25 (JMP indirect), it's hooked
Local $b1 = DllStructGetData($funcHead, 1, 1)
If $b1 = 0xE9 Then
	ConsoleWrite("HOOKED with JMP rel32" & @CRLF)
ElseIf $b1 = 0xFF Then
	ConsoleWrite("HOOKED with JMP indirect" & @CRLF)
Else
	ConsoleWrite("NOT hooked (or inline hook)" & @CRLF)
EndIf

_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\direct_play_before.png', $gwHWnd)

; =============================================================================
; TEST 1: Call callback[0] directly with frame message args
; Signature guess: callback(Frame*, UIMessage, void*, void*)
; =============================================================================
ConsoleWrite(@CRLF & "=== TEST 1: Direct callback[0] call ===" & @CRLF)
If $cb0 > 0x10000 Then
	; Read first bytes of callback to understand it
	Local $cbHead = DllStructCreate('byte[16]')
	DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
		'handle', $processHandle, 'ptr', Ptr($cb0), _
		'ptr', DllStructGetPtr($cbHead), 'ulong_ptr', 16, 'ulong_ptr*', 0)
	$hex = ""
	For $b = 1 To 16
		$hex &= Hex(DllStructGetData($cbHead, 1, $b), 2) & " "
	Next
	ConsoleWrite("callback[0] bytes: " & $hex & @CRLF)

	; Try: push 0, push 0, push 0x2F, push frame_ptr, call callback
	Local $sc1 = _Alloc(64)
	Local $cbSt = $sc1 + 56
	MemoryWrite($processHandle, $cbSt, $cb0, 'dword')
	Local $b1sc = DllStructCreate('byte[48]')
	$p = 1
	; push 0 (lParam)
	DllStructSetData($b1sc, 1, 0x6A, $p)
	$p += 1
	DllStructSetData($b1sc, 1, 0x00, $p)
	$p += 1
	; push 0 (wParam)
	DllStructSetData($b1sc, 1, 0x6A, $p)
	$p += 1
	DllStructSetData($b1sc, 1, 0x00, $p)
	$p += 1
	; push 0x2F (kMouseAction)
	DllStructSetData($b1sc, 1, 0x6A, $p)
	$p += 1
	DllStructSetData($b1sc, 1, 0x2F, $p)
	$p += 1
	; push frame_ptr
	DllStructSetData($b1sc, 1, 0x68, $p)
	$p += 1
	_WriteLE32($b1sc, $p, $fp)
	$p += 4
	; call [cbSt]
	DllStructSetData($b1sc, 1, 0xFF, $p)
	$p += 1
	DllStructSetData($b1sc, 1, 0x15, $p)
	$p += 1
	_WriteLE32($b1sc, $p, $cbSt)
	$p += 4
	; add esp, 16 (cdecl: clean 4 args)
	DllStructSetData($b1sc, 1, 0x83, $p)
	$p += 1
	DllStructSetData($b1sc, 1, 0xC4, $p)
	$p += 1
	DllStructSetData($b1sc, 1, 0x10, $p)
	$p += 1
	; ret
	DllStructSetData($b1sc, 1, 0xC3, $p)
	_WQ($sc1, $b1sc, $p)
	If _Chk("callback(frame,0x2F,0,0)") Then Exit 0
EndIf

; =============================================================================
; TEST 2: callback as thiscall (ECX = frame_ptr)
; =============================================================================
ConsoleWrite(@CRLF & "=== TEST 2: callback thiscall ===" & @CRLF)
If $cb0 > 0x10000 Then
	Local $sc2 = _Alloc(64)
	Local $cbSt2 = $sc2 + 56
	MemoryWrite($processHandle, $cbSt2, $cb0, 'dword')
	Local $b2 = DllStructCreate('byte[48]')
	$p = 1
	; mov ecx, frame_ptr
	DllStructSetData($b2, 1, 0xB9, $p)
	$p += 1
	_WriteLE32($b2, $p, $fp)
	$p += 4
	; push 0
	DllStructSetData($b2, 1, 0x6A, $p)
	$p += 1
	DllStructSetData($b2, 1, 0x00, $p)
	$p += 1
	; push 0
	DllStructSetData($b2, 1, 0x6A, $p)
	$p += 1
	DllStructSetData($b2, 1, 0x00, $p)
	$p += 1
	; push 0x2F
	DllStructSetData($b2, 1, 0x6A, $p)
	$p += 1
	DllStructSetData($b2, 1, 0x2F, $p)
	$p += 1
	; call [cbSt2]
	DllStructSetData($b2, 1, 0xFF, $p)
	$p += 1
	DllStructSetData($b2, 1, 0x15, $p)
	$p += 1
	_WriteLE32($b2, $p, $cbSt2)
	$p += 4
	; ret
	DllStructSetData($b2, 1, 0xC3, $p)
	_WQ($sc2, $b2, $p)
	If _Chk("callback thiscall(frame,0x2F,0,0)") Then Exit 0
EndIf

; =============================================================================
; TEST 3: Use ClickFrameButton from GWA2_FrameUI (now with fixed _WriteLE32)
; =============================================================================
ConsoleWrite(@CRLF & "=== TEST 3: ClickFrameButton (fixed _WriteLE32) ===" & @CRLF)
ClickFrameButton($FRAME_HASH_PLAY_BUTTON)
If _Chk("ClickFrameButton") Then Exit 0

; =============================================================================
; TEST 4: Read the Action hook trampoline to find UNHOOKED func
; Then call it directly, bypassing BotsHub hook
; =============================================================================
ConsoleWrite(@CRLF & "=== TEST 4: Bypass Action hook ===" & @CRLF)
; Read what the Action function looks like
; If hooked: E9 xx xx xx xx (JMP rel32 to BotsHub handler)
; The original function bytes should be saved in the BotsHub injection area
; OR we can skip the hook by jumping past it

; Actually, read more bytes to understand the function
Local $funcDump = DllStructCreate('byte[32]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
	'handle', $processHandle, 'ptr', Ptr($sendFunc), _
	'ptr', DllStructGetPtr($funcDump), 'ulong_ptr', 32, 'ulong_ptr*', 0)
$hex = ""
For $b = 1 To 32
	$hex &= Hex(DllStructGetData($funcDump, 1, $b), 2) & " "
Next
ConsoleWrite("Full function dump: " & $hex & @CRLF)

; If the function starts with a 5-byte JMP (hook), the original function
; entry was overwritten. The original first N bytes were saved to a trampoline.
; We'd need to find that trampoline. But for now, let's try calling at
; sendFunc + 5 (skip the hook JMP) — this only works if the hook is a 5-byte JMP
; and the next instruction is intact.
If DllStructGetData($funcDump, 1, 1) = 0xE9 Then
	Local $hookTarget = $sendFunc + 5 + _ReadLE32($funcDump, 2)
	ConsoleWrite("Hook JMP target: 0x" & Hex($hookTarget) & @CRLF)
	ConsoleWrite("Trying call at sendFunc+5 (skip hook)..." & @CRLF)

	; Build action data
	Local $actMem = _Alloc(32)
	For $act = 6 To 7
		Local $ad = DllStructCreate('dword[5]')
		DllStructSetData($ad, 1, $fid, 1)
		DllStructSetData($ad, 1, $coff, 2)
		DllStructSetData($ad, 1, $act, 3)
		DllStructSetData($ad, 1, 0, 4)
		DllStructSetData($ad, 1, 0, 5)
		DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
			'handle', $processHandle, 'ptr', Ptr($actMem), _
			'ptr', DllStructGetPtr($ad), 'ulong_ptr', 20, 'ulong_ptr*', 0)

		Local $sc4 = _Alloc(64)
		Local $fst4 = $sc4 + 56
		MemoryWrite($processHandle, $fst4, $sendFunc + 5, 'dword')  ; skip hook JMP
		Local $b4 = DllStructCreate('byte[48]')
		$p = 1
		; mov ecx, frame+0x84
		DllStructSetData($b4, 1, 0xB9, $p)
		$p += 1
		_WriteLE32($b4, $p, $fp + 0x84)
		$p += 4
		; push 0
		DllStructSetData($b4, 1, 0x6A, $p)
		$p += 1
		DllStructSetData($b4, 1, 0x00, $p)
		$p += 1
		; push actMem
		DllStructSetData($b4, 1, 0x68, $p)
		$p += 1
		_WriteLE32($b4, $p, $actMem)
		$p += 4
		; push 0x2F
		DllStructSetData($b4, 1, 0x6A, $p)
		$p += 1
		DllStructSetData($b4, 1, 0x2F, $p)
		$p += 1
		; call [fst4]
		DllStructSetData($b4, 1, 0xFF, $p)
		$p += 1
		DllStructSetData($b4, 1, 0x15, $p)
		$p += 1
		_WriteLE32($b4, $p, $fst4)
		$p += 4
		; ret
		DllStructSetData($b4, 1, 0xC3, $p)
		_WQ($sc4, $b4, $p)
		Sleep(100)
	Next
	If _Chk("Unhooked SendFrameUIMsg") Then Exit 0
Else
	ConsoleWrite("Function not hooked with JMP — skipping bypass test" & @CRLF)
EndIf

; --- Final ---
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\direct_play_after.png', $gwHWnd)
ConsoleWrite(@CRLF & "=== ALL FAILED ===" & @CRLF)
Local $alive = DllCall($kernel_handle, 'bool', 'GetExitCodeProcess', 'handle', $processHandle, 'dword*', 0)
ConsoleWrite("Game alive: " & (IsArray($alive) And $alive[2] = 259) & @CRLF)

; =============================================================================
Func _Alloc($sz)
	Local $m = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
		'handle', $processHandle, 'ptr', 0, 'ulong_ptr', $sz, _
		'dword', 0x1000, 'dword', 0x40)
	If Not IsArray($m) Or $m[0] = 0 Then Return 0
	Return Int($m[0])
EndFunc

Func _WQ($addr, ByRef $buf, $len)
	DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
		'handle', $processHandle, 'ptr', Ptr($addr), _
		'ptr', DllStructGetPtr($buf), 'ulong_ptr', $len, 'ulong_ptr*', 0)
	$queue_counter = MemoryRead($processHandle, GetLabel('QueueCounter'), 'dword')
	Local $c = DllStructCreate('dword;dword')
	DllStructSetData($c, 1, $addr)
	DllStructSetData($c, 2, 0)
	Enqueue(DllStructGetPtr($c), DllStructGetSize($c))
EndFunc

Func _Chk($label)
	Sleep(3000)
	Local $ec = DllCall($kernel_handle, 'bool', 'GetExitCodeProcess', 'handle', $processHandle, 'dword*', 0)
	If Not (IsArray($ec) And $ec[2] = 259) Then
		ConsoleWrite("  CRASHED: " & $label & @CRLF)
		Exit 1
	EndIf
	Local $st = MemoryRead($processHandle, GetLabel('StatusCode'), 'dword')
	ConsoleWrite("  StatusCode: " & $st & @CRLF)
	If $st <> 0 Then
		ConsoleWrite("  *** SUCCESS: " & $label & " ***" & @CRLF)
		Return True
	EndIf
	ConsoleWrite("  Still at char select" & @CRLF)
	Return False
EndFunc

Func _ReadLE32(ByRef $struct, $pos)
	Return BitOR( _
		DllStructGetData($struct, 1, $pos), _
		BitShift(DllStructGetData($struct, 1, $pos + 1), -8), _
		BitShift(DllStructGetData($struct, 1, $pos + 2), -16), _
		BitShift(DllStructGetData($struct, 1, $pos + 3), -24))
EndFunc
