#RequireAdmin
#include "lib\Froggy_Includes.au3"
#include <ScreenCapture.au3>

; =============================================================================
; test_gwca_manual_ptrs.au3
;
; Inject gwca.dll, skip GW::Initialize, manually write function pointers
; into GWCA's data section using known offsets from BotsHub scan.
; Then call ButtonClick/ButtonFrame::Click via rendering hook.
;
; GWCA function pointer offsets (from inject_research, confirmed stable):
;   +0x8A39C: SendFrameUIMsg (original)  → game+0x2286D0
;   +0x8A3A0: SendFrameUIMsg (hooked)    → skip (we want original)
;   +0x8A37C: GetChildFrame              → game+0x20E2B0
;   +0x8A410: RootFrame                  → game+0x22DC20
;   +0x8A3B0: Frame hash table           → FrameArray label value
;   +0x8A3D0: SetWindowVisible           → unknown, leave 0
; =============================================================================

ConsoleWrite("=== GWCA Manual Pointers Test ===" & @CRLF)

; Use existing client or launch
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
MemoryWrite($processHandle, $marker, 0x1111, 'dword')
Local $sc0 = _Alloc(16)
Local $b0 = DllStructCreate('byte[16]')
Local $p = 1
DllStructSetData($b0, 1, 0xC7, $p)
$p += 1
DllStructSetData($b0, 1, 0x05, $p)
$p += 1
_WriteLE32($b0, $p, $marker)
$p += 4
_WriteLE32($b0, $p, 0x2222)
$p += 4
DllStructSetData($b0, 1, 0xC3, $p)
_WQ($sc0, $b0, $p)
Sleep(1500)
If MemoryRead($processHandle, $marker, 'dword') <> 0x2222 Then
	ConsoleWrite("ERROR: Rendering hook not active" & @CRLF)
	Exit 1
EndIf
ConsoleWrite("Rendering hook active" & @CRLF)

; Get game base and Play button
Local $gwBase = $pe_sections_ranges[0][0] - 0x1000
ConsoleWrite("GW base: 0x" & Hex($gwBase) & @CRLF)

Local $pf = GetFrameByHash($FRAME_HASH_PLAY_BUTTON)
If $pf[0] = 0 Then
	ConsoleWrite("ERROR: Play button not found" & @CRLF)
	Exit 1
EndIf
Local $fp = Int($pf[0])
Local $fid = $pf[1]
ConsoleWrite("Play: ptr=0x" & Hex($fp) & " id=" & $fid & @CRLF)

; --- Inject gwca.dll ---
Local $gwcaBase = _FindModule($gwPID, "gwca.dll")
If $gwcaBase = 0 Then
	ConsoleWrite("Injecting gwca.dll..." & @CRLF)
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
EndIf
ConsoleWrite("gwca.dll at 0x" & Hex($gwcaBase) & @CRLF)

; --- Manually populate GWCA data section ---
; Known offsets (from game base):
;   SendFrameUIMsg:  +0x2286D0
;   GetChildFrame:   +0x20E2B0
;   RootFrame:       +0x22DC20
;   FrameHashTable:  from FrameArray label
ConsoleWrite(@CRLF & "=== Populating GWCA data section ===" & @CRLF)

Local $sendFrameAddr = $gwBase + 0x2286D0
Local $getChildAddr = $gwBase + 0x20E2B0
Local $rootFrameAddr = $gwBase + 0x22DC20
Local $frameArrayAddr = Int(GetLabel('FrameArray'))

ConsoleWrite("Writing SendFrameUIMsg: 0x" & Hex($sendFrameAddr) & " -> +0x8A39C" & @CRLF)
MemoryWrite($processHandle, $gwcaBase + 0x8A39C, $sendFrameAddr, 'dword')

ConsoleWrite("Writing GetChildFrame: 0x" & Hex($getChildAddr) & " -> +0x8A37C" & @CRLF)
MemoryWrite($processHandle, $gwcaBase + 0x8A37C, $getChildAddr, 'dword')

ConsoleWrite("Writing RootFrame: 0x" & Hex($rootFrameAddr) & " -> +0x8A410" & @CRLF)
MemoryWrite($processHandle, $gwcaBase + 0x8A410, $rootFrameAddr, 'dword')

ConsoleWrite("Writing FrameHashTable: 0x" & Hex($frameArrayAddr) & " -> +0x8A3B0" & @CRLF)
MemoryWrite($processHandle, $gwcaBase + 0x8A3B0, $frameArrayAddr, 'dword')

; Verify
ConsoleWrite("Verify: " & @CRLF)
ConsoleWrite("  SendFrame: 0x" & Hex(MemoryRead($processHandle, $gwcaBase + 0x8A39C, 'dword')) & @CRLF)
ConsoleWrite("  GetChild: 0x" & Hex(MemoryRead($processHandle, $gwcaBase + 0x8A37C, 'dword')) & @CRLF)
ConsoleWrite("  Root: 0x" & Hex(MemoryRead($processHandle, $gwcaBase + 0x8A410, 'dword')) & @CRLF)
ConsoleWrite("  HashTbl: 0x" & Hex(MemoryRead($processHandle, $gwcaBase + 0x8A3B0, 'dword')) & @CRLF)

; Verify rendering hook still works
MemoryWrite($processHandle, $marker, 0x5555, 'dword')
Local $sc0b = _Alloc(16)
Local $b0b = DllStructCreate('byte[16]')
$p = 1
DllStructSetData($b0b, 1, 0xC7, $p)
$p += 1
DllStructSetData($b0b, 1, 0x05, $p)
$p += 1
_WriteLE32($b0b, $p, $marker)
$p += 4
_WriteLE32($b0b, $p, 0x6666)
$p += 4
DllStructSetData($b0b, 1, 0xC3, $p)
_WQ($sc0b, $b0b, $p)
Sleep(1500)
If MemoryRead($processHandle, $marker, 'dword') <> 0x6666 Then
	ConsoleWrite("ERROR: Rendering hook broken!" & @CRLF)
	Exit 1
EndIf
ConsoleWrite("Rendering hook still active" & @CRLF)

; Screenshot
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\gwca_manual_before.png', $gwHWnd)

; =============================================================================
; APPROACH 1: GWCA ButtonClick(frame_ptr) — now with manual function pointers
; =============================================================================
ConsoleWrite(@CRLF & "=== ButtonClick(frame_ptr) ===" & @CRLF)
Local $sc1 = _Alloc(32)
Local $b1 = DllStructCreate('byte[32]')
$p = 1
DllStructSetData($b1, 1, 0x68, $p)
$p += 1
_WriteLE32($b1, $p, $fp)
$p += 4
DllStructSetData($b1, 1, 0xE8, $p)
$p += 1
_WriteLE32($b1, $p, ($gwcaBase + 0x255E0) - ($sc1 + $p - 1 + 4))
$p += 4
DllStructSetData($b1, 1, 0x83, $p)
$p += 1
DllStructSetData($b1, 1, 0xC4, $p)
$p += 1
DllStructSetData($b1, 1, 0x04, $p)
$p += 1
DllStructSetData($b1, 1, 0xC3, $p)
_WQ($sc1, $b1, $p)
If _Chk("ButtonClick(frame_ptr)") Then Exit 0

; =============================================================================
; APPROACH 2: ButtonFrame::Click (thiscall)
; =============================================================================
ConsoleWrite(@CRLF & "=== ButtonFrame::Click ===" & @CRLF)
Local $sc2 = _Alloc(16)
Local $b2 = DllStructCreate('byte[16]')
$p = 1
DllStructSetData($b2, 1, 0xB9, $p)
$p += 1
_WriteLE32($b2, $p, $fp)
$p += 4
DllStructSetData($b2, 1, 0xE8, $p)
$p += 1
_WriteLE32($b2, $p, ($gwcaBase + 0x16660) - ($sc2 + $p - 1 + 4))
$p += 4
DllStructSetData($b2, 1, 0xC3, $p)
_WQ($sc2, $b2, $p)
If _Chk("ButtonFrame::Click") Then Exit 0

; =============================================================================
; APPROACH 3: GetFrameById + Click
; =============================================================================
ConsoleWrite(@CRLF & "=== GetFrameById + Click ===" & @CRLF)
Local $sc3 = _Alloc(32)
Local $b3 = DllStructCreate('byte[32]')
$p = 1
DllStructSetData($b3, 1, 0x6A, $p)
$p += 1
DllStructSetData($b3, 1, $fid, $p)
$p += 1
DllStructSetData($b3, 1, 0xE8, $p)
$p += 1
_WriteLE32($b3, $p, ($gwcaBase + 0x25CC0) - ($sc3 + $p - 1 + 4))
$p += 4
DllStructSetData($b3, 1, 0x83, $p)
$p += 1
DllStructSetData($b3, 1, 0xC4, $p)
$p += 1
DllStructSetData($b3, 1, 0x04, $p)
$p += 1
DllStructSetData($b3, 1, 0x85, $p)
$p += 1
DllStructSetData($b3, 1, 0xC0, $p)
$p += 1
DllStructSetData($b3, 1, 0x74, $p)
$p += 1
DllStructSetData($b3, 1, 0x07, $p)
$p += 1
DllStructSetData($b3, 1, 0x8B, $p)
$p += 1
DllStructSetData($b3, 1, 0xC8, $p)
$p += 1
DllStructSetData($b3, 1, 0xE8, $p)
$p += 1
_WriteLE32($b3, $p, ($gwcaBase + 0x16660) - ($sc3 + $p - 1 + 4))
$p += 4
DllStructSetData($b3, 1, 0xC3, $p)
_WQ($sc3, $b3, $p)
If _Chk("GetFrameById+Click") Then Exit 0

; --- Final ---
_ScreenCapture_CaptureWnd(@ScriptDir & '\tests\gwca_manual_after.png', $gwHWnd)
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
