#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

; =============================================================================
; test_gwca_scanner_only.au3
;
; Inject gwca.dll + Scanner::Initialize ONLY (no GW::Initialize).
; This populates GWCA's function pointer table without setting up hooks
; that conflict with BotsHub.
;
; Then call GWCA's ButtonClick via our rendering hook.
; =============================================================================

ConsoleWrite("=== GWCA Scanner-Only Click Test ===" & @CRLF)

ScanAndUpdateGameClients()
If $game_clients[0][0] = 0 Then
	ConsoleWrite("Launching BEASTRIT..." & @CRLF)
	Local $accounts = GWLauncher_LoadAccounts()
	Local $accIdx = GWLauncher_FindAccountByCharacter($accounts, "B E A S T R I T")
	Local $result = GWLauncher_LaunchAccount($accounts, $accIdx)
	ConsoleWrite("PID=" & $result[0] & ", waiting 40s..." & @CRLF)
	Sleep(40000)
	ScanAndUpdateGameClients()
EndIf

SelectClient($game_clients[0][0])
InitializeGameClientForGWA2(False)
Local $processHandle = GetProcessHandle()
Local $gwPID = $game_clients[$game_clients[0][0]][0]
Local $gwHWnd = $game_clients[$game_clients[0][0]][2]
ConsoleWrite("PID=" & $gwPID & @CRLF)

WinActivate($gwHWnd)
Sleep(2000)

; Verify rendering hook
Local $marker = _Alloc(4)
MemoryWrite($processHandle, $marker, 0xAAAA, 'dword')
Local $sc0 = _Alloc(16)
Local $b0 = DllStructCreate('byte[16]')
Local $p = 1
DllStructSetData($b0, 1, 0xC7, $p)
$p += 1
DllStructSetData($b0, 1, 0x05, $p)
$p += 1
_WriteLE32($b0, $p, $marker)
$p += 4
_WriteLE32($b0, $p, 0xBEEF)
$p += 4
DllStructSetData($b0, 1, 0xC3, $p)
_WQ($sc0, $b0, $p)
Sleep(1500)
Local $mv = MemoryRead($processHandle, $marker, 'dword')
ConsoleWrite("Rendering hook: 0x" & Hex($mv) & " (want 0xBEEF)" & @CRLF)
If $mv <> 0xBEEF Then
	ConsoleWrite("ERROR: Rendering hook not working" & @CRLF)
	Exit 1
EndIf

; Get Play button
Local $pf = GetFrameByHash($FRAME_HASH_PLAY_BUTTON)
If $pf[0] = 0 Then
	ConsoleWrite("ERROR: Play button not found" & @CRLF)
	Exit 1
EndIf
Local $fp = Int($pf[0])
Local $fid = $pf[1]
ConsoleWrite("Play: ptr=0x" & Hex($fp) & " id=" & $fid & @CRLF)

; --- Inject gwca.dll + Scanner::Initialize ONLY ---
ConsoleWrite(@CRLF & "=== Injecting GWCA (Scanner only) ===" & @CRLF)
Local $gwcaBase = _FindModule($gwPID, "gwca.dll")

If $gwcaBase = 0 Then
	Local $dllPath = "c:\Users\Robert\Documents\GWA Censured X BotsHub\toolbox\GWToolboxpp-master\Dependencies\GWCA\bin\gwca.dll"
	Local $dllPathW = StringToBinary($dllPath, 2) & Binary("0x0000")
	Local $pathLen = BinaryLen($dllPathW)
	Local $rp = DllCall($kernel_handle, 'ptr', 'VirtualAllocEx', _
		'handle', $processHandle, 'ptr', 0, 'ulong_ptr', $pathLen, _
		'dword', 0x1000, 'dword', 0x04)
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
	DllCall($kernel_handle, 'dword', 'WaitForSingleObject', 'handle', $th[0], 'dword', 10000)
	Local $ec = DllCall($kernel_handle, 'bool', 'GetExitCodeThread', 'handle', $th[0], 'dword*', 0)
	$gwcaBase = $ec[2]
	DllCall($kernel_handle, 'bool', 'CloseHandle', 'handle', $th[0])
	DllCall($kernel_handle, 'bool', 'VirtualFreeEx', _
		'handle', $processHandle, 'ptr', $rp[0], 'ulong_ptr', 0, 'dword', 0x8000)

	If $gwcaBase = 0 Then
		ConsoleWrite("ERROR: injection failed" & @CRLF)
		Exit 1
	EndIf

	; Scanner::Initialize ONLY
	Local $gwBase = $pe_sections_ranges[0][0] - 0x1000
	Local $si = DllCall($kernel_handle, 'handle', 'CreateRemoteThread', _
		'handle', $processHandle, 'ptr', 0, 'ulong_ptr', 0, _
		'ptr', Ptr($gwcaBase + 0x21850), 'ptr', Ptr($gwBase), 'dword', 0, 'dword*', 0)
	DllCall($kernel_handle, 'dword', 'WaitForSingleObject', 'handle', $si[0], 'dword', 10000)
	DllCall($kernel_handle, 'bool', 'CloseHandle', 'handle', $si[0])

	; NO GW::Initialize — skip it!
	ConsoleWrite("gwca.dll at 0x" & Hex($gwcaBase) & " (Scanner only, no GW::Init)" & @CRLF)
EndIf

; Verify scanner populated some pointers
Local $origSF = MemoryRead($processHandle, $gwcaBase + 0x8A39C, 'dword')
Local $getChild = MemoryRead($processHandle, $gwcaBase + 0x8A37C, 'dword')
Local $rootFrame = MemoryRead($processHandle, $gwcaBase + 0x8A410, 'dword')
Local $frameHash = MemoryRead($processHandle, $gwcaBase + 0x8A3B0, 'dword')
ConsoleWrite("GWCA ptrs: SendFrame=0x" & Hex($origSF) & " GetChild=0x" & Hex($getChild) & _
	" Root=0x" & Hex($rootFrame) & " HashTbl=0x" & Hex($frameHash) & @CRLF)

; Verify rendering hook STILL works after gwca injection (no GW::Init = no hooks)
MemoryWrite($processHandle, $marker, 0x3333, 'dword')
Local $sc0b = _Alloc(16)
Local $b0b = DllStructCreate('byte[16]')
$p = 1
DllStructSetData($b0b, 1, 0xC7, $p)
$p += 1
DllStructSetData($b0b, 1, 0x05, $p)
$p += 1
_WriteLE32($b0b, $p, $marker)
$p += 4
_WriteLE32($b0b, $p, 0x4444)
$p += 4
DllStructSetData($b0b, 1, 0xC3, $p)
_WQ($sc0b, $b0b, $p)
Sleep(1500)
$mv = MemoryRead($processHandle, $marker, 'dword')
ConsoleWrite("Post-injection hook: 0x" & Hex($mv) & " (want 0x4444)" & @CRLF)
If $mv <> 0x4444 Then
	ConsoleWrite("ERROR: Hook broken after gwca injection!" & @CRLF)
	Exit 1
EndIf
ConsoleWrite("Rendering hook still active!" & @CRLF)

; Screenshot
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\gwca_scanner_before.png', $gwHWnd)

; =============================================================================
; APPROACH A: GWCA ButtonClick(frame_ptr) — uses Scanner's pointers
; =============================================================================
ConsoleWrite(@CRLF & "=== A: GWCA ButtonClick ===" & @CRLF)
Local $scA = _Alloc(32)
Local $bA = DllStructCreate('byte[32]')
$p = 1
DllStructSetData($bA, 1, 0x68, $p)
$p += 1
_WriteLE32($bA, $p, $fp)
$p += 4
DllStructSetData($bA, 1, 0xE8, $p)
$p += 1
_WriteLE32($bA, $p, ($gwcaBase + 0x255E0) - ($scA + $p - 1 + 4))
$p += 4
DllStructSetData($bA, 1, 0x83, $p)
$p += 1
DllStructSetData($bA, 1, 0xC4, $p)
$p += 1
DllStructSetData($bA, 1, 0x04, $p)
$p += 1
DllStructSetData($bA, 1, 0xC3, $p)

; Verify shellcode bytes
DllCall($kernel_handle, 'bool', 'WriteProcessMemory', _
	'handle', $processHandle, 'ptr', Ptr($scA), _
	'ptr', DllStructGetPtr($bA), 'ulong_ptr', $p, 'ulong_ptr*', 0)
Local $readBack = DllStructCreate('byte[16]')
DllCall($kernel_handle, 'bool', 'ReadProcessMemory', _
	'handle', $processHandle, 'ptr', Ptr($scA), _
	'ptr', DllStructGetPtr($readBack), 'ulong_ptr', 14, 'ulong_ptr*', 0)
Local $hex = ""
For $b = 1 To 14
	$hex &= Hex(DllStructGetData($readBack, 1, $b), 2) & " "
Next
ConsoleWrite("Shellcode bytes: " & $hex & @CRLF)

_WQ2($scA)
If _Chk("ButtonClick(frame_ptr)") Then Exit 0

; =============================================================================
; APPROACH B: GWCA ButtonFrame::Click (thiscall)
; =============================================================================
ConsoleWrite(@CRLF & "=== B: ButtonFrame::Click ===" & @CRLF)
Local $scB = _Alloc(16)
Local $bB = DllStructCreate('byte[16]')
$p = 1
DllStructSetData($bB, 1, 0xB9, $p)
$p += 1
_WriteLE32($bB, $p, $fp)
$p += 4
DllStructSetData($bB, 1, 0xE8, $p)
$p += 1
_WriteLE32($bB, $p, ($gwcaBase + 0x16660) - ($scB + $p - 1 + 4))
$p += 4
DllStructSetData($bB, 1, 0xC3, $p)
_WQ($scB, $bB, $p)
If _Chk("ButtonFrame::Click") Then Exit 0

; =============================================================================
; APPROACH C: GWCA GetFrameById + Click
; =============================================================================
ConsoleWrite(@CRLF & "=== C: GetFrameById(" & $fid & ") + Click ===" & @CRLF)
Local $scC = _Alloc(32)
Local $bC = DllStructCreate('byte[32]')
$p = 1
DllStructSetData($bC, 1, 0x6A, $p)
$p += 1
DllStructSetData($bC, 1, $fid, $p)
$p += 1
DllStructSetData($bC, 1, 0xE8, $p)
$p += 1
_WriteLE32($bC, $p, ($gwcaBase + 0x25CC0) - ($scC + $p - 1 + 4))
$p += 4
DllStructSetData($bC, 1, 0x83, $p)
$p += 1
DllStructSetData($bC, 1, 0xC4, $p)
$p += 1
DllStructSetData($bC, 1, 0x04, $p)
$p += 1
DllStructSetData($bC, 1, 0x85, $p)
$p += 1
DllStructSetData($bC, 1, 0xC0, $p)
$p += 1
DllStructSetData($bC, 1, 0x74, $p)
$p += 1
DllStructSetData($bC, 1, 0x07, $p)
$p += 1
DllStructSetData($bC, 1, 0x8B, $p)
$p += 1
DllStructSetData($bC, 1, 0xC8, $p)
$p += 1
DllStructSetData($bC, 1, 0xE8, $p)
$p += 1
_WriteLE32($bC, $p, ($gwcaBase + 0x16660) - ($scC + $p - 1 + 4))
$p += 4
DllStructSetData($bC, 1, 0xC3, $p)
_WQ($scC, $bC, $p)
If _Chk("GetFrameById+Click") Then Exit 0

; =============================================================================
; APPROACH D: GWCA ButtonClick(frame_id) — it might take ID not ptr
; =============================================================================
ConsoleWrite(@CRLF & "=== D: ButtonClick(frame_id=" & $fid & ") ===" & @CRLF)
Local $scD = _Alloc(32)
Local $bD = DllStructCreate('byte[32]')
$p = 1
DllStructSetData($bD, 1, 0x6A, $p)
$p += 1
DllStructSetData($bD, 1, $fid, $p)
$p += 1
DllStructSetData($bD, 1, 0xE8, $p)
$p += 1
_WriteLE32($bD, $p, ($gwcaBase + 0x255E0) - ($scD + $p - 1 + 4))
$p += 4
DllStructSetData($bD, 1, 0x83, $p)
$p += 1
DllStructSetData($bD, 1, 0xC4, $p)
$p += 1
DllStructSetData($bD, 1, 0x04, $p)
$p += 1
DllStructSetData($bD, 1, 0xC3, $p)
_WQ($scD, $bD, $p)
If _Chk("ButtonClick(frame_id)") Then Exit 0

; --- Final ---
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\gwca_scanner_after.png', $gwHWnd)
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

Func _WQ2($addr)
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
