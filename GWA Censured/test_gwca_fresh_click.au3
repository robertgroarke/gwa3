#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

; =============================================================================
; test_gwca_fresh_click.au3
;
; All-in-one: on the currently running fresh GW client (PID 45068),
; inject GWCA, init via remote threads, then try ButtonClick on game thread.
;
; Key difference from previous attempts:
; - Same process throughout (no PID mismatch)
; - GWCA Scanner + GW::Init via remote thread (proven to work)
; - ButtonClick via rendering hook (proven active on this client)
; - Also try: msg=0x2B (seen at the game's actual call site)
; =============================================================================

ConsoleWrite("=== GWCA Fresh Click Test ===" & @CRLF)

; Connect to the running client
ScanAndUpdateGameClients()
If $game_clients[0][0] = 0 Then
	ConsoleWrite("ERROR: No GW clients. Run test_click_ecx_fix.au3 first to launch one." & @CRLF)
	Exit 1
EndIf

; Use the first (and only) client
SelectClient(1)
InitializeGameClientForGWA2(False)
Local $processHandle = GetProcessHandle()
Local $gwPID = $game_clients[1][0]
Local $gwHWnd = $game_clients[1][2]
ConsoleWrite("PID=" & $gwPID & @CRLF)

WinActivate($gwHWnd)
Sleep(2000)

; Verify char select + rendering hook
ConsoleWrite("IsAtCharSelect: " & IsAtCharSelect() & @CRLF)

; Quick rendering hook test
Local $marker = _Alloc(4)
MemoryWrite($processHandle, $marker, 0xAAAA, 'dword')
Local $testSc = _Alloc(16)
Local $tb = DllStructCreate('byte[16]')
Local $tp = 1
; mov dword [marker], 0xBBBB
DllStructSetData($tb, 1, 0xC7, $tp)
$tp += 1
DllStructSetData($tb, 1, 0x05, $tp)
$tp += 1
_WriteLE32($tb, $tp, $marker)
$tp += 4
_WriteLE32($tb, $tp, 0xBBBB)
$tp += 4
DllStructSetData($tb, 1, 0xC3, $tp)
_WQ($testSc, $tb, $tp)
Sleep(1000)
Local $mv = MemoryRead($processHandle, $marker, 'dword')
ConsoleWrite("Hook test: marker=0x" & Hex($mv) & " (want 0xBBBB)" & @CRLF)

If $mv <> 0xBBBB Then
	ConsoleWrite("Rendering hook not executing shellcode correctly" & @CRLF)
	; Try reading what's actually at the marker address to debug
	ConsoleWrite("Marker addr: 0x" & Hex($marker) & @CRLF)
	ConsoleWrite("Shellcode addr: 0x" & Hex($testSc) & @CRLF)
	; Read shellcode bytes back
	Local $readBack = DllStructCreate('byte[16]')
	DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
		'handle', $processHandle, 'ptr', Ptr($testSc), _
		'ptr', DllStructGetPtr($readBack), 'ulong_ptr', 16, 'ulong_ptr*', 0)
	Local $hex = ""
	For $b = 1 To 12
		$hex &= Hex(DllStructGetData($readBack, 1, $b), 2) & " "
	Next
	ConsoleWrite("Shellcode bytes: " & $hex & @CRLF)

	; Verify queue is right
	Local $qc = MemoryRead($processHandle, GetLabel('QueueCounter'), 'dword')
	ConsoleWrite("QueueCounter: " & $qc & @CRLF)

	; Don't bail — continue with GWCA approach anyway
EndIf

; Get Play button info
Local $pf = GetFrameByHash($FRAME_HASH_PLAY_BUTTON)
If $pf[0] = 0 Then
	ConsoleWrite("ERROR: Play button not found" & @CRLF)
	Exit 1
EndIf
Local $framePtr = Int($pf[0])
Local $frameId = $pf[1]
ConsoleWrite("Play button: ptr=0x" & Hex($framePtr) & " id=" & $frameId & @CRLF)

; --- Inject GWCA ---
ConsoleWrite(@CRLF & "=== Injecting GWCA ===" & @CRLF)
Local $gwcaBase = _FindModule($gwPID, "gwca.dll")

If $gwcaBase = 0 Then
	Local $dllPath = "c:\Users\Robert\Documents\GWA Censured X BotsHub\toolbox\GWToolboxpp-master\Dependencies\GWCA\bin\gwca.dll"
	Local $dllPathW = StringToBinary($dllPath, 2) & Binary("0x0000")
	Local $pathLen = BinaryLen($dllPathW)

	Local $remotePath = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
		'handle', $processHandle, 'ptr', 0, 'ulong_ptr', $pathLen, _
		'dword', 0x1000, 'dword', 0x04)
	Local $pathBuf = DllStructCreate('byte[' & $pathLen & ']')
	DllStructSetData($pathBuf, 1, $dllPathW)
	DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
		'handle', $processHandle, 'ptr', $remotePath[0], _
		'ptr', DllStructGetPtr($pathBuf), 'ulong_ptr', $pathLen, 'ulong_ptr*', 0)

	Local $k32 = DllCall('kernel32.dll', 'ptr', 'GetModuleHandleW', 'wstr', 'kernel32.dll')
	Local $loadLib = DllCall('kernel32.dll', 'ptr', 'GetProcAddress', 'ptr', $k32[0], 'str', 'LoadLibraryW')
	Local $th = DllCall($kernel_handle, 'handle', 'CreateRemoteThread', _
		'handle', $processHandle, 'ptr', 0, 'ulong_ptr', 0, _
		'ptr', $loadLib[0], 'ptr', $remotePath[0], 'dword', 0, 'dword*', 0)
	DllCall($kernel_handle, 'dword', 'WaitForSingleObject', 'handle', $th[0], 'dword', 10000)
	Local $ec = DllCall($kernel_handle, 'bool', 'GetExitCodeThread', 'handle', $th[0], 'dword*', 0)
	$gwcaBase = $ec[2]
	DllCall($kernel_handle, 'bool', 'CloseHandle', 'handle', $th[0])
	DllCall($kernel_handle, 'bool', 'VirtualFreeEx', _
		'handle', $processHandle, 'ptr', $remotePath[0], 'ulong_ptr', 0, 'dword', 0x8000)

	If $gwcaBase = 0 Then
		ConsoleWrite("ERROR: injection failed" & @CRLF)
		Exit 1
	EndIf
	ConsoleWrite("Loaded at 0x" & Hex($gwcaBase) & @CRLF)

	; Scanner::Initialize
	Local $gwBase = $pe_sections_ranges[0][0] - 0x1000
	_RemoteCall($gwcaBase + 0x21850, $gwBase, 10000)
	ConsoleWrite("Scanner init done" & @CRLF)

	; GW::Initialize
	_RemoteCall($gwcaBase + 0x18E90, 0, 15000)
	ConsoleWrite("GW::Initialize done" & @CRLF)
	Sleep(2000)
Else
	ConsoleWrite("Already loaded at 0x" & Hex($gwcaBase) & @CRLF)
EndIf

; Verify GWCA state
Local $origFunc = MemoryRead($processHandle, $gwcaBase + 0x8A39C, 'dword')
ConsoleWrite("GWCA SendFrameUIMsg: 0x" & Hex($origFunc) & @CRLF)

; Re-check rendering hook after GWCA injection
ConsoleWrite("Re-checking rendering hook..." & @CRLF)
MemoryWrite($processHandle, $marker, 0xCCCC, 'dword')
Local $testSc2 = _Alloc(16)
Local $tb2 = DllStructCreate('byte[16]')
$tp = 1
DllStructSetData($tb2, 1, 0xC7, $tp)
$tp += 1
DllStructSetData($tb2, 1, 0x05, $tp)
$tp += 1
_WriteLE32($tb2, $tp, $marker)
$tp += 4
_WriteLE32($tb2, $tp, 0xDDDD)
$tp += 4
DllStructSetData($tb2, 1, 0xC3, $tp)
_WQ($testSc2, $tb2, $tp)
Sleep(2000)
$mv = MemoryRead($processHandle, $marker, 'dword')
ConsoleWrite("Post-GWCA hook test: marker=0x" & Hex($mv) & " (want 0xDDDD)" & @CRLF)

If $mv <> 0xDDDD Then
	ConsoleWrite("WARNING: Rendering hook broken after GWCA init!" & @CRLF)
	ConsoleWrite("Trying approaches anyway..." & @CRLF)
EndIf

; Screenshot before
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\gwca_fresh_before.png', $gwHWnd)

; =============================================================================
; APPROACH A: GWCA ButtonClick(frame_ptr) — cdecl wrapper
; =============================================================================
ConsoleWrite(@CRLF & "=== A: GWCA ButtonClick(0x" & Hex($framePtr) & ") ===" & @CRLF)
Local $scA = _Alloc(32)
Local $bA = DllStructCreate('byte[32]')
$tp = 1
; push frame_ptr
DllStructSetData($bA, 1, 0x68, $tp)
$tp += 1
_WriteLE32($bA, $tp, $framePtr)
$tp += 4
; call ButtonClick
DllStructSetData($bA, 1, 0xE8, $tp)
$tp += 1
_WriteLE32($bA, $tp, ($gwcaBase + 0x255E0) - ($scA + $tp - 1 + 4))
$tp += 4
; add esp, 4
DllStructSetData($bA, 1, 0x83, $tp)
$tp += 1
DllStructSetData($bA, 1, 0xC4, $tp)
$tp += 1
DllStructSetData($bA, 1, 0x04, $tp)
$tp += 1
; ret
DllStructSetData($bA, 1, 0xC3, $tp)
_WQ($scA, $bA, $tp)
If _Chk("ButtonClick(frame_ptr)") Then Exit 0

; =============================================================================
; APPROACH B: GWCA ButtonFrame::Click (thiscall)
; =============================================================================
ConsoleWrite(@CRLF & "=== B: GWCA ButtonFrame::Click ===" & @CRLF)
Local $scB = _Alloc(16)
Local $bB = DllStructCreate('byte[16]')
$tp = 1
DllStructSetData($bB, 1, 0xB9, $tp)
$tp += 1
_WriteLE32($bB, $tp, $framePtr)
$tp += 4
DllStructSetData($bB, 1, 0xE8, $tp)
$tp += 1
_WriteLE32($bB, $tp, ($gwcaBase + 0x16660) - ($scB + $tp - 1 + 4))
$tp += 4
DllStructSetData($bB, 1, 0xC3, $tp)
_WQ($scB, $bB, $tp)
If _Chk("ButtonFrame::Click") Then Exit 0

; =============================================================================
; APPROACH C: Use msg 0x2B (actual game call site message)
; with ECX = frame+0x84 (game's actual ECX adjustment)
; =============================================================================
ConsoleWrite(@CRLF & "=== C: msg=0x2B, ECX=frame+0x84 ===" & @CRLF)
Local $sendFunc = Int(GetLabel('SendFrameUIMsg'))
; Build a simple kMouseAction-like struct on the stack
; Actually, at the call site, wParam = lea eax,[ebp-0x14] = pointer to local struct
; The struct content is unknown. Let's try with our standard action data.
Local $actMem = _Alloc(32)
For $act = 6 To 7
	Local $ad = DllStructCreate('dword[5]')
	DllStructSetData($ad, 1, $frameId, 1)
	DllStructSetData($ad, 1, 7, 2)   ; childOffsetId
	DllStructSetData($ad, 1, $act, 3)
	DllStructSetData($ad, 1, 0, 4)
	DllStructSetData($ad, 1, 0, 5)
	DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
		'handle', $processHandle, 'ptr', Ptr($actMem), _
		'ptr', DllStructGetPtr($ad), 'ulong_ptr', 20, 'ulong_ptr*', 0)

	Local $scC = _Alloc(64)
	Local $fst = $scC + 56
	MemoryWrite($processHandle, $fst, $sendFunc, 'dword')
	Local $bC = DllStructCreate('byte[48]')
	$tp = 1
	; mov ecx, frame+0x84
	DllStructSetData($bC, 1, 0xB9, $tp)
	$tp += 1
	_WriteLE32($bC, $tp, $framePtr + 0x84)
	$tp += 4
	; push 0 (lParam)
	DllStructSetData($bC, 1, 0x6A, $tp)
	$tp += 1
	DllStructSetData($bC, 1, 0x00, $tp)
	$tp += 1
	; push actMem (wParam)
	DllStructSetData($bC, 1, 0x68, $tp)
	$tp += 1
	_WriteLE32($bC, $tp, $actMem)
	$tp += 4
	; push 0x2B (msgid from actual call site)
	DllStructSetData($bC, 1, 0x6A, $tp)
	$tp += 1
	DllStructSetData($bC, 1, 0x2B, $tp)
	$tp += 1
	; call [fst]
	DllStructSetData($bC, 1, 0xFF, $tp)
	$tp += 1
	DllStructSetData($bC, 1, 0x15, $tp)
	$tp += 1
	_WriteLE32($bC, $tp, $fst)
	$tp += 4
	; ret
	DllStructSetData($bC, 1, 0xC3, $tp)
	_WQ($scC, $bC, $tp)
	Sleep(100)
Next
If _Chk("msg=0x2B, ECX=frame+0x84") Then Exit 0

; =============================================================================
; APPROACH D: GWCA GetFrameById + ButtonClick
; =============================================================================
ConsoleWrite(@CRLF & "=== D: GWCA GetFrameById(" & $frameId & ") + Click ===" & @CRLF)
Local $scD = _Alloc(32)
Local $bD = DllStructCreate('byte[32]')
$tp = 1
; push frame_id
DllStructSetData($bD, 1, 0x6A, $tp)
$tp += 1
DllStructSetData($bD, 1, $frameId, $tp)
$tp += 1
; call GetFrameById
DllStructSetData($bD, 1, 0xE8, $tp)
$tp += 1
_WriteLE32($bD, $tp, ($gwcaBase + 0x25CC0) - ($scD + $tp - 1 + 4))
$tp += 4
; add esp, 4
DllStructSetData($bD, 1, 0x83, $tp)
$tp += 1
DllStructSetData($bD, 1, 0xC4, $tp)
$tp += 1
DllStructSetData($bD, 1, 0x04, $tp)
$tp += 1
; test eax,eax
DllStructSetData($bD, 1, 0x85, $tp)
$tp += 1
DllStructSetData($bD, 1, 0xC0, $tp)
$tp += 1
; jz +7
DllStructSetData($bD, 1, 0x74, $tp)
$tp += 1
DllStructSetData($bD, 1, 0x07, $tp)
$tp += 1
; mov ecx, eax
DllStructSetData($bD, 1, 0x8B, $tp)
$tp += 1
DllStructSetData($bD, 1, 0xC8, $tp)
$tp += 1
; call ButtonFrame::Click
DllStructSetData($bD, 1, 0xE8, $tp)
$tp += 1
_WriteLE32($bD, $tp, ($gwcaBase + 0x16660) - ($scD + $tp - 1 + 4))
$tp += 4
; ret
DllStructSetData($bD, 1, 0xC3, $tp)
_WQ($scD, $bD, $tp)
If _Chk("GetFrameById+Click") Then Exit 0

; --- Final ---
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\gwca_fresh_after.png', $gwHWnd)
ConsoleWrite(@CRLF & "=== ALL FAILED ===" & @CRLF)
Local $alive = DllCall($kernel_handle, 'bool', 'GetExitCodeProcess', 'handle', $processHandle, 'dword*', 0)
ConsoleWrite("Game alive: " & (IsArray($alive) And $alive[2] = 259) & @CRLF)
ConsoleWrite("=== DONE ===" & @CRLF)

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

Func _RemoteCall($func, $arg, $timeout)
	Local $t = DllCall($kernel_handle, 'handle', 'CreateRemoteThread', _
		'handle', $processHandle, 'ptr', 0, 'ulong_ptr', 0, _
		'ptr', Ptr($func), 'ptr', Ptr($arg), 'dword', 0, 'dword*', 0)
	If Not IsArray($t) Or $t[0] = 0 Then Return
	DllCall($kernel_handle, 'dword', 'WaitForSingleObject', 'handle', $t[0], 'dword', $timeout)
	DllCall($kernel_handle, 'bool', 'CloseHandle', 'handle', $t[0])
EndFunc

Func _FindModule($pid, $name)
	Local $sn = DllCall('kernel32.dll', 'handle', 'CreateToolhelp32Snapshot', 'dword', 0x8, 'dword', $pid)
	If Not IsArray($sn) Or $sn[0] = -1 Then Return 0
	Local $me = DllStructCreate('dword dwSize;dword th32ModuleID;dword th32ProcessID;dword GlblcntUsage;dword ProccntUsage;ptr modBaseAddr;dword modBaseSize;handle hModule;wchar szModule[256];wchar szExePath[260]')
	DllStructSetData($me, 'dwSize', DllStructGetSize($me))
	Local $r = DllCall('kernel32.dll', 'bool', 'Module32FirstW', 'handle', $sn[0], 'struct*', $me)
	While IsArray($r) And $r[0]
		If StringLower(DllStructGetData($me, 'szModule')) = StringLower($name) Then
			Local $base = Int(DllStructGetData($me, 'modBaseAddr'))
			DllCall('kernel32.dll', 'bool', 'CloseHandle', 'handle', $sn[0])
			Return $base
		EndIf
		$r = DllCall('kernel32.dll', 'bool', 'Module32NextW', 'handle', $sn[0], 'struct*', $me)
	WEnd
	DllCall('kernel32.dll', 'bool', 'CloseHandle', 'handle', $sn[0])
	Return 0
EndFunc
